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



#include "audio.h"
#include <SDL2/SDL.h>



/* Preferred audio sample increment fraction */
#define AUDIO_INC_P    (((auint)(15734U) * 0x10000U) / 48000U)

/* Audio output buffer size, must be a power of 2 (48KHz samples) */
#ifndef __EMSCRIPTEN__
#define AUDIO_OUT_SIZE 2048U
#else
#define AUDIO_OUT_SIZE 4096U
#endif

/* Preferred buffer filledness. Good value depends on output buffer size.
** (A 48KHz output sample roughly equals 3 Uzebox samples, the fill must be
** at least the consumed Uzebox sample count to have a chance of smooth
** playback) */
#define AUDIO_FILL     ((AUDIO_OUT_SIZE / 3U) * 2U)

/* Ring buffer size, must be a power of 2 (15.7KHz Uzebox samples) */
#define AUDIO_BUF_SIZE (AUDIO_OUT_SIZE * 4U)


/* Ring buffer of samples */
static uint8 audio_buf[AUDIO_BUF_SIZE];
static auint audio_buf_r;
static auint audio_buf_w;

/* Audio device */
static auint audio_dev = 0U;

/* Read pointer increment fraction for audio output (used to scale frequency) */
static auint audio_inc;

/* Current pointer fraction */
static auint audio_frac;

/* Previous remaining samples counts for averaging */
static auint audio_pbrem[31];

/* Previous remaining samples average count for differential correction */
static auint audio_pbrav;



/*
** Audio callback
*/
void audio_callback(void* dummy, Uint8* stream, int len)
{
 auint i;
 auint brem = (audio_buf_w - audio_buf_r) & (AUDIO_BUF_SIZE - 1U);
 auint brav = brem;
 boole tbd  = FALSE;

 for (i = 0U; i < 31U; i++){ brav += audio_pbrem[i]; }
 brav = (brav + 15U) >> 5;

 /* Frequency scaling */

 /* Propotional */

 if (brav < AUDIO_FILL){
  if (audio_inc > (((AUDIO_INC_P) *  75U) / 100U)){ /* Allow up to 25% slow down (for too slow machines) */
   if (brav <= audio_pbrav){ /* Only push it until tendency turns around */
    i = (AUDIO_FILL - brav);
    audio_inc -= ((AUDIO_INC_P * i) + 0x7FFFFU) / 0x80000U;
   }
  }
 }else{
  if (audio_inc < (((AUDIO_INC_P) * 105U) / 100U)){
   if (brav >= audio_pbrav){ /* Only push it until tendency turns around */
    i = (brav - AUDIO_FILL);
    audio_inc += ((AUDIO_INC_P * i) + 0x7FFFFU) / 0x80000U;
   }
  }
 }

 /* Differential */

 if (brav < audio_pbrav){
  i = audio_pbrav - brav;
  audio_inc -= ((AUDIO_INC_P * i) + 0x7FFFU) / 0x8000U;
 }else{
  i = brav - audio_pbrav;
  audio_inc += ((AUDIO_INC_P * i) + 0x7FFFU) / 0x8000U;
 }

 /* Sample output */

 for (i = 0U; i < len; i++){
  if (audio_buf_w == audio_buf_r){
   stream[i] = audio_buf[(audio_buf_r - 1U) & (AUDIO_BUF_SIZE - 1U)];
   tbd = TRUE;
  }else{
   stream[i] = audio_buf[audio_buf_r];
   audio_frac += audio_inc;
   if (audio_frac >= 0x10000U){
    audio_frac &= 0xFFFFU;
    audio_buf_r = (audio_buf_r + 1U) & (AUDIO_BUF_SIZE - 1U);
   }
  }
 }

 /* If there was a total buffer depletion, assume some transient problem and
 ** ignore it instead of scaling frequency to it. */

 if (tbd){ brem = (AUDIO_FILL / 8U) * 7U; }

 /* Finalize scaling state */

 for (i = 30U; i != 0U; i--){ audio_pbrem[i] = audio_pbrem[i - 1U]; }
 audio_pbrem[0] = brem;
 audio_pbrav = brav;
}



/*
** Attempts to initialize audio. If it returns false, no sound will be
** generated.
*/
boole audio_init(void)
{
 SDL_AudioSpec desired;
 SDL_AudioSpec dummy;
 auint         i;

 audio_buf_r = 0U;
 audio_buf_w = 262U * 4U;
 audio_inc   = AUDIO_INC_P;
 audio_frac  = 0U;
 audio_pbrav = audio_buf_w;
 for (i = 0U; i < 31U; i++){ audio_pbrem[i] = audio_buf_w; }

 memset(&(audio_buf[0]), 0x7FU, sizeof(audio_buf));
 memset(&desired, 0, sizeof(desired));

 desired.freq     = 48000U;
 desired.format   = AUDIO_U8;
 desired.callback = audio_callback;
 desired.channels = 1U;
 desired.samples  = AUDIO_OUT_SIZE;

 audio_dev = SDL_OpenAudioDevice(NULL, 0, &desired, &dummy, 0);
 if (audio_dev == 0U){
  return FALSE;
 }
 SDL_PauseAudioDevice(audio_dev, 0);

 return TRUE;
}



/*
** Tears down audio (if initialization succeed)
*/
void  audio_quit(void)
{
 if (audio_dev != 0U){
  SDL_CloseAudioDevice(audio_dev);
  audio_dev = 0U;
 }
}



/*
** Send a frame (262 unsigned 8 bit samples) to the audio device.
*/
void  audio_sendframe(uint8 const* samples)
{
 auint i;
 auint brem = (audio_buf_r - audio_buf_w) & (AUDIO_BUF_SIZE - 1U);

 if ( (brem != 0U) &&
      (brem < 262U) ){ return; } /* Buffer full */

 for (i = 0U; i < 262U; i++){
  audio_buf[audio_buf_w] = samples[i];
  audio_buf_w = (audio_buf_w + 1U) & (AUDIO_BUF_SIZE - 1U);
 }
}
