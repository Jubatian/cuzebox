/*
 *  Code ROM dump functions
 *
 *  Copyright (C) 2017
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



#ifndef ROMDUMP_H
#define ROMDUMP_H



#include "types.h"



/*
** Note: Should be handled along with eepdump.h & c, adding further
** functionality there should be mirrored here (paths etc.).
*/



/*
** Tries to load Code ROM state from a rom dump. If the Code ROM dump does not
** exist, it clears the Code ROM. Returns TRUE if a Code ROM dump existed.
*/
boole romdump_load(uint8* crom);


/*
** Tries to write out Code ROM state into a rom dump. It fails silently if
** this is not possible.
*/
void romdump_save(uint8 const* crom);


#endif
