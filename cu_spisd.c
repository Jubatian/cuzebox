/*
 *  SD card peripheral (on SPI bus)
 *
 *  Copyright (C) 2016
 *    Sandor Zsuga (Jubatian)
 *  Uzem (the base of CUzeBox) is copyright (C)
 *    David Etherton,
 *    Eric Anderton,
 *    Alec Bourque (Uze),
 *    Filipe Rinaldi,
 *    Sandor Zsuga (Jubatian),
 *    Matt Pandina (Artcfox)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/



#include "cu_spisd.h"
#include "cu_vfat.h"



/* SD card state */
static cu_state_spisd_t sd_state;

/* CRC7 table (for SD commands) */
static auint sd_crc7_table[256];

/* CRC16 table (for SD data) */
static auint sd_crc16_table[256];



/* 400KHz SPI transfer clocks */
#define SPI_400   ( 71U * 8U)
/* 100KHz SPI transfer clocks */
#define SPI_100   (287U * 8U)
/* 1ms clocks */
#define SPI_1MS   28634U

/* Frequency high limit during initializing. Note that there are a few games
** which don't respect the 400 KHz recommendation (notably Alter Ego), to keep
** them working, the high limit has to be turned off (set anything less or
** equal to 16). To get strict SD, use SPI_400 here. */
#define INIF_HI   16U

/* Frequency low limit during initializing. */
#define INIF_LO   SPI_100

/* Milliseconds to require after pulling high CS for a successful
** initialization. Some games doesn't respect this (notably Alter Ego), to
** keep them working, use zero here. Otherwise set it to 1 or 2. */
#define INICS_T   0U

/* Milliseconds taken for the card's initialization */
#define SD_INIMS  500U

/* Command response time (bytes, should be 0 - 8). Note: There is a buggy
** version of Tempest which only works with this set zero (that is, it doesn't
** handle 0xFF stuff bytes before the response for certain commands). */
#define CMD_N     0U

/* Data preparation time for reads (bytes, arbitrary) */
#define READ_P_N  2U

/* Milliseconds of busy timeout after writes */
#define WRITE_T   100U


/* SD card state machine states */
/* Uninitialized: waiting for CS and DI going high */
#define STAT_UNINIT     0U
/* Native mode initializing: waiting for 74 pulses (10 data transmissions of 0xFF) */
#define STAT_NINIT      1U
/* Native mode */
#define STAT_NATIVE     2U
/* Idle state */
#define STAT_IDLE       3U
/* Verified state (a CMD8 was sent, so ACMD41 becomes enabled) */
#define STAT_VERIFIED   4U
/* Initializing */
#define STAT_IINIT      5U
/* Available for data transfer */
#define STAT_AVAIL      6U
/* Read single block */
#define STAT_CMD17      7U
/* Read multiple blocks */
#define STAT_CMD18      8U
/* Write single block */
#define STAT_CMD24      9U
/* Write multiple blocks */
#define STAT_CMD25     10U


/* Command state machine. The low 6 bits hold the command */
/* Receiving flag: If set, a command is under reception */
#define SCMD_R      0x100U
/* Application flag: If set, the next command will be an App Command */
#define SCMD_A      0x040U
/* Ticking out command response time */
#define SCMD_N      0x200U
/* Ticking out extra response bytes (beyond the R1) */
#define SCMD_X      0x400U

/* R1 Response flags */
#define R1_IDLE      0x01U
#define R1_ERES      0x02U
#define R1_ILL       0x04U
#define R1_CRC       0x08U
#define R1_ESEQ      0x10U
#define R1_ADDR      0x20U
#define R1_PAR       0x40U

/* Data transmission state machine */
/* Idle */
#define PSTAT_IDLE      0U
/* Read data preparation & data token */
#define PSTAT_RPREP     1U
/* Read data bytes (512) */
#define PSTAT_RDATA     2U
/* Read CRC bytes (2) */
#define PSTAT_RCRC      3U
/* CMD24 Write wait for data token */
#define PSTAT_W24PREP   4U
/* CMD24 Write accept data (512) */
#define PSTAT_W24DATA   5U
/* CMD24 Write accept CRC (2) */
#define PSTAT_W24CRC    6U
/* CMD25 Write wait for data token */
#define PSTAT_W25PREP   7U
/* CMD25 Write accept data (512) */
#define PSTAT_W25DATA   8U
/* CMD25 Write accept CRC (2) */
#define PSTAT_W25CRC    9U
/* Busy after the end of writes */
#define PSTAT_WBUSY    10U



/*
** Running CRC7 calculation for a byte.
*/
static auint cu_spisd_crc7_byte(auint crcval, auint byte)
{
 return sd_crc7_table[(byte ^ (crcval << 1)) & 0xFFU];
}



/*
** Running CRC16 calculation for a byte.
*/
static auint cu_spisd_crc16_byte(auint crcval, auint byte)
{
 return (sd_crc16_table[(byte ^ (crcval >> 8)) & 0xFFU] ^ (crcval << 8)) & 0xFFFFU;
}



/*
** Resets SD card peripheral. Cycle is the CPU cycle when it happens which
** might be used for emulating timing constraints.
*/
void  cu_spisd_reset(auint cycle)
{
 auint byt;
 auint bit;
 auint crc;

 sd_state.ena   = FALSE;
 sd_state.crc   = TRUE;
 sd_state.enac  = cycle;
 sd_state.state = STAT_UNINIT;
 sd_state.next  = cycle;
 sd_state.recvc = cycle;
 sd_state.evcnt = 0U;
 sd_state.cmd   = 0U;
 sd_state.crarg = 0U;
 sd_state.r1    = 0U;
 sd_state.data  = 0xFFU;
 sd_state.pstat = PSTAT_IDLE;

 /* Generate CRC7 table */

 for (byt = 0U; byt < 256U; byt ++){
  crc = byt;
  if ((crc & 0x80U) != 0U){ crc ^= 0x89U; }
  for (bit = 1U; bit < 8U; bit ++){
   crc <<= 1;
   if ((crc & 0x80U) != 0U){ crc ^= 0x89U; }
  }
  sd_crc7_table[byt] = (crc & 0x7FU);
 }

 /* Generate CRC16 table */

 for (byt = 0U; byt < 256U; byt ++){
  crc = byt << 8;
  for (bit = 0U; bit < 8U; bit ++){
   crc <<= 1;
   if ((crc & 0x10000U) != 0U){ crc ^= 0x1021U; }
  }
  sd_crc16_table[byt] = (crc & 0xFFFFU);
 }

 cu_vfat_reset();
}



/*
** Sets chip select's state, TRUE to enable, FALSE to disable.
*/
void  cu_spisd_cs_set(boole ena, auint cycle)
{
 if ( (sd_state.ena && (!ena)) ||
      ((!sd_state.ena) && ena) ){
  sd_state.ena  = ena;
  sd_state.enac = cycle;
  if (sd_state.ena == FALSE){ /* Kill any command if CS goes away */
   sd_state.cmd &= SCMD_A;    /* (Except for the app. command flag which indicates waiting for such a command) */
  }
 }
}



/*
** Sends a byte of data to the SD card. The passed cycle corresponds the cycle
** when it was clocked out of the AVR.
*/
void  cu_spisd_send(auint data, auint cycle)
{
 boole atend = FALSE;   /* Mark that a command ended, need to form a response */
 boole write = FALSE;   /* In writing (to block command processing) */
 auint cmd   = sd_state.cmd; /* Save for command processing */

 sd_state.data = 0xFFU; /* Default data out */

 /* Data transmission state machine (lowest priority in determining output
 ** data bytes) */

 switch (sd_state.pstat){

  case PSTAT_IDLE:      /* Idle */
   break;

  case PSTAT_RPREP:     /* Read data preparation & data token */
   if (sd_state.ppos >= READ_P_N){
    sd_state.data  = 0xFEU; /* Read data token */
    sd_state.pstat = PSTAT_RDATA;
    sd_state.ppos  = 0U;
    sd_state.cc16v = 0x0000U;
   }else{
    sd_state.ppos  ++;
   }
   break;

  case PSTAT_RDATA:     /* Read data bytes (512) */
   sd_state.data  = cu_vfat_read((sd_state.paddr << 9) + sd_state.ppos);
   sd_state.cc16v = cu_spisd_crc16_byte(sd_state.cc16v, sd_state.data);
   sd_state.ppos  ++;
   if (sd_state.ppos == 512U){
    sd_state.pstat = PSTAT_RCRC;
    sd_state.ppos  = 0U;
   }
   break;

  case PSTAT_RCRC:      /* Read CRC bytes (2) */
   if       (sd_state.ppos == 0U){
    sd_state.data  = (sd_state.cc16v >> 8) & 0xFFU;
   }else{
    sd_state.data  = (sd_state.cc16v     ) & 0xFFU;
    sd_state.pstat = PSTAT_IDLE;
   }
   sd_state.ppos  ++;
   break;

  case PSTAT_W24PREP:   /* CMD24 Write wait for data token */
   if (sd_state.ppos != 0U){
    if (data == 0xFEU){ /* Data token */
     sd_state.pstat = PSTAT_W24DATA;
     sd_state.ppos  = 0U;
     sd_state.cc16v = 0x0000U;
    }
   }else{
    sd_state.ppos ++;
   }
   write = TRUE;
   break;

  case PSTAT_W24DATA:   /* CMD24 Write accept data (512) */
   cu_vfat_write((sd_state.paddr << 9) + sd_state.ppos, data);
   sd_state.cc16v = cu_spisd_crc16_byte(sd_state.cc16v, data);
   sd_state.ppos  ++;
   if (sd_state.ppos == 512U){
    sd_state.pstat = PSTAT_W24CRC;
    sd_state.ppos  = 0U;
   }
   write = TRUE;
   break;

  case PSTAT_W24CRC:    /* CMD24 Write accept CRC (2) */
   if       (sd_state.ppos == 0U){
    sd_state.cc16c = data << 8;
   }else{
    sd_state.cc16c |= data;
    sd_state.pstat = PSTAT_WBUSY;
    sd_state.next  = WRAP32(cycle + (WRITE_T * SPI_1MS));
    if ((sd_state.crc) && (sd_state.cc16c != sd_state.cc16v)){
     /* Note: Actual rejection is not implemented (data is still written) */
     sd_state.data  = 0x0BU; /* Data response token: Rejected due to CRC. */
    }else{
     sd_state.data  = 0x05U; /* Data response token: Accepted. */
    }
   }
   sd_state.ppos  ++;
   write = TRUE;
   break;

  case PSTAT_W25PREP:   /* CMD25 Write wait for data token */
   if (sd_state.ppos != 0U){
    if (data == 0xFCU){    /* Data token */
     sd_state.pstat = PSTAT_W25DATA;
     sd_state.ppos  = 0U;
     sd_state.cc16v = 0x0000U;
    }else{
     if (data == 0xFDU){   /* Stop transmission */
      sd_state.pstat = PSTAT_WBUSY;
      sd_state.next  = WRAP32(cycle + (WRITE_T * SPI_1MS));
     }
    }
   }else{
    sd_state.ppos ++;
   }
   write = TRUE;
   break;

  case PSTAT_W25DATA:   /* CMD25 Write accept data (512) */
   cu_vfat_write((sd_state.paddr << 9) + sd_state.ppos, data);
   sd_state.cc16v = cu_spisd_crc16_byte(sd_state.cc16v, data);
   sd_state.ppos  ++;
   if (sd_state.ppos == 512U){
    sd_state.pstat = PSTAT_W25CRC;
    sd_state.ppos  = 0U;
   }
   write = TRUE;
   break;

  case PSTAT_W25CRC:    /* CMD25 Write accept CRC (2) */
   if       (sd_state.ppos == 0U){
    sd_state.cc16c = data << 8;
    sd_state.ppos  ++;
   }else{
    sd_state.cc16c |= data;
    sd_state.pstat = PSTAT_W25PREP;
    sd_state.ppos  = 0U;    /* Skip being busy here */
    if ((sd_state.crc) && (sd_state.cc16c != sd_state.cc16v)){
     /* Note: Actual rejection is not implemented (data is still written) */
     sd_state.data  = 0x0BU; /* Data response token: Rejected due to CRC. */
    }else{
     sd_state.data  = 0x05U; /* Data response token: Accepted. */
    }
   }
   write = TRUE;
   break;

  case PSTAT_WBUSY:     /* Busy after the end of writes */
   if (WRAP32(sd_state.next - cycle) >= 0x80000000U){ /* Done */
    sd_state.pstat = PSTAT_IDLE;
   }else{
    if (sd_state.ena){  /* Only pulls it low if Chip Select is enabled */
     sd_state.data = 0x00U;
    }
   }
   break;

  default:
   break;

 }

 /* Generic SD command processing */

 if ((sd_state.ena) && (!write)){

  if ( (sd_state.state != STAT_UNINIT) &&
       (sd_state.state != STAT_NINIT) ){

   if       ((sd_state.cmd & SCMD_X) != 0U){ /* Returning extra response bytes */

    sd_state.cmd = SCMD_X;    /* Just make sure only SCMD_X is present (see hack below) */
    if (sd_state.evcnt < 4U){ /* Data bytes */
     sd_state.data = (sd_state.crarg >> ((3U - sd_state.evcnt) * 8U)) & 0xFFU;
     sd_state.evcnt ++;
    }else{                    /* End of response */
     sd_state.cmd = 0U;
    }

   }

   /* Normally this below should be in an "else if". This is non-standard as
   ** it allows breaking a response to interpret a new command. This bypass is
   ** added to support Tempest which doesn't respect the R1b response, thus
   ** allowing that game to bypass it (that game only works on such SD cards
   ** which send very few "busy" bytes in an R1b, that is, less than 4). */

   if       ((sd_state.cmd & SCMD_R) == 0U){ /* Waiting for command */

    if ((data & 0xC0U) == 0x40U){            /* Valid command byte */
     sd_state.cmd = (sd_state.cmd & SCMD_A) |
                    (data & 0x3FU) | /* Receiving */
                    SCMD_R;          /* While retaining SCMD_A if it was there */
     sd_state.crarg = 0U;
     sd_state.evcnt = 0U;
     sd_state.r1    = 0U;
     sd_state.cc7v  = cu_spisd_crc7_byte(0x00U, data);
    }

   }else if ((sd_state.cmd & SCMD_N) == 0U){ /* Processing command */

    if (sd_state.evcnt < 4U){ /* Data bytes */
     sd_state.crarg |= data << ((3U - sd_state.evcnt) * 8U);
     sd_state.cc7v  = cu_spisd_crc7_byte(sd_state.cc7v, data);
     sd_state.evcnt ++;
    }else{                    /* CRC byte */
     sd_state.cmd  |= SCMD_N;
     sd_state.evcnt = 0U;
     if (sd_state.crc && (((sd_state.cc7v << 1) | 0x01U) != data)){
      sd_state.r1 |= R1_CRC;  /* CRC error detected (Only if CRC is ON) */
     }
     sd_state.data  = 0xFFU;  /* Forced stuff byte (overriding transmission data if any) */
    }

   }else{                                    /* Command response waits */

    if (sd_state.evcnt >= CMD_N){
     if ((sd_state.cmd & 0x3FU) == 55U){ /* App. command */
      sd_state.cmd = SCMD_A;  /* No command end mark since this will be an app. command */
      if ( (sd_state.state == STAT_NATIVE) ||
           (sd_state.state == STAT_IDLE) ||
           (sd_state.state == STAT_VERIFIED) ||
           (sd_state.state == STAT_IINIT) ){
       sd_state.r1 |= R1_IDLE;
      }
      sd_state.data = sd_state.r1; /* Create an R1 response for it (will not be processed otherwise) */
     }else{
      sd_state.cmd = 0U;
      atend = TRUE;
     }
    }else{
     sd_state.data  = 0xFFU;  /* Forced stuff byte (overriding transmission data if any) */
     sd_state.evcnt ++;
    }

   }

  }

 }

 /* SD state machine */

 switch (sd_state.state){

  case STAT_UNINIT:        /* Uninitialized: Waiting for CS high and Data high */

   sd_state.pstat = PSTAT_IDLE;
   if ( (WRAP32(cycle - sd_state.recvc) >= INIF_HI) && /* SPI timing constraint OK */
        (WRAP32(cycle - sd_state.recvc) <= INIF_LO) && /* SPI timing constraint OK */
        (data == 0xFFU) &&                             /* Data high satisfied */
        (!sd_state.ena) &&                             /* CS high (card is deselected) satisfied */
        (WRAP32(cycle - sd_state.enac)  >= (INICS_T * SPI_1MS)) ){ /* At least the required init time passed */
    sd_state.state = STAT_NINIT;
    sd_state.evcnt = 1U;   /* Initializing native mode, 1 data byte already got */
    sd_state.data  = 0xFFU;
   }
   break;

  case STAT_NINIT:         /* Wait for init pulses */

   sd_state.pstat = PSTAT_IDLE;
   if ( (WRAP32(cycle - sd_state.recvc) >= INIF_HI) && /* SPI timing constraint OK */
        (WRAP32(cycle - sd_state.recvc) <= INIF_LO) && /* SPI timing constraint OK */
        (data == 0xFFU) &&                             /* Data high satisfied */
        (!sd_state.ena) ){                             /* CS high (card is deselected) satisfied */
    sd_state.evcnt ++;
    if (sd_state.evcnt == 10U){ /* 80 pulses got, so enter Native mode */
     sd_state.state = STAT_NATIVE;
     sd_state.cmd   = 0U;
     sd_state.crc   = TRUE;     /* CRC is turned ON by this transition */
    }
   }else{
    sd_state.state = STAT_UNINIT;
   }
   break;

  case STAT_NATIVE:        /* Native mode: Waiting for a CMD0 */

   sd_state.pstat = PSTAT_IDLE;
   if ( (WRAP32(cycle - sd_state.recvc) >= INIF_HI) && /* SPI timing constraint OK */
        (WRAP32(cycle - sd_state.recvc) <= INIF_LO) ){ /* SPI timing constraint OK */

    if ((atend) && (sd_state.r1 == 0x00U)){ /* No error yet */
     switch (cmd & (0x3FU | SCMD_A)){

      case  0U: /* Go idle state */
       sd_state.r1   |= R1_IDLE;
       sd_state.state = STAT_IDLE;
       sd_state.crc   = FALSE; /* CRC is turned OFF by this command */
       break;

      default:  /* Other commands are not supported in native mode */
       sd_state.r1 |= R1_ILL;
       break;
     }
    }
    if (atend){
     sd_state.data  = sd_state.r1;
    }

   }
   break;

  case STAT_IDLE:          /* Idle state, waiting for some initialization */
  case STAT_VERIFIED:      /* Verified state, same */

   sd_state.pstat = PSTAT_IDLE;
   if ( (WRAP32(cycle - sd_state.recvc) >= INIF_HI) && /* SPI timing constraint OK */
        (WRAP32(cycle - sd_state.recvc) <= INIF_LO) ){ /* SPI timing constraint OK */
    /* Here the SD card should accept the followings:
    ** CMD0
    ** CMD1
    ** CMD8 (enters Verified state)
    ** CMD58
    ** CMD59
    ** ACMD41 (only in Verified state)
    */
    if ((atend) && (sd_state.r1 == 0x00U)){ /* No error yet */
     switch (cmd & (0x3FU | SCMD_A)){

      case  0U: /* Go idle state */
       sd_state.state = STAT_IDLE;
       break;

      case  1U: /* Initiate initialization (bypassing CMD8 - ACMD41) */
       sd_state.state = STAT_IINIT;
       sd_state.next  = WRAP32(cycle + (SPI_1MS * SD_INIMS));
       break;

      case  8U: /* Send Interface Condition */
       if (sd_state.crarg != 0x000001AAU){
        sd_state.r1    = 0xFFU; /* Bad argument, reject */
       }else{
        sd_state.crarg = 0x000001AAU;
        sd_state.cmd   = SCMD_X;
        sd_state.evcnt = 0U;
        sd_state.state = STAT_VERIFIED;
       }
       break;

      case 58U: /* Read OCR */
       sd_state.crarg = 0x00FF0000U; /* Report as an SDSC card & Not finished power up */
       sd_state.cmd   = SCMD_X;
       sd_state.evcnt = 0U;
       break;

      case 59U: /* Toggle CRC checks */
       sd_state.crc = ((sd_state.crarg & 1U) != 0U);
       break;

      case (41U | SCMD_A): /* Initiate initialization */
       if ( (sd_state.state == STAT_VERIFIED) &&
            ((sd_state.crarg & 0xBF00FFFFU) == 0U) ){
        sd_state.state = STAT_IINIT;
        sd_state.next  = WRAP32(cycle + (SPI_1MS * SD_INIMS));
        sd_state.crarg = 0x00FF0000U; /* Report as an SDSC card & Not finished power up */
        sd_state.cmd   = SCMD_X;
        sd_state.evcnt = 0U;
       }else{
        sd_state.r1 |= R1_ILL;
       }
       break;

      default:  /* Other commands are not supported in idle state */
       sd_state.r1 |= R1_ILL;
       break;
     }
    }
    if (atend){
     sd_state.r1   |= R1_IDLE;
     sd_state.data  = sd_state.r1;
    }
   }
   break;

  case STAT_IINIT:         /* Initializing */

   sd_state.pstat = PSTAT_IDLE;
   if ( (WRAP32(cycle - sd_state.recvc) >= INIF_HI) && /* SPI timing constraint OK */
        (WRAP32(cycle - sd_state.recvc) <= INIF_LO) ){ /* SPI timing constraint OK */
    /* Here the SD card should accept the followings:
    ** CMD0
    ** CMD1
    ** ACMD41
    */
    if ((atend) && (sd_state.r1 == 0x00U)){ /* No error yet */
     switch (cmd & (0x3FU | SCMD_A)){

      case  0U: /* Go idle state */
       sd_state.state = STAT_IDLE;
       sd_state.r1 |= R1_IDLE;
       break;

      case  1U: /* Initiate initialization */
      case (41U | SCMD_A):
       sd_state.crarg = 0x00FF0000U; /* Report as an SDSC card & Not finished power up */
       sd_state.cmd   = SCMD_X;
       sd_state.evcnt = 0U;
       if (WRAP32(sd_state.next - cycle) >= 0x80000000U){ /* Initialized */
        sd_state.state = STAT_AVAIL;
        sd_state.crarg |= 0x80000000U; /* Powered up */
       }else{
        sd_state.r1 |= R1_IDLE;
       }
       break;

      default:  /* Other commands are not supported during init */
       sd_state.r1 |= R1_ILL;
       sd_state.r1 |= R1_IDLE;
       break;
     }
    }
    if (atend){
     sd_state.data  = sd_state.r1;
    }
   }
   break;

  case STAT_AVAIL:         /* Available for data transfer (at any SPI rate) */
   /* Here the SD card should accept the followings (at least... to function
   ** as an useful card):
   ** CMD0  (Go idle state - for re-initializing)
   ** CMD16 (Set block length, just accept it and ignore, 512 is the only sensible value)
   ** CMD17 (Read single block)
   ** CMD18 (Read multiple blocks)
   ** CMD24 (Write block)
   ** CMD25 (Write multiple blocks)
   ** CMD58 (Read OCR - some init methods might do it here to get card type)
   ** CMD59 (Toggle CRC)
   ** ACMD23 (Blocks to pre-erase, just accept and ignore)
   */
   if ((atend) && (sd_state.r1 == 0x00U)){ /* No error yet */
    switch (cmd & (0x3FU | SCMD_A)){

     case  0U: /* Go idle state */
      sd_state.state = STAT_IDLE;
      sd_state.r1 |= R1_IDLE;
      break;

     case 13U: /* Send Status */
      sd_state.r1    = 0U;     /* A bit of hack to produce a zero status */
      sd_state.crarg = 0U;     /* (R2 response, 2 bytes, first byte is produced */
      sd_state.cmd   = SCMD_X; /* by the R1, second byte by the highest byte of */
      sd_state.evcnt = 3U;     /* the normally 32 bit argument) */
      break;

     case 16U: /* Set block length */
      break;   /* Accept but ignore (assuming it is just used to set 512 bytes) */

     case 17U: /* Read single block */
      sd_state.state = STAT_CMD17;
      sd_state.ppos  = 0U;
      sd_state.pstat = PSTAT_RPREP;
      sd_state.paddr = sd_state.crarg >> 9; /* SDSC card, byte argument */
      break;

     case 18U: /* Read multiple blocks */
      sd_state.state = STAT_CMD18;
      sd_state.ppos  = 0U;
      sd_state.pstat = PSTAT_RPREP;
      sd_state.paddr = sd_state.crarg >> 9; /* SDSC card, byte argument */
      break;

     case 24U: /* Write block */
      sd_state.state = STAT_CMD24;
      sd_state.ppos  = 0U;
      sd_state.pstat = PSTAT_W24PREP;
      sd_state.paddr = sd_state.crarg >> 9; /* SDSC card, byte argument */
      break;

     case 25U: /* Write multiple blocks */
      sd_state.state = STAT_CMD25;
      sd_state.ppos  = 0U;
      sd_state.pstat = PSTAT_W25PREP;
      sd_state.paddr = sd_state.crarg >> 9; /* SDSC card, byte argument */
      break;

     case 58U: /* Read OCR */
      sd_state.crarg = 0x80FF0000U; /* Report as an SDSC card & Finished power up */
      sd_state.cmd   = SCMD_X;
      sd_state.evcnt = 0U;
      break;

     case 59U: /* Toggle CRC checks */
      sd_state.crc = ((sd_state.crarg & 1U) != 0U);
      break;

     case (23U | SCMD_A): /* Blocks to pre-erase */
      break;   /* Accept but ignore (pre-erased blocks would have undefined state anyway) */

     default:
      sd_state.r1 |= R1_ILL;
      break;
    }
   }
   if (atend){
    sd_state.data  = sd_state.r1;
   }
   break;

  case STAT_CMD17:         /* CMD17: Read single block */
  case STAT_CMD18:         /* CMD18: Read multiple blocks */
   /* Only a CMD12 might be used to terminate a read */
   if (sd_state.pstat == PSTAT_IDLE){
    if (sd_state.state == STAT_CMD17){
     sd_state.state = STAT_AVAIL;
    }else{
     sd_state.ppos  = 0U;
     sd_state.pstat = PSTAT_RPREP;
     sd_state.paddr = (sd_state.paddr + 1U) & 0x007FFFFFU; /* Still an SDSC card... */
    }
   }
   if (atend){
    if ((cmd & (0x3FU | SCMD_A)) == 12U){ /* Stop transmission */
     sd_state.state = STAT_AVAIL;
     sd_state.pstat = PSTAT_IDLE;
     sd_state.data  = sd_state.r1;
     sd_state.crarg = 0x00000000U; /* Generate 4 busy bytes (a bit of hack to get a "complete" R1b) */
     sd_state.cmd   = SCMD_X;
     sd_state.evcnt = 0U;
    }
   }
   break;

  case STAT_CMD24:         /* CMD24: Write single block */
  case STAT_CMD25:         /* CMD25: Write multiple blocks */
   if (sd_state.pstat == PSTAT_IDLE){
    sd_state.state = STAT_AVAIL; /* Simply wait until the end of the transfers */
   }
   break;

  default:                 /* State machine error, shouldn't happen */
   sd_state.state = STAT_UNINIT;
   break;

 }

/* printf("%08X (r: %08X); SD: Ena: %u, Byte: %02X, %02X, Stat: %u, PSta: %u, Cmd: %03X, PPos: %3u\n",
**     cycle, sd_state.recvc, (auint)(sd_state.ena), data, sd_state.data, sd_state.state, sd_state.pstat, sd_state.cmd, sd_state.ppos);
*/

}



/*
** Receives a byte of data from the SD card. The passed cycle corresponds the
** cycle when it must start to clock into the AVR. 0xFF is sent when the card
** tri-states the line.
*/
auint cu_spisd_recv(auint cycle)
{
 sd_state.recvc = cycle;
 return sd_state.data;
}



/*
** Returns SD card state. It may be written, then the cu_spisd_update()
** function has to be called to rebuild any internal state depending on it.
*/
cu_state_spisd_t* cu_spisd_get_state(void)
{
 return &sd_state;
}



/*
** Rebuild internal state according to the current state. Call after writing
** the SD card state.
*/
void  cu_spisd_update(void)
{
}
