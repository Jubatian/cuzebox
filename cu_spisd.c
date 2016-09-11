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



/* SD card state */
static cu_state_spisd_t sd_state;



/* Macro for enforcing 32 bit wrapping math (does nothing if auint is 32 bits) */
#define WRAP32(x) ((x) & 0xFFFFFFFFU)

/* 400KHz SPI transfer clocks */
#define SPI_400   ( 71U * 8U)
/* 100KHz SPI transfer clocks */
#define SPI_100   (287U * 8U)

/* 1ms clocks */
#define SPI_1MS   28634U

/* Command response time (bytes, should be 0 - 8) */
#define CMD_N     2U


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

/* Command state machine. The low 6 bits hold the command */
/* Receiving flag: If set, a command is under reception */
#define SCMD_R      0x100U
/* Application flag: If set, the next command will be an App Command */
#define SCMD_A      0x200U
/* Ticking out command resposne time */
#define SCMD_N      0x400U



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
 sd_state.carg  = 0U;
 sd_state.data  = 0xFFU;
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
 }
/* printf("%08X; CS: %u\n", cycle, (auint)(ena));
*/
}



/*
** Sends a byte of data to the SD card. The passed cycle corresponds the cycle
** when it was clocked out of the AVR.
*/
void  cu_spisd_send(auint data, auint cycle)
{
 boole atcrc = FALSE;   /* Mark that data contains the CRC of a parsed command */
 boole atend = FALSE;   /* Mark that a command ended, need to form a response */
 boole appcm = FALSE;   /* Mark that an application command is being received */

 sd_state.data = 0xFFU; /* Default data out */

 /* Generic SD command processing */

 if ( (sd_state.state != STAT_UNINIT) &&
      (sd_state.state != STAT_NINIT) ){

  if (sd_state.ena){

   if ((sd_state.cmd & SCMD_A) != 0U){ appcm = TRUE; }

   if       ((sd_state.cmd & SCMD_R) == 0U){ /* Waiting for command */

    if ((data & 0xC0U) == 0x40U){            /* Valid command byte */
     sd_state.cmd = (sd_state.cmd & (~(0xFFU | SCMD_N))) |
                    (data & 0x3FU) |         /* Receiving */
                    SCMD_R;                  /* (while retaining SCMD_A if it was there) */
     sd_state.carg  = 0U;
     sd_state.evcnt = 0U;
    }

   }else if ((sd_state.cmd & SCMD_N) == 0U){ /* Processing command */

    if (sd_state.evcnt < 4U){ /* Data bytes */
     sd_state.carg |= data << ((3U - sd_state.evcnt) * 8U);
     sd_state.evcnt ++;
    }else{                    /* CRC byte */
     sd_state.cmd |= SCMD_N;
     sd_state.evcnt = 0U;
     atcrc = TRUE;
    }

   }else{                     /* Command response waits */

    if (sd_state.evcnt >= CMD_N){
     if ((sd_state.cmd & 0x3FU) == 55U){ /* App. command */
      sd_state.cmd = SCMD_A;  /* No command end mark since this will be an app. command */
     }else{
      sd_state.cmd = 0U;
      atend = TRUE;
     }
    }else{
     sd_state.evcnt ++;
    }

   }

  }

 }

 /* SD state machine */

 switch (sd_state.state){

  case STAT_UNINIT:        /* Uninitialized: Waiting for CS high and Data high */

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

   if (atcrc){
    if ( ((sd_state.cmd & 0x3FU) != 0U) ||
         (sd_state.carg != 0U) ||
         (data != 0x95U) ){
     sd_state.cmd = 0U;    /* Not a valid CMD0, drop it */
    }
   }
   if (atend){             /* Assume it was a valid CMD0 as it was filtered out above */
    sd_state.data  = 0x01U;   /* R1 response: Idle state */
    sd_state.state = STAT_IDLE;
   }
   break;

  case STAT_IDLE:          /* Idle state, waiting for some initialization */

   break;

  default:

   break;

 }

/* printf("%08X (r: %08X); SD Send: Ena: %u, Byte: %02X, Stat: %u, Cmd: %03X\n",
**     cycle, sd_state.recvc, (auint)(sd_state.ena), data, sd_state.state, sd_state.cmd);
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
