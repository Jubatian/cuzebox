/*
 *  AVR flag generator
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



#include "cu_avrfg.h"


/* Flags in CU_IO_SREG */
#define SREG_H  5U
#define SREG_S  4U
#define SREG_V  3U
#define SREG_N  2U
#define SREG_Z  1U
#define SREG_C  0U
#define SREG_HM 0x20U
#define SREG_SM 0x10U
#define SREG_VM 0x08U
#define SREG_NM 0x04U
#define SREG_ZM 0x02U
#define SREG_CM 0x01U


/* Macros for managing the flags */

/* Set flags by mask */
#define SREG_SET(fl, xm) (fl |= (xm))

/* Set Zero if (at most) 16 bit "val" is zero */
#define SREG_SET_Z(fl, val) (fl |= SREG_ZM & (((auint)(val) - 1U) >> 16))

/* Set Carry by bit 0 (for right shifts) */
#define SREG_SET_C_BIT0(fl, val) (fl |= (val) & 1U)

/* Set Carry by bit 8 (for various adds & subs) */
#define SREG_SET_C_BIT8(fl, val) (fl |= ((val) >> 8) & 1U)

/* Set Half-Carry by bit 4 (for various adds & subs) */
#define SREG_SET_H_BIT4(fl, val) (fl |= ((val) << 1) & 0x20U)

/* Set Negative by bit 7 (for all ops updating N) */
#define SREG_SET_N_BIT7(fl, val) (fl |= ((val) >> 5) & 0x04U)

/* Set Overflow by bit 7 (for various adds & subs) */
#define SREG_SET_V_BIT7(fl, val) (fl |= ((val) >> 4) & 0x08U)

/* Set Overflow if value is 0x80U (for inc) */
#define SREG_SET_V_80(fl, val) (fl |= SREG_VM & (((auint)(((val) - 0x80U) & 0xFFU) - 1U) >> 16))

/* Set Overflow if value is 0x7FU (for dec) */
#define SREG_SET_V_7F(fl, val) (fl |= SREG_VM & (((auint)(((val) - 0x7FU) & 0xFFU) - 1U) >> 16))

/* Set S by bit 7 (used where it is more efficient than SREG_COM_NV) */
#define SREG_SET_S_BIT7(fl, val) (fl |= ((val) >> 3) & 0x10U)

/* Combine N and V into S (for all ops updating N or V) */
#define SREG_COM_NV(fl) (fl |= (((fl) << 1) ^ ((fl) << 2)) & 0x10U)

/* Set N, V and S for logical ops (where only N depends on the result's bit 7) */
#define SREG_LOG_NVS(fl, val) (fl |= ((0x7FU - (auint)(val)) >> 8) & (SREG_SM | SREG_NM))


/* Macro for generating an addition's flags */
#define GENFLAGS_ADD(fl, dst, src, res, tmp) \
 do{ \
  tmp = (src) ^ (dst); \
  SREG_SET_H_BIT4(fl, (tmp) ^ (res)); \
  tmp = (~tmp) & ((res) ^ (dst)); \
  SREG_SET_C_BIT8(fl, (res)); \
  SREG_SET_N_BIT7(fl, (res)); \
  SREG_SET_V_BIT7(fl, (tmp)); \
  SREG_SET_S_BIT7(fl, (tmp) ^ (res)); \
  SREG_SET_Z(fl, (res) & 0xFFU); \
 }while(0)

/* Macro for generating a subtraction's flags */
#define GENFLAGS_SUB(fl, dst, src, res, tmp) \
 do{ \
  tmp = (src) ^ (dst); \
  SREG_SET_H_BIT4(fl, (tmp) ^ (res)); \
  tmp = ( tmp) & ((res) ^ (dst)); \
  SREG_SET_C_BIT8(fl, (res)); \
  SREG_SET_N_BIT7(fl, (res)); \
  SREG_SET_V_BIT7(fl, (tmp)); \
  SREG_SET_S_BIT7(fl, (tmp) ^ (res)); \
  SREG_SET_Z(fl, (res) & 0xFFU); \
 }while(0)

/* Macro for generating a right shift's flags */
#define GENFLAGS_SHR(fl, src, res, tmp) \
 do{ \
  SREG_SET_C_BIT0(fl, (src)); \
  tmp = ((src) << 7) ^ (res); \
  SREG_SET_V_BIT7(fl, (tmp)); \
  SREG_SET_N_BIT7(fl, (res)); \
  SREG_SET_S_BIT7(fl, (tmp) ^ (res)); \
  SREG_SET_Z(fl, (res)); \
 }while(0)

/* Macro for generating a logical op's flags */
#define GENFLAGS_LOG(fl, res) \
 do{ \
  SREG_LOG_NVS(fl, res); \
  SREG_SET_Z(fl, res); \
 }while(0)

/* Macro for generating an INC's flags */
#define GENFLAGS_INC(fl, res) \
 do{ \
  SREG_SET_V_80(fl, res); \
  SREG_SET_N_BIT7(fl, res); \
  SREG_COM_NV(fl); \
  SREG_SET_Z(fl, (res) & 0xFFU); \
 }while(0)

/* Macro for generating a DEC's flags */
#define GENFLAGS_DEC(fl, res) \
 do{ \
  SREG_SET_V_7F(fl, res); \
  SREG_SET_N_BIT7(fl, res); \
  SREG_COM_NV(fl); \
  SREG_SET_Z(fl, (res) & 0xFFU); \
 }while(0)



/*
** Fills up the flag precalc table
*/
void cu_avrfg_fill(uint8* ftable)
{
 auint fl;
 auint cy;
 auint src;
 auint dst;
 auint res;
 auint tmp;

 /*
 ** Add / Sub flag generation:
 **
 ** table[((res & 0x1FFU)     ) |  <- -------X XXXXXXXX
 **       ((src &  0x90U) << 5) |  <- ---X--X- --------
 **       ((dst &  0x90U) << 6)]   <- --X--X-- --------
 */

 for (cy = 0U; cy < 2U; cy ++){
  for (src = 0U; src < 256U; src ++){
   for (dst = 0U; dst < 256U; dst ++){
    fl = 0U;
    res = dst + (src + cy);
    GENFLAGS_ADD(fl, dst, src, res, tmp);
    ftable[CU_AVRFG_ADD + (res & 0x1FFU) + ((src & 0x90U) << 5) + ((dst & 0x90U) << 6)] = fl;
   }
  }
 }

 for (cy = 0U; cy < 2U; cy ++){
  for (src = 0U; src < 256U; src ++){
   for (dst = 0U; dst < 256U; dst ++){
    fl = 0U;
    res = dst - (src + cy);
    GENFLAGS_SUB(fl, dst, src, res, tmp);
    ftable[CU_AVRFG_SUB + (res & 0x1FFU) + ((src & 0x90U) << 5) + ((dst & 0x90U) << 6)] = fl;
   }
  }
 }

 for (src = 0U; src < 2U; src ++){
  for (res = 0U; res < 256U; res ++){
   fl = 0U;
   GENFLAGS_SHR(fl, src, res, tmp);
   ftable[CU_AVRFG_SHR + ((src << 8) + res)] = fl;
  }
 }

 for (res = 0U; res < 256U; res ++){
  fl = 0U;
  GENFLAGS_LOG(fl, res);
  ftable[CU_AVRFG_LOG + res] = fl;
 }

 for (res = 0U; res < 256U; res ++){
  fl = 0U;
  GENFLAGS_INC(fl, res);
  ftable[CU_AVRFG_INC + res] = fl;
 }

 for (res = 0U; res < 256U; res ++){
  fl = 0U;
  GENFLAGS_DEC(fl, res);
  ftable[CU_AVRFG_DEC + res] = fl;
 }

}
