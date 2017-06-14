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
#include <string.h>
#ifdef USE_SDL1
#include <SDL/SDL.h>
#else
#include <SDL2/SDL.h>
#endif
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif



/* Macro for the message / error string output. This is used to redirect or
** cancel messages or errors in certain builds. */
#if (FLAG_NOCONSOLE != 0)
#define print_error(...)
#define print_message(...)
#define print_unf(str)
#else
#ifdef __EMSCRIPTEN__
#if (FLAG_SELFCONT != 0)
#define print_error(...)
#define print_message(...)
#define print_unf(str)
#else
#define print_error(...) fprintf(stderr, __VA_ARGS__)
#define print_message(...) printf(__VA_ARGS__)
#define print_unf(str) fputs(str, stdout)
#endif
#else
#define print_error(...) fprintf(stderr, __VA_ARGS__)
#define print_message(...) printf(__VA_ARGS__)
#define print_unf(str) fputs(str, stdout)
#endif
#endif



/* Macro for enforcing 32 bit wrapping math (does nothing if auint is 32 bits)
** This is used for manipulating values which are meant to be stored in 32
** bits (such as in an emulator state), so 64 bit unsigned variables wouldn't
** work with them. Correct usage may be tested by reducing it to 24 bits,
** enforcing wraps about twice a second (used on the cycle counter). */
#define WRAP32(x) ((x) & 0xFFFFFFFFU)



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
