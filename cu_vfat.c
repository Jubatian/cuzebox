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
#include "filesys.h"


/*
** Filesystem layout:
**
** Sector     Cluster   Size (Sec)
** 0x000000   ------    0x000001    MBR
** 0x000001   ------    0x000100    FAT0
** 0x000101   ------    0x000100    FAT1
** 0x000201   ------    0x000020    Root directory (512 entries)
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


/* Size of data area in clusters */
#define VFAT_DATA_SIZE     0xFFEEU

/* End cluster of data area + 1 */
#define VFAT_DATA_END      (VFAT_DATA_SIZE + 2U)


/* Master Boot Record data (Note: Records are Little Endian!).
** Not included are the signature bytes (0x55, 0xAA) on the end of the 512
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
 (CU_VFAT_ROOT_SIZE & 0xFFU), (CU_VFAT_ROOT_SIZE >> 8), /* No. of root directory entries */
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

/* Last accessed file cache. This is used to remember which file the position
** belongs to which is difficult to find from the position itself (it demands
** a full FAT scan). It can be anything, if a match fails, this cache is
** simply recalculated. */
static auint vfat_last;

/* Broken FAT marker. This is set if a cluster chain traversal ended up in a
** deadlock, cleared if a write is performed on the first FAT (which is used
** to track the disk's structure). When the FAT is broken, no reads or writes
** succeed on the data area (to prevent locking up the host forcing it to scan
** the bogus FAT over and over). */
static boole vfat_broken;

/* Last opened file ID. Used to prevent re-opening files */
static auint vfat_lfile;



/*
** Returns TRUE if the given position belongs to a given file ID, that is, the
** position's cluster is in the file's cluster chain. If it succeeds, it
** returns the file position in fpos.
*/
static boole cu_vfat_belongs(auint pos, auint id, auint* fpos)
{
 auint fclus;
 auint pclus;
 auint maxch = VFAT_DATA_SIZE; /* Max chain limit to prevent crashing on a bad FAT */

 if (id >= CU_VFAT_ROOT_SIZE){ return FALSE; }

 fclus = ((auint)(vfat_state.sys[0x040200U + (id * 32U) + 0x1AU])     ) +
         ((auint)(vfat_state.sys[0x040200U + (id * 32U) + 0x1BU]) << 8);
 pclus = ((pos - CU_VFAT_SYS_SIZE) >> 15) + 0x0002U;
 (*fpos) = (pos - CU_VFAT_SYS_SIZE) & 0x7FFFU;

 while ( (fclus < VFAT_DATA_END) &&
         (fclus >= 0x0002U) &&
         (maxch != 0U) ){
  if (pclus == fclus){ return TRUE; } /* Match OK */
  fclus = ((auint)(vfat_state.sys[0x000200U + (fclus << 1) + 0U])     ) +
          ((auint)(vfat_state.sys[0x000200U + (fclus << 1) + 1U]) << 8);
  (*fpos) += 0x8000U;
  maxch --;
 }

 if (maxch == 0U){ vfat_broken = TRUE; } /* Fat is bogus (contains a loop) */

 return FALSE;
}



/*
** Returns the file ID the disk position belongs to. Returns CU_VFAT_ROOT_SIZE
** if no match is found. If it succeeds, it returns the file position in fpos.
*/
static auint cu_vfat_findbypos(auint pos, auint* fpos)
{
 auint i = 0U;
 auint fch;

 if (vfat_broken){ return CU_VFAT_ROOT_SIZE; }

 /* If cache contains it, then return fast */

 if (cu_vfat_belongs(pos, vfat_last, fpos)){ return vfat_last; }

 /* Cache miss, do a slow search through all available files */

 while (i < CU_VFAT_ROOT_SIZE){

  if (vfat_broken){ return CU_VFAT_ROOT_SIZE; }

  fch = vfat_state.sys[0x040200U + (i * 32U)];
  if ( (fch != 0x00U) &&
       (fch != 0x2EU) &&
       (fch != 0xE5U) ){ /* Only if the first character identifies valid file */
   if (cu_vfat_belongs(pos, i, fpos)){
    vfat_last = i;       /* Found, so set cache */
    return i;
   }
  }

  i ++;

 }

 /* The position didn't belong anything */

 return CU_VFAT_ROOT_SIZE;
}



/*
** Converts a Host filename to a FAT16 filename (11 chars as required by the
** directory entry). Sources of up to 8 + 3 chars are converted normally. For
** now don't care a lot about long file names, the filesystem will work with
** repetitions if one happens because of it.
*/
static void cu_vfat_tofat(const char* src, uint8* dest)
{
 auint spos = 0U;
 auint dpos = 0U;

 memset(dest, ' ', 11U);

 while (src[spos] != 0){

  if       ((src[spos] >= 'a') && (src[spos] <= 'z')){
   dest[dpos] = (uint8)((src[spos] - 'a') + 'A');
  }else if ((src[spos] >= 'A') && (src[spos] <= 'Z')){
   dest[dpos] = (uint8)(src[spos]);
  }else if ((src[spos] >= '0') && (src[spos] <= '9')){
   dest[dpos] = (uint8)(src[spos]);
  }else if ( (src[spos] == '~') ||
             (src[spos] == '!') ||
             (src[spos] == '-') ||
             (src[spos] == '_') ){
   dest[dpos] = (uint8)(src[spos]);
  }else if ( (src[spos] == '.') &&
             (dpos < 8U) ){
   dpos = 7U; /* Go on to extension */
   spos --;   /* Bounce back to stay at the extension dot */
  }else{
   dest[dpos] = (uint8)('$');
  }

  spos ++;
  dpos ++;

  if (dpos == 8U){
   while ( (src[spos] != 0) &&
           (src[spos] != '.') ){
    spos ++;  /* Long name */
   }
   if (src[spos] == '.'){ spos ++; } /* Jump over extension dot */
  }else if (dpos == 11U){
   while (src[spos] != 0){
    spos ++;  /* Long extension */
   }
  }

 }
}



/*
** Converts a FAT16 filename to Host filename. This is used when a new file's
** creation is detected to produce an output file accordingly (if possible).
** The destination needs to be able to hold up to 13 characters (8 filename,
** 1 dot, 3 extension, terminating zero).
*/
static void cu_vfat_tohost(uint8 const* src, char* dest)
{
 auint spos = 0U;
 auint dpos = 0U;

 while (spos < 11U){

  if       ((src[spos] >= 'a') && (src[spos] <= 'z')){
   dest[dpos] = (char)(src[spos]);
  }else if ((src[spos] >= 'A') && (src[spos] <= 'Z')){
   dest[dpos] = ((char)(src[spos]) - 'A') + 'a';
  }else if ((src[spos] >= '0') && (src[spos] <= '9')){
   dest[dpos] = (char)(src[spos]);
  }else if ( (src[spos] == '~') ||
             (src[spos] == '!') ||
             (src[spos] == '-') ||
             (src[spos] == '_') ){
   dest[dpos] = (char)(src[spos]);
  }else{
   dest[dpos] = '$';
  }

  spos ++;
  dpos ++;

  if (src[spos] == ' '){ spos = 8U; } /* Skip to extension */

  if (spos == 8U){
   dest[dpos] = '.';
   dpos ++;
  }

 }

 dest[dpos] = 0;
}



/*
** Checks if a given file in the FAT16 filesystem has a proper host file
** assigned. If not, create the assignment by converting the FAT16 filename.
** This function allows creating new files from the emulator.
*/
static void cu_vfat_checkassign(auint id)
{
 auint fch;

 if (id >= CU_VFAT_ROOT_SIZE){ return; }

 if (vfat_state.hnames[id][0] == 0){ /* Need a host file name */

  fch = vfat_state.sys[0x040200U + (id * 32U)];
  if ( (fch != 0x00U) &&
       (fch != 0x2EU) &&
       (fch != 0xE5U) ){ /* Only if the first character identifies valid file */

   cu_vfat_tohost(&(vfat_state.sys[0x040200U + (id * 32U)]), &(vfat_state.hnames[id][0]));

  }

 }
}



/*
** Resets the virtual filesystem. This requests it building new filesystem
** structures according to the underlaying source.
*/
void  cu_vfat_reset(void)
{
 auint i;
 auint siz;
 auint cpos = 0x0002U; /* Current cluster position */
 auint tpos;
 char  hname[CU_VFAT_HNAME_SIZE];

 /* Clear all */

 memset(&vfat_state, 0, sizeof(vfat_state));
 vfat_broken = FALSE;
 vfat_lfile  = CU_VFAT_ROOT_SIZE;

 /* Add Master Boot Record */

 for (i = 0U; i < VFAT_DATA_MBR_SIZE; i++){
  vfat_state.sys[i] = vfat_data_mbr[i];
 }
 vfat_state.sys[0x0001FEU] = 0x55U;
 vfat_state.sys[0x0001FFU] = 0xAAU;

 /* Add FAT0 */

 for (i = 0U; i < VFAT_DATA_FAT_SIZE; i++){
  vfat_state.sys[0x000200U + i] = vfat_data_fat[i];
 }

 /* Add FAT1 */

 memcpy(&(vfat_state.sys[0x020200U]), &(vfat_state.sys[0x000200U]), 0x020000U);

 /* Add root directory */

 filesys_find_reset();

 i = 0U;
 while (i < CU_VFAT_ROOT_SIZE){

  siz = filesys_find_next(&(hname[0]), CU_VFAT_HNAME_SIZE);
  if (siz == 0xFFFFFFFFU){ break; }

  print_message("Found: %s (%u bytes)\n", &(hname[0]), siz);

  tpos = cpos + ((siz + 32767U) >> 15); /* File's allocation */
  if (siz == 0U){ tpos = cpos + 1U; }   /* Empty file */

  if (tpos <= VFAT_DATA_END){           /* The file fits on the virtual disk */

   /* Create file's directory entry and link to host file */

   cu_vfat_tofat(&(hname[0]), &(vfat_state.sys[0x040200U + (i * 32U) + 0x00U]));
   strcpy(&(vfat_state.hnames[i][0]), &(hname[0]));

   vfat_state.sys[0x040200U + (i * 32U) + 0x1AU] =  cpos        & 0xFFU;
   vfat_state.sys[0x040200U + (i * 32U) + 0x1BU] = (cpos >>  8) & 0xFFU;
   vfat_state.sys[0x040200U + (i * 32U) + 0x1CU] =  siz         & 0xFFU;
   vfat_state.sys[0x040200U + (i * 32U) + 0x1DU] = (siz  >>  8) & 0xFFU;
   vfat_state.sys[0x040200U + (i * 32U) + 0x1EU] = (siz  >> 16) & 0xFFU;
   vfat_state.sys[0x040200U + (i * 32U) + 0x1FU] = (siz  >> 24) & 0xFFU;

   /* Lay out the file's clusters on the virtual disk (in both FATs) */

   while (cpos < (tpos - 1U)){
    vfat_state.sys[0x000200U + (cpos << 1) + 0U] = ((cpos + 1U)     ) & 0xFFU;
    vfat_state.sys[0x020200U + (cpos << 1) + 0U] = ((cpos + 1U)     ) & 0xFFU;
    vfat_state.sys[0x000200U + (cpos << 1) + 1U] = ((cpos + 1U) >> 8) & 0xFFU;
    vfat_state.sys[0x020200U + (cpos << 1) + 1U] = ((cpos + 1U) >> 8) & 0xFFU;
    cpos ++;
   }
   vfat_state.sys[0x000200U + (cpos << 1) + 0U] = 0xFFU;
   vfat_state.sys[0x020200U + (cpos << 1) + 0U] = 0xFFU;
   vfat_state.sys[0x000200U + (cpos << 1) + 1U] = 0xFFU;
   vfat_state.sys[0x020200U + (cpos << 1) + 1U] = 0xFFU;
   cpos ++;

  }

  i++;

 }

 vfat_state.isacc = FALSE;
}



/*
** Reads a byte from a given byte position in the file system's (virtual) disk
** image. Note that a FAT16 volume is at most 2^31 bytes large (64K * 32Kbytes
** clusters), so a 32 bit value is sufficient to address it. Reading the first
** byte of a sector triggers buffering the sector, subsequent bytes are always
** returned from within the buffer.
*/
auint cu_vfat_read(auint pos)
{
 auint fid;
 auint fpos;

 vfat_state.isacc = TRUE;

 /* System area */

 if (pos < CU_VFAT_SYS_SIZE){ return vfat_state.sys[pos]; }

 /* Data area */

 if ((pos & 0x1FFU) == 0U){ /* First byte of sector: Buffer it */

  fid = cu_vfat_findbypos(pos, &fpos);

  if (fid != CU_VFAT_ROOT_SIZE){
   if (fid != vfat_lfile){
    cu_vfat_checkassign(fid); /* Maybe a not yet assigned file */
    filesys_open(FILESYS_CH_SD, &(vfat_state.hnames[fid][0]));
    vfat_lfile = fid;
   }
   filesys_setpos(FILESYS_CH_SD, fpos);
   filesys_read(FILESYS_CH_SD, &(vfat_state.rwbuf[0]), 512U);
  }else{
   memset(&(vfat_state.rwbuf[0]), 0U, 512U);
  }

 }

 return vfat_state.rwbuf[pos & 0x1FFU];
}



/*
** Writes a byte to the given byte position in the file system's (virtual)
** disk image. Writing the last byte of a sector triggers writing out the
** buffered sector (so ideally a sector should be filled up as a sequence of
** writes).
*/
void  cu_vfat_write(auint pos, auint data)
{
 auint fid;
 auint fpos;

 vfat_state.isacc = TRUE;

 /* System area */

 if (pos < CU_VFAT_SYS_SIZE){
  vfat_state.sys[pos] = data;
  vfat_broken = FALSE;      /* Allow for repairing the FAT */
  return;
 }

 /* Data area */

 vfat_state.rwbuf[pos & 0x1FFU] = data;

 if ((pos & 0x1FFU) == 0x1FFU){ /* Last byte of sector: Write it */

  fid = cu_vfat_findbypos(pos & (~(auint)(0x1FFU)), &fpos);
  if (fid != CU_VFAT_ROOT_SIZE){
   if (fid != vfat_lfile){
    cu_vfat_checkassign(fid); /* Maybe a not yet assigned file */
    filesys_open(FILESYS_CH_SD, &(vfat_state.hnames[fid][0]));
    vfat_lfile = fid;
   }
   filesys_setpos(FILESYS_CH_SD, fpos);
   filesys_write(FILESYS_CH_SD, &(vfat_state.rwbuf[0]), 512U);
  }

 }
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
 vfat_broken = FALSE;
 vfat_lfile  = CU_VFAT_ROOT_SIZE;
}



/*
** Returns whether the virtual filesystem was accessed since reset. If a read
** or write happens to it, then it is accessed. This can be used to decide
** whether to add the filesystem's state to a state save.
*/
boole cu_vfat_isaccessed(void)
{
 return vfat_state.isacc;
}
