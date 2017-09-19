/*
 *  AVR microcontroller emulation, opcode decoder for Emscripten builds
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



#ifndef CU_AVR_C
#error "This file is part of cu_avr.c!"
#endif



/*
** This is a variant of the instruction decoder optimized for Emscripten,
** breaking it down into small functions, using a jump table. This is easier
** to process by JIT compilers, so it results in better in-browser
** performance, but it is larger and worse for native builds.
*/



typedef void(avr_opcode)(auint arg1, auint arg2);



/* Trailing cycles */

#define cy0_tail() \
 do{ \
  if (event_it_enter){ cu_avr_interrupt(); } \
 }while(0)

#define cy1_tail() \
 do{ \
  UPDATE_HARDWARE_IT; \
  cy0_tail(); \
 }while(0)

#define cy2_tail() \
 do{ \
  UPDATE_HARDWARE; \
  cy1_tail(); \
 }while(0)

#define cy3_tail() \
 do{ \
  UPDATE_HARDWARE; \
  cy2_tail(); \
 }while(0)

#define cy4_tail() \
 do{ \
  UPDATE_HARDWARE; \
  cy3_tail(); \
 }while(0)



/* Opcode common tails */

#define mul_tail_fmul() \
 do{ \
  cpu_state.iors[0x00U] = (res     ) & 0xFFU; \
  cpu_state.iors[0x01U] = (res >> 8) & 0xFFU; \
  cpu_state.iors[CU_IO_SREG] = flags; \
  cy2_tail(); \
 }while(0)

#define mul_tail() \
 do{ \
  auint res   = dst * src; \
  auint flags = cpu_state.iors[CU_IO_SREG]; \
  PROCFLAGS_MUL(flags, res); \
  mul_tail_fmul(); \
 }while(0)

#define fmul_tail() \
 do{ \
  auint res   = (dst * src) << 1; \
  auint flags = cpu_state.iors[CU_IO_SREG]; \
  PROCFLAGS_FMUL(flags, res); \
  mul_tail_fmul(); \
 }while(0)

#define add_tail() \
 do{ \
  cpu_state.iors[arg1] = res; \
  cpu_state.iors[CU_IO_SREG] = (cpu_state.iors[CU_IO_SREG] & (SREG_IM | SREG_TM)) | \
                               cpu_pflags[CU_AVRFG_ADD + (res & 0x1FFU) + ((src & 0x90U) << 5) + ((dst & 0x90U) << 6)]; \
  cy1_tail(); \
 }while(0)

#define sub_tail_flg() \
 do{ \
  cpu_state.iors[CU_IO_SREG] = (cpu_state.iors[CU_IO_SREG] & (SREG_IM | SREG_TM)) | \
                               cpu_pflags[CU_AVRFG_SUB + (res & 0x1FFU) + ((src & 0x90U) << 5) + ((dst & 0x90U) << 6)]; \
  cy1_tail(); \
 }while(0)

#define sub_tail() \
 do{ \
  auint res   = dst - src; \
  cpu_state.iors[arg1] = res; \
  sub_tail_flg(); \
 }while(0)

#define sbc_tail_flg() \
 do{ \
  cpu_state.iors[CU_IO_SREG] = (cpu_state.iors[CU_IO_SREG] | (SREG_HM | SREG_SM | SREG_VM | SREG_NM | SREG_CM)) & \
                               ( (cpu_pflags[CU_AVRFG_SUB + (res & 0x1FFU) + ((src & 0x90U) << 5) + ((dst & 0x90U) << 6)]) | \
                                 (SREG_IM | SREG_TM) ); \
  cy1_tail(); \
 }while(0)

#define sbc_tail() \
 do{ \
  auint dst   = cpu_state.iors[arg1]; \
  auint res   = dst - (src + SREG_GET_C(cpu_state.iors[CU_IO_SREG])); \
  cpu_state.iors[arg1] = res; \
  sbc_tail_flg(); \
 }while(0)

#define log_tail() \
 do{ \
  cpu_state.iors[arg1] = res; \
  cpu_state.iors[CU_IO_SREG] = (cpu_state.iors[CU_IO_SREG] & (SREG_IM | SREG_TM | SREG_HM | SREG_CM)) | \
                               cpu_pflags[CU_AVRFG_LOG + res]; \
  cy1_tail(); \
 }while(0)

#define stk_tail() \
 do{ \
  cpu_state.iors[CU_IO_SPL] = (tmp     ) & 0xFFU; \
  cpu_state.iors[CU_IO_SPH] = (tmp >> 8) & 0xFFU; \
  cy2_tail(); \
 }while(0)

#define st_tail() \
 do{ \
  UPDATE_HARDWARE; \
  UPDATE_HARDWARE_IT; \
  if (tmp >= 0x0100U){ \
   cpu_state.sram[tmp & 0x0FFFU] = cpu_state.iors[arg1]; \
   access_mem[tmp & 0x0FFFU] |= CU_MEM_W; \
  }else{ \
   cu_avr_write_io(tmp, cpu_state.iors[arg1]); \
  } \
  cy0_tail(); \
 }while(0)

#define ld_tail() \
 do{ \
  UPDATE_HARDWARE; \
  if (tmp >= 0x0100U){ \
   cpu_state.iors[arg1] = cpu_state.sram[tmp & 0x0FFFU]; \
   access_mem[tmp & 0x0FFFU] |= CU_MEM_R; \
  }else{ \
   cpu_state.iors[arg1] = cu_avr_read_io(tmp); \
  } \
  cy1_tail(); \
 }while(0)

#define shr_tail() \
 do{ \
  res  |= (src >> 1); \
  cpu_state.iors[arg1] = res; \
  cpu_state.iors[CU_IO_SREG] = (cpu_state.iors[CU_IO_SREG] & (SREG_IM | SREG_TM | SREG_HM)) | \
                               cpu_pflags[CU_AVRFG_SHR + ((src & 1U) << 8) + res]; \
  cy1_tail(); \
 }while(0)

#define ret_tail() \
 do{ \
  auint tmp   = ((auint)(cpu_state.iors[CU_IO_SPL])     ) + \
                ((auint)(cpu_state.iors[CU_IO_SPH]) << 8); \
  tmp ++; \
  cpu_state.pc  = (auint)(cpu_state.sram[tmp & 0x0FFFU]) << 8; \
  access_mem[tmp & 0x0FFFU] |= CU_MEM_R; \
  tmp ++; \
  cpu_state.pc |= (auint)(cpu_state.sram[tmp & 0x0FFFU]); \
  access_mem[tmp & 0x0FFFU] |= CU_MEM_R; \
  cpu_state.iors[CU_IO_SPL] = (tmp     ) & 0xFFU; \
  cpu_state.iors[CU_IO_SPH] = (tmp >> 8) & 0xFFU; \
  cy4_tail(); \
 }while(0)

#define adiw_tail() \
 do{ \
  cpu_state.iors[arg1 + 0U] = (res     ) & 0xFFU; \
  cpu_state.iors[arg1 + 1U] = (res >> 8) & 0xFFU; \
  SREG_SET(flags, SREG_NM & ((          res ) >> (15U - SREG_N))); \
  SREG_SET_C_BIT16(flags, res); \
  SREG_SET_Z(flags, res & 0xFFFFU); \
  SREG_COM_NV(flags); \
  cpu_state.iors[CU_IO_SREG] = flags; \
  cy2_tail(); \
 }while(0)

#define out_tail() \
 do{ \
  UPDATE_HARDWARE_IT; \
  cu_avr_write_io(arg1, tmp); \
  cy0_tail(); \
 }while(0)

#define oub_tail() \
 do{ \
  UPDATE_HARDWARE; \
  out_tail(); \
 }while(0)

#define call_tail() \
 do{ \
  auint tmp   = ((auint)(cpu_state.iors[CU_IO_SPL])     ) + \
                ((auint)(cpu_state.iors[CU_IO_SPH]) << 8); \
  cpu_state.sram[tmp & 0x0FFFU] = (cpu_state.pc     ) & 0xFFU; \
  access_mem[tmp & 0x0FFFU] |= CU_MEM_W; \
  tmp --; \
  cpu_state.sram[tmp & 0x0FFFU] = (cpu_state.pc >> 8) & 0xFFU; \
  access_mem[tmp & 0x0FFFU] |= CU_MEM_W; \
  tmp --; \
  cpu_state.iors[CU_IO_SPL] = (tmp     ) & 0xFFU; \
  cpu_state.iors[CU_IO_SPH] = (tmp >> 8) & 0xFFU; \
  cpu_state.pc = res; \
  cy3_tail(); \
 }while(0)

#define skip_tail() \
 do{ \
  if (((cpu_code[cpu_state.pc & 0x7FFFU] >> 7) & 1U) != 0U){ \
   cpu_state.pc += 2U; \
   cy3_tail(); \
  }else{ \
   cpu_state.pc ++; \
   cy2_tail(); \
  } \
 }while(0)



/* Opcodes */

static void op_00(auint arg1, auint arg2) /* NOP */
{
 cy1_tail();
}

static void op_01(auint arg1, auint arg2) /* MOVW */
{
 cpu_state.iors[arg1 + 0U] = cpu_state.iors[arg2 + 0U];
 cpu_state.iors[arg1 + 1U] = cpu_state.iors[arg2 + 1U];
 cy1_tail();
}

static void op_02(auint arg1, auint arg2) /* MULS */
{
 auint dst   = cpu_state.iors[arg1];
 auint src   = cpu_state.iors[arg2];
 dst  -= (dst & 0x80U) << 1; /* Sign extend from 8 bits */
 src  -= (src & 0x80U) << 1; /* Sign extend from 8 bits */
 mul_tail();
}

static void op_03(auint arg1, auint arg2) /* MULSU */
{
 auint dst   = cpu_state.iors[arg1];
 auint src   = cpu_state.iors[arg2];
 dst  -= (dst & 0x80U) << 1; /* Sign extend from 8 bits */
 mul_tail();
}

static void op_04(auint arg1, auint arg2) /* FMUL */
{
 auint dst   = cpu_state.iors[arg1];
 auint src   = cpu_state.iors[arg2];
 fmul_tail();
}

static void op_05(auint arg1, auint arg2) /* FMULS */
{
 auint dst   = cpu_state.iors[arg1];
 auint src   = cpu_state.iors[arg2];
 dst  -= (dst & 0x80U) << 1; /* Sign extend from 8 bits */
 src  -= (src & 0x80U) << 1; /* Sign extend from 8 bits */
 fmul_tail();
}

static void op_06(auint arg1, auint arg2) /* FMULSU */
{
 auint dst   = cpu_state.iors[arg1];
 auint src   = cpu_state.iors[arg2];
 dst  -= (dst & 0x80U) << 1; /* Sign extend from 8 bits */
 fmul_tail();
}

static void op_07(auint arg1, auint arg2) /* CPC */
{
 auint src   = cpu_state.iors[arg2];
 auint dst   = cpu_state.iors[arg1];
 auint res   = dst - (src + SREG_GET_C(cpu_state.iors[CU_IO_SREG]));
 sbc_tail_flg();
}

static void op_08(auint arg1, auint arg2) /* SBC */
{
 auint src   = cpu_state.iors[arg2];
 sbc_tail();
}

static void op_09(auint arg1, auint arg2) /* ADD */
{
 auint src   = cpu_state.iors[arg2];
 auint dst   = cpu_state.iors[arg1];
 auint res   = dst + src;
 add_tail();
}

static void op_0A(auint arg1, auint arg2) /* CPSE */
{
 if (cpu_state.iors[arg1] != cpu_state.iors[arg2]){
  cy1_tail();
 }else{
  skip_tail();
 }
}

static void op_0B(auint arg1, auint arg2) /* CP */
{
 auint src   = cpu_state.iors[arg2];
 auint dst   = cpu_state.iors[arg1];
 auint res   = dst - src;
 sub_tail_flg();
}

static void op_0C(auint arg1, auint arg2) /* SUB */
{
 auint src   = cpu_state.iors[arg2];
 auint dst   = cpu_state.iors[arg1];
 sub_tail();
}

static void op_0D(auint arg1, auint arg2) /* ADC */
{
 auint src   = cpu_state.iors[arg2];
 auint dst   = cpu_state.iors[arg1];
 auint res   = dst + (src + SREG_GET_C(cpu_state.iors[CU_IO_SREG]));
 add_tail();
}

static void op_0E(auint arg1, auint arg2) /* AND */
{
 auint res   = cpu_state.iors[arg1] & cpu_state.iors[arg2];
 log_tail();
}

static void op_0F(auint arg1, auint arg2) /* EOR */
{
 auint res   = cpu_state.iors[arg1] ^ cpu_state.iors[arg2];
 log_tail();
}

static void op_10(auint arg1, auint arg2) /* OR */
{
 auint res   = cpu_state.iors[arg1] | cpu_state.iors[arg2];
 log_tail();
}

static void op_11(auint arg1, auint arg2) /* MOV */
{
 cpu_state.iors[arg1] = cpu_state.iors[arg2];
 cy1_tail();
}

static void op_12(auint arg1, auint arg2) /* CPI */
{
 auint src   = arg2;
 auint dst   = cpu_state.iors[arg1];
 auint res   = dst - src;
 sub_tail_flg();
}

static void op_13(auint arg1, auint arg2) /* SBCI */
{
 auint src   = arg2;
 sbc_tail();
}

static void op_14(auint arg1, auint arg2) /* SUBI */
{
 auint src   = arg2;
 auint dst   = cpu_state.iors[arg1];
 sub_tail();
}

static void op_15(auint arg1, auint arg2) /* ORI */
{
 auint res   = cpu_state.iors[arg1] | arg2;
 log_tail();
}

static void op_16(auint arg1, auint arg2) /* ANDI */
{
 auint res   = cpu_state.iors[arg1] & arg2;
 log_tail();
}

static void op_17(auint arg1, auint arg2) /* SPM */
{
 auint tmp;
 auint res;
 if ( ((cpu_state.iors[CU_IO_SPMCSR] & 0x01U) != 0U) && /* SPM instruction enabled */
      (!cpu_state.spm_prge) ){            /* Programming is not in progress */
  if ( (cpu_state.spm_mode == 0x02U) ||   /* Page erase */
       (cpu_state.spm_mode == 0x04U) ){   /* Page write */
   res = (auint)(cpu_state.iors[31]) << 8;
   if (cpu_state.spm_mode == 0x02U){      /* Page erase */
    for (tmp = 0U; tmp < 256U; tmp ++){ cpu_state.crom[tmp + res]  = 0xFFU; }
   }else{                                 /* Page write */
    for (tmp = 0U; tmp < 256U; tmp ++){ cpu_state.crom[tmp + res] &= cpu_state.sbuf[tmp]; }
   }
   if (res != 0U){ cu_avr_crom_update(res - 2U, 258U); } /* Fix 2 word instructions crossing page boundary */
   else          { cu_avr_crom_update(res,      256U); }
   cpu_state.spm_prge = TRUE;
   cpu_state.iors[CU_IO_SPMCSR] |= 0x40U; /* RRW section busy */
   cpu_state.spm_end  = WRAP32(SPM_PROG_TIM + cpu_state.cycle);
   if ( (WRAP32(cycle_next_event  - cpu_state.cycle)) >
        (WRAP32(cpu_state.spm_end - cpu_state.cycle)) ){
    cycle_next_event = cpu_state.spm_end; /* Set SPM HW processing target */
   }
  }else if (cpu_state.spm_mode == 0x08U){ /* Boot lock bit set (unimplemented) */
   cpu_state.iors[CU_IO_SPMCSR] &= 0xE0U;
  }else if (cpu_state.spm_mode == 0x10U){ /* RWW section read enable */
   cpu_state.iors[CU_IO_SPMCSR] &= 0xA0U;
  }else{                                  /* Page loading (temp. buffer filling) */
   cpu_state.sbuf[(cpu_state.iors[30] & 0xFEU) + 0U] = cpu_state.iors[0];
   cpu_state.sbuf[(cpu_state.iors[30] & 0xFEU) + 1U] = cpu_state.iors[1];
   cpu_state.iors[CU_IO_SPMCSR] &= 0xA0U;
  }
 }
 cy4_tail();
}

static void op_18(auint arg1, auint arg2) /* LPM */
{
 auint tmp = ((auint)(cpu_state.iors[30])     ) +
             ((auint)(cpu_state.iors[31]) << 8);
 cpu_state.iors[arg1] = cpu_state.crom[tmp];
 cy3_tail();
}

static void op_19(auint arg1, auint arg2) /* LPM (+) */
{
 auint tmp = ((auint)(cpu_state.iors[30])     ) +
             ((auint)(cpu_state.iors[31]) << 8);
 cpu_state.iors[arg1] = cpu_state.crom[tmp];
 tmp ++;
 cpu_state.iors[30] = (tmp     ) & 0xFFU;
 cpu_state.iors[31] = (tmp >> 8) & 0xFFU;
 cy3_tail();
}

static void op_1A(auint arg1, auint arg2) /* PUSH */
{
 auint tmp = ((auint)(cpu_state.iors[CU_IO_SPL])     ) +
             ((auint)(cpu_state.iors[CU_IO_SPH]) << 8);
 cpu_state.sram[tmp & 0x0FFFU] = cpu_state.iors[arg1];
 access_mem[tmp & 0x0FFFU] |= CU_MEM_W;
 tmp --;
 stk_tail();
}

static void op_1B(auint arg1, auint arg2) /* POP */
{
 auint tmp = ((auint)(cpu_state.iors[CU_IO_SPL])     ) +
             ((auint)(cpu_state.iors[CU_IO_SPH]) << 8);
 tmp ++;
 cpu_state.iors[arg1] = cpu_state.sram[tmp & 0x0FFFU];
 access_mem[tmp & 0x0FFFU] |= CU_MEM_R;
 stk_tail();
}

static void op_1C(auint arg1, auint arg2) /* STS */
{
 auint tmp = arg2;
 cpu_state.pc ++;
 st_tail();
}

static void op_1D(auint arg1, auint arg2) /* ST */
{
 auint tmp = ( ((auint)(cpu_state.iors[(arg2 & 0xFFU) + 0U])     ) +
               ((auint)(cpu_state.iors[(arg2 & 0xFFU) + 1U]) << 8) +
               (arg2 >> 8) ) & 0xFFFFU; /* Mask: Just in case someone is tricky accessing IO */
 st_tail();
}

static void op_1E(auint arg1, auint arg2) /* ST (-) */
{
 auint tmp = ((auint)(cpu_state.iors[arg2 + 0U])     ) +
             ((auint)(cpu_state.iors[arg2 + 1U]) << 8);
 tmp --;
 cpu_state.iors[arg2 + 0U] = (tmp     ) & 0xFFU;
 cpu_state.iors[arg2 + 1U] = (tmp >> 8) & 0xFFU;
 st_tail();
}

static void op_1F(auint arg1, auint arg2) /* ST (+) */
{
 auint tmp = ((auint)(cpu_state.iors[arg2 + 0U])     ) +
             ((auint)(cpu_state.iors[arg2 + 1U]) << 8);
 tmp ++;
 cpu_state.iors[arg2 + 0U] = (tmp     ) & 0xFFU;
 cpu_state.iors[arg2 + 1U] = (tmp >> 8) & 0xFFU;
 tmp --;
 st_tail();
}

static void op_20(auint arg1, auint arg2) /* LDS */
{
 auint tmp = arg2;
 cpu_state.pc ++;
 ld_tail();
}

static void op_21(auint arg1, auint arg2) /* LD */
{
 auint tmp = ( ((auint)(cpu_state.iors[(arg2 & 0xFFU) + 0U])     ) +
               ((auint)(cpu_state.iors[(arg2 & 0xFFU) + 1U]) << 8) +
               (arg2 >> 8) ) & 0xFFFFU; /* Mask: Just in case someone is tricky accessing IO */
 ld_tail();
}

static void op_22(auint arg1, auint arg2) /* LD (-) */
{
 auint tmp = ((auint)(cpu_state.iors[arg2 + 0U])     ) +
             ((auint)(cpu_state.iors[arg2 + 1U]) << 8);
 tmp --;
 cpu_state.iors[arg2 + 0U] = (tmp     ) & 0xFFU;
 cpu_state.iors[arg2 + 1U] = (tmp >> 8) & 0xFFU;
 ld_tail();
}

static void op_23(auint arg1, auint arg2) /* LD (+) */
{
 auint tmp = ((auint)(cpu_state.iors[arg2 + 0U])     ) +
             ((auint)(cpu_state.iors[arg2 + 1U]) << 8);
 tmp ++;
 cpu_state.iors[arg2 + 0U] = (tmp     ) & 0xFFU;
 cpu_state.iors[arg2 + 1U] = (tmp >> 8) & 0xFFU;
 tmp --;
 ld_tail();
}

static void op_24(auint arg1, auint arg2) /* COM */
{
 auint res   = cpu_state.iors[arg1] ^ 0xFFU;
 cpu_state.iors[arg1] = res;
 cpu_state.iors[CU_IO_SREG] = (cpu_state.iors[CU_IO_SREG] & (SREG_IM | SREG_TM | SREG_HM)) |
                              (cpu_pflags[CU_AVRFG_LOG + res] | SREG_CM);
 cy1_tail();
}

static void op_25(auint arg1, auint arg2) /* NEG */
{
 auint src   = cpu_state.iors[arg1];
 auint dst   = 0x00U;
 sub_tail();
}

static void op_26(auint arg1, auint arg2) /* SWAP */
{
 auint res   = cpu_state.iors[arg1];
 cpu_state.iors[arg1] = (res >> 4) | (res << 4);
 cy1_tail();
}

static void op_27(auint arg1, auint arg2) /* INC */
{
 auint res   = cpu_state.iors[arg1];
 res ++;
 cpu_state.iors[arg1] = res;
 cpu_state.iors[CU_IO_SREG] = (cpu_state.iors[CU_IO_SREG] & (SREG_IM | SREG_TM | SREG_HM | SREG_CM)) |
                              cpu_pflags[CU_AVRFG_INC + (res & 0xFFU)];
 cy1_tail();
}

static void op_28(auint arg1, auint arg2) /* ASR */
{
 auint src   = cpu_state.iors[arg1];
 auint res   = (src & 0x80U);
 shr_tail();
}

static void op_29(auint arg1, auint arg2) /* LSR */
{
 auint src   = cpu_state.iors[arg1];
 auint res   = 0U;
 shr_tail();
}

static void op_2A(auint arg1, auint arg2) /* ROR */
{
 auint flags = cpu_state.iors[CU_IO_SREG];
 auint src   = cpu_state.iors[arg1];
 auint res   = (SREG_GET_C(flags) << 7);
 shr_tail();
}

static void op_2B(auint arg1, auint arg2) /* DEC */
{
 auint res   = cpu_state.iors[arg1];
 res --;
 cpu_state.iors[arg1] = res;
 cpu_state.iors[CU_IO_SREG] = (cpu_state.iors[CU_IO_SREG] & (SREG_IM | SREG_TM | SREG_HM | SREG_CM)) |
                              cpu_pflags[CU_AVRFG_DEC + (res & 0xFFU)];
 cy1_tail();
}

static void op_2C(auint arg1, auint arg2) /* JMP */
{
 cpu_state.pc = arg2;
 cy3_tail();
}

static void op_2D(auint arg1, auint arg2) /* CALL */
{
 auint res   = arg2;
 cpu_state.pc ++;
 UPDATE_HARDWARE;
 call_tail();
}

static void op_2E(auint arg1, auint arg2) /* BSET */
{
 auint flags = cpu_state.iors[CU_IO_SREG];
 cpu_state.iors[CU_IO_SREG] |=  arg1;
 if ((((~flags) & arg1) & SREG_IM) != 0U){
  event_it = TRUE; /* Interrupts become enabled, so check them */
 }
 cy1_tail();
}

static void op_2F(auint arg1, auint arg2) /* BCLR */
{
 cpu_state.iors[CU_IO_SREG] &= ~arg1;
 cy1_tail();
}

static void op_30(auint arg1, auint arg2) /* IJMP */
{
 auint tmp   = ((auint)(cpu_state.iors[30])     ) +
               ((auint)(cpu_state.iors[31]) << 8);
 cpu_state.pc = tmp;
 cy2_tail();
}

static void op_31(auint arg1, auint arg2) /* RET */
{
 ret_tail();
}

static void op_32(auint arg1, auint arg2) /* ICALL */
{
 auint res   = ((auint)(cpu_state.iors[30])     ) +
               ((auint)(cpu_state.iors[31]) << 8);
 call_tail();
}

static void op_33(auint arg1, auint arg2) /* RETI */
{
 auint flags = cpu_state.iors[CU_IO_SREG];
 SREG_SET(flags, SREG_IM);
 event_it = TRUE; /* Interrupts (might) become enabled, so check them */
 cpu_state.iors[CU_IO_SREG] = flags;
 ret_tail();
}

static void op_34(auint arg1, auint arg2) /* SLEEP */
{
 /* Will implement later */
 cy1_tail();
}

static void op_35(auint arg1, auint arg2) /* BREAK */
{
 /* No operation */
 cy1_tail();
}

static void op_36(auint arg1, auint arg2) /* WDR */
{
 cpu_state.wd_end = WRAP32(cu_avr_getwdto() + cpu_state.cycle);
 if ( (WRAP32(cycle_next_event - cpu_state.cycle)) >
      (WRAP32(cpu_state.wd_end - cpu_state.cycle)) ){
  cycle_next_event = cpu_state.wd_end; /* Set Watchdog timeout HW processing target */
 }
 if ( (WRAP32(cpu_state.cycle - wd_last)) < wd_interval_min[0] ){ /* WDR debug counter */
  wd_interval_min[0] = WRAP32(cpu_state.cycle - wd_last);
  wd_interval_beg[0] = wd_last_pc;
  wd_interval_end[0] = (cpu_state.pc - 1U) & 0x7FFFU;
 }
 wd_last    = cpu_state.cycle;
 wd_last_pc = (cpu_state.pc - 1U) & 0x7FFFU;
 cy1_tail();
}

static void op_37(auint arg1, auint arg2) /* MUL */
{
 auint dst   = cpu_state.iors[arg1];
 auint src   = cpu_state.iors[arg2];
 mul_tail();
}

static void op_38(auint arg1, auint arg2) /* IN */
{
 cpu_state.iors[arg1] = cu_avr_read_io(arg2);
 cy1_tail();
}

static void op_39(auint arg1, auint arg2) /* OUT */
{
 auint tmp = cpu_state.iors[arg2];
 out_tail();
}

static void op_3A(auint arg1, auint arg2) /* ADIW */
{
 auint flags = cpu_state.iors[CU_IO_SREG];
 auint dst   = ((auint)(cpu_state.iors[arg1 + 0U])     ) +
               ((auint)(cpu_state.iors[arg1 + 1U]) << 8);
 auint src   = arg2; /* Flags are simplified assuming this is less than 0x8000 (it is so on AVR) */
 auint res   = dst + src;
 SREG_CLR(flags, SREG_CM | SREG_ZM | SREG_NM | SREG_VM | SREG_SM);
 SREG_SET(flags, SREG_VM & (((~dst) & (res)) >> (15U - SREG_V)));
 adiw_tail();
}

static void op_3B(auint arg1, auint arg2) /* SBIW */
{
 auint flags = cpu_state.iors[CU_IO_SREG];
 auint dst   = ((auint)(cpu_state.iors[arg1 + 0U])     ) +
               ((auint)(cpu_state.iors[arg1 + 1U]) << 8);
 auint src   = arg2; /* Flags are simplified assuming this is less than 0x8000 (it is so on AVR) */
 auint res   = dst - src;
 SREG_CLR(flags, SREG_CM | SREG_ZM | SREG_NM | SREG_VM | SREG_SM);
 SREG_SET(flags, SREG_VM & (((dst) & (~res)) >> (15U - SREG_V)));
 adiw_tail();
}

static void op_3C(auint arg1, auint arg2) /* CBI */
{
 auint tmp   = cu_avr_read_io(arg1) & (~arg2);
 oub_tail();
}

static void op_3D(auint arg1, auint arg2) /* SBIC */
{
 if ((cu_avr_read_io(arg1) & arg2) != 0U){
  cy1_tail();
 }else{
  skip_tail();
 }
}

static void op_3E(auint arg1, auint arg2) /* SBI */
{
 auint tmp   = cu_avr_read_io(arg1) | ( arg2);
 oub_tail();
}

static void op_3F(auint arg1, auint arg2) /* SBIS */
{
 if ((cu_avr_read_io(arg1) & arg2) == 0U){
  cy1_tail();
 }else{
  skip_tail();
 }
}

static void op_40(auint arg1, auint arg2) /* RJMP */
{
 cpu_state.pc += arg2;
 cy2_tail();
}

static void op_41(auint arg1, auint arg2) /* RCALL */
{
 auint res   = cpu_state.pc + arg2;
 call_tail();
}

static void op_42(auint arg1, auint arg2) /* BRBS */
{
 if ((cpu_state.iors[CU_IO_SREG] & arg1) == 0U){
  cy1_tail();
 }else{
  cpu_state.pc += arg2;
  cy2_tail();
 }
}

static void op_43(auint arg1, auint arg2) /* BRBC */
{
 if ((cpu_state.iors[CU_IO_SREG] & arg1) != 0U){
  cy1_tail();
 }else{
  cpu_state.pc += arg2;
  cy2_tail();
 }
}

static void op_44(auint arg1, auint arg2) /* BLD */
{
 auint src   = (cpu_state.iors[CU_IO_SREG] >> SREG_T) & 1U;
 auint tmp   = cpu_state.iors[arg1];
 tmp   = (tmp & (~(1U << arg2))) | (src << arg2);
 cpu_state.iors[arg1] = tmp;
 cy1_tail();
}

static void op_45(auint arg1, auint arg2) /* BST */
{
 auint flags = cpu_state.iors[CU_IO_SREG] & (~(auint)(SREG_TM));
 flags = flags | (((cpu_state.iors[arg1] >> arg2) & 1U) << SREG_T);
 cpu_state.iors[CU_IO_SREG] = flags;
 cy1_tail();
}

static void op_46(auint arg1, auint arg2) /* SBRC */
{
 if ((cpu_state.iors[arg1] & arg2) != 0U){
  cy1_tail();
 }else{
  skip_tail();
 }
}

static void op_47(auint arg1, auint arg2) /* SBRS */
{
 if ((cpu_state.iors[arg1] & arg2) == 0U){
  cy1_tail();
 }else{
  skip_tail();
 }
}

static void op_48(auint arg1, auint arg2) /* LDI */
{
 cpu_state.iors[arg1] = arg2;
 cy1_tail();
}

static void op_49(auint arg1, auint arg2) /* PIXEL */
{
 /* Note: Normally should execute after UPDATE_HARDWARE, here it doesn't
 ** matter (just shifts visual output one cycle left) */
 cpu_state.iors[CU_IO_PORTC] = cpu_state.iors[arg2] &
                               cpu_state.iors[CU_IO_DDRC];
 cy1_tail();
}

static void op_4A(auint arg1, auint arg2)
{
 /* Undefined op. error here, implement! */
 cy1_tail();
}



static avr_opcode* const avr_opcode_table[128U] = {
 &op_00, &op_01, &op_02, &op_03, &op_04, &op_05, &op_06, &op_07,
 &op_08, &op_09, &op_0A, &op_0B, &op_0C, &op_0D, &op_0E, &op_0F,
 &op_10, &op_11, &op_12, &op_13, &op_14, &op_15, &op_16, &op_17,
 &op_18, &op_19, &op_1A, &op_1B, &op_1C, &op_1D, &op_1E, &op_1F,
 &op_20, &op_21, &op_22, &op_23, &op_24, &op_25, &op_26, &op_27,
 &op_28, &op_29, &op_2A, &op_2B, &op_2C, &op_2D, &op_2E, &op_2F,
 &op_30, &op_31, &op_32, &op_33, &op_34, &op_35, &op_36, &op_37,
 &op_38, &op_39, &op_3A, &op_3B, &op_3C, &op_3D, &op_3E, &op_3F,
 &op_40, &op_41, &op_42, &op_43, &op_44, &op_45, &op_46, &op_47,
 &op_48, &op_49, &op_4A, &op_4A, &op_4A, &op_4A, &op_4A, &op_4A,
 &op_4A, &op_4A, &op_4A, &op_4A, &op_4A, &op_4A, &op_4A, &op_4A,
 &op_4A, &op_4A, &op_4A, &op_4A, &op_4A, &op_4A, &op_4A, &op_4A,
 &op_4A, &op_4A, &op_4A, &op_4A, &op_4A, &op_4A, &op_4A, &op_4A,
 &op_4A, &op_4A, &op_4A, &op_4A, &op_4A, &op_4A, &op_4A, &op_4A,
 &op_4A, &op_4A, &op_4A, &op_4A, &op_4A, &op_4A, &op_4A, &op_4A,
 &op_4A, &op_4A, &op_4A, &op_4A, &op_4A, &op_4A, &op_4A, &op_4A
};



/*
** Emulates a single (compiled) AVR instruction and any associated hardware
** tasks.
*/
static void cu_avr_exec(void)
{
 auint opcode = cpu_code[cpu_state.pc & 0x7FFFU];
 auint arg1   = (opcode >>  8) & 0xFFU;
 auint arg2   = (opcode >> 16) & 0xFFFFU;

 /* GDB stuff should be added here later */

 cpu_state.pc ++;

 /*
 ** Instruction decoder notes:
 **
 ** The instruction's timing is determined by how many UPDATE_HARDWARE (macro)
 ** calls are executed during its decoding.
 **
 ** Stack accesses are only performed on the SRAM (probably on real AVR it is
 ** undefined to place the stack onto the I/O area, such is not emulated).
 */

 avr_opcode_table[opcode & 0x7FU](arg1, arg2);
}
