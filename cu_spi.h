/*
 *  SPI peripherals
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



#ifndef CU_SPI_H
#define CU_SPI_H



#include "types.h"



/* SPI Chip select ID for SD card */
#define CU_SPI_CS_SD     0U
/* SPI Chip select ID for SPI RAM */
#define CU_SPI_CS_RAM    1U



/*
** Resets SPI peripherals. Cycle is the CPU cycle when it happens which might
** be used by the peripherals for emulating timing constraints.
*/
void  cu_spi_reset(auint cycle);


/*
** Sets a chip select's state, TRUE to enable, FALSE to disable.
*/
void  cu_spi_cs_set(auint id, boole ena, auint cycle);


/*
** Sends a byte of data to the SPI peripheral. The passed cycle corresponds
** the cycle when it was clocked out of the AVR.
*/
void  cu_spi_send(auint data, auint cycle);


/*
** Receives a byte of data from the SPI peripheral. The passed cycle
** corresponds the cycle when it must start to clock into the AVR.
*/
auint cu_spi_recv(auint cycle);


#endif
