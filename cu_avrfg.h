/*
 *  AVR flag generator
 *
 *  Copyright (C) 2016 - 2017
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



#ifndef CU_AVRFG_H
#define CU_AVRFG_H



#include "cu_types.h"


/* Position of ADD flags ((c << 16) + (src << 8) + dst) */
#define CU_AVRFG_ADD  0x00000U
/* Position of SUB flags ((c << 16) + (src << 8) + dst) */
#define CU_AVRFG_SUB  0x20000U
/* Position of SHR flags (((src & 1U) << 8) + res) */
#define CU_AVRFG_SHR  0x40000U
/* Position of LOG flags (res) */
#define CU_AVRFG_LOG  0x40200U
/* Position of INC flags (res) */
#define CU_AVRFG_INC  0x40300U
/* Position of DEC flags (res) */
#define CU_AVRFG_DEC  0x40400U

/* Total size of flag precalc table */
#define CU_AVRFG_SIZE 0x40500U


/*
** Fills up the flag precalc table
*/
void cu_avrfg_fill(uint8* ftable);


#endif
