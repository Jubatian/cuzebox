/*
 *  UzeRom file parser
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



#include "cu_ufile.h"
#include <stdio.h>
#include <stdlib.h>



/* UzeRom header magic value */
static const uint8* cu_magic = (const uint8*)("UZEBOX");


/* String constants */
static const char* cu_id  = "UzeRom parser: ";
static const char* cu_err = "Error: ";
static const char* cu_war = "Warning: ";



/*
** Attempts to load the passed file into code memory. The code memory is not
** cleared, so bootloader image may be added before this if there is any.
**
** The code memory must be 64 KBytes.
**
** Returns TRUE if the loading was successful.
*/
boole cu_ufile_load(char const* fname, uint8* cmem, cu_ufile_header_t* head)
{
 FILE* fp;
 asint rv;
 uint8 buf[512];
 auint i;
 auint len;

 fp = fopen(fname, "rb");
 if (fp == NULL){
  fprintf(stderr, "%s%sCouldn't open %s.\n", cu_id, cu_err, fname);
  goto ex_none;
 }

 rv = fread(buf, 1U, 512U, fp);
 if (rv != 512){
  fprintf(stderr, "%s%sNot enough data for header in %s.\n", cu_id, cu_err, fname);
  goto ex_file;
 }

 for (i = 0U; i < 6U; i++){
  if (buf[i] != cu_magic[i]){
   fprintf(stderr, "%s%s%s is not a UzeRom file.\n", cu_id, cu_err, fname);
   goto ex_file;
  }
 }

 if (buf[6] != 1U){
  fprintf(stderr, "%s%s%s has unknown version (%d).\n", cu_id, cu_war, fname, buf[6]);
 }
 head->version = buf[6];

 if (buf[7] != 0U){
  fprintf(stderr, "%s%s%s has unknown target UC (%d).\n", cu_id, cu_err, fname, buf[7]);
  goto ex_file;
 }
 head->target = buf[7];

 len = ((auint)(buf[ 8])      ) +
       ((auint)(buf[ 9]) <<  8) +
       ((auint)(buf[10]) << 16) +
       ((auint)(buf[11]) << 24);
 if (len > 65535U){
  fprintf(stderr, "%s%s%s has too large program size (%d).\n", cu_id, cu_err, fname, len);
  goto ex_file;
 }
 head->pmemsize = len;

 head->year = ((auint)(buf[12])      ) +
              ((auint)(buf[13]) <<  8);

 for (i = 0U; i < 31U; i++){ head->name[i] = buf[14U + i]; }
 head->name[31] = 0U;

 for (i = 0U; i < 31U; i++){ head->author[i] = buf[46U + i]; }
 head->author[31] = 0U;

 for (i = 0U; i < 256U; i++){ head->icon[i] = buf[78U + i]; }

 head->crc32 = ((auint)(buf[334])      ) +
               ((auint)(buf[335]) <<  8) +
               ((auint)(buf[336]) << 16) +
               ((auint)(buf[337]) << 24);

 head->mouse = buf[338];

 for (i = 0U; i < 63U; i++){ head->desc[i] = buf[339U + i]; }
 head->desc[63] = 0U;

 rv = fread(cmem, 1U, len, fp);
 if (rv != len){
  fprintf(stderr, "%s%sNot enough data for program in %s.\n", cu_id, cu_err, fname);
  goto ex_file;
 }

 fclose(fp);

 /* Print out successful result */

 fprintf(stdout,
  "%sSuccesfully loaded program from %s:\n", cu_id, fname);
 fprintf(stdout,
  "\tName .......: %s\n"
  "\tAuthor .....: %s\n"
  "\tYear .......: %d\n"
  "\tDescription : %s\n",
  (char*)(&(head->name[0])),
  (char*)(&(head->author[0])),
  head->year,
  (char*)(&(head->desc[0])));

 /* Successful */

 return TRUE;

 /* Failing exits */

ex_file:
 fclose(fp);

ex_none:
 return FALSE;
}
