/*
 *  AVR instruction deflagger
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



#ifndef CU_AVRDF_H
#define CU_AVRDF_H



#include "cu_types.h"


/*
** The instruction deflagger parses the compiled AVR code attempting to
** transform instructions to remove costly flag logic (mainly N, H, S and V
** flags) where it can be proven that the flags aren't used. It can not
** revert already transformed instructions. It works on the whole code
** memory (32 KWords).
*/
void  cu_avrdf_deflag(uint32* code);


#endif
