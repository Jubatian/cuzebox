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



#ifndef CU_SPISD_H
#define CU_SPISD_H



#include "types.h"


/* SD card state structure. This isn't really meant to be edited, but it is
** necessary for emulator state dumps. Every value is at most 32 bits. */
typedef struct{
 boole ena;      /* Chip select state, TRUE: Enabled (CS low) */
 boole crc;      /* CRC checking state, TRUE: Enabled */
 auint cc7v;     /* CRC7 value for running CRC calculations */
 auint cc16v;    /* CRC16 value for running CRC calculations */
 auint cc16c;    /* CRC16 collected value for comparison */
 auint enac;     /* Cycle of last chip select toggle */
 auint state;    /* SD card state machine */
 auint next;     /* Next event's cycle. Actual interpretation depends on state. */
 auint recvc;    /* Last receive's cycle, used to determine bus speed where necessary */
 auint evcnt;    /* Event counter, used when a transition needs a certain number of events */
 auint cmd;      /* Command / Response state machine */
 auint crarg;    /* Command / Response argument */
 auint r1;       /* Command response (R1) (8 bits)*/
 auint data;     /* Data waiting to get on the output (8 bits) */
 auint pstat;    /* Packet transmission state machine */
 auint paddr;    /* Packet sector (512 byte units) address */
 auint ppos;     /* Packet transmission byte position */
}cu_state_spisd_t;


/*
** Resets SD card peripheral. Cycle is the CPU cycle when it happens which
** might be used for emulating timing constraints.
*/
void  cu_spisd_reset(auint cycle);


/*
** Sets chip select's state, TRUE to enable, FALSE to disable.
*/
void  cu_spisd_cs_set(boole ena, auint cycle);


/*
** Sends a byte of data to the SD card. The passed cycle corresponds the cycle
** when it was clocked out of the AVR.
*/
void  cu_spisd_send(auint data, auint cycle);


/*
** Receives a byte of data from the SD card. The passed cycle corresponds the
** cycle when it must start to clock into the AVR. 0xFF is sent when the card
** tri-states the line.
*/
auint cu_spisd_recv(auint cycle);


/*
** Returns SD card state. It may be written, then the cu_spisd_update()
** function has to be called to rebuild any internal state depending on it.
*/
cu_state_spisd_t* cu_spisd_get_state(void);


/*
** Rebuild internal state according to the current state. Call after writing
** the SD card state.
*/
void  cu_spisd_update(void);


#endif
