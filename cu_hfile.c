/*
 *  Hex file parser
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



/* String constants */
static const char* cu_id  = "Hex parser: ";
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
boole cu_hfile_load(char const* fname, uint8* cmem)
{
 FILE* fp;
 uint8 buf[128];
 uint8 dec[64];
 auint lpos = 0U; /* Input file line counter */
 auint cpos = 0U; /* Code memory pointer */
 auint rp;
 auint wp;
 auint lh;
 auint dv;
 boole eflg = FALSE;

 fp = fopen(fname, "rb");
 if (fp == NULL){
  fprintf(stderr, "%s%sCouldn't open %s.\n", cu_id, cu_err, fname);
  goto ex_none;
 }

 while (TRUE){

  if (feof(fp)){ eflg = TRUE; }
  if (fgets((char*)(&(buf[0])), 128U, fp) == NULL){ eflg = TRUE; }
  if (eflg){
   fprintf(stderr, "%s%sEOF without end marker on line %u in %s.\n", cu_id, cu_war, lpos, fname);
   goto ex_ok;
  }

  if ( (buf[0] != (uint8)(':')) &&
       (buf[0] != (uint8)('\n')) &&
       (buf[0] != (uint8)('\r')) &&
       (buf[0] != 0U) ){
   fprintf(stderr, "%s%sInvalid content on line %u in %s.\n", cu_id, cu_err, lpos, fname);
   goto ex_file;
  }

  if (buf[0] == (uint8)(':')){ /* A possibly valid hex line */

   /* Decode the line into binary */

   rp = 1U;
   wp = 0U;
   lh = 0U;
   dv = 0U;
   while (buf[rp] > (uint8)(' ')){
    dv <<= 4U;
    if       ( (buf[rp] >= (uint8)('0')) &&
               (buf[rp] <= (uint8)('9')) ){
     dv |= (buf[rp] - (uint8)('0'));
    }else if ( (buf[rp] >= (uint8)('a')) &&
               (buf[rp] <= (uint8)('f')) ){
     dv |= (buf[rp] - (uint8)('a') + 10U);
    }else if ( (buf[rp] >= (uint8)('A')) &&
               (buf[rp] <= (uint8)('F')) ){
     dv |= (buf[rp] - (uint8)('A') + 10U);
    }else{
     fprintf(stderr, "%s%sInvalid character on line %u in %s.\n", cu_id, cu_err, lpos, fname);
     goto ex_file;
    }
    lh ^= 1U;
    if (lh == 0U){
     dec[wp] = dv;
     dv = 0U;
     wp ++;
    }
    rp ++;
   }
   if (lh != 0U){
    fprintf(stderr, "%s%sOdd count of hex characters on line %u in %s.\n", cu_id, cu_err, lpos, fname);
    goto ex_file;
   }

   /* Verify checksum: Simply the line must add up to zero */

   dv = 0U;
   for (rp = 0U; rp < wp; rp ++){
    dv += dec[rp];
   }
   if ((dv & 0xFFU) != 0U){
    fprintf(stderr, "%s%sBad checksum on line %u in %s.\n", cu_id, cu_err, lpos, fname);
    goto ex_file;
   }

   /* Verify data size */

   if (wp < 5U){
    fprintf(stderr, "%s%sToo short line %u in %s.\n", cu_id, cu_err, lpos, fname);
    goto ex_file;
   }
   if (dec[0] != (wp - 5U)){
    fprintf(stderr, "%s%sBad data count on line %u in %s.\n", cu_id, cu_err, lpos, fname);
    goto ex_file;
   }

   /* Parse the line */

   switch (dec[3]){

    case 0x00U: /* Data */
     cpos = (cpos & 0xFFFF0000U) |
            ((auint)(dec[1]) << 8) |
            ((auint)(dec[2])     );
     if (cpos > (0x10000U - (auint)(dec[0]))){
      fprintf(stderr, "%s%sToo big address on line %u in %s.\n", cu_id, cu_err, lpos, fname);
      goto ex_file;
     }
     for (rp = 0U; rp < dec[0]; rp ++){
      cmem[cpos + rp] = dec[rp + 4U];
     }
     break;

    case 0x01U: /* End of file */
     goto ex_ok;
     break;

    case 0x04U: /* Extended linear address */
     if (dec[0] != 2U){
      fprintf(stderr, "%s%sInvalid Ext. linear address on line %u in %s.\n", cu_id, cu_err, lpos, fname);
      goto ex_file;
     }
     cpos = ((auint)(dec[4]) << 8) |
            ((auint)(dec[5])     );
     break;

    default:    /* Invalid for AVR programming */
     fprintf(stderr, "%s%sInvalid record type on line %u in %s.\n", cu_id, cu_err, lpos, fname);
     goto ex_file;
     break;

   }

  }

  lpos ++; /* Input file line counter */

 }

 /* Correct exit */

ex_ok:

 fclose(fp);

 /* Print out successful result */

 fprintf(stdout, "%sSuccesfully loaded program from %s\n", cu_id, fname);

 /* Successful */

 return TRUE;

 /* Failing exits */

ex_file:
 fclose(fp);

ex_none:
 return FALSE;
}