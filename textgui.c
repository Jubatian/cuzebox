/*
 *  Text GUI elements
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



#include "textgui.h"
#include "guicore.h"
#include "chars.h"



/* Text GUI elements */
static textgui_struct_t textgui_elements;



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
 ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
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
 ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
 ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',

 'P', 'o', 'r', 't', '3', 'A', ':', ' ', ' ', ' ',
 ' ', ';', ' ', '0', 'x', ' ', ' ', ';', ' ', ' ',
 ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'b', ' ', ' ',
 ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
 ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
};

/* Controller mappings */
static uint8 const textgui_kbsnes[] = {'S', 'N', 'E', 'S', 0U};
static uint8 const textgui_kbuzem[] = {'U', 'Z', 'E', 'M', 0U};



/*
** Renders a character at a given position, the surface assumed to be
** 320 x 270 pixels (so character pixels are 2 actual pixels wide, about 53
** characters fit horizontally). No bound checks are performed, the character
** must be on the surface.
*/
static void textgui_putchar(auint x, auint y, auint chr,
                            uint32* pix, auint ptc, uint32 const* pal)
{
 auint  i;
 auint  dp = (y * ptc) + (x * 2U);
 auint  cp = (chr & 0x7FU) * 6U;
 auint  ch;
 uint32 tc;

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



/*
** Outputs a decimal number between 0 - 999. The number is truncated if it is
** larger, leading zeroes will be generated this case.
*/
static void textgui_putdec(auint x, auint y, auint val,
                           uint32* pix, auint ptc, uint32 const* pal)
{
 auint n;

 n = (val / 100U) % 10U;
 if ((val >= 1000U) || (n != 0U)){
  textgui_putchar(x +  0U, y, n + '0', pix, ptc, pal);
 }

 n = (val /  10U) % 10U;
 if ((val >=  100U) || (n != 0U)){
  textgui_putchar(x +  6U, y, n + '0', pix, ptc, pal);
 }

 textgui_putchar(x + 12U, y, (val % 10U) + '0', pix, ptc, pal);
}



/*
** Outputs a string (zero or string size limit terminated)
*/
static void textgui_putstr(auint x, auint y, uint8 const* str,
                           uint32* pix, auint ptc, uint32 const* pal)
{
 auint p = 0U;

 while ((str[p] != 0U) && (p < TEXTGUI_STR_MAX)){

  textgui_putchar(x + (p * 6U), y, str[p], pix, ptc, pal);
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
** Redraws text GUI elements
*/
void textgui_draw(void)
{
 uint32 const*  pal = guicore_getpalette();
 uint32*        pix = guicore_getpixbuf();
 auint          ptc = guicore_getpitch();
 auint          i;
 auint          j;
 auint          t;

 /* Place static stuff */

 for (j = 0U; j < 3U; j++){
  for (i = 0U; i < 50U; i++){
   textgui_putchar(TOP_X(i), TOP_Y(j), textgui_top[(j * 50U) + i], pix, ptc, pal);
  }
 }

 for (j = 0U; j < 2U; j++){
  for (i = 0U; i < 50U; i++){
   textgui_putchar(BOT_X(i), BOT_Y(j), textgui_bot[(j * 50U) + i], pix, ptc, pal);
  }
 }

 /* Get speed percentage from CPU frequency (which should be 28636400Hz) */

 i = (textgui_elements.cpufreq + (286364U / 2U)) / 286364U;
 textgui_putdec(TOP_X( 8U), TOP_Y(0U), i, pix, ptc, pal);

 /* Output frequencies */

 textgui_putdec(TOP_X(41U), TOP_Y(1U), textgui_elements.cpufreq / 1000000U, pix, ptc, pal);
 textgui_putdec(TOP_X(45U), TOP_Y(1U), textgui_elements.cpufreq /    1000U, pix, ptc, pal);
 textgui_putdec(TOP_X(41U), TOP_Y(0U), textgui_elements.dispfreq /   1000U, pix, ptc, pal);
 textgui_putdec(TOP_X(45U), TOP_Y(0U), textgui_elements.dispfreq,           pix, ptc, pal);
 textgui_putdec(TOP_X(41U), TOP_Y(2U), textgui_elements.aufreq /     1000U, pix, ptc, pal);
 textgui_putdec(TOP_X(45U), TOP_Y(2U), textgui_elements.aufreq,             pix, ptc, pal);

 /* SNES / UZEM keyboard mapping */

 if (textgui_elements.kbuzem){
  textgui_putstr(TOP_X(32U), TOP_Y(0U), &(textgui_kbuzem[0]), pix, ptc, pal);
 }else{
  textgui_putstr(TOP_X(32U), TOP_Y(0U), &(textgui_kbsnes[0]), pix, ptc, pal);
 }

 /* Frame merging */

 if (textgui_elements.merge){
  textgui_putchar(TOP_X(37U), TOP_Y(0U),  2U, pix, ptc, pal);
 }else{
  textgui_putchar(TOP_X(37U), TOP_Y(0U), ' ', pix, ptc, pal);
 }

 /* Video capturing */

 if (textgui_elements.capture){
  textgui_putchar(TOP_X(27U), TOP_Y(0U), 13U, pix, ptc, pal);
  textgui_putchar(TOP_X(28U), TOP_Y(0U), 14U, pix, ptc, pal);
 }else{
  textgui_putchar(TOP_X(27U), TOP_Y(0U), ' ', pix, ptc, pal);
  textgui_putchar(TOP_X(28U), TOP_Y(0U), ' ', pix, ptc, pal);
 }

 /* Game & Author name */

 textgui_putstr(TOP_X(6U), TOP_Y(1U), &(textgui_elements.game[0]), pix, ptc, pal);
 textgui_putstr(TOP_X(6U), TOP_Y(2U), &(textgui_elements.auth[0]), pix, ptc, pal);

 /* Whisper ports */

 for (i = 0U; i < 2U; i++){
  t = textgui_elements.ports[i];
  textgui_putdec (BOT_X( 8U), BOT_Y(i), t, pix, ptc, pal);
  textgui_putchar(BOT_X(15U), BOT_Y(i), textgui_tohex(t >> 4), pix, ptc, pal);
  textgui_putchar(BOT_X(16U), BOT_Y(i), textgui_tohex(     t), pix, ptc, pal);
  for (j = 0U; j < 8U; j++){
   textgui_putchar(BOT_X(19U + j), BOT_Y(i), ((t >> (7U - j)) & 1U) + '0', pix, ptc, pal);
  }
 }
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
