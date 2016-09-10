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



/* Collects audio data */
static uint8 frame_audio[262];

/* Fade-out memory accesses */
static uint8 frame_memf[4096U];

/* Fade-out I/O accesses */
static uint8 frame_iof[256U];

/* Frame counter to time memory access logic */
static auint frame_ctr = 0U;

/* Last audio frame was zero? */
static boole frame_isauz = TRUE;


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

 t0 = (pal[src[(sp - 1U) & 0x7FFU]] & 0xF0F0F0F0U) >> 4;
 t1 = (pal[src[(sp + 0U) & 0x7FFU]] & 0xF0F0F0F0U) >> 4;
 for (dp = 0U; dp < ((len / 3U) * 3U); dp += 3U)
 {
  /*
  ** Shrink roughly does this:
  ** Source:      |----|----|----|----|----|----|----| (7px)
  ** Destination: |-----------|----------|-----------| (3px)
  ** Note that when combining pixels back together, in total a multiplication
  ** with 17 will restore full intensity (0xF * 17 = 0xFF)
  */
  t2 = (pal[src[(sp + 1U) & 0x7FFU]] & 0xF0F0F0F0U) >> 4;
  t3 = (pal[src[(sp + 2U) & 0x7FFU]] & 0xF0F0F0F0U) >> 4;
  t4 = (pal[src[(sp + 3U) & 0x7FFU]] & 0xF0F0F0F0U) >> 4;
  px = (t0 * 4U) + (t1 * 4U) + (t2 * 4U) + (t3 * 4U) + (t4 * 1U);
  dest [dp + 0U] = px;
  t0 = (pal[src[(sp + 4U) & 0x7FFU]] & 0xF0F0F0F0U) >> 4;
  t1 = (pal[src[(sp + 5U) & 0x7FFU]] & 0xF0F0F0F0U) >> 4;
  px = (t2 * 2U) + (t3 * 4U) + (t4 * 5U) + (t0 * 4U) + (t1 * 2U);
  dest [dp + 1U] = px;
  t2 = (pal[src[(sp + 6U) & 0x7FFU]] & 0xF0F0F0F0U) >> 4;
  t3 = (pal[src[(sp + 7U) & 0x7FFU]] & 0xF0F0F0F0U) >> 4;
  px = (t4 * 1U) + (t0 * 4U) + (t1 * 4U) + (t2 * 4U) + (t3 * 4U);
  dest [dp + 2U] = px;
  sp += 7U;
  t0 = t2;
  t1 = t3;
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
** Attempts to run a frame of emulation (262 display rows ending with a
** CU_GET_FRAME result). Returns the last return of cu_avr_run() so it can be
** acted upon as necessary. If no proper sync was generated, it returns as
** soon as a GU_SYNCERR result is produced. If drop is TRUE, the render of the
** frame is dropped which can be used to help slow targets.
*/
auint frame_run(boole drop)
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
 cu_row_t const* erowd;
 cu_frameinfo_t const* finfo;

 /* Clear buffer. Maybe later will implement some more optimal solution here
 ** to avoid this full clear, but it is not known which lines the emulator
 ** would produce. */

 if (!drop){
  frame_clear(pix, ptc, 0U, 0U, 640U, 270U, pal[0]);
 }

 /* Build the frame */

 row = 0U;

 do{

  ret = cu_avr_run();

  if ((ret & CU_GET_ROW) != 0U){

   erowd = cu_avr_get_row();
   if (row < 262U){
    frame_audio[row] = (uint8)(erowd->sample);
   }
   if ( ((erowd->pno) >  18U) &&
        ((erowd->pno) < 246U) &&
        (!drop) ){
    frame_line32(
        pix + (((erowd->pno) * ptc) + 8U),
        &(erowd->pixels[0]),
        291U,
        624U,
        pal);
   }
   row ++;

   if ((ret & CU_SYNCERR) != 0U){ /* Still attempt to get rows if possible */
    if (row < 272U){
     ret &= ~CU_SYNCERR;          /* Ignore sync error (will show on sync bars in some manner) */
    }
   }

  }

 }while ((ret & (CU_SYNCERR | CU_GET_FRAME)) == 0U);

 /* Manage the audio result */

 /* If there was a sync error due to producing too few rows, pad the audio
 ** buffer so it remains tolerable */

 if (row != 0U){
  i = frame_audio[row - 1U];
 }else{
  i = 0U;
 }
 while (row < 262U){
  frame_audio[row] = i;
  row ++;
 }

 /* Check if the entire audio buffer is zero. If so, assume that the game
 ** didn't produce any audio yet. */

 i = 0U;
 for (row = 0U; row < 262U; row ++){
  i += frame_audio[row];
 }
 if (i == 0U){ frame_isauz = TRUE; }

 /* If no audio was produced, replace all 0x00U samples to 0x80U to prevent
 ** audio distortion on the user's side (by applying that constant DC on the
 ** output). */

 if (frame_isauz){
  for (row = 0U; row < 262U; row ++){
   if (frame_audio[row] != 0U){ break; } /* Audio output starts */
   frame_audio[row] = 0x80U;
  }
 }
 if (i != 0U){ frame_isauz = FALSE; } /* There was valid audio in the original contents */

 /* Fill in other areas providing info */

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

 frame_ctr ++;

 return ret;
}



/*
** Returns the 262 received unsigned audio samples of the frame.
*/
uint8 const* frame_getaudio(void)
{
 return &(frame_audio[0]);
}
