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
#include "filesys.h"
#include "guicore.h"
#include "audio.h"
#include "frame.h"
#include "eepdump.h"
#ifdef ENABLE_VCAP
#include "avconv.h"
#endif



/* Initial title */
static const char* main_title = "CUzeBox";

/* Cuzebox window title formatter */
static const char* main_title_fstr = "CUzeBox CPU: %3u%% (%2u.%03u MHz); FPS: %2u; %sFreq: %2u.%03u KHz; Keys: %s %s";

/* Appended info: None */
static const char* main_title_fstr_e = "";

/* Keymap: SNES */
static const char* main_title_fstr_ksnes = "SNES";

/* Keymap: UZEM */
static const char* main_title_fstr_kuzem = "UZEM";

/* Keymap current */
static const char* main_title_fstr_k;

/* Frame merge ON */
static const char* main_title_fstr_m1 = "Merge ";

/* Frame merge OFF */
static const char* main_title_fstr_m0 = "";

/* Frame merge current */
static const char* main_title_fstr_m;

#ifdef ENABLE_VCAP
/* Appended info: Capturing */
static const char* main_title_fstr_c = "(Capturing)";
#endif

/* Exit request */
static boole main_exit = FALSE;

/* 60Hz main loop timing to coarsely maintain 60Hz output */
static auint main_tdrift = 0U;

/* Previous millisecond tick */
static auint main_ptick;

/* 1/3 counter to get the 16.6667ms average frame time */
static auint main_tfrac = 0U;

/* Frame counter for frame dropping and limiting */
static auint main_frc = 0U;

/* Frame dropping tendency; if nonzero, it drops frames */
static auint main_fdrop = 0U;

/* 500 millisecond counter to generate FPS info */
static auint main_t500 = 0U;

/* Current frame counter during the 500ms tick */
static auint main_t5_frc = 0U;

/* Previous frame count for the 500ms tick */
static auint main_t5_frp = 30U;

/* Previous cycle counter value for the 500ms tick */
static auint main_t5_cc;

/* Request for discarding frame limitation */
static boole main_nolimit = FALSE;

/* Request Uzem style keymapping */
static boole main_kbuzem = FALSE;

/* Request frame merging (flicker reduction) */
static boole main_fmerge = TRUE;

/* Previous state of EEPROM changes to save only after a write burst was completed */
static boole main_peepch = FALSE;

/* Default game to start without parameters */
static const char* main_game = "gamefile.uze";

#ifdef ENABLE_VCAP
/* Whether video capturing is enabled */
static boole main_isvcap = FALSE;
#endif



/*
** Sets a controller button by SDL event
*/
static void main_setctr(SDL_Event const* ev)
{
 boole press = FALSE;

 if ( ((ev->type) == SDL_KEYDOWN) ||
      ((ev->type) == SDL_KEYUP) ){

  if ((ev->type) == SDL_KEYDOWN){ press = TRUE; }

  /* Note: For SDL2 the scancode has more sense here, but SDL1 does not
  ** support that. This solution works for now on both. For the Uzem
  ** keymapping both Y and Z triggers SNES_Y, so it remains useful on both a
  ** QWERTY and a QWERTZ keyboard. */

  switch (ev->key.keysym.sym){
   case SDLK_LEFT:
    cu_ctr_setsnes_single(0U, CU_CTR_SNES_LEFT, press);
    break;
   case SDLK_RIGHT:
    cu_ctr_setsnes_single(0U, CU_CTR_SNES_RIGHT, press);
    break;
   case SDLK_UP:
    cu_ctr_setsnes_single(0U, CU_CTR_SNES_UP, press);
    break;
   case SDLK_DOWN:
    cu_ctr_setsnes_single(0U, CU_CTR_SNES_DOWN, press);
    break;
   case SDLK_q:
    if (!main_kbuzem){ cu_ctr_setsnes_single(0U, CU_CTR_SNES_Y, press); }
    break;
   case SDLK_w:
    if (!main_kbuzem){ cu_ctr_setsnes_single(0U, CU_CTR_SNES_X, press); }
    break;
   case SDLK_a:
    if (!main_kbuzem){ cu_ctr_setsnes_single(0U, CU_CTR_SNES_B, press); }
    if ( main_kbuzem){ cu_ctr_setsnes_single(0U, CU_CTR_SNES_A, press); }
    break;
   case SDLK_s:
    if (!main_kbuzem){ cu_ctr_setsnes_single(0U, CU_CTR_SNES_A, press); }
    if ( main_kbuzem){ cu_ctr_setsnes_single(0U, CU_CTR_SNES_B, press); }
    break;
   case SDLK_y:
    if ( main_kbuzem){ cu_ctr_setsnes_single(0U, CU_CTR_SNES_Y, press); }
    break;
   case SDLK_z:
    if ( main_kbuzem){ cu_ctr_setsnes_single(0U, CU_CTR_SNES_Y, press); }
    break;
   case SDLK_x:
    if ( main_kbuzem){ cu_ctr_setsnes_single(0U, CU_CTR_SNES_X, press); }
    break;
   case SDLK_SPACE:
    cu_ctr_setsnes_single(0U, CU_CTR_SNES_SELECT, press);
    break;
   case SDLK_TAB:
    cu_ctr_setsnes_single(0U, CU_CTR_SNES_SELECT, press);
    break;
   case SDLK_RETURN:
    cu_ctr_setsnes_single(0U, CU_CTR_SNES_START, press);
    break;
   case SDLK_LSHIFT:
    cu_ctr_setsnes_single(0U, CU_CTR_SNES_LSH, press);
    break;
   case SDLK_RSHIFT:
    cu_ctr_setsnes_single(0U, CU_CTR_SNES_RSH, press);
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
 auint rows;
 auint fdtmp = main_fdrop;
 auint afreq;
 boole fdrop = FALSE;
 boole eepch;
 char  tstr[128];
 SDL_Event sdlevent;
 cu_state_cpu_t* ecpu;
 const char*     titext = main_title_fstr_e;

 /* First "sandbox" the drift to silently skip main loops if the emulator is
 ** running too fast */

 /* Calculate drift from perfect 60Hz */

 if (main_tfrac >= 2U){ drift += 16U; }
 else{                  drift += 17U; }
 drift -= tdif;

 /* Handle divergences */

 if (drift >= 0x80000000U){   /* Possibly too slow */
  if ((drift + 8U) >= 0x80000000U){   /* Transient problem (late by at least 1 frame) */
   if ((drift + 75U) >= 0x80000000U){ /* Permanent problem (late by several frames) */
    if ((drift + 225U) >= 0x80000000U){
     drift = (auint)(0U) - 225U;      /* Limit (throw away memory) */
    }
    if (fdtmp < 30U){ fdtmp = 30U; }  /* Semi-permanently limit frame rate */
   }
   if (fdtmp < 60U){ fdtmp ++; }      /* Push frame drop request */
  }
 }else{                       /* Possibly too fast */
  if (fdtmp != 0U){ fdtmp --; }  /* Eat away frame drop request */
  if (drift > 75U){           /* Limit drift (to not let it accumulating for a nolimit run) */
   drift = 75U;
  }
  if ( ((drift > 25U)) &&     /* Waste away time by dropping main loops */
       (!main_nolimit) ){     /* (unless frame rate limiter was turned off) */
#ifndef __EMSCRIPTEN__
   SDL_Delay(5);              /* Emscripten doesn't need this since the loop is a callback */
#endif
   return;
  }
 }
 if ( (fdtmp != 0U) &&
      ((main_frc & 1U) != 0U) ){ fdrop = TRUE; } /* Skip every second frame if necessary */

 /* Now the drift can be saved */

 main_tdrift = drift;
 main_ptick  = ctick;
 main_fdrop  = fdtmp;
 main_frc   ++;
 if (main_tfrac >= 2U){ main_tfrac = 0U; }
 else{                  main_tfrac ++;   }

 /* Tolerate some divergence from perfect 60Hz (1.5%) */

 if ((main_frc & 0x3U) == 0U){
  if       (main_tdrift > 0x80000000U){
   main_tdrift ++;
  }else if (main_tdrift != 0U){
   main_tdrift --;
  }else{}
 }

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
 if (!fdrop){ main_t5_frc ++; }

#ifdef ENABLE_VCAP
 if (main_isvcap){ titext = main_title_fstr_c; }
#endif

 if (main_t500 >= 500U){
  main_t500 -= 500U;

  favg = main_t5_frc + main_t5_frp;
  main_t5_frp = main_t5_frc;
  main_t5_frc = 0U;

  ccur       = cu_avr_getcycle();
  cdif       = WRAP32(ccur - main_t5_cc);
  main_t5_cc = ccur;

  afreq = audio_getfreq();

  snprintf(
      &(tstr[0]),
      128U,
      main_title_fstr,
      (cdif + 71591U) / 143182U,
      ((cdif * 2U) / 1000000U),
      ((cdif * 2U) / 1000U) % 1000U,
      favg,
      main_title_fstr_m,
      afreq / 1000U,
      afreq % 1000U,
      main_title_fstr_k,
      titext);
  guicore_setcaption(&(tstr[0]));
  fputs(&(tstr[0]), stdout);
  fputs("\n",       stdout);
 }

 /* Process events */

 while (SDL_PollEvent(&sdlevent) != 0){

  main_setctr(&sdlevent);

  if (sdlevent.type == SDL_QUIT){ main_exit = TRUE; }

  if ((sdlevent.type) == SDL_KEYDOWN){

   switch (sdlevent.key.keysym.sym){

    case SDLK_ESCAPE:
     main_exit = TRUE;
     break;

    case SDLK_F11:
     if (!guicore_init(guicore_getflags() ^ GUICORE_FULLSCREEN, main_title)){ main_exit = TRUE; }
     break;

    case SDLK_F2:
     if (!guicore_init(guicore_getflags() ^ GUICORE_SMALL, main_title)){ main_exit = TRUE; }
     break;

    case SDLK_F3:
     if (!guicore_init(guicore_getflags() ^ GUICORE_GAMEONLY, main_title)){ main_exit = TRUE; }
     break;

    case SDLK_F4:
     if (main_nolimit){
      main_nolimit = FALSE;
      if (!guicore_init(guicore_getflags() & (~GUICORE_NOVSYNC), main_title)){ main_exit = TRUE; }
      audio_freqscale_ena(TRUE);
     }else{
      main_nolimit = TRUE;
      if (!guicore_init(guicore_getflags() | ( GUICORE_NOVSYNC), main_title)){ main_exit = TRUE; }
      audio_freqscale_ena(FALSE);
     }
     break;

#ifdef ENABLE_VCAP
    case SDLK_F5:
     main_isvcap = !main_isvcap;
     break;
#endif

    case SDLK_F7:
     main_fmerge = !main_fmerge;
     if (main_fmerge){ main_title_fstr_m = main_title_fstr_m1; }
     else            { main_title_fstr_m = main_title_fstr_m0; }
     break;

    case SDLK_F8:
     main_kbuzem = !main_kbuzem;
     if (main_kbuzem){ main_title_fstr_k = main_title_fstr_kuzem; }
     else            { main_title_fstr_k = main_title_fstr_ksnes; }
     break;

    default:
     break;
   }

  }
 }

 /* Go on with the frame's logic. Does it here so the SDL_GetTicks() on the
 ** top of the loop is as close to the render's end as reasonably possible. */

#ifdef ENABLE_VCAP

 rows = frame_run(fdrop && (!main_isvcap), main_fmerge);
 audio_sendframe(frame_getaudio(), rows);
 guicore_update(fdrop);

 if (main_isvcap){
  avconv_push(frame_getaudio(), rows);
 }

#else

 rows = frame_run(fdrop, main_fmerge);
 audio_sendframe(frame_getaudio(), rows);
 guicore_update(fdrop);

#endif
}



/*
** Main entry point
*/
int main (int argc, char** argv)
{
 cu_ufile_header_t ufhead;
 cu_state_cpu_t*   ecpu;
 char              tstr[128];
 char const*       game = main_game;

 if (argc > 1){ game = argv[1]; }
 filesys_setpath(game, &(tstr[0]), 100U); /* Locate everything beside the game */

 ecpu = cu_avr_get_state();
 eepdump_load(&(ecpu->eepr[0]));

 if (!cu_ufile_load(&(tstr[0]), &(ecpu->crom[0]), &ufhead)){
  if (!cu_hfile_load(&(tstr[0]), &(ecpu->crom[0]))){
   return 1;
  }
 }

 ecpu->wd_seed = rand(); /* Seed the WD timeout used for PRNG seed in Uzebox games */

 fprintf(stdout, "Starting emulator\n");


 if (main_kbuzem){ main_title_fstr_k = main_title_fstr_kuzem; }
 else            { main_title_fstr_k = main_title_fstr_ksnes; }
 if (main_fmerge){ main_title_fstr_m = main_title_fstr_m1; }
 else            { main_title_fstr_m = main_title_fstr_m0; }
 snprintf(&(tstr[0]), 128U, main_title_fstr, 100U, 28U, 636U, 60U, main_title_fstr_m, 15U, 734U, main_title_fstr_k, main_title_fstr_e);


 if (!guicore_init(0U, main_title)){
  return 1;
 }
 (void)(audio_init());


 cu_avr_reset();
 main_t5_cc = cu_avr_getcycle();
 main_ptick = SDL_GetTicks();
 audio_reset();


#ifdef __EMSCRIPTEN__
 emscripten_set_main_loop(&main_loop, 0, 1);
#else
 while (!main_exit){ main_loop(); }

 ecpu = cu_avr_get_state();
 eepdump_save(&(ecpu->eepr[0]));

 audio_quit();
 guicore_quit();
 filesys_flushall();
#ifdef ENABLE_VCAP
 avconv_finalize();
#endif
#endif

 return 0;
}
