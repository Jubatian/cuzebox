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



#include "cu_vfat.h"


/*
** Filesystem layout:
**
** Sector     Cluster   Size (Sec)
** 0x000000   ------    0x000001    MBR
** 0x000001   ------    0x000100    FAT0
** 0x000101   ------    0x000100    FAT1
** 0x000201   ------    0x000020    Root directory
** 0x000221   0x0002    0x3FFB80    Data area
** 0x3FFDA1   0xFFF0    --------    End of disk
**
** Notes on how to access each of the locations:
**
** MBR:  At address 0.
** FAT0: After the Reserved Sectors
** FAT1: After FAT0
** Root: After FAT1
** Data: After the Root directory
**
** Obtaining the sizes:
**
** MBR:  The Reserved sector count provides it (with any additional sectors)
** FAT0: The FAT size field
** FAT1: (Same)
** Root: The number of root dir. entries (32 bytes each) padded to sectors
** Data: Total size minus all the above
*/


/* Master Boot Record data (Note: Records are Little Endian!).
** Not included are the signature bytes (0xAA, 0x55) on the end of the 512
** byte sector */
#define VFAT_DATA_MBR_SIZE 0x3EU
static const uint8 vfat_data_mbr[VFAT_DATA_MBR_SIZE] = {
 0x00U, 0x00U, 0x00U,        /* Jump to bootstrap */
 'C',   'U',   'Z',   'E',
 'B',   'O',   'X',   ' ',   /* OEM ID */
 0x00U, 0x02U,               /* 512 bytes / sector */
 0x40U,                      /* 64 sectors / cluster */
 0x01U, 0x00U,               /* Reserved sectors (1: Boot sector) */
 0x02U,                      /* Number of FATs */
 0x00U, 0x02U,               /* 512 root directory entries */
 0x00U, 0x00U,               /* Number of sectors (indicates > 65535 sectors) */
 0xF8U,                      /* Media descriptor: Fixed disk */
 0x00U, 0x01U,               /* 256 sectors per FAT (One FAT uses 16 bit * 64K clusters) */
 0x00U, 0x00U,               /* CHS geometry sectors / track */
 0x00U, 0x00U,               /* CHS geometry number of heads */
 0x00U, 0x00U, 0x00U, 0x00U, /* Hidden sectors */
 0xA1U, 0xFDU, 0x3FU, 0x00U, /* 0x3FFDA1 sectors total size (~2 GBytes) */
 0x81U,                      /* Drive number (second HDD) */
 0x00U,                      /* Reserved */
 0x29U,                      /* Extended boot signature present */
 0xEFU, 0xBEU, 0xADU, 0xDEU, /* Serial number */
 'U',   'Z',   'E',   'B',
 'O',   'X',   'G',   'A',
 'M',   'E',   'S',          /* Volume label */
 'F',   'A',   'T',   '1',
 '6',   ' ',   ' ',   ' '};  /* File system type */


/* FAT data (Note: Records are Little Endian!). */
#define VFAT_DATA_FAT_SIZE 4U
static const uint8 vfat_data_fat[VFAT_DATA_FAT_SIZE] = {
 0xFFU, 0xF8U,
 0xFFU, 0xFFU,
};



/* Virtual filesystem state */
static cu_state_vfat_t vfat_state;



/*
** Resets the virtual filesystem. This requests it building new filesystem
** structures according to the underlaying source.
*/
void  cu_vfat_reset(void)
{
 auint i;

 /* Clear all */

 memset(&vfat_state, 0, sizeof(vfat_state));

 /* Add Master Boot Record */

 for (i = 0U; i < VFAT_DATA_MBR_SIZE; i++){
  vfat_state.sys[i] = vfat_data_mbr[i];
 }
 vfat_state.sys[0x0001FEU] = 0xAAU;
 vfat_state.sys[0x0001FFU] = 0x55U;

 /* Add FAT0 */

 for (i = 0U; i < VFAT_DATA_FAT_SIZE; i++){
  vfat_state.sys[0x000200U + i] = vfat_data_fat[i];
 }

 /* Add FAT1 */

 memcpy(&(vfat_state.sys[0x020200U]), &(vfat_state.sys[0x000200U]), 0x020000U);

 /* Add root directory */

 /* (Currently just empty) */
}



/*
** Reads a byte from a given byte position in the file system's (virtual) disk
** image. Note that a FAT16 volume is at most 2^31 bytes large (64K * 32Kbytes
** clusters), so a 32 bit value is sufficient to address it.
*/
auint cu_vfat_read(auint pos)
{
 if (pos < VFAT_SYS_SIZE){ return vfat_state.sys[pos]; }
 return 0U;
}



/*
** Writes a byte to the given byte position in the file system's (virtual)
** disk image. Writes to last bytes of sectors might trigger filesystem
** activity (so writing full 512 byte sectors is preferred).
*/
void  cu_vfat_write(auint pos, auint data)
{
}



/*
** Returns virtual filesystem state. It may be written, then the
** cu_vfat_update() function has to be called to rebuild any internal state
** depending on it.
*/
cu_state_vfat_t* cu_vfat_get_state(void)
{
 return &(vfat_state);
}



/*
** Rebuild internal state according to the current state. Call after writing
** the filesystem state.
*/
void  cu_vfat_update(void)
{
}
