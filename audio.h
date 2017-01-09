/*
 *  Audio output
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



#ifndef AUDIO_H
#define AUDIO_H



#include "types.h"



/*
** Attempts to initialize audio. If it returns false, no sound will be
** generated.
*/
boole audio_init(void);


/*
** Resets audio, call it along with the start of emulation.
*/
void  audio_reset(void);


/*
** Tears down audio (if initialization succeed)
*/
void  audio_quit(void);


/*
** Send a frame (unsigned 8 bit samples) to the audio device. If NULL is
** passed, then silence will be added.
*/
void  audio_sendframe(uint8 const* samples, auint len);


/*
** Returns current long-term audio frequency.
*/
auint audio_getfreq(void);


/*
** Enables or disables frequency scaling. By default frequency scaling is
** enabled. When disabled, the long term PD controller is fixed at whatever
** frequency it determined last.
*/
void  audio_freqscale_ena(boole ena);


#endif
