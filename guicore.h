/*
 *  GUI core elements
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



#ifndef GUICORE_H
#define GUICORE_H



#include "types.h"



/* Request full screen GUI */
#define GUICORE_FULLSCREEN 0x0001U
/* Disable sticking to display refresh rate (assumed >= 60Hz) */
#define GUICORE_NOVSYNC    0x0002U



/*
** Attempts to initialize the GUI. Returns TRUE on success. This must be
** called first before any platform specific elements as it initializes
** those.
*/
boole guicore_init(auint flags, const char* title);


/*
** Tears down the GUI.
*/
void  guicore_quit(void);


/*
** Retrieves 640 x 560 GUI surface's pixel buffer. A matching
** guicore_relpixbuf() must be made after rendering. It may return NULL if it
** is not possible to lock the pixel buffer for access.
*/
uint32* guicore_getpixbuf(void);


/*
** Releases the pixel buffer.
*/
void  guicore_relpixbuf(void);


/*
** Retrieves pitch of GUI surface
*/
auint guicore_getpitch(void);


/*
** Retrieves 256 color Uzebox palette which should be used to generate
** pixels. Algorithms can assume some 8 bit per channel representation, but
** channel order might vary.
*/
uint32 const* guicore_getpalette(void);


/*
** Updates display. Normally this sticks to the display's refresh rate. If
** drop is TRUE, it does nothing (drops the frame).
*/
void guicore_update(boole drop);


/*
** Sets the window caption (if any can be displayed)
*/
void guicore_setcaption(const char* title);


#endif
