/*
**  Converts binary gamefile to C source.
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
**  The input should be a .uze file, it reads it from standard input.
*/



#include <stdio.h>
#include <stdlib.h>



int main(void)
{
 unsigned int  bc;
 unsigned int  tc;
 unsigned char c;

 /* Add heading */

 printf(
   "/* Converted binary gamefile (see binconv.c in assets) */\n"
   "\n"
   "#include \"types.h\"\n"
   "\n"
   "uint8 const gamefile[] = {\n");

 /* Just read and output as long as there is input */

 tc = 0U;

 while (1){

  bc = fread(&c, 1U, 1U, stdin);
  if (bc != 1U){ break; } /* Assume end of stream */

  printf(" 0x%02XU,", c);
  tc ++;
  if ((tc & 0xFU) == 0U){
   printf("\n");
  }

 }

 if ((tc & 0xFU) != 0U){
  printf("\n");
 }

 /* Add tail */

 printf(
   "};\n"
   "\n"
   "auint const gamefile_size = %u;\n", tc);

 return 0;
}
