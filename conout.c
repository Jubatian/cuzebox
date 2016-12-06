/*
 *  Simple console output
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



/*
** This simple console output is used to generate a single line of status,
** also sent to the window title if possible.
*/



#include "conout.h"
#include "guicore.h"



/* Maximal length of string to output */
#define MAX_STR_LEN 78U


/* String buffer */
char conout_str[MAX_STR_LEN + 2U];

/* Position in the string buffer */
auint conout_str_pos = 0U;



/*
** Adds a string to the console output under preparation.
*/
void conout_addstr(char const* str)
{
 auint i = 0U;

 while ( (conout_str_pos < MAX_STR_LEN) &&
         (str[i] != 0U) ){
  conout_str[conout_str_pos] = str[i];
  conout_str_pos ++;
  i ++;
 }
}



/*
** Adds a single character to the console output under preparation.
*/
void conout_addchr(char chr)
{
 if (conout_str_pos < MAX_STR_LEN){
  conout_str[conout_str_pos] = chr;
  conout_str_pos ++;
 }
}



/*
** Send the prepared string
*/
void conout_send(void)
{
 conout_str[conout_str_pos] = 0;

 guicore_setcaption(&conout_str[0]);

 conout_str[conout_str_pos + 0U] = '\n';
 conout_str[conout_str_pos + 1U] = 0;

 print_unf(&conout_str[0]);

 conout_str_pos = 0U;
}
