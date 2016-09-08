/*
 *  AVR instruction set compiler
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



#include "cu_avrc.h"


/* Undefined instruction */
#define UNDEF 0x4AU


/*
** Compiles an instruction by its two words (second word only used if two word
** instruction), and returns its 32 bit opcode.
*/
auint cu_avrc_compile(auint word0, auint word1)
{
 auint ret;

 switch (word0 >> 10){

  case 0x00U:

   switch ((word0 >> 8) & 0x3U){

    case 0x00U: /* NOP */

     if ((word0 & 0xFFU) == 0x00U){ return 0x00000000U; } /* NOP */
     else                         { return UNDEF; }
     break;

    case 0x01U: /* MOVW */

     ret  = 0x01U;
     ret |= ((word0 >> 4) & 0xFU) <<  9; /* Ar1 (Destination) */
     ret |= ((word0     ) & 0xFU) << 17; /* Ar2 (Source) */
     return ret;
     break;

    case 0x02U: /* MULS */

     ret  = 0x02U;
     ret |= ((word0 >> 4) & 0xFU) <<  8; /* Ar1 (Destination) */
     ret |= ((auint)(0x10U))      <<  8; /* Ar1 selects r16 - r31 */
     ret |= ((word0     ) & 0xFU) << 16; /* Ar2 (Source) */
     ret |= ((auint)(0x10U))      << 16; /* Ar2 selects r16 - r31 */
     return ret;
     break;

    default:    /* MULSU, FMUL, FMULS, FMULSU */

     ret  = 0x03U + ((word0 >> 3) & 1U) + ((word0 >> 6) & 2U);
     ret |= ((word0 >> 4) & 0x7U) <<  8; /* Ar1 (Destination) */
     ret |= ((auint)(0x10U))      <<  8; /* Ar1 selects r16 - r23 */
     ret |= ((word0     ) & 0x7U) << 16; /* Ar2 (Source) */
     ret |= ((auint)(0x10U))      << 16; /* Ar2 selects r16 - r23 */
     return ret;
     break;

   }
   break;

  case 0x01U: /* CPC */
  case 0x02U: /* SBC */
  case 0x03U: /* ADD */
  case 0x04U: /* CPSE */
  case 0x05U: /* CP */
  case 0x06U: /* SUB */
  case 0x07U: /* ADC */
  case 0x08U: /* AND */
  case 0x09U: /* EOR */
  case 0x0AU: /* OR */
  case 0x0BU: /* MOV */

   ret  = 0x07U + ((word0 >> 10) - 1U);
   ret |= ((word0 >> 4) & 0x1FU) <<  8; /* Ar1 (Destination) */
   ret |= ((word0     ) & 0x0FU) << 16; /* Ar2 (Source) */
   ret |= ((word0 >> 9) & 0x01U) << 20; /* Ar2 (Source), high bit */
   return ret;
   break;

  case 0x0CU: /* CPI */
  case 0x0DU:
  case 0x0EU:
  case 0x0FU:
  case 0x10U: /* SBCI */
  case 0x11U:
  case 0x12U:
  case 0x13U:
  case 0x14U: /* SUBI */
  case 0x15U:
  case 0x16U:
  case 0x17U:
  case 0x18U: /* ORI */
  case 0x19U:
  case 0x1AU:
  case 0x1BU:
  case 0x1CU: /* ANDI */
  case 0x1DU:
  case 0x1EU:
  case 0x1FU:

   ret  = 0x12U + ((word0 >> 12) - 3U);
   ret |= ((word0 >> 4) & 0xFU) <<  8; /* Ar1 (Destination) */
   ret |= ((auint)(0x10U))      <<  8; /* Ar1 selects r16 - r31 */
   ret |= ((word0     ) & 0xFU) << 16; /* Ar2 (Source), low 4 bits */
   ret |= ((word0 >> 8) & 0xFU) << 20; /* Ar2 (Source), high 4 bits */
   return ret;
   break;

  case 0x20U: /* LD with displacement, ST with displacement */
  case 0x21U:
  case 0x22U:
  case 0x23U:
  case 0x28U:
  case 0x29U:
  case 0x2AU:
  case 0x2BU:

   ret  = ((word0 >>  4) & 0x1FU) <<  8; /* Ar1 (Register operand) */
   ret |= ((((word0 >> 2) & 2U) ^ 2U) + 28U) << 16; /* Ar2 (Pointer operand: Y or Z) */
   ret |= ((word0      ) & 0x07U) << 24; /* Displacement */
   ret |= ((word0 >> 10) & 0x03U) << 27;
   ret |= ((word0 >> 13) & 0x01U) << 29;

   if ((word0 & 0x0200U) == 0U){ ret |= 0x21U; } /* LD */
   else                        { ret |= 0x1DU; } /* ST */

   return ret;
   break;

  case 0x24U: /* A variety of load / store instructions in this group */

   ret  = ((word0 >>  4) & 0x1FU) <<  8; /* Ar1 (Register operand) */

   if ((word0 & 0x0200U) == 0U){ /* Loads */

    switch (word0 & 0xFU){

     case 0x0U: /* LDS */

      ret |= 0x20U;
      ret |= 0x80U; /* 2 word instruction */
      ret |= word1 << 16;
      return ret;
      break;

     case 0x1U: /* LD Ar1(Reg), Z+ */

      ret |= 0x23U;
      ret |= ((auint)(30U)) << 16; /* Ar2 (Pointer operand: Z) */
      return ret;
      break;

     case 0x2U: /* LD Ar1(Reg), Z- */

      ret |= 0x22U;
      ret |= ((auint)(30U)) << 16; /* Ar2 (Pointer operand: Z) */
      return ret;
      break;

     case 0x4U: /* LPM Ar1(Reg), Z */

      ret |= 0x18U;
      return ret;
      break;

     case 0x5U: /* LPM Ar1(Reg), Z+ */

      ret |= 0x19U;
      return ret;
      break;

     case 0x9U: /* LD Ar1(Reg), Y+ */

      ret |= 0x23U;
      ret |= ((auint)(28U)) << 16; /* Ar2 (Pointer operand: Y) */
      return ret;
      break;

     case 0xAU: /* LD Ar1(Reg), Y- */

      ret |= 0x22U;
      ret |= ((auint)(28U)) << 16; /* Ar2 (Pointer operand: Y) */
      return ret;
      break;

     case 0xCU: /* LD Ar1(Reg), X */

      ret |= 0x21U;
      ret |= ((auint)(26U)) << 16; /* Ar2 (Pointer operand: X) */
      return ret;
      break;

     case 0xDU: /* LD Ar1(Reg), X+ */

      ret |= 0x23U;
      ret |= ((auint)(26U)) << 16; /* Ar2 (Pointer operand: X) */
      return ret;
      break;

     case 0xEU: /* LD Ar1(Reg), X- */

      ret |= 0x22U;
      ret |= ((auint)(26U)) << 16; /* Ar2 (Pointer operand: X) */
      return ret;
      break;

     case 0xFU: /* POP */

      ret |= 0x1BU;
      return ret;
      break;

     default:   /* Assume undefined */

      return UNDEF;
      break;

    }

   }else{       /* Stores */

    switch (word0 & 0xFU){

     case 0x0U: /* STS */

      ret |= 0x1CU;
      ret |= 0x80U; /* 2 word instruction */
      ret |= word1 << 16;
      return ret;
      break;

     case 0x1U: /* ST Z+, Ar1(Reg) */

      ret |= 0x1FU;
      ret |= ((auint)(30U)) << 16; /* Ar2 (Pointer operand: Z) */
      return ret;
      break;

     case 0x2U: /* ST Z-, Ar1(Reg) */

      ret |= 0x1EU;
      ret |= ((auint)(30U)) << 16; /* Ar2 (Pointer operand: Z) */
      return ret;
      break;

     case 0x9U: /* ST Y+, Ar1(Reg) */

      ret |= 0x1FU;
      ret |= ((auint)(28U)) << 16; /* Ar2 (Pointer operand: Y) */
      return ret;
      break;

     case 0xAU: /* ST Y-, Ar1(Reg) */

      ret |= 0x1EU;
      ret |= ((auint)(28U)) << 16; /* Ar2 (Pointer operand: Y) */
      return ret;
      break;

     case 0xCU: /* ST X, Ar1(Reg) */

      ret |= 0x1DU;
      ret |= ((auint)(26U)) << 16; /* Ar2 (Pointer operand: X) */
      return ret;
      break;

     case 0xDU: /* ST X+, Ar1(Reg) */

      ret |= 0x1FU;
      ret |= ((auint)(26U)) << 16; /* Ar2 (Pointer operand: X) */
      return ret;
      break;

     case 0xEU: /* ST X-, Ar1(Reg) */

      ret |= 0x1EU;
      ret |= ((auint)(26U)) << 16; /* Ar2 (Pointer operand: X) */
      return ret;
      break;

     case 0xFU: /* PUSH */

      ret |= 0x1AU;
      return ret;
      break;

     default:   /* Assume undefined */

      return UNDEF;
      break;

    }

   }
   break;

  case 0x25U: /* Various instructions */

   if ((word0 & 0x0200U) == 0U){

    ret  = ((word0 >>  4) & 0x1FU) <<  8; /* Ar1 (Register operand), for most of these */

    switch (word0 & 0xFU){

     case 0x0U: /* COM */
     case 0x1U: /* NEG */
     case 0x2U: /* SWAP */
     case 0x3U: /* INC */

      ret |= 0x24U + (word0 & 0xFU);
      return ret;
      break;

     case 0x5U: /* ASR */
     case 0x6U: /* LSR */
     case 0x7U: /* ROR */

      ret |= 0x28U + ((word0 & 0xFU) - 0x5U);
      return ret;
      break;

     case 0x8U: /* BSET, BCLR, various */

      if ((word0 & 0x0100U) == 0U){ /* BSET, BCLR */

       ret  = ((auint)(1U) << ((word0 >> 4) & 0x7U)) << 8; /* Ar1 (Mask) */
       ret |= 0x2EU + ((word0 >> 7) & 1U);
       return ret;

      }else{ /* Various */

       switch ((word0 >> 4) & 0xFU){

        case 0x0U: /* RET */

         ret  = 0x31U;
         return ret;
         break;

        case 0x1U: /* RETI */

         ret  = 0x33U;
         return ret;
         break;

        case 0x8U: /* SLEEP */
        case 0x9U: /* BREAK */
        case 0xAU: /* WDR */

         ret  = 0x34U + (((word0 >> 4) & 0xFU) - 0x8U);
         return ret;
         break;

        case 0xCU: /* LPM (r0) */

         ret  = 0x18U;
         return ret;
         break;

        case 0xEU: /* SPM */

         ret  = 0x17U;
         return ret;
         break;

        default:   /* Assume undefined */

         return UNDEF;
         break;

       }

      }
      break;

     case 0x9U: /* ICALL, IJMP */

      ret  = 0x30U + ((word0 >> 7) & 0x2U);
      return ret;
      break;

     case 0xAU: /* DEC */

      ret |= 0x2BU;
      return ret;
      break;

     case 0xCU: /* JMP */
     case 0xDU:
     case 0xEU: /* CALL */
     case 0xFU:

      ret  = 0x2CU + ((word0 >> 1) & 1U);
      ret |= 0x80U; /* 2 word instruction */
      ret |= word1 << 16;
      return ret;
      break;

     default:   /* Assume undefined */

      return UNDEF;
      break;

    }

   }else{ /* ADIW, SBIW */

    ret  = 0x3AU + ((word0 >> 8) & 1U);
    ret |= ((word0 >> 4) & 0x3U) <<  9; /* Ar1 (Destination) */
    ret |= ((auint)(0x18U))      <<  8; /* Ar1 selects r24 - r31 */
    ret |= ((word0     ) & 0xFU) << 16; /* Ar2 (Immediate) */
    ret |= ((word0 >> 6) & 0x3U) << 20;
    return ret;
    break;

   }

  case 0x26U: /* CBI, SBIC, SBI, SBIS */

   ret  = (((word0 >> 3) & 0x1FU) + 0x20U) << 8; /* Ar1 (Absolute I/O offset, 0x20 based) */
   ret |= 0x3CU + ((word0 >> 8) & 3U);
   ret |= ((auint)(1U) << (word0 & 0x7U)) << 16; /* Ar2 (Mask) */
   return ret;
   break;

  case 0x27U: /* MUL */

   ret  = 0x37U;
   ret |= ((word0 >> 4) & 0x1FU) <<  8; /* Ar1 (Destination) */
   ret |= ((word0     ) & 0x0FU) << 16; /* Ar2 (Source) */
   ret |= ((word0 >> 9) & 0x01U) << 20; /* Ar2 (Source), high bit */
   return ret;
   break;

  case 0x2CU: /* IN */
  case 0x2DU:

   ret  = (((word0     ) & 0x0FU) +
           ((word0 >> 5) & 0x30U) + 0x20U) << 16; /* Ar2 (Absolute I/O offset, 0x20 based) */
   ret |= 0x38U;
   ret |= ((word0 >> 4) & 0x1FU) <<  8; /* Ar1 (Destination) */
   return ret;
   break;

  case 0x2EU: /* OUT */
  case 0x2FU:

   ret  = (((word0     ) & 0x0FU) +
           ((word0 >> 5) & 0x30U) + 0x20U) <<  8; /* Ar1 (Absolute I/O offset, 0x20 based) */
   if (ret == (auint)(CU_IO_PORTC) << 8){ /* PIXEL */
    ret  = 0x49U;
   }else{                                 /* Normal OUT */
    ret |= 0x39U;
   }
   ret |= ((word0 >> 4) & 0x1FU) << 16; /* Ar2 (Source) */
   return ret;
   break;

  case 0x30U: /* RJMP */
  case 0x31U:
  case 0x32U:
  case 0x33U:
  case 0x34U: /* RCALL */
  case 0x35U:
  case 0x36U:
  case 0x37U:

   ret  = 0x40U + ((word0 >> 12) & 1U);
   ret |= (word0 & 0xFFFU) << 16; /* Ar2 (Relative target) */
   if ((word0 & 0x800U) != 0U){ ret |= 0xF0000000U; }
   return ret;
   break;

  case 0x38U: /* LDI */
  case 0x39U:
  case 0x3AU:
  case 0x3BU:

   ret  = 0x48U;
   ret |= ((word0 >> 4) & 0xFU) <<  8; /* Ar1 (Destination) */
   ret |= ((auint)(0x10U))      <<  8; /* Ar1 selects r16 - r31 */
   ret |= ((word0     ) & 0xFU) << 16; /* Ar2 (Source), low 4 bits */
   ret |= ((word0 >> 8) & 0xFU) << 20; /* Ar2 (Source), high 4 bits */
   return ret;
   break;

  case 0x3CU: /* BRBS */
  case 0x3DU: /* BRBC */

   ret  = 0x42U + ((word0 >> 10) & 1U);
   ret |= ((auint)(1U) << (word0 & 0x7U)) << 8; /* Ar1 (Mask) */
   ret |= ((word0 >> 3) & 0x7FU) << 16;         /* Ar2 (Relative target) */
   if ((word0 & 0x200U) != 0U){ ret |= 0xFF800000U; }
   return ret;
   break;

  case 0x3EU: /* BLD, BST */

   ret  = 0x44U + ((word0 >> 9) & 1U);
   ret |= ((word0 >> 4) & 0x1FU) <<  8; /* Ar1 (Register operand) */
   ret |= (word0 & 0x7U) << 16;         /* Ar2 (Bit select) */
   return ret;
   break;

  case 0x3FU: /* SBRC, SBRS */

   ret  = 0x46U + ((word0 >> 9) & 1U);
   ret |= ((word0 >> 4) & 0x1FU) <<  8;          /* Ar1 (Register operand) */
   ret |= ((auint)(1U) << (word0 & 0x7U)) << 16; /* Ar2 (Mask) */
   return ret;
   break;

  default:    /* Assume undefined */

   return UNDEF;
   break;

 }

 return UNDEF;
}
