/*
 *  SPI RAM peripheral (on SPI bus)
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



#include "cu_spir.h"



/* SPI RAM state */
static cu_state_spir_t spir_state;



/* SPI RAM state machine */
/* Parameter position mask & shift */
#define STAT_PPMASK  0xC0U
#define STAT_PPSH    6U
/* Read (Data bytes) */
#define STAT_READB   0x00U
/* Wait for instruction */
#define STAT_IDLE    0x01U
/* Read */
#define STAT_READ    0x02U
/* Write */
#define STAT_WRITE   0x03U
/* Write (data bytes) */
#define STAT_WRITEB  0x04U
/* Read mode register */
#define STAT_RMODE   0x05U
/* Write mode register */
#define STAT_WMODE   0x06U
/* Sink (don't accept further data) */
#define STAT_SINK    0x07U



/*
** Resets SPI RAM peripheral. Cycle is the CPU cycle when it happens which
** might be used for emulating timing constraints.
*/
void  cu_spir_reset(auint cycle)
{
 memset(&spir_state, 0, sizeof(spir_state));
 spir_state.mode = 0x40U; /* Sequential mode */
 spir_state.data = 0xFFU;
 spir_state.state = STAT_IDLE;
}



/*
** Sets chip select's state, TRUE to enable, FALSE to disable.
*/
void  cu_spir_cs_set(boole ena, auint cycle)
{
 if ( (spir_state.ena && (!ena)) ||
      ((!spir_state.ena) && ena) ){
  spir_state.ena = ena;
  if (spir_state.ena == FALSE){ /* Back to idle state */
   spir_state.state = STAT_IDLE;
  }
 }
}



/*
** Sends a byte of data to the SPI RAM. The passed cycle corresponds the cycle
** when it was clocked out of the AVR.
*/
void  cu_spir_send(auint data, auint cycle)
{
 auint ppos;

 /* This fast path serves reads, beneficial if the SPI RAM is used for
 ** generating video data. */

 if (spir_state.state == STAT_READB){

  if       (spir_state.mode == 0x80U){ /* Page mode */
   spir_state.addr = (spir_state.addr & 0x1FFE0U) +
                     ((spir_state.addr + 1U) & 0x1FU);
  }else if (spir_state.mode == 0x40U){ /* Sequential mode */
   spir_state.addr ++;
  }else{}
  spir_state.data  = spir_state.ram[spir_state.addr & 0x1FFFFU];

  return;
 }

 /* Normal SPI RAM processing (excluding processing reads) */

 spir_state.data = 0xFFU; /* Default data out */

 if (spir_state.ena){     /* Good, this only tampers with the bus if actually enabled (unlike the SD card...) */

  switch (spir_state.state & (~STAT_PPMASK)){

   case STAT_IDLE:        /* Wait for valid command byte */

    switch (data){
     case 0x01U:          /* Write mode register */
      spir_state.state = STAT_WMODE;
      break;
     case 0x02U:          /* Write data */
      spir_state.state = STAT_WRITE;
      spir_state.addr  = 0U;
      break;
     case 0x03U:          /* Read data */
      spir_state.state = STAT_READ;
      spir_state.addr  = 0U;
      break;
     case 0x05U:          /* Read mode register */
      spir_state.state = STAT_RMODE;
      spir_state.data  = spir_state.mode;
      break;
     default:             /* Ignore all else, going into sinking data */
      spir_state.state = STAT_SINK;
      break;
    }
    break;

   case STAT_READ:        /* Read (preparation) */

    ppos = (spir_state.state & STAT_PPMASK) >> STAT_PPSH;
    spir_state.addr |= data << ((2U - ppos) * 8U);
    ppos ++;
    spir_state.state = STAT_READ | (ppos << STAT_PPSH);
    if (ppos == 3U){
     spir_state.state = STAT_READB;
     spir_state.data  = spir_state.ram[spir_state.addr & 0x1FFFFU]; /* First data byte */
    }
    break;

   case STAT_WRITE:       /* Write (preparation) */

    ppos = (spir_state.state & STAT_PPMASK) >> STAT_PPSH;
    spir_state.addr |= data << ((2U - ppos) * 8U);
    ppos ++;
    spir_state.state = STAT_WRITE | (ppos << STAT_PPSH);
    if (ppos == 3U){
     spir_state.state = STAT_WRITEB;
    }
    break;

   case STAT_WRITEB:      /* Write (data bytes) */

    spir_state.ram[spir_state.addr & 0x1FFFFU] = data;
    if       (spir_state.mode == 0x80U){ /* Page mode */
     spir_state.addr = (spir_state.addr & 0x1FFE0U) +
                       ((spir_state.addr + 1U) & 0x1FU);
    }else if (spir_state.mode == 0x40U){ /* Sequential mode */
     spir_state.addr ++;
    }else{}
    break;

   case STAT_RMODE:       /* Read mode register */

    spir_state.state = STAT_SINK;
    break;

   case STAT_WMODE:       /* Write mode register */

    spir_state.mode = data & 0xC0U;
    spir_state.state = STAT_SINK;
    break;

   default:               /* Sinking data until CS is deasserted */

    break;

  }

 }
}



/*
** Receives a byte of data from the SPI RAM. The passed cycle corresponds the
** cycle when it must start to clock into the AVR. 0xFF is sent when the card
** tri-states the line.
*/
auint cu_spir_recv(auint cycle)
{
 return spir_state.data;
}



/*
** Returns SPI RAM state. It may be written, then the cu_spir_update()
** function has to be called to rebuild any internal state depending on it.
*/
cu_state_spir_t* cu_spir_get_state(void)
{
 return &spir_state;
}



/*
** Rebuild internal state according to the current state. Call after writing
** the SPI RAM state.
*/
void  cu_spir_update(void)
{
}
