/*
 *  Text GUI elements
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



#include "textgui.h"
#include "guicore.h"
#include "conout.h"
#include "chars.h"



/* Text GUI elements */
static textgui_struct_t textgui_elements;


/* Timer for console output */
static auint textgui_tim = 0U;



/* Text color (Uzebox color index) */
#define TEXT_COLOR ((0x2U << 6) + (0x4U << 3) + 0x4U)



/* Text GUI character grid assistance functions */
#define TOP_X(x) (((x) * 6U) +  10U)
#define TOP_Y(y) (((y) * 6U) +   1U)
#define BOT_X(x) (((x) * 6U) +  10U)
#define BOT_Y(y) (((y) * 6U) + 267U)



/* Map of constant elements of the top part */
static uint8 const textgui_top[50U * 3U] = {
 'C', 'U', 'z', 'e', 'B', 'o', 'x', ' ', ' ', ' ',
 ' ', '%', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
 ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'P', ' ',
  0U,  1U, ' ', ' ', ' ', ' ', ' ', ' ',  3U, ' ',
 11U, ' ', ' ', ' ', '.', ' ', ' ', ' ',  6U,  7U,

 'G', 'a', 'm', 'e', ':', ' ', ' ', ' ', ' ', ' ',
 ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
 ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
 ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
 10U, ' ', ' ', ' ', '.', ' ', ' ', ' ',  4U,  5U,

 'A', 'u', 't', 'h', ':', ' ', ' ', ' ', ' ', ' ',
 ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
 ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
 ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
 12U, ' ', ' ', ' ', '.', ' ', ' ', ' ',  8U,  9U,
};

/* Map of constant elements of the bottom part */
static uint8 const textgui_bot[50U * 2U] = {
 'P', 'o', 'r', 't', '3', '9', ':', ' ', ' ', ' ',
 ' ', ';', ' ', '0', 'x', ' ', ' ', ';', ' ', ' ',
 ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'b', ' ', ' ',
 ' ', ' ', 'W', 'D', 'R', ' ', ' ', ' ', ' ', ' ',
 ' ', ' ', ' ', ' ', ' ', ' ',
 '0' + ((VER_DATE >> 28) & 0xFU),
 '0' + ((VER_DATE >> 24) & 0xFU),
 '0' + ((VER_DATE >> 20) & 0xFU),
 '0' + ((VER_DATE >> 16) & 0xFU),

 'P', 'o', 'r', 't', '3', 'A', ':', ' ', ' ', ' ',
 ' ', ';', ' ', '0', 'x', ' ', ' ', ';', ' ', ' ',
 ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'b', ' ', ' ',
 ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
 ' ', ' ', ' ', ' ', ' ', ' ',
 '0' + ((VER_DATE >> 12) & 0xFU),
 '0' + ((VER_DATE >>  8) & 0xFU),
 '0' + ((VER_DATE >>  4) & 0xFU),
 '0' + ((VER_DATE      ) & 0xFU)
};

/* Controller mappings */
static uint8 const textgui_kbsnes[] = {'S', 'N', 'E', 'S', 0U};
static uint8 const textgui_kbuzem[] = {'U', 'Z', 'E', 'M', 0U};



/* Structure to pass around parameters */
typedef struct{
 uint32*       pix;    /* Target pixel buffer */
 auint         ptc;    /* Pixel buffer's pitch */
 uint32 const* pal;    /* Palette to use for graphics render */
 boole         prmsg;  /* Whether to print on the console */
 boole         nogrf;  /* Whether to omit graphics output */
}textgui_rs_t;



/*
** Renders a character at a given position, the surface assumed to be
** 320 x 270 pixels (so character pixels are 2 actual pixels wide, about 53
** characters fit horizontally). No bound checks are performed, the character
** must be on the surface.
*/
static void textgui_putchar(auint x, auint y, auint chr,
                            textgui_rs_t const* rs)
{
 auint   i;
 auint   dp;
 auint   cp;
 auint   ch;
 uint32  tc;
 uint32* pix;
 auint   ptc;
 uint32 const* pal;

 if (!(rs->nogrf)){

  pix = rs->pix;
  ptc = rs->ptc;
  pal = rs->pal;
  dp  = (y * ptc) + (x * 2U);
  cp  = (chr & 0x7FU) * 6U;

  for (i = 0U; i < 6U; i++){
   ch = chars[cp];
   tc = pal[((ch & 0x80U) >> 7) * TEXT_COLOR];
   pix[dp +  0U] = tc;
   pix[dp +  1U] = tc;
   tc = pal[((ch & 0x40U) >> 6) * TEXT_COLOR];
   pix[dp +  2U] = tc;
   pix[dp +  3U] = tc;
   tc = pal[((ch & 0x20U) >> 5) * TEXT_COLOR];
   pix[dp +  4U] = tc;
   pix[dp +  5U] = tc;
   tc = pal[((ch & 0x10U) >> 4) * TEXT_COLOR];
   pix[dp +  6U] = tc;
   pix[dp +  7U] = tc;
   tc = pal[((ch & 0x08U) >> 3) * TEXT_COLOR];
   pix[dp +  8U] = tc;
   pix[dp +  9U] = tc;
   tc = pal[((ch & 0x04U) >> 2) * TEXT_COLOR];
   pix[dp + 10U] = tc;
   pix[dp + 11U] = tc;
   cp ++;
   dp += ptc;
  }

 }

 if (rs->prmsg){
  conout_addchr(chr & 0x7FU);
 }
}



/*
** Outputs a decimal number between 0 - 999. The number is truncated if it is
** larger, leading zeroes will be generated this case.
*/
static void textgui_putdec(auint x, auint y, auint val,
                           textgui_rs_t const* rs)
{
 auint n;

 n = (val / 100U) % 10U;
 if ((val >= 1000U) || (n != 0U)){
  textgui_putchar(x +  0U, y, n + '0', rs);
 }

 n = (val /  10U) % 10U;
 if ((val >=  100U) || (n != 0U)){
  textgui_putchar(x +  6U, y, n + '0', rs);
 }

 n = (val       ) % 10U;
 textgui_putchar(x + 12U, y, n + '0', rs);
}



/*
** Outputs a decimal number between 0 - 999999. The number is truncated if it
** is larger, leading zeroes will be generated this case.
*/
static void textgui_putdecx(auint x, auint y, auint val,
                            textgui_rs_t const* rs)
{
 auint n;

 n = (val / 100000U) % 10U;
 if ((val >= 1000000U) || (n != 0U)){
  textgui_putchar(x +  0U, y, n + '0', rs);
 }

 n = (val /  10000U) % 10U;
 if ((val >=  100000U) || (n != 0U)){
  textgui_putchar(x +  6U, y, n + '0', rs);
 }

 n = (val /   1000U) % 10U;
 if ((val >=   10000U) || (n != 0U)){
  textgui_putchar(x + 12U, y, n + '0', rs);
 }

 textgui_putdec(x + 18U, y, val, rs);
}



/*
** Outputs a string (zero or string size limit terminated)
*/
static void textgui_putstr(auint x, auint y, uint8 const* str,
                           textgui_rs_t const* rs)
{
 auint p = 0U;

 while ((str[p] != 0U) && (p < TEXTGUI_STR_MAX)){

  textgui_putchar(x + (p * 6U), y, str[p], rs);
  p ++;

 }
}



/*
** Converts low 4 bits to hexadecimal digit (uppercase)
*/
static uint8 textgui_tohex(auint val)
{
 val = val & 0xFU;
 if (val < 10U){ return ((val + '0')      ); }
 else          { return ((val + 'A') - 10U); }
}



/*
** Redraws text GUI elements. If nogrf is set, no on-screen graphics are
** generated. Terminal output is generated on every 30th call (1/2 secs).
*/
void textgui_draw(boole nogrf)
{
 auint          i;
 auint          j;
 auint          t;
 textgui_rs_t   rs;
 textgui_rs_t   rsnoprn;
 boole          prmsg = (textgui_tim == 0U);

 /* Prepare */

 rs.pal   = guicore_getpalette();
 rs.pix   = guicore_getpixbuf();
 rs.ptc   = guicore_getpitch();
 rs.nogrf = nogrf;
 rs.prmsg = prmsg;

 memcpy(&rsnoprn, &rs, sizeof(rs));
 rsnoprn.prmsg = FALSE;

 textgui_tim ++;
 if (textgui_tim == 60U){ textgui_tim = 0U; }

 if ((nogrf) && (!prmsg)){ return; } /* No output at all */

 /* Place static stuff */

 for (j = 0U; j < 3U; j++){
  for (i = 0U; i < 50U; i++){
   textgui_putchar(TOP_X(i), TOP_Y(j), textgui_top[(j * 50U) + i], &rsnoprn);
  }
 }

 for (j = 0U; j < 2U; j++){
  for (i = 0U; i < 50U; i++){
   textgui_putchar(BOT_X(i), BOT_Y(j), textgui_bot[(j * 50U) + i], &rsnoprn);
  }
 }

 /* Get speed percentage from CPU frequency (which should be 28636400Hz) */

 i = (textgui_elements.cpufreq + (286364U / 2U)) / 286364U;
 if (prmsg){ conout_addstr("CUzeBox: "); }
 textgui_putdec(TOP_X( 8U), TOP_Y(0U), i, &rs);
 if (prmsg){ conout_addstr("% "); }

 /* Output frequencies */

 textgui_putdec(TOP_X(41U), TOP_Y(1U), textgui_elements.cpufreq / 1000000U, &rs);
 if (prmsg){ conout_addchr('.'); }
 textgui_putdec(TOP_X(45U), TOP_Y(1U), textgui_elements.cpufreq /    1000U, &rs);
 if (prmsg){ conout_addstr("MHz; "); }
 textgui_putdec(TOP_X(41U), TOP_Y(0U), textgui_elements.dispfreq /   1000U, &rs);
 textgui_putdec(TOP_X(45U), TOP_Y(0U), textgui_elements.dispfreq,           &rsnoprn);
 if (prmsg){ conout_addstr("Fps; Audio: "); }
 textgui_putdec(TOP_X(41U), TOP_Y(2U), textgui_elements.aufreq /     1000U, &rs);
 if (prmsg){ conout_addchr('.'); }
 textgui_putdec(TOP_X(45U), TOP_Y(2U), textgui_elements.aufreq,             &rs);
 if (prmsg){ conout_addstr("KHz; "); }

 /* SNES / UZEM keyboard mapping */

 if (textgui_elements.kbuzem){
  textgui_putstr(TOP_X(32U), TOP_Y(0U), &(textgui_kbuzem[0]), &rsnoprn);
  if (prmsg){ conout_addstr("Keys: UZEM "); }
 }else{
  textgui_putstr(TOP_X(32U), TOP_Y(0U), &(textgui_kbsnes[0]), &rsnoprn);
  if (prmsg){ conout_addstr("Keys: SNES "); }
 }

 /* 1 player / 2 players */

 if (textgui_elements.player2){
  textgui_putchar(TOP_X(27U), TOP_Y(0U), '2', &rsnoprn);
  if (prmsg){ conout_addstr("2P "); }
 }else{
  textgui_putchar(TOP_X(27U), TOP_Y(0U), '1', &rsnoprn);
  if (prmsg){ conout_addstr("1P "); }
 }

 /* Frame merging */

 if (textgui_elements.merge){
  textgui_putchar(TOP_X(37U), TOP_Y(0U),  2U, &rsnoprn);
  if (prmsg){ conout_addstr("FrameMerge "); }
 }else{
  textgui_putchar(TOP_X(37U), TOP_Y(0U), ' ', &rsnoprn);
 }

 /* Video capturing */

 if (textgui_elements.capture){
  textgui_putchar(TOP_X(24U), TOP_Y(0U), 13U, &rsnoprn);
  textgui_putchar(TOP_X(25U), TOP_Y(0U), 14U, &rsnoprn);
  if (prmsg){ conout_addstr("Capture "); }
 }else{
  textgui_putchar(TOP_X(24U), TOP_Y(0U), ' ', &rsnoprn);
  textgui_putchar(TOP_X(25U), TOP_Y(0U), ' ', &rsnoprn);
 }

 /* Game & Author name */

 textgui_putstr(TOP_X(6U), TOP_Y(1U), &(textgui_elements.game[0]), &rsnoprn);
 textgui_putstr(TOP_X(6U), TOP_Y(2U), &(textgui_elements.auth[0]), &rsnoprn);

 /* Whisper ports */

 for (i = 0U; i < 2U; i++){
  t = textgui_elements.ports[i];
  textgui_putdec (BOT_X( 8U), BOT_Y(i), t, &rsnoprn);
  textgui_putchar(BOT_X(15U), BOT_Y(i), textgui_tohex(t >> 4), &rsnoprn);
  textgui_putchar(BOT_X(16U), BOT_Y(i), textgui_tohex(     t), &rsnoprn);
  for (j = 0U; j < 8U; j++){
   textgui_putchar(BOT_X(19U + j), BOT_Y(i), ((t >> (7U - j)) & 1U) + '0', &rsnoprn);
  }
 }

 /* WDR interval debug counter */

 textgui_putdecx(BOT_X(36U), BOT_Y(0), textgui_elements.wdrint, &rsnoprn);
 t = textgui_elements.wdrbeg << 1;
 textgui_putchar(BOT_X(32U), BOT_Y(1), textgui_tohex(t >> 12), &rsnoprn);
 textgui_putchar(BOT_X(33U), BOT_Y(1), textgui_tohex(t >>  8), &rsnoprn);
 textgui_putchar(BOT_X(34U), BOT_Y(1), textgui_tohex(t >>  4), &rsnoprn);
 textgui_putchar(BOT_X(35U), BOT_Y(1), textgui_tohex(t >>  0), &rsnoprn);
 t = textgui_elements.wdrend << 1;
 textgui_putchar(BOT_X(38U), BOT_Y(1), textgui_tohex(t >> 12), &rsnoprn);
 textgui_putchar(BOT_X(39U), BOT_Y(1), textgui_tohex(t >>  8), &rsnoprn);
 textgui_putchar(BOT_X(40U), BOT_Y(1), textgui_tohex(t >>  4), &rsnoprn);
 textgui_putchar(BOT_X(41U), BOT_Y(1), textgui_tohex(t >>  0), &rsnoprn);

 if (prmsg){ conout_send(); }
}



/*
** Returns a pointer to the text GUI elements, can be used to update them for
** a later textgui_draw() call.
*/
textgui_struct_t* textgui_getelementptr(void)
{
 return &textgui_elements;
}



/*
** Resets text GUI element values.
*/
void textgui_reset(void)
{
 textgui_elements.cpufreq  = 28636400U;
 textgui_elements.aufreq   = 15734U;
 textgui_elements.dispfreq = 60000U;
 textgui_elements.kbuzem   = FALSE;
 textgui_elements.merge    = FALSE;
 textgui_elements.capture  = FALSE;
 textgui_elements.game[0]  = 0U;
 textgui_elements.auth[0]  = 0U;
 textgui_elements.ports[0] = 0U;
 textgui_elements.ports[1] = 0U;
}
