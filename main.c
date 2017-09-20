/*
 *  Main
 *
 *  Copyright (C) 2016 - 2017
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
#include "ginput.h"
#include "frame.h"
#include "eepdump.h"
#include "romdump.h"
#include "textgui.h"
#ifdef ENABLE_VCAP
#include "avconv.h"
#endif



/* Initial title */
static const char main_title[] = "CUzeBox";

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

/* Request frame merging (flicker reduction) */
#ifdef FLAG_DISPLAY_FRAMEMERGE
static boole main_fmerge = TRUE;
#else
static boole main_fmerge = FALSE;
#endif

/* Previous state of EEPROM changes to save only after a write burst was completed */
static boole main_peepch = FALSE;

/* Previous state of Code ROM changes to save only after a write burst was completed */
static boole main_promch = FALSE;

/* Default game to start without parameters */
static const char* main_game = "gamefile.uze";

#ifdef ENABLE_VCAP
/* Whether video capturing is enabled */
static boole main_isvcap = FALSE;
#endif

/* Whether the emulator is paused */
static boole main_ispause = FALSE;

/* Whether a single frame advance was called (1 frame unpause) */
static boole main_isadvfr = FALSE;





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
 boole romch;
 SDL_Event sdlevent;
 cu_state_cpu_t* ecpu;
 textgui_struct_t* tgui;

 /* First "sandbox" the drift to silently skip main loops if the emulator is
 ** running too fast */

 /* Calculate drift from perfect 60Hz */

 if (main_tfrac >= 2U){ drift += 16U; }
 else{                  drift += 17U; }
 drift -= tdif;

 /* Check for possible total CPU starvation. When such a situation arises (a
 ** huge difference in prev. and current tick), throw drift away. */

 if (tdif > 100U){
  drift = 0U;
  tdif  = 16U;
 }

 /* Handle divergences */

 if (drift >= 0x80000000U){   /* Possibly too slow */
  if ((drift + 8U) >= 0x80000000U){   /* Transient problem (late by at least 1 frame) */
   if ((drift + 58U) >= 0x80000000U){ /* Permanent problem (late by several frames) */
    if ((drift + 225U) >= 0x80000000U){
     drift = (auint)(0U) - 225U;      /* Limit (throw away memory) */
    }
    if (fdtmp < 160U){ fdtmp = 160U; }   /* Semi-permanently limit frame rate */
   }
   if (fdtmp < 240U){ fdtmp ++; }     /* Push frame drop request */
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

 /* Check for EEPROM & Code ROM changes and save as needed */

 ecpu = cu_avr_get_state(); /* Also needed by emulator whisper ports */

 eepch = cu_avr_eeprom_ischanged(TRUE);
 if ( (main_peepch) &&
      (!eepch) ){ /* End of EEPROM write burst */
  eepdump_save(&(ecpu->eepr[0]));
 }
 main_peepch = eepch;

 romch = cu_avr_crom_ischanged(TRUE);
 if ( (main_promch) &&
      (!romch) ){ /* End of Code ROM write burst */
  romdump_save(&(ecpu->crom[0]));
 }
 main_promch = romch;

 /* Generate various emulator info for the GUI */

 main_t500 += tdif;
 if (!fdrop){ main_t5_frc ++; }

 tgui = textgui_getelementptr();

 tgui->merge   = main_fmerge;
 tgui->kbuzem  = ginput_iskbuzem();
 tgui->player2 = ginput_is2palloc();
#ifdef ENABLE_VCAP
 tgui->capture = main_isvcap;
#else
 tgui->capture = FALSE;
#endif

 tgui->ports[0] = ecpu->iors[0x39U];
 tgui->ports[1] = ecpu->iors[0x3AU];
 tgui->wdrint = cu_avr_get_lastwdrinterval(&(tgui->wdrbeg), &(tgui->wdrend));

 if (main_t500 >= 500U){
  main_t500 -= 500U;

  favg = main_t5_frc + main_t5_frp;
  main_t5_frp = main_t5_frc;
  main_t5_frc = 0U;

  ccur       = cu_avr_getcycle();
  cdif       = WRAP32(ccur - main_t5_cc);
  main_t5_cc = ccur;

  afreq = audio_getfreq();

  tgui->cpufreq  = cdif * 2U;
  tgui->aufreq   = afreq;
  tgui->dispfreq = favg * 1000U;
 }

 /* Process events */

 while (SDL_PollEvent(&sdlevent) != 0){

  ginput_sendevent(&sdlevent);

  if (sdlevent.type == SDL_QUIT){ main_exit = TRUE; }

  if ((sdlevent.type) == SDL_KEYDOWN){

   switch (sdlevent.key.keysym.sym){

    case SDLK_ESCAPE:
     main_exit = TRUE;
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
     break;

    case SDLK_F8:
     ginput_setkbuzem(!ginput_iskbuzem());
     break;

    case SDLK_F9:
     main_ispause = !main_ispause;
     break;

    case SDLK_F10:
     main_isadvfr = TRUE;
     break;

    case SDLK_F11:
     if (!guicore_init(guicore_getflags() ^ GUICORE_FULLSCREEN, main_title)){ main_exit = TRUE; }
     break;

    case SDLK_F12:
     ginput_set2palloc(!ginput_is2palloc());
     break;

    default:
     break;
   }

  }
 }

 /* Go on with the frame's logic. Does it here so the SDL_GetTicks() on the
 ** top of the loop is as close to the render's end as reasonably possible. */

 if (main_isadvfr){
  main_ispause = FALSE;
 }

 if (!main_ispause){

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

 }else{

  audio_sendframe(NULL, 262U);
  guicore_update(FALSE);

 }

 if (main_isadvfr){
  main_isadvfr = FALSE;
  main_ispause = TRUE;
 }

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
 auint             flg;
 textgui_struct_t* tgui;
 boole             uzefile = FALSE;
 boole             hasrom;
 boole             bootpri = FALSE;


 print_unf(main_title);
 print_message(" %08X\n", VER_DATE);


 if (argc > 1){ game = argv[1]; }
 filesys_setpath(game, &(tstr[0]), 100U); /* Locate everything beside the game */

 ecpu = cu_avr_get_state();
 hasrom = romdump_load(&(ecpu->crom[0]));
 eepdump_load(&(ecpu->eepr[0]));

 uzefile = cu_ufile_load(&(tstr[0]), &(ecpu->crom[0]), &ufhead);
 if (!uzefile){
  /* Boot loader prioritized when loading a .HEX file. This will result in the
  ** boot loader starting when explicitly using a bootloader .HEX file, but
  ** also in any other cases if a bootloader can be found either in restored
  ** ROM or the .HEX file (so after using a bootloader, to load .HEX file
  ** games, it is necessary to delete the ROM dump). */
  bootpri = TRUE;
  if (!cu_hfile_load(&(tstr[0]), &(ecpu->crom[0]))){
   if (!hasrom){ return 1; }
  }
 }

 ecpu->wd_seed = rand(); /* Seed the WD timeout used for PRNG seed in Uzebox games */

 print_unf("Starting emulator\n");


 flg = 0U;
#ifdef FLAG_DISPLAY_SMALL
 flg |= GUICORE_SMALL;
#endif
#ifdef FLAG_DISPLAY_GAMEONLY
 flg |= GUICORE_GAMEONLY;
#endif

 if (!guicore_init(flg, main_title)){
  return 1;
 }
 (void)(audio_init());
 (void)(ginput_init());


 cu_avr_reset(bootpri);
 main_t5_cc = cu_avr_getcycle();
 audio_reset();
 textgui_reset();


 /* Add game info if available */

 if (uzefile){
  tgui = textgui_getelementptr();
  strncpy((char*)(&(tgui->game[0])), (char const*)(&(ufhead.name[0]  )), TEXTGUI_STR_MAX);
  strncpy((char*)(&(tgui->auth[0])), (char const*)(&(ufhead.author[0])), TEXTGUI_STR_MAX);
 }


 main_ptick = SDL_GetTicks() - 16U; /* First frame interval should be normal */
#ifdef __EMSCRIPTEN__
 emscripten_set_main_loop(&main_loop, 0, 1);
#else
 while (!main_exit){ main_loop(); }


 ecpu = cu_avr_get_state();
 if ( (main_promch) || (cu_avr_crom_ischanged  (TRUE)) ){ romdump_save(&(ecpu->crom[0])); }
 if ( (main_peepch) || (cu_avr_eeprom_ischanged(TRUE)) ){ eepdump_save(&(ecpu->eepr[0])); }

 ginput_quit();
 audio_quit();
 guicore_quit();
 filesys_flushall();
#ifdef ENABLE_VCAP
 avconv_finalize();
#endif
 exit(0);
#endif

 return 0;
}
