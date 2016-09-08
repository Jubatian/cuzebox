/*
 *  Main
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


#include "types.h"
#include "cu_ufile.h"
#include "cu_hfile.h"
#include "cu_avr.h"
#include "cu_ctr.h"
#include "guicore.h"
#include "audio.h"
#include "frame.h"
#include "eepdump.h"
#include <SDL2/SDL.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif



/* Cuzebox window title formatter */
static const char* main_title_fstr = "CUzeBox CPU: %3u%% (%2u.%03u MHz); FPS: %2u";

/* Exit request */
static boole main_exit = FALSE;

/* 60Hz main loop timing to coarsely maintain 60Hz output */
static auint main_tdrift = 0U;

/* Previous millisecond tick */
static auint main_ptick;

/* 1/3 counter to get the 16.6667ms average frame time */
static auint main_tfrac = 0U;

/* Frame counter for frame dropping */
static auint main_frc = 0U;

/* 500 millisecond counter to generate FPS info */
static auint main_t500 = 0U;

/* Current frame counter during the 500ms tick */
static auint main_t5_frc = 0U;

/* Previous three frame counts for the 500ms tick */
static auint main_t5_frp[3] = {30U, 30U, 30U};

/* Previous cycle counter value for the 500ms tick */
static auint main_t5_cc;

/* Previous state of EEPROM changes to save only after a write burst was completed */
static boole main_peepch = FALSE;

/* Default game to start without parameters */
static const char* main_game = "gamefile.uze";



/*
** Sets a controller button by SDL event
*/
static void main_setctr(SDL_Event const* ev)
{
 boole press = FALSE;

 if ( ((ev->type) == SDL_KEYDOWN) ||
      ((ev->type) == SDL_KEYUP) ){

  if ((ev->type) == SDL_KEYDOWN){ press = TRUE; }

  switch (ev->key.keysym.scancode){
   case SDL_SCANCODE_LEFT:
    cu_ctr_setsnes_single(0U, CU_CTR_SNES_LEFT, press);
    break;
   case SDL_SCANCODE_RIGHT:
    cu_ctr_setsnes_single(0U, CU_CTR_SNES_RIGHT, press);
    break;
   case SDL_SCANCODE_UP:
    cu_ctr_setsnes_single(0U, CU_CTR_SNES_UP, press);
    break;
   case SDL_SCANCODE_DOWN:
    cu_ctr_setsnes_single(0U, CU_CTR_SNES_DOWN, press);
    break;
   case SDL_SCANCODE_A:
    cu_ctr_setsnes_single(0U, CU_CTR_SNES_Y, press);
    break;
   case SDL_SCANCODE_S:
    cu_ctr_setsnes_single(0U, CU_CTR_SNES_X, press);
    break;
   case SDL_SCANCODE_Z:
    cu_ctr_setsnes_single(0U, CU_CTR_SNES_B, press);
    break;
   case SDL_SCANCODE_X:
    cu_ctr_setsnes_single(0U, CU_CTR_SNES_A, press);
    break;
   case SDL_SCANCODE_RETURN:
    cu_ctr_setsnes_single(0U, CU_CTR_SNES_SELECT, press);
    break;
   case SDL_SCANCODE_SPACE:
    cu_ctr_setsnes_single(0U, CU_CTR_SNES_START, press);
    break;
   default:
    break;
  }

 }
}



/*
** Main loop body
*/
static void main_loop(void)
{
 auint ctick = SDL_GetTicks();
 auint tdif  = ctick - main_ptick;
 auint drift = main_tdrift;
 auint favg;
 auint ccur;
 auint cdif;
 boole fdrop = FALSE;
 boole eepch;
 char  tstr[100];
 SDL_Event sdlevent;
 cu_state_cpu_t* ecpu;

 /* First "sandbox" the drift to silently skip main loops if the emulator is
 ** running too fast */

 /* Calculate drift from perfect 60Hz */

 if (main_tfrac >= 2U){ drift += 16U; }
 else{                  drift += 17U; }
 drift -= tdif;

 /* Handle divergences */

 if (drift > 0x80000000U){    /* Possibly too slow */
  if ((drift + 50U) > 0x80000000U){
   drift = (auint)(0U) - 50U; /* Just limit */
   if ((main_frc & 1U) != 0U){ fdrop = TRUE; } /* Drop a frame */
  }
 }else{                       /* Possibly too fast */
  if (drift > 50U){           /* Waste away time by dropping main loops */
   SDL_Delay(5);
   return;
  }
 }

 /* Now the drift can be saved */

 main_tdrift = drift;
 main_ptick  = ctick;
 main_frc   ++;
 if (main_tfrac >= 2U){ main_tfrac = 0U; }
 else{                  main_tfrac ++;   }

 /* Tolerate some divergence from perfect 60Hz (2%) */

 if (main_tfrac == 0U){
  if       (main_tdrift > 0x80000000U){
   main_tdrift ++;
  }else if (main_tdrift != 0U){
   main_tdrift --;
  }else{}
 }

 /* Go on with the frame's logic */

 (void)(frame_run(fdrop)); /* Don't care for return, it will be about a frame worth of emulation anyway */
 audio_sendframe(frame_getaudio());
 guicore_update(fdrop);

 /* Check for EEPROM changes and save as needed */

 eepch = cu_avr_eeprom_ischanged(TRUE);
 if ( (main_peepch) &&
      (!eepch) ){ /* End of EEPROM write burst */
  ecpu = cu_avr_get_state();
  eepdump_save(&(ecpu->eepr[0]));
 }
 main_peepch = eepch;

 /* Generate FPS info */

 main_t500 += tdif;
 main_t5_frc ++;

 if (main_t500 >= 500U){
  main_t500 -= 500U;

  favg = ( main_t5_frc +
           main_t5_frp[0] +
           main_t5_frp[1] +
           main_t5_frp[2] ) / 2U;
  main_t5_frp[2] = main_t5_frp[1];
  main_t5_frp[1] = main_t5_frp[0];
  main_t5_frp[0] = main_t5_frc;
  main_t5_frc    = 0U;

  ccur       = cu_avr_getcycle();
  cdif       = ccur - main_t5_cc;
  main_t5_cc = ccur;

  snprintf(
      &(tstr[0]),
      100U,
      main_title_fstr,
      ((cdif * 100U) + 7159090U) / 14318180U,
      ((cdif * 2U) / 1000000U),
      ((cdif * 2U) / 1000U) % 1000U,
      favg);
  guicore_setcaption(&(tstr[0]));
  fputs(&(tstr[0]), stdout);
  fputs("\n",       stdout);
 }

 /* Process events */

 while (SDL_PollEvent(&sdlevent) != 0){
  main_setctr(&sdlevent);
  if (sdlevent.type == SDL_QUIT){ main_exit = TRUE; }
 }
}



/*
** Main entry point
*/
int main (int argc, char** argv)
{
 cu_ufile_header_t ufhead;
 cu_state_cpu_t*   ecpu;
 char              tstr[100];
 char const*       game = main_game;

 if (argc > 1){ game = argv[1]; }


 ecpu = cu_avr_get_state();
 eepdump_load(&(ecpu->eepr[0]));

 if (!cu_ufile_load(game, &(ecpu->crom[0]), &ufhead)){
  if (!cu_hfile_load(game, &(ecpu->crom[0]))){
   return 1;
  }
 }

 ecpu->wdseed = rand(); /* Seed the WD timeout used for PRNG seed in Uzebox games */

 fprintf(stdout, "Starting emulator\n");


 snprintf(&(tstr[0]), 100U, main_title_fstr, 100U, 28U, 636U, 60U);


 if (!guicore_init(0U, &(tstr[0]))){
  return 1;
 }
 (void)(audio_init());


 cu_avr_reset();
 main_t5_cc = cu_avr_getcycle();
 main_ptick = SDL_GetTicks();


#ifdef __EMSCRIPTEN__
 emscripten_set_main_loop(&main_loop, 0, 1);
#else
 while (!main_exit){ main_loop(); }

 ecpu = cu_avr_get_state();
 eepdump_save(&(ecpu->eepr[0]));

 audio_quit();
 guicore_quit();
#endif

 return 0;
}
