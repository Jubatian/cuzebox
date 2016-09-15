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



#ifndef CU_SPIR_H
#define CU_SPIR_H



#include "types.h"


/* SPI RAM state structure. This isn't really meant to be edited, but it is
** necessary for emulator state dumps. Every value is at most 32 bits. */
typedef struct{
 uint8 ram[0x20000U];
 boole ena;      /* Chip select state, TRUE: enabled (CS low) */
 auint mode;     /* Mode register's contents (on bit 6 and 7) */
 auint state;    /* SPI RAM state machine */
 auint addr;     /* Address within the RAM */
 auint data;     /* Data waiting to get on the output (8 bits) */
}cu_state_spir_t;


/*
** Resets SPI RAM peripheral. Cycle is the CPU cycle when it happens which
** might be used for emulating timing constraints.
*/
void  cu_spir_reset(auint cycle);


/*
** Sets chip select's state, TRUE to enable, FALSE to disable.
*/
void  cu_spir_cs_set(boole ena, auint cycle);


/*
** Sends a byte of data to the SPI RAM. The passed cycle corresponds the cycle
** when it was clocked out of the AVR.
*/
void  cu_spir_send(auint data, auint cycle);


/*
** Receives a byte of data from the SPI RAM. The passed cycle corresponds the
** cycle when it must start to clock into the AVR. 0xFF is sent when the card
** tri-states the line.
*/
auint cu_spir_recv(auint cycle);


/*
** Returns SPI RAM state. It may be written, then the cu_spir_update()
** function has to be called to rebuild any internal state depending on it.
*/
cu_state_spir_t* cu_spir_get_state(void);


/*
** Rebuild internal state according to the current state. Call after writing
** the SPI RAM state.
*/
void  cu_spir_update(void);


#endif
