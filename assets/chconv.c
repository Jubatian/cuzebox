/*
**  Converts GIMP header to 1bpp charset, C header.
**
**  By Sandor Zsuga (Jubatian)
**
**  Licensed under GNU General Public License version 3.
**
**  This program is free software: you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation, either version 3 of the License, or
**  (at your option) any later version.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with this program.  If not, see <http://www.gnu.org/licenses/>.
**
**  ---
**
**  The input image must be 96 x 48 pixels, 1bpp, index 0 being the
**  background, other indices the foreground. The palette is ignored.
**
**  Result is produced onto the standard output.
**
**  The output is a hexadecimal stream of image data, 6 bytes for each
**  character, each byte a character row, high bits first (bit 0 and 1 are
**  always zero), 128 characters.
*/



/*  The GIMP header to use */
#include "charset.h"


#include <stdio.h>
#include <stdlib.h>



int main(void)
{
 unsigned int  mct;
 unsigned int  cct;
 unsigned int  rct;
 unsigned int  dp;
 unsigned char c;

 /* Basic tests */

 if (width != 96U){
  fprintf(stderr, "Input width must be 96 pixels!\n");
  return 1;
 }
 if (height != 48U){
  fprintf(stderr, "Input height must be 48 pixels!\n");
  return 1;
 }

 /* Add heading */

 printf(
   "/* Converted character set (see chconv.c in assets) */\n"
   "\n"
   "#include \"types.h\"\n"
   "\n"
   "uint8 const chars[128U * 6U] = {\n");

 /* Process image data */

 for (mct = 0U; mct < 8U; mct ++){   /* Character rows (8) */

  for (cct = 0U; cct < 16U; cct ++){ /* Characters (16 / row) */

   for (rct = 0U; rct < 6U; rct ++){ /* Pixel rows (6 / character) */

    /* Collect six pixels */

    dp = (mct * 96U * 6U) + (cct * 6U) + (rct * 96U);
    c  = (header_data[dp + 0U] & 1U) << 7;
    c |= (header_data[dp + 1U] & 1U) << 6;
    c |= (header_data[dp + 2U] & 1U) << 5;
    c |= (header_data[dp + 3U] & 1U) << 4;
    c |= (header_data[dp + 4U] & 1U) << 3;
    c |= (header_data[dp + 5U] & 1U) << 2;

    /* Output it */

    printf(" 0x%02XU,", c);

   }

   printf("\n");

  }

 }

 /* Add tail */

 printf("};\n");

 return 0;
}
