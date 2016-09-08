/*
 *  Base types
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


#ifndef TYPES_H
#define TYPES_H


#include <stdint.h>             /* Fixed width integer types */
#include <stdio.h>
#include <stdlib.h>


typedef   signed  int   asint;  /* Architecture signed integer (At least 2^31) */
typedef unsigned  int   auint;  /* Architecture unsigned integer (At least 2^31) */
typedef  int16_t        sint16;
typedef uint16_t        uint16;
typedef  int32_t        sint32;
typedef uint32_t        uint32;
typedef   int8_t        sint8;
typedef  uint8_t        uint8;
typedef _Bool           boole;

/* True and False to be used with the "boole" type where needed. */

#define TRUE  1
#define FALSE 0


#endif
