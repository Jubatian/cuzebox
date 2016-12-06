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



#include "frame.h"
#include "guicore.h"
#include "textgui.h"



/* Maximal number of rows to process in a frame */
#define MAX_ROWS     268U

/* Start sync pulse of picture output */
#define RPULSE_START  19U

/* Number of sync pulses to render */
#define RPULSE_NO    225U

/* Length of picture output in pixels (*(7/3) gives Uzebox cycles) */
#define ROW_PIXELS   624U

/* Begin cycle of picture output (note: ROW_BEGINCY and ROW_PIXELS must stay within the fetched Uzebox line) */
#define ROW_BEGINCY  291U

/* Prev. frame storage line width (1 extra leading & trailing Uzebox cycles are necessary) */
#define PREVS_WIDTH  (((ROW_PIXELS / 3U) * 7U) + 2U)


/* Previous frame storage (Uzebox cycles & palette) */
static uint8  frame_prev[RPULSE_NO * PREVS_WIDTH];

/* Collects audio data */
static uint8  frame_audio[MAX_ROWS];

/* Fade-out memory accesses */
static uint8  frame_memf[4096U];

/* Fade-out I/O accesses */
static uint8  frame_iof[256U];

/* Frame counter to time memory access logic */
static auint  frame_ctr = 0U;

/* Last audio frame was zero? */
static boole  frame_isauz = TRUE;

/* Previous state of frame merging */
static boole  frame_pmerge = FALSE;


/* Sync: Good signal marker */
static const uint8 frame_syn_ok[5] = {0x00U, 0x00U, 0x80U, 0x00U, 0x00U};
/* Sync: Invalid signal */
static const uint8 frame_syn_in[5] = {0x7FU, 0x7FU, 0x7DU, 0x7FU, 0x7FU};
/* Sync: Left 1 */
static const uint8 frame_syn_l1[5] = {0x00U, 0x4FU, 0x4DU, 0x00U, 0x00U};
/* Sync: Left 2 */
static const uint8 frame_syn_l2[5] = {0x4FU, 0x4FU, 0x4DU, 0x00U, 0x00U};
/* Sync: Left 2+ */
static const uint8 frame_syn_l3[5] = {0x7FU, 0x4FU, 0x4DU, 0x00U, 0x00U};
/* Sync: Right 1 */
static const uint8 frame_syn_r1[5] = {0x00U, 0x00U, 0x4DU, 0x4FU, 0x00U};
/* Sync: Right 2 */
static const uint8 frame_syn_r2[5] = {0x00U, 0x00U, 0x4DU, 0x4FU, 0x4FU};
/* Sync: Right 2+ */
static const uint8 frame_syn_r3[5] = {0x00U, 0x00U, 0x4DU, 0x4FU, 0x7FU};
/* Sync: Good line count (as VSync separator) */
static const uint8 frame_syn_ng[5] = {0x80U, 0x80U, 0x80U, 0x80U, 0x80U};
/* Sync: Bad line count (as VSync separator) */
static const uint8 frame_syn_nf[5] = {0x4FU, 0x4FU, 0x4FU, 0x4FU, 0x4FU};



/*
** Clears a rectangular region to the given color
*/
static void frame_clear(uint32* pix, auint ptc,
                        auint x, auint y, auint w, auint h, uint32 col)
{
 auint i;
 auint j;
 auint p;

 for (j = 0U; j < h; j++){
  p = ((y + j) * ptc) + x;
  for (i = 0U; i < w; i++){
   pix[p + i] = col;
  }
 }
}



/*
** Renders a line using a fast software scaling.
** dest is the target line to fill; len is in target pixels.
** pal must be prepared (low 4 bits of components zeroed & shifted down).
*/
static void frame_line32(uint32* dest,
                         uint8 const* src, auint off, auint len,
                         uint32 const* pal)
{
 auint  sp = off;
 auint  dp;
 auint  px;
 auint  t0, t1, t2, t3, t4;

 /* Note: This function relies on the destination using a 8 bits per
 ** channel representation, but the channel order is irrelevant. */

 t0 = pal[src[sp - 1U]];
 t1 = pal[src[sp + 0U]];
 for (dp = 0U; dp < ((len / 3U) * 3U); dp += 3U)
 {
  /*
  ** Shrink roughly does this:
  ** Source:      |----|----|----|----|----|----|----| (7px)
  ** Destination: |-----------|----------|-----------| (3px)
  ** Note that when combining pixels back together, in total a multiplication
  ** with 17 will restore full intensity (0xF * 17 = 0xFF)
  */
  t2 = pal[src[sp + 1U]];
  t3 = pal[src[sp + 2U]];
  t4 = pal[src[sp + 3U]];
  px = (t0 * 4U) + (t1 * 4U) + (t2 * 4U) + (t3 * 4U) + (t4 * 1U);
  dest [dp + 0U] = px;
  t0 = pal[src[sp + 4U]];
  t1 = pal[src[sp + 5U]];
  px = (t2 * 2U) + (t3 * 4U) + (t4 * 5U) + (t0 * 4U) + (t1 * 2U);
  dest [dp + 1U] = px;
  t2 = pal[src[sp + 6U]];
  t3 = pal[src[sp + 7U]];
  px = (t4 * 1U) + (t0 * 4U) + (t1 * 4U) + (t2 * 4U) + (t3 * 4U);
  dest [dp + 2U] = px;
  sp += 7U;
  t0 = t2;
  t1 = t3;
 }
}



/*
** Renders a line using a fast software scaling & copying the Uzebox source
** to the previous frame storage, merging the two frames.
** dest is the target line to fill; len is in target pixels.
** pal must be prepared (low 4 bits of components zeroed & shifted down).
*/
static void frame_line32_copy(uint32* dest,
                              uint8* dsu,
                              uint8 const* src, auint off, auint len,
                              uint32 const* pal)
{
 auint  sp = off;
 auint  up = 1U;
 auint  dp;
 auint  px;
 auint  t0, t1, t2, t3, t4;
 auint  u0, u1, u2, u3, u4;

 /* Note: This function relies on the destination using a 8 bits per
 ** channel representation, but the channel order is irrelevant. */

 t0 = pal[src[sp - 1U]];
 u0 = pal[dsu[up - 1U]];
 t1 = pal[src[sp + 0U]];
 u1 = pal[dsu[up + 0U]];
 dsu  [up - 1U] = src[sp - 1U];
 dsu  [up + 0U] = src[sp + 0U];
 for (dp = 0U; dp < ((len / 3U) * 3U); dp += 3U)
 {
  /*
  ** Shrink roughly does this:
  ** Source:      |----|----|----|----|----|----|----| (7px)
  ** Destination: |-----------|----------|-----------| (3px)
  ** Note that when combining pixels back together, in total a multiplication
  ** with 17 will restore full intensity (0xF * 17 = 0xFF)
  */
  t2 = pal[src[sp + 1U]];
  u2 = pal[dsu[up + 1U]];
  t3 = pal[src[sp + 2U]];
  u3 = pal[dsu[up + 2U]];
  t4 = pal[src[sp + 3U]];
  u4 = pal[dsu[up + 3U]];
  px = (t0 * 2U) + (t1 * 2U) + (t2 * 2U) + (t3 * 2U) + (t4 * 1U) +
       (u0 * 2U) + (u1 * 2U) + (u2 * 2U) + (u3 * 2U) + (u4 * 0U);
  dest [dp + 0U] = px;
  dsu  [up + 1U] = src[sp + 1U];
  dsu  [up + 2U] = src[sp + 2U];
  dsu  [up + 3U] = src[sp + 3U];
  t0 = pal[src[sp + 4U]];
  u0 = pal[dsu[up + 4U]];
  t1 = pal[src[sp + 5U]];
  u1 = pal[dsu[up + 5U]];
  px = (t2 * 1U) + (t3 * 2U) + (t4 * 2U) + (t0 * 2U) + (t1 * 1U) +
       (u2 * 1U) + (u3 * 2U) + (u4 * 3U) + (u0 * 2U) + (u1 * 1U);
  dest [dp + 1U] = px;
  dsu  [up + 4U] = src[sp + 4U];
  dsu  [up + 5U] = src[sp + 5U];
  t2 = pal[src[sp + 6U]];
  u2 = pal[dsu[up + 6U]];
  t3 = pal[src[sp + 7U]];
  u3 = pal[dsu[up + 7U]];
  px = (t4 * 1U) + (t0 * 2U) + (t1 * 2U) + (t2 * 2U) + (t3 * 2U) +
       (u4 * 0U) + (u0 * 2U) + (u1 * 2U) + (u2 * 2U) + (u3 * 2U);
  dest [dp + 2U] = px;
  dsu  [up + 6U] = src[sp + 6U];
  dsu  [up + 7U] = src[sp + 7U];
  sp += 7U;
  up += 7U;
  t0 = t2;
  u0 = u2;
  t1 = t3;
  u1 = u3;
 }
}



/*
** Copies lines of the Uzebox source to the previous frame storage. Should be
** used in dropped frames to keep frame merging operational.
*/
static void frame_line_dropcopy(uint8* dsu,
                                uint8 const* src, auint off, auint len)
{
 auint  sp = off;
 auint  up = 1U;
 auint  dp;

 dsu  [up - 1U] = src[sp - 1U];
 dsu  [up + 0U] = src[sp + 0U];
 for (dp = 0U; dp < ((len / 3U) * 3U); dp += 3U)
 {
  dsu  [up + 1U] = src[sp + 1U];
  dsu  [up + 2U] = src[sp + 2U];
  dsu  [up + 3U] = src[sp + 3U];
  dsu  [up + 4U] = src[sp + 4U];
  dsu  [up + 5U] = src[sp + 5U];
  dsu  [up + 6U] = src[sp + 6U];
  dsu  [up + 7U] = src[sp + 7U];
  sp += 7U;
  up += 7U;
 }
}



/*
** Plots a pixel box for the frameinfo display. Uses the Uzebox palette.
*/
static void frame_pixbox(uint32* pix, auint ptc,
                         auint x, auint y, uint8 col,
                         uint32 const* pal)
{
 auint  pos = (x << 1) + (y * ptc);
 uint32 c32 = ((pal[col] & 0xF8F8F8F8U) >> 3) * 6U;

 pix[pos + 0U] = c32;
 pix[pos + 1U] = c32;
}



/*
** Plots a sync signal notification (5 pixel boxes wide)
*/
static void frame_pixsyn(uint32* pix, auint ptc,
                         auint x, auint y, uint8 const* col,
                         uint32 const* pal)
{
 auint i;
 for (i = 0U; i < 5U; i++){
  frame_pixbox(pix, ptc, x + i, y, col[i], pal);
 }
}



/*
** Plots a sync signal notification by the report
*/
static void frame_synrep(uint32* pix, auint ptc,
                         auint x, auint y, auint rep,
                         uint32 const* pal)
{
 if       (rep == 0U){
  frame_pixsyn(pix, ptc, x, y, frame_syn_ok, pal);
 }else if (rep == CU_NOSYNC){
  frame_pixsyn(pix, ptc, x, y, frame_syn_in, pal);
 }else if ((asint)(rep) == -1){
  frame_pixsyn(pix, ptc, x, y, frame_syn_l1, pal);
 }else if ((asint)(rep) == -2){
  frame_pixsyn(pix, ptc, x, y, frame_syn_l2, pal);
 }else if ((asint)(rep) <  -2){
  frame_pixsyn(pix, ptc, x, y, frame_syn_l3, pal);
 }else if ((asint)(rep) ==  1){
  frame_pixsyn(pix, ptc, x, y, frame_syn_r1, pal);
 }else if ((asint)(rep) ==  2){
  frame_pixsyn(pix, ptc, x, y, frame_syn_r2, pal);
 }else{
  frame_pixsyn(pix, ptc, x, y, frame_syn_r3, pal);
 }
}



/*
** Prepare palette for line renders (masking low 4 bits)
*/
static void frame_palprep(uint32* dest, uint32 const* src)
{
 auint i;

 for (i = 0U; i < 256U; i++){
  dest[i] = (src[i] & 0xF0F0F0F0U) >> 4;
 }
}



/*
** Attempts to run a frame of emulation (262 display rows ending with a
** CU_GET_FRAME result). If no proper sync was generated, it still attempts
** to get about a frame worth of lines. If drop is TRUE, the render of the
** frame is dropped which can be used to help slow targets. If merge is TRUE,
** frames following each other are averaged (weak motion blur, cancels out
** flickers). Returns the number of rows generated, which is also the number
** of samples within the audio buffer (normally 262).
*/
auint frame_run(boole drop, boole merge)
{
 auint           ret;
 uint32 const*   pal = guicore_getpalette();
 uint32*         pix = guicore_getpixbuf();
 auint           ptc = guicore_getpitch();
 auint           row;
 uint8*          mem;
 auint           i;
 auint           j;
 auint           k;
 auint           adr;
 auint           col;
 boole           gonly = ((guicore_getflags() & GUICORE_GAMEONLY) != 0U);
 cu_row_t const* erowd;
 cu_frameinfo_t const* finfo;
 uint32          ppal[256U];

 /* Prepares palette for line renders */

 if (!drop){
  frame_palprep(&(ppal[0]), pal);
 }

 /* Clear buffer. Maybe later will implement some more optimal solution here
 ** to avoid this full clear, but it is not known which lines the emulator
 ** would produce. */

 if (!drop){
  if (!gonly){
   frame_clear(pix, ptc,  0U,  0U, 640U, 280U, pal[0]);
  }else{
   frame_clear(pix, ptc, 10U, 18U, 620U, 230U, pal[0]);
  }
 }

 /* Shift whole display a little down to give more clearance to the text info
 ** on the top */

 pix = pix + (1U * ptc);

 /* Build the frame */

 row = 0U;

 while (row < MAX_ROWS){

  ret = cu_avr_run();

  if ((ret & CU_GET_ROW) != 0U){

   erowd = cu_avr_get_row();
   frame_audio[row] = (uint8)(erowd->sample);
   if ( ((erowd->pno) >=  (RPULSE_START            )) &&
        ((erowd->pno) <   (RPULSE_START + RPULSE_NO)) ){

    if (!merge){

     if (!drop){
      frame_line32(
          pix + (((erowd->pno) * ptc) + 8U),
          &(erowd->pixels[0]),
          ROW_BEGINCY,
          ROW_PIXELS,
          &(ppal[0]));
     }

    }else{

     if (!frame_pmerge){ /* Merging is just turned on: buffer uninitialized */

      if (!drop){
       frame_line32(
           pix + (((erowd->pno) * ptc) + 8U),
           &(erowd->pixels[0]),
           ROW_BEGINCY,
           ROW_PIXELS,
           &(ppal[0]));
      }
      frame_line_dropcopy(
          &(frame_prev[((erowd->pno) - RPULSE_START) * PREVS_WIDTH]),
          &(erowd->pixels[0]),
          ROW_BEGINCY,
          ROW_PIXELS);

     }else{              /* Previous frame buffer is present */

      if (!drop){
       frame_line32_copy(
           pix + (((erowd->pno) * ptc) + 8U),
           &(frame_prev[((erowd->pno) - RPULSE_START) * PREVS_WIDTH]),
           &(erowd->pixels[0]),
           ROW_BEGINCY,
           ROW_PIXELS,
           &(ppal[0]));
      }else{
       frame_line_dropcopy(
           &(frame_prev[((erowd->pno) - RPULSE_START) * PREVS_WIDTH]),
           &(erowd->pixels[0]),
           ROW_BEGINCY,
           ROW_PIXELS);
      }

     }

    }

   }
   row ++;

  }

  if ((ret & CU_GET_FRAME) != 0U){ break; }

 }

 ret = row;
 frame_pmerge = merge;

 /* Manage the audio result */

 /* If there was a sync error due to producing too few rows, pad the audio
 ** buffer so it remains tolerable */

 if (row != 0U){
  i = frame_audio[row - 1U];
 }else{
  i = 0U;
 }
 while (row < MAX_ROWS){
  frame_audio[row] = i;
  row ++;
 }

 /* Check if the entire audio buffer is zero. If so, assume that the game
 ** didn't produce any audio yet. */

 i = 0U;
 for (row = 0U; row < MAX_ROWS; row ++){
  i += frame_audio[row];
 }
 if (i == 0U){ frame_isauz = TRUE; }

 /* If no audio was produced, replace all 0x00U samples to 0x80U to prevent
 ** audio distortion on the user's side (by applying that constant DC on the
 ** output). */

 if (frame_isauz){
  for (row = 0U; row < MAX_ROWS; row ++){
   if (frame_audio[row] != 0U){ break; } /* Audio output starts */
   frame_audio[row] = 0x80U;
  }
 }
 if (i != 0U){ frame_isauz = FALSE; } /* There was valid audio in the original contents */

 /* Fill in other areas providing info (only if they are displayed) */

 if (!gonly){

  /* Produce sync info sidebar */

  if (!drop){
   finfo = cu_avr_get_frameinfo();

   for (row =   0U; row < 252U; row ++){ /* Display pulses */
    frame_synrep(pix, ptc,   0U, row,      finfo->pulse[row].rise, pal);
    frame_synrep(pix, ptc, 315U, row,      finfo->pulse[row].fall, pal);
   }
   if (finfo->rowcdif == 0U){            /* Row count separator */
    frame_pixsyn(pix, ptc,   0U, 252U, frame_syn_ng, pal);
    frame_pixsyn(pix, ptc, 315U, 252U, frame_syn_ng, pal);
   }else{
    frame_pixsyn(pix, ptc,   0U, 252U, frame_syn_nf, pal);
    frame_pixsyn(pix, ptc, 315U, 252U, frame_syn_nf, pal);
   }
   for (row = 252U; row < 271U; row ++){ /* VSync pulses */
    frame_synrep(pix, ptc,   0U, row + 1U, finfo->pulse[row].rise, pal);
    frame_synrep(pix, ptc, 315U, row + 1U, finfo->pulse[row].fall, pal);
   }
  }

  /* Produce memory access info blocks */

  mem = cu_avr_get_meminfo();

  for (k = 0U; k < 16U; k ++){
   for (j = 0U; j < 16U; j ++){
    for (i = 0U; i < 16U; i ++){
     adr = (((k + 1U) & 0xFU) << 8) |
           (j << 4) |
           (i     );
     if ((frame_ctr & 0x7U) == 0U){
      if ((frame_memf[adr] & 0x07U) != 0U){ frame_memf[adr] -= 0x01U; }
      if ((frame_memf[adr] & 0x38U) != 0U){ frame_memf[adr] -= 0x08U; }
      if ((mem[adr]) & CU_MEM_R){ frame_memf[adr] |= 0x38U; }
      if ((mem[adr]) & CU_MEM_W){ frame_memf[adr] |= 0x07U; }
      mem[adr] = 0U; /* Clear cell's flags */
     }
     if (!drop){
      col = 0x80U | frame_memf[adr];
      frame_pixbox(pix, ptc, 36U + (k * 17U) + j, 248U + i, col, pal);
     }
    }
   }
  }

  /* Produce I/O access info blocks */

  mem = cu_avr_get_ioinfo();

  for (j = 0U; j < 16U; j ++){
   for (i = 0U; i < 16U; i ++){
    adr = (j << 4) |
          (i     );
    if ((frame_ctr & 0x7U) == 0U){
     if ((frame_iof[adr] & 0x07U) != 0U){ frame_iof[adr] -= 0x01U; }
     if ((frame_iof[adr] & 0x38U) != 0U){ frame_iof[adr] -= 0x08U; }
     if ((mem[adr]) & CU_MEM_R){ frame_iof[adr] |= 0x38U; }
     if ((mem[adr]) & CU_MEM_W){ frame_iof[adr] |= 0x07U; }
     mem[adr] = 0U; /* Clear cell's flags */
    }
    if (!drop){
     col = 0x80U | frame_iof[adr];
     frame_pixbox(pix, ptc, 12U + j, 248U + i, col, pal);
    }
   }
  }

 }

 frame_ctr ++;

 /* Add text GUI elements */

 textgui_draw(drop || gonly);

 return ret;
}



/*
** Returns the received unsigned audio samples of the frame (normally 262).
*/
uint8 const* frame_getaudio(void)
{
 return &(frame_audio[0]);
}
