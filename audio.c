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



/*
** Note:
**
** This uses the legacy SDL API to support compiling with SDL1 as well. Don't
** try to "fix" this! (It will break SDL1 builds, notably Emscripten!)
*/




/* Whole for the audio sample increment, bits */
#define AUDIO_INC_W    17U

/* Preferred audio sample increment fraction */
#define AUDIO_INC_P    (((auint)(15734U) << AUDIO_INC_W) / 48000U)

/* Audio output buffer size, must be a power of 2 (48KHz samples) */
#ifndef __EMSCRIPTEN__
#define AUDIO_OUT_SIZE 2048U
#else
#define AUDIO_OUT_SIZE 1024U
#endif

/* Preferred buffer filledness. Good value depends on output buffer size.
** (A 48KHz output sample roughly equals 3 Uzebox samples, the fill must be
** at least the consumed Uzebox sample count to have a chance of smooth
** playback) */
#ifndef __EMSCRIPTEN__
#define AUDIO_FILL     ((AUDIO_OUT_SIZE / 3U) * 2U)
#else
#define AUDIO_FILL     ((AUDIO_OUT_SIZE / 3U) * 8U)
#endif

/* Ring buffer size, must be a power of 2 (15.7KHz Uzebox samples) */
#ifndef __EMSCRIPTEN__
#define AUDIO_BUF_SIZE (AUDIO_OUT_SIZE * 2U)
#else
#define AUDIO_BUF_SIZE (AUDIO_OUT_SIZE * 8U)
#endif

/* Emscripten notes: The total buffer size intentionally is roughly two times
** larger than the size used for native compiles, but balanced differently,
** maintaining a longer buffer within the emulator, requesting a rather small
** one for output. Normally this wouldn't be optimal (if the callback can't
** happen in time, skips occur), but Emscripten has some pecularity about
** lags, larger output buffers steeply (NOT propotionally) increasing audio
** lag. */


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

/* First run after reset mark (to reset in sync) */
static boole audio_frun;

/* Frequency scaling state: enabled or disabled */
static boole audio_fs_isena = TRUE;



/*
** Audio callback
*/
void audio_callback(void* dummy, Uint8* stream, int len)
{
 auint i;
 auint brem;
 auint brav;
 auint bras;
 auint xinc;

 /* If this is the first run, init */

 if (audio_frun){
  audio_buf_r = 0U;
  audio_buf_w = AUDIO_FILL;
  audio_inc   = AUDIO_INC_P;
  audio_frac  = 0U;
  audio_pbrav = audio_buf_w;
  for (i = 0U; i < 31U; i++){ audio_pbrem[i] = audio_buf_w; }
  memset(&(audio_buf[0]), 0x80U, sizeof(audio_buf));
  audio_frun  = FALSE;
 }

 /* Calculate averaging buffer of remaining samples. */

 brem = (audio_buf_w - audio_buf_r) & (AUDIO_BUF_SIZE - 1U);
 brav = brem;
 bras = brem;
 for (i = 0U; i < 31U; i++){ brav += audio_pbrem[i]; }
 for (i = 0U; i <  7U; i++){ bras += audio_pbrem[i]; }
 brav = (brav + 15U) >> 5;
 bras = (bras +  3U) >> 3;

 /* Frequency scaling */

 if (audio_fs_isena){

  /* Propotional */

  if       (brav < AUDIO_FILL){
   if (audio_inc > (((AUDIO_INC_P) *  50U) / 100U)){ /* Allow slow down to 50% (for too slow machines) */
    audio_inc --;
   }
  }else if (brav > AUDIO_FILL){
   if (audio_inc < (((AUDIO_INC_P) * 105U) / 100U)){
    audio_inc ++;
   }
  }else{}

  /* Differential */

  if       (brav < audio_pbrav){
   audio_inc --;
  }else if (brav > audio_pbrav){
   audio_inc ++;
  }else{}

 }

 /* Produce a temporary increment to push the buffer's filledness towards
 ** the ideal point faster by a short term average. Too large buffers are
 ** drained faster, possibly caused by event congestion (many events firing
 ** at once such as after a load burst). */

 xinc = audio_inc;
 if (bras < AUDIO_FILL){
  xinc -= (AUDIO_FILL - bras) >> 3;
  if (bras < ((AUDIO_FILL * 7U) / 8U)){
   xinc -= (((AUDIO_FILL * 7U) / 8U) - bras) >> 1;
  }
 }else{
  xinc += (bras - AUDIO_FILL) >> 3;
  if (bras > ((AUDIO_FILL * 9U) / 8U)){
   xinc += (bras - ((AUDIO_FILL * 9U) / 8U));
  }
  if (bras > ((AUDIO_FILL * 3U) / 2U)){
   xinc += (bras - ((AUDIO_FILL * 3U) / 2U)) << 3;
  }
 }

 /* Sample output */

 for (i = 0U; i < len; i++){
  if (audio_buf_w == audio_buf_r){
   stream[i] = audio_buf[(audio_buf_r - 1U) & (AUDIO_BUF_SIZE - 1U)];
  }else{
   stream[i] = audio_buf[audio_buf_r];
   audio_frac += xinc;
   if (audio_frac >= (1U << AUDIO_INC_W)){
    audio_frac &= ((1U << AUDIO_INC_W) - 1U);
    audio_buf_r = (audio_buf_r + 1U) & (AUDIO_BUF_SIZE - 1U);
   }
  }
 }

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

 if (audio_dev == 0U){

  memset(&desired, 0, sizeof(desired));

  desired.freq     = 48000U;
  desired.format   = AUDIO_U8;
  desired.callback = audio_callback;
  desired.channels = 1U;
  desired.samples  = AUDIO_OUT_SIZE;

  audio_dev = 1U;
  if (SDL_OpenAudio(&desired, &dummy) < 0){ audio_dev = 0U; }
  if (audio_dev != 0U){ SDL_PauseAudio(1); }

  audio_frun = TRUE;

 }

 return (audio_dev != 0U);
}



/*
** Resets audio, call it along with the start of emulation.
*/
void  audio_reset(void)
{
 audio_frun = TRUE;
 if (audio_dev != 0U){ SDL_PauseAudio(0); }
}



/*
** Tears down audio (if initialization succeed)
*/
void  audio_quit(void)
{
 if (audio_dev != 0U){
  SDL_CloseAudio();
  audio_dev = 0U;
 }
}



/*
** Send a frame (unsigned 8 bit samples) to the audio device. If NULL is
** passed, then silence will be added.
*/
void  audio_sendframe(uint8 const* samples, auint len)
{
 auint i;
 auint brem = (audio_buf_r - audio_buf_w) & (AUDIO_BUF_SIZE - 1U);

 if ( (brem != 0U) &&
      (brem < len) ){ return; } /* Buffer full */

 if (samples != NULL){
  for (i = 0U; i < len; i++){
   audio_buf[audio_buf_w] = samples[i];
   audio_buf_w = (audio_buf_w + 1U) & (AUDIO_BUF_SIZE - 1U);
  }
 }else{
  for (i = 0U; i < len; i++){
   audio_buf[audio_buf_w] = 0x80U;
   audio_buf_w = (audio_buf_w + 1U) & (AUDIO_BUF_SIZE - 1U);
  }
 }
}



/*
** Returns current long-term audio frequency.
*/
auint audio_getfreq(void)
{
 return (48000U * audio_inc) >> AUDIO_INC_W;
}



/*
** Enables or disables frequency scaling. By default frequency scaling is
** enabled. When disabled, the long term PD controller is fixed at whatever
** frequency it determined last.
*/
void  audio_freqscale_ena(boole ena)
{
 audio_fs_isena = ena;
}
