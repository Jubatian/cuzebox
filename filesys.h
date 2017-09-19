/*
 *  Filesystem interface layer
 *
 *  Copyright (C) 2016 - 2017
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



#ifndef FILESYS_H
#define FILESYS_H



#include "types.h"



/*
** The emulator uses this layer to communicate with the underlaying system's
** files. This is provided to make it possible to entirely replace the
** filesystem in need, such as to provide a self-contained package (planned
** mainly for Emscripten use)
*/


/*
** Filesystem channels. These are fixed, statically allocated to various
** parts of the emulator. Each channel can hold one file open at any time.
*/
/* SD card emulation */
#define FILESYS_CH_SD      0U
/* Various emulator one-shot tasks (such as game loading) */
#define FILESYS_CH_EMU     1U
/* EEPROM & Code ROM dumps */
#define FILESYS_CH_ROM     2U

/* Number of filesystem channels (must be one larger than the largest entry
** of the list above) */
#define FILESYS_CH_NO      3U


/*
** Sets path string. All filesystem operations will be carried out on the
** set path. If there is a file name on the end of the path, it is removed
** from it (bare paths must terminate with a '/'). If name is non-null, the
** filename part of the path (if any) is loaded into it (up to len bytes).
*/
void  filesys_setpath(char const* path, char* name, auint len);


/*
** Opens a file. Position is at the beginning. Returns TRUE on success.
** Initially the file internally is opened for reading (so write protected
** files may also be opened), adding write access is only attempted when
** trying to write the file. If there is already a file open, it is closed
** first.
*/
boole filesys_open(auint ch, char const* name);


/*
** Read bytes from a file. The reading increases the internal position.
** Returns the number of bytes read, which may be zero if no file is open
** on the channel or it can not be read (such as because the position is at
** or beyond the end). This automatically reopens the last opened file if
** necessary, seeking to the last set position.
*/
auint filesys_read(auint ch, uint8* dest, auint len);


/*
** Writes bytes to a file. The writing increases the internal position.
** Returns the number of bytes written, which may be zero if writing is not
** possible for any reason. This automatically reopens the file for writing.
*/
auint filesys_write(auint ch, uint8 const* src, auint len);


/*
** Sets file position. It may be set beyond the end of the actual file. This
** case a write access will attempt to pad the file with zeroes until the
** requested position.
*/
void  filesys_setpos(auint ch, auint pos);


/*
** Flushes a channel. It internally closes any opened file, safely flushing
** them as needed.
*/
void  filesys_flush(auint ch);


/*
** Flushes all channels. Should be used before exit.
*/
void  filesys_flushall(void);


/*
** Resets file finder to start a new search.
*/
void  filesys_find_reset(void);


/*
** Finds next file. Returns the found file's size and name in dest. The return
** is 0xFFFFFFFF on the end of the directory. It will skip any directory or
** file whose size is equal or larger than 0xFFFFFFFF bytes or whose name is
** too long to fit in dest (by len). The returned name is relative to the path
** (so it has no path component, only a bare file name).
*/
auint filesys_find_next(char* dest, auint len);


/*
** Terminates a file find operation. It is not necessary to call it if using
** filesys_find_next() until it returns an empty string.
*/
void  filesys_find_end(void);


/*
** Returns the given file's size in bytes. If the file doesn't exist, its size
** is out of the 32 bit range or it is a directory, it returns 0xFFFFFFFF.
*/
auint filesys_getsize(char const* name);


#endif
