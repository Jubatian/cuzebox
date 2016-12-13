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



/*
** Notes on the display layout:
**
** The backbuffer is 640 x 270 with 2:1 PAR. This is selected for retaining
** efficiency for both 640 x 560 and 320 x 270 renders. In 640 x 560 it
** allows good horizontal resolution (better render of high-resolution
** Uzebox graphics modes).
**
** It is possible that only the Uzebox's screen is displayed (and nothing of
** the debug and user interface regions around). This screen has the upper
** left corner at 5:18 with 310 x 228 dimensions (assuming a 320 x 270 render
** target).
*/



/* Request full screen GUI */
#define GUICORE_FULLSCREEN 0x0001U
/* Disable sticking to display refresh rate */
#define GUICORE_NOVSYNC    0x0002U
/* Use a small screen (320 x 270 instead of 640 x 560) */
#define GUICORE_SMALL      0x0004U
/* Display only the game region */
#define GUICORE_GAMEONLY   0x0008U



/* Descriptor for the pixel format of the pixel buffer. When using the Uzebox
** palette it is not necessary to query this as it contains correct colors. */
typedef struct{
 auint rsh;      /* Left shift amount to generate 8 bit Red component */
 auint gsh;      /* Left shift amount to generate 8 bit Green component */
 auint bsh;      /* Left shift amount to generate 8 bit Blue component */
}guicore_pixfmt_t;



/*
** Attempts to initialize the GUI. Returns TRUE on success. This must be
** called first before any platform specific elements as it initializes
** those. It can be recalled to change the display's properties (flags).
** Note that if it fails, it tears fown the GUI and any platform specific
** extensions!
*/
boole guicore_init(auint flags, const char* title);


/*
** Tears down the GUI.
*/
void  guicore_quit(void);


/*
** Gets current GUI flags.
*/
auint guicore_getflags(void);


/*
** Retrieves 640 x 280 GUI surface's pixel buffer.
*/
uint32* guicore_getpixbuf(void);


/*
** Retrieves the pixel format of the pixel buffer.
*/
void  guicore_getpixfmt(guicore_pixfmt_t* pixfmt);


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
