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



#include "avconv.h"
#include "guicore.h"
#include <unistd.h>



/* Identifies whether any capturing was performed */
static boole avconv_isinit = FALSE;

/* Identifies whether the recording is available (that opening the pipes did not fail) */
static boole avconv_ok     = FALSE;

/* The pipe used for the audio stream */
static FILE* avconv_pipe_a;

/* The pipe used for the video stream */
static FILE* avconv_pipe_v;

/* Audio fraction (to get 800.8008 samples / frame) */
static auint avconv_afrac = 0U;



/*
** Request pushing a frame into the video stream. If it wasn't initialized
** yet, it is initialized. It fetches the image data by reading the game
** region of the 640 x 270 backbuffer from guicore, and the audio data of
** the frame as directly passed to it.
*/
void avconv_push(uint8 const* samples, auint len)
{
 uint32* pixbuf;
 auint   areq;
 uint8   samp48k[801];
 auint   i;
 auint   j;
 auint   frac;
 char    tstr[256];
 char    pixs[5] = {'0', '0', '0', '0', 0};
 guicore_pixfmt_t pixfmt;

 /* Initialize recording if necessary */

 if (!avconv_isinit){

  /* Note: This method of just mashing up a pixel format string for ffmpeg
  ** might utterly fail if it doesn't support what the GUI came up with.
  ** Usually it should work. If it doesn't, a (costly) pixel format conversion
  ** stage would have to be added here. */

  guicore_getpixfmt(&pixfmt);
  pixs[pixfmt.rsh / 8U] = 'r';
  pixs[pixfmt.gsh / 8U] = 'g';
  pixs[pixfmt.bsh / 8U] = 'b';

  snprintf(&(tstr[0]),
      256U,
      "ffmpeg"
      " -y"                   /* Override output files */
      " -f rawvideo"          /* Raw video input */
      " -s 640x228"           /* Input size */
      " -pix_fmt %s"          /* Pixel format */
      " -r 59.94"             /* Input rate (Hz) */
      " -i -"                 /* Input source: pipe */
      " -vf "                 /* Video filters */
        "crop=620:228:10:0"   /* Crop to the game area */
      " -an"                  /* Audio disabled */
      " -preset ultrafast"    /* H.264 encoding speed / quality */
      " -qp 0"                /* H.264 lossless (so generating output as fast as possible) */
      " -tune animation"      /* H.264 tune */
      " ~tempvid.mp4"         /* Output file */
      " 1> ~tempvid.log"      /* Output log */
      " 2> ~tempvid.err",     /* Output log */
      &pixs[0]);

  avconv_pipe_v = popen(
      &(tstr[0]),
      "w");

  avconv_pipe_a = popen(
      "ffmpeg"
      " -y"                   /* Override output files */
      " -f u8"                /* Raw unsigned 8 bit samples */
      " -ar 48000"            /* Input rate (Hz) */
      " -ac 1"                /* Input channels */
      " -i -"                 /* Input source: pipe */
      " -acodec: libmp3lame"  /* Encoder */
      " ~tempaud.mp3"         /* Output file */
      " 1> ~tempaud.log"      /* Output log */
      " 2> ~tempaud.err",     /* Output log */
      "w");

  if ( (avconv_pipe_v == NULL) ||
       (avconv_pipe_a == NULL) ){
   if (avconv_pipe_v != NULL){ pclose(avconv_pipe_v); }
   if (avconv_pipe_a != NULL){ pclose(avconv_pipe_a); }
  }else{
   avconv_ok = TRUE;
  }

  avconv_isinit = TRUE;

 }

 /* Generate data for the pipes */

 if (avconv_ok){

  /* Generate 800.8008 samples per frame */

  areq = 800U;
  avconv_afrac += 8008U;
  if (avconv_afrac >= 10000U){
   areq ++;
   avconv_afrac -= 10000U;
  }

  frac = 0U;
  j = 0U;
  for (i = 0U; i < areq; i ++){
   samp48k[i] = samples[j];
   frac += len;
   if (frac >= areq){
    frac -= areq;
    j++;
   }
  }

  /* Output stuff */

  pixbuf = guicore_getpixbuf() + (640U * 20U); /* At game area top */
  fwrite(pixbuf, (640U * 228U) * sizeof(uint32), 1U, avconv_pipe_v);
  fwrite(&(samp48k[0]), areq, 1U, avconv_pipe_a);

 }
}



/*
** Finalizes the generated video stream (if any capturing was requested).
*/
void avconv_finalize(void)
{
 FILE* mux;

 if ( (avconv_isinit) &&
      (avconv_ok) ){

  pclose(avconv_pipe_v);
  pclose(avconv_pipe_a);

  mux = popen(
      "ffmpeg"
      " -y"
      " -i ~tempvid.mp4"
      " -i ~tempaud.mp3"
      " -vf "                 /* Video filters */
        "scale=940:228:flags=bilinear,"   /* Scale up width smoothly */
        "scale=940:684:flags=neighbor,"   /* Scale up height to 3x with nearest */
        "pad=960:720:16:18"               /* Pad to 720p */
      " -preset medium"       /* H.264 encoding speed / quality */
      " -crf 22"              /* H.264 output quality / size */
      " -tune animation"      /* H.264 tune */
      " -acodec copy"
      " -f mp4"
      " capture.mp4",
      "r");
  if (mux != NULL){
   pclose(mux);
   unlink("~tempvid.mp4");
   unlink("~tempaud.mp3");
   unlink("~tempvid.log");
   unlink("~tempaud.log");
   unlink("~tempvid.err");
   unlink("~tempaud.err");
  }

 }
}
