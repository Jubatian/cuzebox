/*
 *  AVR instruction deflagger
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



#include "cu_avrdf.h"



/* Maximal depth for searching proof of non-use */
#define MAX_DEPTH  10U

/* Flag specifiers (Matching the SREG's layout) */
#define FLAG_C     0x01U
#define FLAG_Z     0x02U
#define FLAG_N     0x04U
#define FLAG_V     0x08U
#define FLAG_S     0x10U
#define FLAG_H     0x20U
/* Specifier indicating that Ar1 has to be used */
#define FLAG_P     0x80U

/* Instruction types */
/* Normal sequential instruction */
#define INS_SEQ    0x00U
/* Absolute jump or call (to Ar2) */
#define INS_AJMP   0x01U
/* Relative jump or call (to Ar2) */
#define INS_RJMP   0x02U
/* Conditional relative jump (to Ar2) */
#define INS_CRJMP  0x03U
/* Conditional skip */
#define INS_CSKIP  0x04U



/* Flag writes by each instruction (128). This indicates that no previous
** state of the flags survive (so it doesn't involve Z updates). */
static const uint8 flag_w[128]={
 /* 0x00: NOP    */ 0U,
 /* 0x01: MOVW   */ 0U,
 /* 0x02: MULS   */ FLAG_C | FLAG_Z,
 /* 0x03: MULSU  */ FLAG_C | FLAG_Z,
 /* 0x04: FMUL   */ FLAG_C | FLAG_Z,
 /* 0x05: FMULS  */ FLAG_C | FLAG_Z,
 /* 0x06: FMULSU */ FLAG_C | FLAG_Z,
 /* 0x07: CPC    */ FLAG_C | FLAG_N | FLAG_V | FLAG_H | FLAG_S,
 /* 0x08: SBC    */ FLAG_C | FLAG_N | FLAG_V | FLAG_H | FLAG_S,
 /* 0x09: ADD    */ FLAG_C | FLAG_Z | FLAG_N | FLAG_V | FLAG_H | FLAG_S,
 /* 0x0A: CPSE   */ 0U,
 /* 0x0B: CP     */ FLAG_C | FLAG_Z | FLAG_N | FLAG_V | FLAG_H | FLAG_S,
 /* 0x0C: SUB    */ FLAG_C | FLAG_Z | FLAG_N | FLAG_V | FLAG_H | FLAG_S,
 /* 0x0D: ADC    */ FLAG_C | FLAG_Z | FLAG_N | FLAG_V | FLAG_H | FLAG_S,
 /* 0x0E: AND    */ FLAG_Z | FLAG_N | FLAG_V | FLAG_S,
 /* 0x0F: EOR    */ FLAG_Z | FLAG_N | FLAG_V | FLAG_S,
 /* 0x10: OR     */ FLAG_Z | FLAG_N | FLAG_V | FLAG_S,
 /* 0x11: MOV    */ 0U,
 /* 0x12: CPI    */ FLAG_C | FLAG_Z | FLAG_N | FLAG_V | FLAG_H | FLAG_S,
 /* 0x13: SBCI   */ FLAG_C | FLAG_N | FLAG_V | FLAG_H | FLAG_S,
 /* 0x14: SUBI   */ FLAG_C | FLAG_Z | FLAG_N | FLAG_V | FLAG_H | FLAG_S,
 /* 0x15: ORI    */ FLAG_Z | FLAG_N | FLAG_V | FLAG_S,
 /* 0x16: ANDI   */ FLAG_Z | FLAG_N | FLAG_V | FLAG_S,
 /* 0x17: SPM    */ 0U,
 /* 0x18: LPM    */ 0U,
 /* 0x19: LPM    */ 0U,
 /* 0x1A: PUSH   */ 0U,
 /* 0x1B: POP    */ 0U,
 /* 0x1C: STS    */ 0U,
 /* 0x1D: ST     */ 0U,
 /* 0x1E: ST     */ 0U,
 /* 0x1F: ST     */ 0U,
 /* 0x20: LDS    */ 0U,
 /* 0x21: LD     */ 0U,
 /* 0x22: LD     */ 0U,
 /* 0x23: LD     */ 0U,
 /* 0x24: COM    */ FLAG_C | FLAG_Z | FLAG_N | FLAG_V | FLAG_S,
 /* 0x25: NEG    */ FLAG_C | FLAG_Z | FLAG_N | FLAG_V | FLAG_H | FLAG_S,
 /* 0x26: SWAP   */ 0U,
 /* 0x27: INC    */ FLAG_Z | FLAG_N | FLAG_V | FLAG_S,
 /* 0x28: ASR    */ FLAG_C | FLAG_Z | FLAG_N | FLAG_V | FLAG_S,
 /* 0x29: LSR    */ FLAG_C | FLAG_Z | FLAG_N | FLAG_V | FLAG_S,
 /* 0x2A: ROR    */ FLAG_C | FLAG_Z | FLAG_N | FLAG_V | FLAG_S,
 /* 0x2B: DEC    */ FLAG_Z | FLAG_N | FLAG_V | FLAG_S,
 /* 0x2C: JMP    */ 0U,
 /* 0x2D: CALL   */ 0U,
 /* 0x2E: BSET   */ FLAG_P,
 /* 0x2F: BCLR   */ FLAG_P,
 /* 0x30: IJMP   */ 0U,
 /* 0x31: RET    */ 0U,
 /* 0x32: ICALL  */ 0U,
 /* 0x33: RETI   */ 0U,
 /* 0x34: SLEEP  */ 0U,
 /* 0x35: BREAK  */ 0U,
 /* 0x36: WDR    */ 0U,
 /* 0x37: MUL    */ FLAG_C | FLAG_Z,
 /* 0x38: IN     */ 0U,
 /* 0x39: OUT    */ 0U,
 /* 0x3A: ADIW   */ FLAG_C | FLAG_Z | FLAG_N | FLAG_V | FLAG_S,
 /* 0x3B: SBIW   */ FLAG_C | FLAG_Z | FLAG_N | FLAG_V | FLAG_S,
 /* 0x3C: CBI    */ 0U,
 /* 0x3D: SBIC   */ 0U,
 /* 0x3E: SBI    */ 0U,
 /* 0x3F: SBIS   */ 0U,
 /* 0x40: RJMP   */ 0U,
 /* 0x41: RCALL  */ 0U,
 /* 0x42: BRBS   */ 0U,
 /* 0x43: BRBC   */ 0U,
 /* 0x44: BLD    */ 0U,
 /* 0x45: BST    */ 0U,
 /* 0x46: SBRC   */ 0U,
 /* 0x47: SBRS   */ 0U,
 /* 0x48: LDI    */ 0U,
 /* 0x49: PIXEL  */ 0U,
};


/* Flag reads by each instruction (128). This indicates which flags are relied
** upon to execute the instruction or that execution continues in an
** untraceable location (so all flags are required). */
static const uint8 flag_r[128]={
 /* 0x00: NOP    */ 0U,
 /* 0x01: MOVW   */ 0U,
 /* 0x02: MULS   */ 0U,
 /* 0x03: MULSU  */ 0U,
 /* 0x04: FMUL   */ 0U,
 /* 0x05: FMULS  */ 0U,
 /* 0x06: FMULSU */ 0U,
 /* 0x07: CPC    */ FLAG_C,
 /* 0x08: SBC    */ FLAG_C,
 /* 0x09: ADD    */ 0U,
 /* 0x0A: CPSE   */ 0U,
 /* 0x0B: CP     */ 0U,
 /* 0x0C: SUB    */ 0U,
 /* 0x0D: ADC    */ FLAG_C,
 /* 0x0E: AND    */ 0U,
 /* 0x0F: EOR    */ 0U,
 /* 0x10: OR     */ 0U,
 /* 0x11: MOV    */ 0U,
 /* 0x12: CPI    */ 0U,
 /* 0x13: SBCI   */ FLAG_C,
 /* 0x14: SUBI   */ 0U,
 /* 0x15: ORI    */ 0U,
 /* 0x16: ANDI   */ 0U,
 /* 0x17: SPM    */ 0U,
 /* 0x18: LPM    */ 0U,
 /* 0x19: LPM    */ 0U,
 /* 0x1A: PUSH   */ 0U,
 /* 0x1B: POP    */ 0U,
 /* 0x1C: STS    */ 0U,
 /* 0x1D: ST     */ 0U,
 /* 0x1E: ST     */ 0U,
 /* 0x1F: ST     */ 0U,
 /* 0x20: LDS    */ 0U,
 /* 0x21: LD     */ 0U,
 /* 0x22: LD     */ 0U,
 /* 0x23: LD     */ 0U,
 /* 0x24: COM    */ 0U,
 /* 0x25: NEG    */ 0U,
 /* 0x26: SWAP   */ 0U,
 /* 0x27: INC    */ 0U,
 /* 0x28: ASR    */ 0U,
 /* 0x29: LSR    */ 0U,
 /* 0x2A: ROR    */ FLAG_C,
 /* 0x2B: DEC    */ 0U,
 /* 0x2C: JMP    */ 0U,
 /* 0x2D: CALL   */ 0U,
 /* 0x2E: BSET   */ 0U,
 /* 0x2F: BCLR   */ 0U,
 /* 0x30: IJMP   */ FLAG_C | FLAG_Z | FLAG_N | FLAG_V | FLAG_H | FLAG_S,
 /* 0x31: RET    */ FLAG_C | FLAG_Z | FLAG_N | FLAG_V | FLAG_H | FLAG_S,
 /* 0x32: ICALL  */ FLAG_C | FLAG_Z | FLAG_N | FLAG_V | FLAG_H | FLAG_S,
 /* 0x33: RETI   */ FLAG_C | FLAG_Z | FLAG_N | FLAG_V | FLAG_H | FLAG_S,
 /* 0x34: SLEEP  */ 0U,
 /* 0x35: BREAK  */ 0U,
 /* 0x36: WDR    */ 0U,
 /* 0x37: MUL    */ 0U,
 /* 0x38: IN     */ 0U,
 /* 0x39: OUT    */ 0U,
 /* 0x3A: ADIW   */ 0U,
 /* 0x3B: SBIW   */ 0U,
 /* 0x3C: CBI    */ 0U,
 /* 0x3D: SBIC   */ 0U,
 /* 0x3E: SBI    */ 0U,
 /* 0x3F: SBIS   */ 0U,
 /* 0x40: RJMP   */ 0U,
 /* 0x41: RCALL  */ 0U,
 /* 0x42: BRBS   */ FLAG_P,
 /* 0x43: BRBC   */ FLAG_P,
 /* 0x44: BLD    */ 0U,
 /* 0x45: BST    */ 0U,
 /* 0x46: SBRC   */ 0U,
 /* 0x47: SBRS   */ 0U,
 /* 0x48: LDI    */ 0U,
 /* 0x49: PIXEL  */ 0U,
};


/* Instruction types. Determines how the instruction is processed by the
** deflagger (it also uses the 2 word flag from the opcode). This is
** irrelevant for instructions requiring every flag. */
static const uint8 ins_type[128]={
 /* 0x00: NOP    */ INS_SEQ,
 /* 0x01: MOVW   */ INS_SEQ,
 /* 0x02: MULS   */ INS_SEQ,
 /* 0x03: MULSU  */ INS_SEQ,
 /* 0x04: FMUL   */ INS_SEQ,
 /* 0x05: FMULS  */ INS_SEQ,
 /* 0x06: FMULSU */ INS_SEQ,
 /* 0x07: CPC    */ INS_SEQ,
 /* 0x08: SBC    */ INS_SEQ,
 /* 0x09: ADD    */ INS_SEQ,
 /* 0x0A: CPSE   */ INS_SEQ,
 /* 0x0B: CP     */ INS_SEQ,
 /* 0x0C: SUB    */ INS_SEQ,
 /* 0x0D: ADC    */ INS_SEQ,
 /* 0x0E: AND    */ INS_SEQ,
 /* 0x0F: EOR    */ INS_SEQ,
 /* 0x10: OR     */ INS_SEQ,
 /* 0x11: MOV    */ INS_SEQ,
 /* 0x12: CPI    */ INS_SEQ,
 /* 0x13: SBCI   */ INS_SEQ,
 /* 0x14: SUBI   */ INS_SEQ,
 /* 0x15: ORI    */ INS_SEQ,
 /* 0x16: ANDI   */ INS_SEQ,
 /* 0x17: SPM    */ INS_SEQ,
 /* 0x18: LPM    */ INS_SEQ,
 /* 0x19: LPM    */ INS_SEQ,
 /* 0x1A: PUSH   */ INS_SEQ,
 /* 0x1B: POP    */ INS_SEQ,
 /* 0x1C: STS    */ INS_SEQ,
 /* 0x1D: ST     */ INS_SEQ,
 /* 0x1E: ST     */ INS_SEQ,
 /* 0x1F: ST     */ INS_SEQ,
 /* 0x20: LDS    */ INS_SEQ,
 /* 0x21: LD     */ INS_SEQ,
 /* 0x22: LD     */ INS_SEQ,
 /* 0x23: LD     */ INS_SEQ,
 /* 0x24: COM    */ INS_SEQ,
 /* 0x25: NEG    */ INS_SEQ,
 /* 0x26: SWAP   */ INS_SEQ,
 /* 0x27: INC    */ INS_SEQ,
 /* 0x28: ASR    */ INS_SEQ,
 /* 0x29: LSR    */ INS_SEQ,
 /* 0x2A: ROR    */ INS_SEQ,
 /* 0x2B: DEC    */ INS_SEQ,
 /* 0x2C: JMP    */ INS_AJMP,
 /* 0x2D: CALL   */ INS_AJMP,
 /* 0x2E: BSET   */ INS_SEQ,
 /* 0x2F: BCLR   */ INS_SEQ,
 /* 0x30: IJMP   */ 0U,
 /* 0x31: RET    */ 0U,
 /* 0x32: ICALL  */ 0U,
 /* 0x33: RETI   */ 0U,
 /* 0x34: SLEEP  */ INS_SEQ,
 /* 0x35: BREAK  */ INS_SEQ,
 /* 0x36: WDR    */ INS_SEQ,
 /* 0x37: MUL    */ INS_SEQ,
 /* 0x38: IN     */ INS_SEQ,
 /* 0x39: OUT    */ INS_SEQ,
 /* 0x3A: ADIW   */ INS_SEQ,
 /* 0x3B: SBIW   */ INS_SEQ,
 /* 0x3C: CBI    */ INS_SEQ,
 /* 0x3D: SBIC   */ INS_CSKIP,
 /* 0x3E: SBI    */ INS_SEQ,
 /* 0x3F: SBIS   */ INS_CSKIP,
 /* 0x40: RJMP   */ INS_RJMP,
 /* 0x41: RCALL  */ INS_RJMP,
 /* 0x42: BRBS   */ INS_CRJMP,
 /* 0x43: BRBC   */ INS_CRJMP,
 /* 0x44: BLD    */ INS_SEQ,
 /* 0x45: BST    */ INS_SEQ,
 /* 0x46: SBRC   */ INS_CSKIP,
 /* 0x47: SBRS   */ INS_CSKIP,
 /* 0x48: LDI    */ INS_SEQ,
 /* 0x49: PIXEL  */ INS_SEQ,
};



/*
** Recursively checks code for overrides. Returns overriden flags masked off
** of the passed flag (flagshave) set. The irem parameter limits depth
** (reducing until reaching zero). The flagsreq flag set indicates flags which
** were proven required (should be started zero, controls the recursive
** search).
*/
static auint cu_avrdf_check(auint flagshave, auint flagsreq,
                            uint32 const* code, auint pc, auint irem)
{
 auint op  = code[pc & 0x7FFFU];
 auint ins = op & 0x7FU;
 auint flw = flag_w[ins];
 auint flr = flag_r[ins];

 if (flw == FLAG_P){ flw = (op >> 8) & 0xFFU; }
 if (flr == FLAG_P){ flr = (op >> 8) & 0xFFU; }

 flagsreq  |= (  flr  & flagshave);
 flagshave &= ((~flw) | flagsreq);
 if ( (flagshave == flagsreq) ||
      (irem == 0U) ){
  return flagshave; /* All what could be cleared was cleared, so path done */
 }

 irem --;

 switch (ins_type[ins]){

  case INS_SEQ:
   pc += ((op >> 7) & 1U) + 1U;
   return cu_avrdf_check(flagshave, flagsreq, code, pc, irem);

  case INS_AJMP:
   pc  = op >> 16;
   return cu_avrdf_check(flagshave, flagsreq, code, pc, irem);

  case INS_RJMP:
   pc += op >> 16;
   return cu_avrdf_check(flagshave, flagsreq, code, pc, irem);

  case INS_CRJMP:
   return cu_avrdf_check(flagshave, flagsreq, code, pc + 1U, irem) |
          cu_avrdf_check(flagshave, flagsreq, code, pc + (op >> 16), irem);

  case INS_CSKIP:
   pc ++;
   return cu_avrdf_check(flagshave, flagsreq, code, pc, irem) |
          cu_avrdf_check(flagshave, flagsreq, code, pc + ((code[pc & 0x7FFFU] >> 7) & 1U) + 1U, irem);

  default:
   return flagshave;

 }
}



/*
** The instruction deflagger parses the compiled AVR code attempting to
** transform instructions to remove costly flag logic (mainly N, H, S and V
** flags) where it can be proven that the flags aren't used. It can not
** revert already transformed instructions. It works on the whole code
** memory (32 KWords).
*/
void  cu_avrdf_deflag(uint32* code)
{
 auint i;
 auint ins;
 auint rep;
 auint flg;
 auint res;

 for (i = 0U; i < 0x8000U; i ++){

  ins = code[i] & 0x7FU;
  rep = ins;
  flg = flag_w[ins] & (~FLAG_P);
  res = flg;

  if (flg != 0U){

   /* There are some flags produced by the instruction. Try to see if it is
   ** possible to skip some of these by replacing instructions */

   res = cu_avrdf_check(flg, 0U, code, i + ((code[i] >> 7) & 1U) + 1U, MAX_DEPTH);

   switch (ins){

    case 0x07U: /* CPC */
     if ((res & (~(FLAG_C | FLAG_Z))) == 0U){ /* Only Z or C might be needed */
//      rep = 0x4AU; /* ZCPC */
     }
     break;

    case 0x08U: /* SBC */
     if (res == 0U){
//      rep = 0x50U;
     }else if ((res & (~(FLAG_C | FLAG_Z))) == 0U){ /* Only Z or C might be needed */
//      rep = 0x4BU; /* ZSBC */
     }
     break;

    case 0x09U: /* ADD */
     if (res == 0U){
//      rep = 0x51U;
     }else if ((res & (~(FLAG_C | FLAG_Z))) == 0U){ /* Only Z or C might be needed */
//      rep = 0x4CU; /* ZADD */
     }
     break;

    case 0x0BU: /* CP */
     if ((res & (~(FLAG_C | FLAG_Z))) == 0U){ /* Only Z or C might be needed */
//      rep = 0x4DU; /* ZCP */
     }
     break;

    case 0x0CU: /* SUB */
     if (res == 0U){
//      rep = 0x52U;
     }else if ((res & (~(FLAG_C | FLAG_Z))) == 0U){ /* Only Z or C might be needed */
//      rep = 0x4EU; /* ZSUB */
     }
     break;

    case 0x0DU: /* ADC */
     if (res == 0U){
//      rep = 0x53U;
     }else if ((res & (~(FLAG_C | FLAG_Z))) == 0U){ /* Only Z or C might be needed */
//      rep = 0x4FU; /* ZADC */
     }
     break;

    case 0x0EU: /* AND */
     if (res == 0U){
//      rep = 0x54U;
     }
     break;

    case 0x0FU: /* EOR */
     if (res == 0U){
//      rep = 0x55U;
     }
     break;

    case 0x10U: /* OR */
     if (res == 0U){
//      rep = 0x56U;
     }
     break;

    case 0x13U: /* SBCI */
     if (res == 0U){
//      rep = 0x57U;
     }
     break;

    case 0x14U: /* SUBI */
     if (res == 0U){
//      rep = 0x58U;
     }
     break;

    case 0x15U: /* ORI */
     if (res == 0U){
//      rep = 0x59U;
     }
     break;

    case 0x16U: /* ANDI */
     if (res == 0U){
//      rep = 0x5AU;
     }
     break;

    default:
     break;

   }

   code[i] = (code[i] & 0xFFFFFF80U) | rep;

  }

printf("%04X: Op: %02X => %02X, Flags: %02X => %02X\n", i, ins, rep, flg, res);

 }
}
