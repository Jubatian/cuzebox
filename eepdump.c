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
#include "filesys.h"



/* EEPROM dump file */
static const char eepdump_file[] = "eeprom.bin";



/*
** Tries to load EEPROM state from an eeprom dump. If the EEPROM dump does not
** exist, it clears the EEPROM.
*/
void eepdump_load(uint8* eeprom)
{
 auint rv;

 if (!filesys_open(FILESYS_CH_EEP, eepdump_file)){ goto ex_fail; }

 rv = filesys_read(FILESYS_CH_EEP, eeprom, 2048U);
 if (rv != 2048){ goto ex_fail; }

 filesys_flush(FILESYS_CH_EEP);
 return;

ex_fail:
 filesys_flush(FILESYS_CH_EEP);
 memset(eeprom, 0xFFU, 2048U);
 return;
}



/*
** Tries to write out EEPROM state into an eeprom dump. It fails silently if
** this is not possible.
*/
void eepdump_save(uint8 const* eeprom)
{
 (void)(filesys_open(FILESYS_CH_EEP, eepdump_file));   /* Might fail since tries to open for reading */

 (void)(filesys_write(FILESYS_CH_EEP, eeprom, 2048U)); /* Don't care for faults (can't do anything about them) */

 filesys_flush(FILESYS_CH_EEP);
 return;
}
