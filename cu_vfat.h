/*
 *  Virtual FAT16 filesystem
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



#ifndef CU_VFAT_H
#define CU_VFAT_H



#include "types.h"


/* Size of System area in bytes */
#define VFAT_SYS_SIZE      (0x221U * 512U)


/* Virtual FAT16 filesystem structure */
typedef struct{
 uint8 sys[VFAT_SYS_SIZE]; /* The system area of the file system (MBR, FATs, root dir) */
 uint8 wbuf[512U];         /* Write buffer, collecting a sector of data */
}cu_state_vfat_t;


/*
** Resets the virtual filesystem. This requests it building new filesystem
** structures according to the underlaying source.
*/
void  cu_vfat_reset(void);


/*
** Reads a byte from a given byte position in the file system's (virtual) disk
** image. Note that a FAT16 volume is at most 2^31 bytes large (64K * 32Kbytes
** clusters), so a 32 bit value is sufficient to address it.
*/
auint cu_vfat_read(auint pos);


/*
** Writes a byte to the given byte position in the file system's (virtual)
** disk image.
*/
void  cu_vfat_write(auint pos, auint data);


/*
** Returns virtual filesystem state. It may be written, then the
** cu_vfat_update() function has to be called to rebuild any internal state
** depending on it.
*/
cu_state_vfat_t* cu_vfat_get_state(void);


/*
** Rebuild internal state according to the current state. Call after writing
** the filesystem state.
*/
void  cu_vfat_update(void);


#endif
