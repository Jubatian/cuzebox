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



#include "eepdump.h"
#include "filesys.h"



/* Code ROM dump file */
static const char romdump_file[] = "coderom.bin";



/*
** Tries to load Code ROM state from a rom dump. If the Code ROM dump does not
** exist, it clears the Code ROM. Returns TRUE if a Code ROM dump existed.
*/
boole romdump_load(uint8* crom)
{
 auint rv;

 if (!filesys_open(FILESYS_CH_ROM, romdump_file)){ goto ex_fail; }

 rv = filesys_read(FILESYS_CH_ROM, crom, 65536U);
 if (rv != 65536U){ goto ex_fail; }

 filesys_flush(FILESYS_CH_ROM);
 return TRUE;

ex_fail:
 filesys_flush(FILESYS_CH_ROM);
 /* Initialize ROM to zero (NOP). Using zero (a valid NOP) is important as
 ** this is used to check the presence of a valid bootloader at reset (auto
 ** fusing the emulated AVR's boot setup to game / bootloader). */
 memset(crom, 0x00U, 65536U);
 return FALSE;
}



/*
** Tries to write out Code ROM state into a rom dump. It fails silently if
** this is not possible.
*/
void romdump_save(uint8 const* crom)
{
 (void)(filesys_open(FILESYS_CH_ROM, romdump_file));  /* Might fail since tries to open for reading */

 (void)(filesys_write(FILESYS_CH_ROM, crom, 65536U)); /* Don't care for faults (can't do anything about them) */

 filesys_flush(FILESYS_CH_ROM);
 return;
}
