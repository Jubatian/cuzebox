/*
 *  Filesystem interface layer (minimal wrapper for self-contained)
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



#include "filesys.h"
#include "gamefile.h"



/* Channel state structure */
typedef struct{
 auint  pos;      /* Position within the file (virtual) */
}filesys_ch_t;



/* There are compiler problems about the uniform zero initializer ({0}) when
** they are used for more complex things. So the initializer below is a hack,
** it is meant to zero initialize everything. But it relies on FILESYS_CH_NO's
** size, so check here */
#if (FILESYS_CH_NO != 3U)
#error "Check filesys_ch's initializer! FILESYS_CH_NO changed!"
#endif



/* Channels */
static filesys_ch_t filesys_ch[FILESYS_CH_NO] = {
 {0U},
 {0U},
 {0U},
};



/*
** Sets path string. All filesystem operations will be carried out on the
** set path. If there is a file name on the end of the path, it is removed
** from it (bare paths must terminate with a '/'). If name is non-null, the
** filename part of the path (if any) is loaded into it (up to len bytes).
*/
void  filesys_setpath(char const* path, char* name, auint len)
{
 if (name != NULL){ name[0] = 0U; }
}



/*
** Opens a file. Position is at the beginning. Returns TRUE on success.
** Initially the file internally is opened for reading (so write protected
** files may also be opened), adding write access is only attempted when
** trying to write the file. If there is already a file open, it is closed
** first.
*/
boole filesys_open(auint ch, char const* name)
{
 if (ch != FILESYS_CH_EMU){
  return FALSE;
 }

 filesys_ch[ch].pos = 0U;
 return TRUE;
}



/*
** Read bytes from a file. The reading increases the internal position.
** Returns the number of bytes read, which may be zero if no file is open
** on the channel or it can not be read (such as because the position is at
** or beyond the end). This automatically reopens the last opened file if
** necessary, seeking to the last set position.
*/
auint filesys_read(auint ch, uint8* dest, auint len)
{
 auint rb = len;

 if (rb == 0U){
  return 0U;
 }

 if (ch != FILESYS_CH_EMU){
  return 0U;
 }

 if (filesys_ch[ch].pos >= gamefile_size){
  return 0U;
 }

 if ((filesys_ch[ch].pos + rb) >= gamefile_size){
  rb = gamefile_size - filesys_ch[ch].pos;
 }

 memcpy(dest, &(gamefile[filesys_ch[ch].pos]), rb);

 filesys_ch[ch].pos += rb;

 return rb;
}



/*
** Writes bytes to a file. The writing increases the internal position.
** Returns the number of bytes written, which may be zero if writing is not
** possible for any reason. This automatically reopens the file for writing.
*/
auint filesys_write(auint ch, uint8 const* src, auint len)
{
 return 0U;
}



/*
** Sets file position. It may be set beyond the end of the actual file. This
** case a write access will attempt to pad the file with zeroes until the
** requested position.
*/
void  filesys_setpos(auint ch, auint pos)
{
 filesys_ch[ch].pos = pos;
}



/*
** Flushes a channel. It internally closes any opened file, safely flushing
** them as needed.
*/
void  filesys_flush(auint ch)
{
}



/*
** Flushes all channels. Should be used before exit.
*/
void  filesys_flushall(void)
{
}



/*
** Resets file finder to start a new search.
*/
void  filesys_find_reset(void)
{
}



/*
** Finds next file. Returns the found file's size and name in dest. The return
** is 0xFFFFFFFF on the end of the directory. It will skip any directory or
** file whose size is equal or larger than 0xFFFFFFFF bytes or whose name is
** too long to fit in dest (by len). The returned name is relative to the path
** (so it has no path component, only a bare file name).
*/
auint filesys_find_next(char* dest, auint len)
{
 return 0xFFFFFFFFU;
}



/*
** Terminates a file find operation. It is not necessary to call it if using
** filesys_find_next() until it returns an empty string.
*/
void  filesys_find_end(void)
{
}



/*
** Returns the given file's size in bytes. If the file doesn't exist, its size
** is out of the 32 bit range or it is a directory, it returns 0xFFFFFFFF.
*/
auint filesys_getsize(char const* name)
{
 return gamefile_size;
}
