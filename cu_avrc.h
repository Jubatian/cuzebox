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



#ifndef CU_AVRC_H
#define CU_AVRC_H



#include "cu_types.h"


/*
** The instruction set compiler compiles the AVR instructions expanding them
** onto 32 bits, allowing a simpler (and faster) decoder. Every 16 bit source
** word is compiled into a 32 bit opcode, retaining the addressing.
**
** The resulting 32 bit opcodes are structured as follows:
**
** - bits  0- 6: Selects the operation
** - bit      7: 2 word instruction flag
** - bits  8-15: Argument 1
** - bits 16-23: Argument 2, low
** - bits 24-32: Argument 2, high or Displacement
**
** The operations are as follows (Destination, Source argument order):
**
** 0x00: NOP
** 0x01: MOVW   Ar1(Reg),  Ar2(Reg)
** 0x02: MULS   Ar1(Reg),  Ar2(Reg)
** 0x03: MULSU  Ar1(Reg),  Ar2(Reg)
** 0x04: FMUL   Ar1(Reg),  Ar2(Reg)
** 0x05: FMULS  Ar1(Reg),  Ar2(Reg)
** 0x06: FMULSU Ar1(Reg),  Ar2(Reg)
** 0x07: CPC    Ar1(Reg),  Ar2(Reg)
** 0x08: SBC    Ar1(Reg),  Ar2(Reg)
** 0x09: ADD    Ar1(Reg),  Ar2(Reg)
** 0x0A: CPSE   Ar1(Reg),  Ar2(Reg)
** 0x0B: CP     Ar1(Reg),  Ar2(Reg)
** 0x0C: SUB    Ar1(Reg),  Ar2(Reg)
** 0x0D: ADC    Ar1(Reg),  Ar2(Reg)
** 0x0E: AND    Ar1(Reg),  Ar2(Reg)
** 0x0F: EOR    Ar1(Reg),  Ar2(Reg)
** 0x10: OR     Ar1(Reg),  Ar2(Reg)
** 0x11: MOV    Ar1(Reg),  Ar2(Reg)
** 0x12: CPI    Ar1(Reg),  Ar2(Imm8)
** 0x13: SBCI   Ar1(Reg),  Ar2(Imm8)
** 0x14: SUBI   Ar1(Reg),  Ar2(Imm8)
** 0x15: ORI    Ar1(Reg),  Ar2(Imm8)
** 0x16: ANDI   Ar1(Reg),  Ar2(Imm8)
** 0x17: SPM    Z
** 0x18: LPM    Ar1(Reg),  Z
** 0x19: LPM    Ar1(Reg),  Z+
** 0x1A: PUSH   Ar1(Reg)
** 0x1B: POP    Ar1(Reg)
** 0x1C: STS    Ar2(Ab16), Ar2(Reg)
** 0x1D: ST     Ar2(Reg) + Disp, Ar1(Reg)
** 0x1E: ST     Ar2(Reg)-, Ar1(Reg)
** 0x1F: ST     Ar2(Reg)+, Ar1(Reg)
** 0x20: LDS    Ar1(Reg),  Ar2(Ab16)
** 0x21: LD     Ar1(Reg),  Ar2(Reg) + Disp
** 0x22: LD     Ar1(Reg),  Ar2(Reg)-
** 0x23: LD     Ar1(Reg),  Ar2(Reg)+
** 0x24: COM    Ar1(Reg)
** 0x25: NEG    Ar1(Reg)
** 0x26: SWAP   Ar1(Reg)
** 0x27: INC    Ar1(Reg)
** 0x28: ASR    Ar1(Reg)
** 0x29: LSR    Ar1(Reg)
** 0x2A: ROR    Ar1(Reg)
** 0x2B: DEC    Ar1(Reg)
** 0x2C: JMP    Ar2(Ab16)
** 0x2D: CALL   Ar2(Ab16)
** 0x2E: BSET   Ar1(Imm8) (Note: Performs OR with Ar1 on flags)
** 0x2F: BCLR   Ar1(Imm8) (Note: Performs AND with ~Ar1 on flags)
** 0x30: IJMP   Z
** 0x31: RET
** 0x32: ICALL  Z
** 0x33: RETI
** 0x34: SLEEP
** 0x35: BREAK
** 0x36: WDR
** 0x37: MUL    Ar1(Reg),  Ar2(Reg)
** 0x38: IN     Ar1(Reg),  Ar2(Abs8)
** 0x39: OUT    Ar1(Abs8), Ar2(Reg)
** 0x3A: ADIW   Ar1(Reg),  Ar2(Im16)
** 0x3B: SBIW   Ar1(Reg),  Ar2(Im16)
** 0x3C: CBI    Ar1(Abs8), Ar2(Imm8) (Note: Performs AND with ~Ar2 on IO reg)
** 0x3D: SBIC   Ar1(Abs8), Ar2(Imm8) (Note: Performs AND and checks zero)
** 0x3E: SBI    Ar1(Abs8), Ar2(Imm8) (Note: Performs OR with Ar2 on IO reg)
** 0x3F: SBIS   Ar1(Abs8), Ar2(Imm8) (Note: Performs AND and checks zero)
** 0x40: RJMP   Ar2(Rl16)
** 0x41: RCALL  Ar2(Rl16)
** 0x42: BRBS   Ar1(Imm8), Ar2(Rl16) (Note: AND on flags and checks zero)
** 0x43: BRBC   Ar1(Imm8), Ar2(Rl16) (Note: AND on flags and checks zero)
** 0x44: BLD    Ar1(Reg),  Ar2(Imm3) (Note: Selects single bit)
** 0x45: BST    Ar1(Reg),  Ar2(Imm3) (Note: Selects single bit)
** 0x46: SBRC   Ar1(Reg),  Ar2(Imm8) (Note: AND on reg and checks zero)
** 0x47: SBRS   Ar1(Reg),  Ar2(Imm8) (Note: AND on reg and checks zero)
** 0x48: LDI    Ar1(Reg),  Ar2(Imm8)
** 0x49: PIXEL  Ar2(Reg)  (Note: Special OUT)
** 0x4A: UNDEF
**
** 0x1C, 0x20, 0x2C and 0x2D are 2 word instructions, so these occur as 0x9C,
** 0xA0, 0xAC and 0xAD on the low 8 bits. This causes the subsequent opcode to
** be skipped (including when such an op is skipped by a skip instruction),
** and adds an extra cycle. The flag distinction is only important for skips.
**
** The PIXEL opcode is a specific OUT to Uzebox (to PORTC, 0x08 or 0x28),
** which is about 5-10 percents of a normal Uzebox game's executed
** instructions. Translating it to this special instruction makes emulation
** considerably faster.
**
** Including and above 0x4A all should be UNDEF.
*/


/*
** Compiles an instruction by its two words (second word only used if two word
** instruction), and returns its 32 bit opcode.
*/
auint cu_avrc_compile(auint word0, auint word1);


#endif
