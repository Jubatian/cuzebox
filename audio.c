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
#define AUDIO_INC_P    (((auint)(15720U) * 0x10000U) / 48000U)

/* Ring buffer size, must be a power of 2. */
#define AUDIO_BUF_SIZE 2048U

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

/* Frame consumption pointer (for timing rescalings) */
static auint audio_fcp;



/*
** Audio callback
*/
void audio_callback(void* dummy, Uint8* stream, int len)
{
 auint i;
 auint brem = (audio_buf_w - audio_buf_r) & (AUDIO_BUF_SIZE - 1U);
 auint prrp = audio_buf_r;

 /* Frequency scaling */

 if (audio_fcp >= (262U * 8U)){

  if       (brem < (262U * 3U)){
   if (audio_inc > (((AUDIO_INC_P) *  95U) / 100U)){
    audio_inc -= ((AUDIO_INC_P * ((262U * 3U) - brem)) + 0xFFFFFU) / 0x100000U;
   }
  }else if (brem > (262U * 5U)){
   if (audio_inc < (((AUDIO_INC_P) * 105U) / 100U)){
    audio_inc += ((AUDIO_INC_P * (brem - (262U * 5U))) + 0xFFFFFU) / 0x100000U;
   }
  }else{
   if       (audio_inc > AUDIO_INC_P){
    audio_inc -= (((audio_inc - AUDIO_INC_P) + 0x1FFU) / 0x200U);
   }else if (audio_inc < AUDIO_INC_P){
    audio_inc += (((AUDIO_INC_P - audio_inc) + 0x1FFU) / 0x200U);
   }else{}
  }

  audio_fcp -= (262U * 8U);

 }

 /* Sample output */

 for (i = 0U; i < len; i++){
  if (audio_buf_w == audio_buf_r){
   stream[i] = 0x80U;
  }else{
   stream[i] = audio_buf[audio_buf_r];
   audio_frac += audio_inc;
   if (audio_frac >= 0x10000U){
    audio_frac &= 0xFFFFU;
    audio_buf_r = (audio_buf_r + 1U) & (AUDIO_BUF_SIZE - 1U);
   }
  }
 }

 audio_fcp += (audio_buf_r - prrp) & (AUDIO_BUF_SIZE - 1U);
}



/*
** Attempts to initialize audio. If it returns false, no sound will be
** generated.
*/
boole audio_init(void)
{
 SDL_AudioSpec desired;
 SDL_AudioSpec dummy;

 audio_buf_r = 0U;
 audio_buf_w = 262U * 3U;
 audio_inc   = AUDIO_INC_P;
 audio_frac  = 0U;
 audio_fcp   = 0U;

 memset(&(audio_buf[0]), 0x7FU, sizeof(audio_buf));
 memset(&desired, 0, sizeof(desired));

 desired.freq     = 48000U;
 desired.format   = AUDIO_U8;
 desired.callback = audio_callback;
 desired.channels = 1U;
 desired.samples  = 512U;

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
