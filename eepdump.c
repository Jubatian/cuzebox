/*
 *  EEPROM dump functions
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



#include "eepdump.h"



/* EEPROM dump file */
static const char* eepdump_file = "eeprom.bin";



/*
** Tries to load EEPROM state from an eeprom dump. If the EEPROM dump does not
** exist, it clears the EEPROM.
*/
void eepdump_load(uint8* eeprom)
{
 FILE* fp;
 asint rv;

 fp = fopen(eepdump_file, "rb");
 if (fp == NULL){ goto ex_none; }

 rv = fread(eeprom, 1U, 2048U, fp);
 if (rv != 2048){ goto ex_file; }

 fclose(fp);
 return;

ex_file:
 fclose(fp);
ex_none:
 memset(eeprom, 0xFFU, 2048U);
 return;
}



/*
** Tries to write out EEPROM state into an eeprom dump. It fails silently if
** this is not possible.
*/
void eepdump_save(uint8 const* eeprom)
{
/* Note: In Emscripten there is no sense in saving */
#ifndef __EMSCRIPTEN__

 FILE* fp;
 asint rv;

 fp = fopen(eepdump_file, "wb");
 if (fp == NULL){ goto ex_none; }

 rv = fwrite(eeprom, 1U, 2048U, fp);
 if (rv != 2048){ goto ex_file; }

 fclose(fp);
 return;

ex_file:
 fclose(fp);
ex_none:
 return;

#endif
}
