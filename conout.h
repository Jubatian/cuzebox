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



#ifndef CONOUT_H
#define CONOUT_H



#include "types.h"


/*
** Adds a string to the console output under preparation.
*/
void conout_addstr(char const* str);


/*
** Adds a single character to the console output under preparation.
*/
void conout_addchr(char chr);


/*
** Send the prepared string
*/
void conout_send(void);


#endif
