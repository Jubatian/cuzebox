/*
 *  Emulation frame renderer
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



#ifndef FRAME_H
#define FRAME_H



#include "cu_avr.h"



/*
** Attempts to run a frame of emulation (262 display rows ending with a
** CU_GET_FRAME result). If no proper sync was generated, it still attempts
** to get about a frame worth of lines. If drop is TRUE, the render of the
** frame is dropped which can be used to help slow targets. If merge is TRUE,
** frames following each other are averaged (weak motion blur, cancels out
** flickers). Returns the number of rows generated, which is also the number
** of samples within the audio buffer (normally 262).
*/
auint frame_run(boole drop, boole merge);


/*
** Returns the received unsigned audio samples of the frame (normally 262).
*/
uint8 const* frame_getaudio(void);


#endif
