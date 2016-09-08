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
** CU_GET_FRAME result). Returns the last return of cu_avr_run() so it can be
** acted upon as necessary. If no proper sync was generated, it returns as
** soon as a GU_SYNCERR result is produced. If drop is TRUE, the render of the
** frame is dropped which can be used to help slow targets.
*/
auint frame_run(boole drop);


/*
** Returns the 262 received unsigned audio samples of the frame.
*/
uint8 const* frame_getaudio(void);


#endif
