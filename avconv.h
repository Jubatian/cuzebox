/*
 *  AVConv video output generator
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



#ifndef AVCONV_H
#define AVCONV_H



#include "types.h"



/*
** Generates a video & audio stream from the emulation.
**
** It operates by processing audio and video separately by piping them to
** ffmpeg instances, then upon exit, it merges those into a proper video
** file.
**
** Some notes on timings:
**
** The NTSC colorburst is 3.579545 MHz, there are 227.5 pulses in a line.
** The normal NTSC video generates one frame in 262.5 lines. This results
** in the exactly 59.94 Hz video output (and 15.734 KHz audio if one sample
** was generated on a line).
**
** The Uzebox however generates only 262 lines for a frame. This normally
** would produce 60.0544 Hz video (along with the same 15.734 KHz audio).
** If the Uzebox's output was slowed down to produce 59.94 Hz, its audio
** should play at 15.704 KHz to remain in sync.
**
** It is more practical to produce the audio however at a fixed rate, and
** keep supplying the necessary number of samples scaling the audio result
** as needed to stay in sync. For 48 KHz audio, 800.8008 samples are
** required for a 59.94 Hz frame. (Accuracy is important in long term as
** even being one sample off would accumulate to a second in a little more
** than 10 minutes).
*/



/*
** Request pushing a frame into the video stream. If it wasn't initialized
** yet, it is initialized. It fetches the image data by reading the game
** region of the 640 x 270 backbuffer from guicore, and the audio data of
** the frame as directly passed to it.
*/
void avconv_push(uint8 const* samples, auint len);


/*
** Finalizes the generated video stream (if any capturing was requested).
*/
void avconv_finalize(void);


#endif
