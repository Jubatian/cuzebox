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


/* Size of Root directory. Should not be changed (this module is designed to
** support changing it, but later the state saves might not be) */
#define CU_VFAT_ROOT_SIZE     512U

/* Size of System area in bytes */
#define CU_VFAT_SYS_SIZE      ((0x201U * 512U) + (CU_VFAT_ROOT_SIZE * 32U))

/* Size of Host filenames. Should not be changed. (this module is designed to
** support changing it, but later the state saves might not be) */
#define CU_VFAT_HNAME_SIZE    64U


/* Virtual FAT16 filesystem structure */
typedef struct{
 uint8 sys[CU_VFAT_SYS_SIZE]; /* The system area of the file system (MBR, FATs, root dir) */
 uint8 rwbuf[512U];           /* Read / Write buffer, collecting a sector of data */
 char  hnames[CU_VFAT_ROOT_SIZE][CU_VFAT_HNAME_SIZE]; /* Host filenames */
 boole isacc;                 /* Whether the Virtual FAT was accessed since reset */
}cu_state_vfat_t;


/*
** Resets the virtual filesystem. This requests it building new filesystem
** structures according to the underlaying source.
*/
void  cu_vfat_reset(void);


/*
** Reads a byte from a given byte position in the file system's (virtual) disk
** image. Note that a FAT16 volume is at most 2^31 bytes large (64K * 32Kbytes
** clusters), so a 32 bit value is sufficient to address it. Reading the first
** byte of a sector triggers buffering the sector, subsequent bytes are always
** returned from within the buffer.
*/
auint cu_vfat_read(auint pos);


/*
** Writes a byte to the given byte position in the file system's (virtual)
** disk image. Writing the last byte of a sector triggers writing out the
** buffered sector (so ideally a sector should be filled up as a sequence of
** writes).
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


/*
** Returns whether the virtual filesystem was accessed since reset. If a read
** or write happens to it, then it is accessed. This can be used to decide
** whether to add the filesystem's state to a state save.
*/
boole cu_vfat_isaccessed(void);


#endif
