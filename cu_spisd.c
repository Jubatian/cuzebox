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



/* 400KHz SPI transfer clocks */
#define SPI_400   ( 71U * 8U)
/* 100KHz SPI transfer clocks */
#define SPI_100   (287U * 8U)

/* 1ms clocks */
#define SPI_1MS   28634U

/* Milliseconds taken for the card's initialization */
#define SD_INIMS  500U

/* Command response time (bytes, should be 0 - 8) */
#define CMD_N     1U

/* Data preparation time for reads (bytes, arbitrary) */
#define READ_P_N  2U


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



/*
** Resets SD card peripheral. Cycle is the CPU cycle when it happens which
** might be used for emulating timing constraints.
*/
void  cu_spisd_reset(auint cycle)
{
 sd_state.ena   = FALSE;
 sd_state.enac  = cycle;
 sd_state.state = STAT_UNINIT;
 sd_state.next  = 0U;
 sd_state.recvc = cycle;
 sd_state.evcnt = 0U;
 sd_state.cmd   = 0U;
 sd_state.crarg = 0U;
 sd_state.r1    = 0U;
 sd_state.data  = 0xFFU;
 sd_state.pstat = PSTAT_IDLE;

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
 boole atcrc = FALSE;   /* Mark that data contains the CRC of a parsed command */
 boole atend = FALSE;   /* Mark that a command ended, need to form a response */
 auint cmd   = sd_state.cmd; /* Save for command processing */

 sd_state.data = 0xFFU; /* Default data out */

 /* Data transmission state machine (lowest priority in determining output
 ** data bytes) */

 switch (sd_state.pstat){

  case PSTAT_IDLE:
   break;

  case PSTAT_RPREP:
   if (sd_state.ppos >= READ_P_N){
    sd_state.data  = 0xFEU; /* Read data token */
    sd_state.pstat = PSTAT_RDATA;
    sd_state.ppos  = 0U;
   }else{
    sd_state.ppos  ++;
   }
   break;

  case PSTAT_RDATA:
   sd_state.data  = cu_vfat_read((sd_state.paddr << 9) + sd_state.ppos);
   sd_state.ppos  ++;
   if (sd_state.ppos == 512U){
    sd_state.pstat = PSTAT_RCRC;
    sd_state.ppos  = 0U;
   }
   break;

  case PSTAT_RCRC:
   sd_state.data  = 0x00U; /* SD CRC is currently unsupported */
   sd_state.ppos  ++;
   if (sd_state.ppos == 2U){
    sd_state.pstat = PSTAT_IDLE;
   }
   break;

  default:
   break;

 }

 /* Generic SD command processing */

 if (sd_state.ena){

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
    }

   }else if ((sd_state.cmd & SCMD_N) == 0U){ /* Processing command */

    if (sd_state.evcnt < 4U){ /* Data bytes */
     sd_state.crarg |= data << ((3U - sd_state.evcnt) * 8U);
     sd_state.evcnt ++;
    }else{                    /* CRC byte */
     sd_state.cmd  |= SCMD_N;
     sd_state.evcnt = 0U;
     atcrc = TRUE;
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
   if ( (WRAP32(cycle - sd_state.recvc) >= SPI_400) && /* SPI timing constraint OK */
        (WRAP32(cycle - sd_state.recvc) <= SPI_100) && /* SPI timing constraint OK */
        (data == 0xFFU) &&                             /* Data high satisfied */
        (!sd_state.ena) &&                             /* CS high (card is deselected) satisfied */
        (WRAP32(cycle - sd_state.enac)  >= SPI_1MS) ){ /* At least 1ms passed */
    sd_state.state = STAT_NINIT;
    sd_state.evcnt = 1U;   /* Initializing native mode, 1 data byte already got */
    sd_state.data  = 0xFFU;
   }
   break;

  case STAT_NINIT:         /* Wait for init pulses */

   sd_state.pstat = PSTAT_IDLE;
   if ( (WRAP32(cycle - sd_state.recvc) >= SPI_400) && /* SPI timing constraint OK */
        (WRAP32(cycle - sd_state.recvc) <= SPI_100) && /* SPI timing constraint OK */
        (data == 0xFFU) &&                             /* Data high satisfied */
        (!sd_state.ena) ){                             /* CS high (card is deselected) satisfied */
    sd_state.evcnt ++;
    if (sd_state.evcnt == 10U){ /* 80 pulses got, so enter Native mode */
     sd_state.state = STAT_NATIVE;
     sd_state.cmd   = 0U;
    }
   }else{
    sd_state.state = STAT_UNINIT;
   }
   break;

  case STAT_NATIVE:        /* Native mode: Waiting for a CMD0 */

   sd_state.pstat = PSTAT_IDLE;
   if ( (WRAP32(cycle - sd_state.recvc) >= SPI_400) && /* SPI timing constraint OK */
        (WRAP32(cycle - sd_state.recvc) <= SPI_100) ){ /* SPI timing constraint OK */
    if (atcrc){
     if ( ((cmd & 0x3FU) != 0U) ||
          (sd_state.crarg != 0U) ||
          (data != 0x95U) ){
      sd_state.r1 |= R1_ILL; /* Not a valid CMD0 */
     }
    }
    if (atend){
     if (sd_state.r1 == 0U){
      sd_state.r1   |= R1_IDLE;
      sd_state.data  = sd_state.r1;
      sd_state.state = STAT_IDLE;
     }
    }
    break;
   }

  case STAT_IDLE:          /* Idle state, waiting for some initialization */
  case STAT_VERIFIED:      /* Verified state, same */

   sd_state.pstat = PSTAT_IDLE;
   if ( (WRAP32(cycle - sd_state.recvc) >= SPI_400) && /* SPI timing constraint OK */
        (WRAP32(cycle - sd_state.recvc) <= SPI_100) ){ /* SPI timing constraint OK */
    /* Here the SD card should accept the followings:
    ** CMD0
    ** CMD1
    ** CMD8 (enters Verified state)
    ** CMD58
    ** CMD59
    ** ACMD41 (only in Verified state)
    */
    if ( (atcrc) &&
         ((cmd & (0x3FU | SCMD_A)) == 8U) &&
         (data != 0x87U) &&
         (sd_state.crarg != 0x000001AAU) ){
     sd_state.r1 |= R1_CRC;
    }
    if (atend){
     switch (cmd & (0x3FU | SCMD_A)){

      case  0U: /* Go idle state */
       sd_state.state = STAT_IDLE;
       break;

      case  1U: /* Initiate initialization (bypassing CMD8 - ACMD41) */
       sd_state.state = STAT_IINIT;
       sd_state.next  = SPI_1MS * SD_INIMS;
       break;

      case  8U: /* Send Interface Condition */
       if (sd_state.r1 == 0U){ /* No error, accept */
        sd_state.crarg = 0x000001AAU;
        sd_state.cmd   = SCMD_X;
        sd_state.evcnt = 0U;
        sd_state.state = STAT_VERIFIED;
       }
       break;

      case 58U: /* Read OCR */
       sd_state.crarg = 0x80FF0000U; /* Report as an SDSC card */
       sd_state.cmd   = SCMD_X;
       sd_state.evcnt = 0U;
       break;

      case 59U: /* Toggle CRC checks */
       break;   /* Not supported, just pretend it works */

      case (41U | SCMD_A): /* Initiate initialization */
       if ( (sd_state.state == STAT_VERIFIED) &&
            ((sd_state.crarg & 0xBF00FFFFU) == 0U) ){
        sd_state.state = STAT_IINIT;
        sd_state.next  = SPI_1MS * SD_INIMS;
       }else{
        sd_state.r1 |= R1_ILL;
       }
       break;

      default:  /* Other commands are not supported in idle state */
       sd_state.r1 |= R1_ILL;
       break;
     }

     sd_state.r1   |= R1_IDLE;
     sd_state.data  = sd_state.r1;
    }
   }
   break;

  case STAT_IINIT:         /* Initializing */

   sd_state.pstat = PSTAT_IDLE;
   if ( (WRAP32(cycle - sd_state.recvc) >= SPI_400) && /* SPI timing constraint OK */
        (WRAP32(cycle - sd_state.recvc) <= SPI_100) ){ /* SPI timing constraint OK */
    /* Here the SD card should accept the followings:
    ** CMD0
    ** CMD1
    ** ACMD41
    */
    if (atend){
     switch (cmd & (0x3FU | SCMD_A)){

      case  0U: /* Go idle state */
       sd_state.state = STAT_IDLE;
       sd_state.r1 |= R1_IDLE;
       break;

      case  1U: /* Initiate initialization */
      case (41U | SCMD_A):
       if (WRAP32(sd_state.next - cycle) > 0x80000000U){ /* Initialized */
        sd_state.state = STAT_AVAIL;
       }else{
        sd_state.r1 |= R1_IDLE;
       }
       break;

      default:  /* Other commands are not supported during init */
       sd_state.r1 |= R1_ILL;
       sd_state.r1 |= R1_IDLE;
       break;
     }
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
   ** CMD59 (Toggle CRC, ignored)
   ** ACMD23 (Blocks to pre-erase, just accept and ignore)
   */
   if (atend){
    switch (cmd & (0x3FU | SCMD_A)){

     case  0U: /* Go idle state */
      sd_state.state = STAT_IDLE;
      sd_state.r1 |= R1_IDLE;
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
      break;

     case 25U: /* Write multiple blocks */
      break;

     case 58U: /* Read OCR */
      sd_state.crarg = 0x80FF0000U; /* Report as an SDSC card */
      sd_state.cmd   = SCMD_X;
      sd_state.evcnt = 0U;
      break;

     case 59U: /* Toggle CRC checks */
      break;   /* Not supported, just pretend it works */

     case (23U | SCMD_A): /* Blocks to pre-erase */
      break;   /* Accept but ignore (pre-erased blocks would have undefined state anyway) */

     default:
      sd_state.r1 |= R1_ILL;
      break;
    }
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

  default:                 /* State machine error, shouldn't happen */
   sd_state.state = STAT_UNINIT;
   break;

 }

/* printf("%08X (r: %08X); SD Send: Ena: %u, Byte: %02X, %02X, Stat: %u, Cmd: %03X, PPos: %3u\n",
**     cycle, sd_state.recvc, (auint)(sd_state.ena), data, sd_state.data, sd_state.state, sd_state.cmd, sd_state.ppos);
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
