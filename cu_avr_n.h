/*
 *  AVR microcontroller emulation, opcode decoder for native builds
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
** Emulates a single (compiled) AVR instruction and any associated hardware
** tasks.
*/
static void cu_avr_exec(void)
{
 auint opcode = cpu_code[cpu_state.pc & 0x7FFFU];
 auint opid   = (opcode      ) & 0x7FU;
 auint arg1   = (opcode >>  8) & 0xFFU;
 auint arg2   = (opcode >> 16) & 0xFFFFU;
 auint flags;
 auint dst;
 auint src;
 auint res;
 auint tmp;

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

 switch (opid){

  case 0x00U: /* NOP */
   goto cy1_tail;

  case 0x01U: /* MOVW */
   cpu_state.iors[arg1 + 0U] = cpu_state.iors[arg2 + 0U];
   cpu_state.iors[arg1 + 1U] = cpu_state.iors[arg2 + 1U];
   goto cy1_tail;

  case 0x02U: /* MULS */
   dst   = cpu_state.iors[arg1];
   src   = cpu_state.iors[arg2];
   dst  -= (dst & 0x80U) << 1; /* Sign extend from 8 bits */
   src  -= (src & 0x80U) << 1; /* Sign extend from 8 bits */
   goto mul_tail;

  case 0x03U: /* MULSU */
   dst   = cpu_state.iors[arg1];
   src   = cpu_state.iors[arg2];
   dst  -= (dst & 0x80U) << 1; /* Sign extend from 8 bits */
   goto mul_tail;

  case 0x04U: /* FMUL */
   dst   = cpu_state.iors[arg1];
   src   = cpu_state.iors[arg2];
   goto fmul_tail;

  case 0x05U: /* FMULS */
   dst   = cpu_state.iors[arg1];
   src   = cpu_state.iors[arg2];
   dst  -= (dst & 0x80U) << 1; /* Sign extend from 8 bits */
   src  -= (src & 0x80U) << 1; /* Sign extend from 8 bits */
   goto fmul_tail;

  case 0x06U: /* FMULSU */
   dst   = cpu_state.iors[arg1];
   src   = cpu_state.iors[arg2];
   dst  -= (dst & 0x80U) << 1; /* Sign extend from 8 bits */
fmul_tail:
   res   = (dst * src) << 1;
   flags = cpu_state.iors[CU_IO_SREG];
   PROCFLAGS_FMUL(flags, res);
   goto mul_tail_fmul;

  case 0x07U: /* CPC */
   src   = cpu_state.iors[arg2];
   dst   = cpu_state.iors[arg1];
   res   = dst - (src + SREG_GET_C(cpu_state.iors[CU_IO_SREG]));
   goto sbc_tail_flg;

  case 0x08U: /* SBC */
   src   = cpu_state.iors[arg2];
   goto sbc_tail;

  case 0x09U: /* ADD */
   src   = cpu_state.iors[arg2];
   dst   = cpu_state.iors[arg1];
   res   = dst + src;
   goto add_tail;

  case 0x0AU: /* CPSE */
   if (cpu_state.iors[arg1] != cpu_state.iors[arg2]){
    goto cy1_tail;
   }
   goto skip_tail;

  case 0x0BU: /* CP */
   src   = cpu_state.iors[arg2];
   dst   = cpu_state.iors[arg1];
   res   = dst - src;
   goto sub_tail_flg;

  case 0x0CU: /* SUB */
   src   = cpu_state.iors[arg2];
   dst   = cpu_state.iors[arg1];
   goto sub_tail;

  case 0x0DU: /* ADC */
   src   = cpu_state.iors[arg2];
   dst   = cpu_state.iors[arg1];
   res   = dst + (src + SREG_GET_C(cpu_state.iors[CU_IO_SREG]));
add_tail:
   cpu_state.iors[arg1] = res;
   cpu_state.iors[CU_IO_SREG] = (cpu_state.iors[CU_IO_SREG] & (SREG_IM | SREG_TM)) |
                                cpu_pflags[CU_AVRFG_ADD + (res & 0x1FFU) + ((src & 0x90U) << 5) + ((dst & 0x90U) << 6)];
   goto cy1_tail;

  case 0x0EU: /* AND */
   res   = cpu_state.iors[arg1] & cpu_state.iors[arg2];
   goto log_tail;

  case 0x0FU: /* EOR */
   res   = cpu_state.iors[arg1] ^ cpu_state.iors[arg2];
   goto log_tail;

  case 0x10U: /* OR */
   res   = cpu_state.iors[arg1] | cpu_state.iors[arg2];
   goto log_tail;

  case 0x11U: /* MOV */
   cpu_state.iors[arg1] = cpu_state.iors[arg2];
   goto cy1_tail;

  case 0x12U: /* CPI */
   src   = arg2;
   dst   = cpu_state.iors[arg1];
   res   = dst - src;
   goto sub_tail_flg;

  case 0x13U: /* SBCI */
   src   = arg2;
sbc_tail:
   dst   = cpu_state.iors[arg1];
   res   = dst - (src + SREG_GET_C(cpu_state.iors[CU_IO_SREG]));
   cpu_state.iors[arg1] = res;
sbc_tail_flg:
   cpu_state.iors[CU_IO_SREG] = (cpu_state.iors[CU_IO_SREG] | (SREG_HM | SREG_SM | SREG_VM | SREG_NM | SREG_CM)) &
                                ( (cpu_pflags[CU_AVRFG_SUB + (res & 0x1FFU) + ((src & 0x90U) << 5) + ((dst & 0x90U) << 6)]) |
                                  (SREG_IM | SREG_TM) );
   goto cy1_tail;

  case 0x14U: /* SUBI */
   src   = arg2;
   dst   = cpu_state.iors[arg1];
   goto sub_tail;

  case 0x15U: /* ORI */
   res   = cpu_state.iors[arg1] | arg2;
   goto log_tail;

  case 0x16U: /* ANDI */
   res   = cpu_state.iors[arg1] & arg2;
log_tail:
   cpu_state.iors[arg1] = res;
   cpu_state.iors[CU_IO_SREG] = (cpu_state.iors[CU_IO_SREG] & (SREG_IM | SREG_TM | SREG_HM | SREG_CM)) |
                                cpu_pflags[CU_AVRFG_LOG + res];
   goto cy1_tail;

  case 0x17U: /* SPM */
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
   goto cy4_tail;

  case 0x18U: /* LPM */
   tmp = ((auint)(cpu_state.iors[30])     ) +
         ((auint)(cpu_state.iors[31]) << 8);
   cpu_state.iors[arg1] = cpu_state.crom[tmp];
   goto cy3_tail;

  case 0x19U: /* LPM (+) */
   tmp = ((auint)(cpu_state.iors[30])     ) +
         ((auint)(cpu_state.iors[31]) << 8);
   cpu_state.iors[arg1] = cpu_state.crom[tmp];
   tmp ++;
   cpu_state.iors[30] = (tmp     ) & 0xFFU;
   cpu_state.iors[31] = (tmp >> 8) & 0xFFU;
   goto cy3_tail;

  case 0x1AU: /* PUSH */
   tmp = ((auint)(cpu_state.iors[CU_IO_SPL])     ) +
         ((auint)(cpu_state.iors[CU_IO_SPH]) << 8);
   cpu_state.sram[tmp & 0x0FFFU] = cpu_state.iors[arg1];
   access_mem[tmp & 0x0FFFU] |= CU_MEM_W;
   tmp --;
   goto stk_tail;

  case 0x1BU: /* POP */
   tmp = ((auint)(cpu_state.iors[CU_IO_SPL])     ) +
         ((auint)(cpu_state.iors[CU_IO_SPH]) << 8);
   tmp ++;
   cpu_state.iors[arg1] = cpu_state.sram[tmp & 0x0FFFU];
   access_mem[tmp & 0x0FFFU] |= CU_MEM_R;
stk_tail:
   cpu_state.iors[CU_IO_SPL] = (tmp     ) & 0xFFU;
   cpu_state.iors[CU_IO_SPH] = (tmp >> 8) & 0xFFU;
   goto cy2_tail;

  case 0x1CU: /* STS */
   tmp = arg2;
   cpu_state.pc ++;
   goto st_tail;

  case 0x1DU: /* ST */
   tmp = ( ((auint)(cpu_state.iors[(arg2 & 0xFFU) + 0U])     ) +
           ((auint)(cpu_state.iors[(arg2 & 0xFFU) + 1U]) << 8) +
           (arg2 >> 8) ) & 0xFFFFU; /* Mask: Just in case someone is tricky accessing IO */
   goto st_tail;

  case 0x1EU: /* ST (-) */
   tmp = ((auint)(cpu_state.iors[arg2 + 0U])     ) +
         ((auint)(cpu_state.iors[arg2 + 1U]) << 8);
   tmp --;
   cpu_state.iors[arg2 + 0U] = (tmp     ) & 0xFFU;
   cpu_state.iors[arg2 + 1U] = (tmp >> 8) & 0xFFU;
   goto st_tail;

  case 0x1FU: /* ST (+) */
   tmp = ((auint)(cpu_state.iors[arg2 + 0U])     ) +
         ((auint)(cpu_state.iors[arg2 + 1U]) << 8);
   tmp ++;
   cpu_state.iors[arg2 + 0U] = (tmp     ) & 0xFFU;
   cpu_state.iors[arg2 + 1U] = (tmp >> 8) & 0xFFU;
   tmp --;
st_tail:
   UPDATE_HARDWARE;
   UPDATE_HARDWARE_IT;
   if (tmp >= 0x0100U){
    cpu_state.sram[tmp & 0x0FFFU] = cpu_state.iors[arg1];
    access_mem[tmp & 0x0FFFU] |= CU_MEM_W;
   }else{
    cu_avr_write_io(tmp, cpu_state.iors[arg1]);
   }
   goto cy0_tail;

  case 0x20U: /* LDS */
   tmp = arg2;
   cpu_state.pc ++;
   goto ld_tail;

  case 0x21U: /* LD */
   tmp = ( ((auint)(cpu_state.iors[(arg2 & 0xFFU) + 0U])     ) +
           ((auint)(cpu_state.iors[(arg2 & 0xFFU) + 1U]) << 8) +
           (arg2 >> 8) ) & 0xFFFFU; /* Mask: Just in case someone is tricky accessing IO */
   goto ld_tail;

  case 0x22U: /* LD (-) */
   tmp = ((auint)(cpu_state.iors[arg2 + 0U])     ) +
         ((auint)(cpu_state.iors[arg2 + 1U]) << 8);
   tmp --;
   cpu_state.iors[arg2 + 0U] = (tmp     ) & 0xFFU;
   cpu_state.iors[arg2 + 1U] = (tmp >> 8) & 0xFFU;
   goto ld_tail;

  case 0x23U: /* LD (+) */
   tmp = ((auint)(cpu_state.iors[arg2 + 0U])     ) +
         ((auint)(cpu_state.iors[arg2 + 1U]) << 8);
   tmp ++;
   cpu_state.iors[arg2 + 0U] = (tmp     ) & 0xFFU;
   cpu_state.iors[arg2 + 1U] = (tmp >> 8) & 0xFFU;
   tmp --;
ld_tail:
   UPDATE_HARDWARE;
   if (tmp >= 0x0100U){
    cpu_state.iors[arg1] = cpu_state.sram[tmp & 0x0FFFU];
    access_mem[tmp & 0x0FFFU] |= CU_MEM_R;
   }else{
    cpu_state.iors[arg1] = cu_avr_read_io(tmp);
   }
   goto cy1_tail;

  case 0x24U: /* COM */
   res   = cpu_state.iors[arg1] ^ 0xFFU;
   cpu_state.iors[arg1] = res;
   cpu_state.iors[CU_IO_SREG] = (cpu_state.iors[CU_IO_SREG] & (SREG_IM | SREG_TM | SREG_HM)) |
                                (cpu_pflags[CU_AVRFG_LOG + res] | SREG_CM);
   goto cy1_tail;

  case 0x25U: /* NEG */
   src   = cpu_state.iors[arg1];
   dst   = 0x00U;
sub_tail:
   res   = dst - src;
   cpu_state.iors[arg1] = res;
sub_tail_flg:
   cpu_state.iors[CU_IO_SREG] = (cpu_state.iors[CU_IO_SREG] & (SREG_IM | SREG_TM)) |
                                cpu_pflags[CU_AVRFG_SUB + (res & 0x1FFU) + ((src & 0x90U) << 5) + ((dst & 0x90U) << 6)];
   goto cy1_tail;

  case 0x26U: /* SWAP */
   res   = cpu_state.iors[arg1];
   cpu_state.iors[arg1] = (res >> 4) | (res << 4);
   goto cy1_tail;

  case 0x27U: /* INC */
   res   = cpu_state.iors[arg1];
   res ++;
   cpu_state.iors[arg1] = res;
   cpu_state.iors[CU_IO_SREG] = (cpu_state.iors[CU_IO_SREG] & (SREG_IM | SREG_TM | SREG_HM | SREG_CM)) |
                                cpu_pflags[CU_AVRFG_INC + (res & 0xFFU)];
   goto cy1_tail;

  case 0x28U: /* ASR */
   src   = cpu_state.iors[arg1];
   res   = (src & 0x80U);
   goto shr_tail;

  case 0x29U: /* LSR */
   src   = cpu_state.iors[arg1];
   res   = 0U;
   goto shr_tail;

  case 0x2AU: /* ROR */
   flags = cpu_state.iors[CU_IO_SREG];
   src   = cpu_state.iors[arg1];
   res   = (SREG_GET_C(flags) << 7);
shr_tail:
   res  |= (src >> 1);
   cpu_state.iors[arg1] = res;
   cpu_state.iors[CU_IO_SREG] = (cpu_state.iors[CU_IO_SREG] & (SREG_IM | SREG_TM | SREG_HM)) |
                                cpu_pflags[CU_AVRFG_SHR + ((src & 1U) << 8) + res];
   goto cy1_tail;

  case 0x2BU: /* DEC */
   res   = cpu_state.iors[arg1];
   res --;
   cpu_state.iors[arg1] = res;
   cpu_state.iors[CU_IO_SREG] = (cpu_state.iors[CU_IO_SREG] & (SREG_IM | SREG_TM | SREG_HM | SREG_CM)) |
                                cpu_pflags[CU_AVRFG_DEC + (res & 0xFFU)];
   goto cy1_tail;

  case 0x2CU: /* JMP */
   cpu_state.pc = arg2;
   goto cy3_tail;

  case 0x2DU: /* CALL */
   res   = arg2;
   cpu_state.pc ++;
   UPDATE_HARDWARE;
   goto call_tail;

  case 0x2EU: /* BSET */
   flags = cpu_state.iors[CU_IO_SREG];
   cpu_state.iors[CU_IO_SREG] |=  arg1;
   if ((((~flags) & arg1) & SREG_IM) != 0U){
    event_it = TRUE; /* Interrupts become enabled, so check them */
   }
   goto cy1_tail;

  case 0x2FU: /* BCLR */
   cpu_state.iors[CU_IO_SREG] &= ~arg1;
   goto cy1_tail;

  case 0x30U: /* IJMP */
   tmp   = ((auint)(cpu_state.iors[30])     ) +
           ((auint)(cpu_state.iors[31]) << 8);
   cpu_state.pc = tmp;
   goto cy2_tail;

  case 0x31U: /* RET */
   goto ret_tail;

  case 0x32U: /* ICALL */
   res   = ((auint)(cpu_state.iors[30])     ) +
           ((auint)(cpu_state.iors[31]) << 8);
   goto call_tail;

  case 0x33U: /* RETI */
   flags = cpu_state.iors[CU_IO_SREG];
   SREG_SET(flags, SREG_IM);
   event_it = TRUE; /* Interrupts (might) become enabled, so check them */
   cpu_state.iors[CU_IO_SREG] = flags;
ret_tail:
   tmp   = ((auint)(cpu_state.iors[CU_IO_SPL])     ) +
           ((auint)(cpu_state.iors[CU_IO_SPH]) << 8);
   tmp ++;
   cpu_state.pc  = (auint)(cpu_state.sram[tmp & 0x0FFFU]) << 8;
   access_mem[tmp & 0x0FFFU] |= CU_MEM_R;
   tmp ++;
   cpu_state.pc |= (auint)(cpu_state.sram[tmp & 0x0FFFU]);
   access_mem[tmp & 0x0FFFU] |= CU_MEM_R;
   cpu_state.iors[CU_IO_SPL] = (tmp     ) & 0xFFU;
   cpu_state.iors[CU_IO_SPH] = (tmp >> 8) & 0xFFU;
   goto cy4_tail;

  case 0x34U: /* SLEEP */
   /* Will implement later */
   goto cy1_tail;

  case 0x35U: /* BREAK */
   /* No operation */
   goto cy1_tail;

  case 0x36U: /* WDR */
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
   goto cy1_tail;

  case 0x37U: /* MUL */
   dst   = cpu_state.iors[arg1];
   src   = cpu_state.iors[arg2];
mul_tail:
   res   = dst * src;
   flags = cpu_state.iors[CU_IO_SREG];
   PROCFLAGS_MUL(flags, res);
mul_tail_fmul:
   cpu_state.iors[0x00U] = (res     ) & 0xFFU;
   cpu_state.iors[0x01U] = (res >> 8) & 0xFFU;
   cpu_state.iors[CU_IO_SREG] = flags;
   goto cy2_tail;

  case 0x38U: /* IN */
   cpu_state.iors[arg1] = cu_avr_read_io(arg2);
   goto cy1_tail;

  case 0x39U: /* OUT */
   tmp = cpu_state.iors[arg2];
   goto out_tail;

  case 0x3AU: /* ADIW */
   flags = cpu_state.iors[CU_IO_SREG];
   dst   = ((auint)(cpu_state.iors[arg1 + 0U])     ) +
           ((auint)(cpu_state.iors[arg1 + 1U]) << 8);
   src   = arg2; /* Flags are simplified assuming this is less than 0x8000 (it is so on AVR) */
   res   = dst + src;
   SREG_CLR(flags, SREG_CM | SREG_ZM | SREG_NM | SREG_VM | SREG_SM);
   SREG_SET(flags, SREG_VM & (((~dst) & (res)) >> (15U - SREG_V)));
   goto adiw_tail;

  case 0x3BU: /* SBIW */
   flags = cpu_state.iors[CU_IO_SREG];
   dst   = ((auint)(cpu_state.iors[arg1 + 0U])     ) +
           ((auint)(cpu_state.iors[arg1 + 1U]) << 8);
   src   = arg2; /* Flags are simplified assuming this is less than 0x8000 (it is so on AVR) */
   res   = dst - src;
   SREG_CLR(flags, SREG_CM | SREG_ZM | SREG_NM | SREG_VM | SREG_SM);
   SREG_SET(flags, SREG_VM & (((dst) & (~res)) >> (15U - SREG_V)));
adiw_tail:
   cpu_state.iors[arg1 + 0U] = (res     ) & 0xFFU;
   cpu_state.iors[arg1 + 1U] = (res >> 8) & 0xFFU;
   SREG_SET(flags, SREG_NM & ((          res ) >> (15U - SREG_N)));
   SREG_SET_C_BIT16(flags, res);
   SREG_SET_Z(flags, res & 0xFFFFU);
   SREG_COM_NV(flags);
   cpu_state.iors[CU_IO_SREG] = flags;
   goto cy2_tail;

  case 0x3CU: /* CBI */
   tmp   = cu_avr_read_io(arg1) & (~arg2);
   goto oub_tail;

  case 0x3DU: /* SBIC */
   if ((cu_avr_read_io(arg1) & arg2) != 0U){
    goto cy1_tail;
   }
   goto skip_tail;

  case 0x3EU: /* SBI */
   tmp   = cu_avr_read_io(arg1) | ( arg2);
oub_tail:
   UPDATE_HARDWARE;
out_tail:
   UPDATE_HARDWARE_IT;
   cu_avr_write_io(arg1, tmp);
   goto cy0_tail;

  case 0x3FU: /* SBIS */
   if ((cu_avr_read_io(arg1) & arg2) == 0U){
    goto cy1_tail;
   }
   goto skip_tail;

  case 0x40U: /* RJMP */
   cpu_state.pc += arg2;
   goto cy2_tail;

  case 0x41U: /* RCALL */
   res   = cpu_state.pc + arg2;
call_tail:
   tmp   = ((auint)(cpu_state.iors[CU_IO_SPL])     ) +
           ((auint)(cpu_state.iors[CU_IO_SPH]) << 8);
   cpu_state.sram[tmp & 0x0FFFU] = (cpu_state.pc     ) & 0xFFU;
   access_mem[tmp & 0x0FFFU] |= CU_MEM_W;
   tmp --;
   cpu_state.sram[tmp & 0x0FFFU] = (cpu_state.pc >> 8) & 0xFFU;
   access_mem[tmp & 0x0FFFU] |= CU_MEM_W;
   tmp --;
   cpu_state.iors[CU_IO_SPL] = (tmp     ) & 0xFFU;
   cpu_state.iors[CU_IO_SPH] = (tmp >> 8) & 0xFFU;
   cpu_state.pc = res;
   goto cy3_tail;

  case 0x42U: /* BRBS */
   if ((cpu_state.iors[CU_IO_SREG] & arg1) == 0U){
    goto cy1_tail;
   }
   cpu_state.pc += arg2;
   goto cy2_tail;

  case 0x43U: /* BRBC */
   if ((cpu_state.iors[CU_IO_SREG] & arg1) != 0U){
    goto cy1_tail;
   }
   cpu_state.pc += arg2;
   goto cy2_tail;

  case 0x44U: /* BLD */
   src   = (cpu_state.iors[CU_IO_SREG] >> SREG_T) & 1U;
   tmp   = cpu_state.iors[arg1];
   tmp   = (tmp & (~(1U << arg2))) | (src << arg2);
   cpu_state.iors[arg1] = tmp;
   goto cy1_tail;

  case 0x45U: /* BST */
   flags = cpu_state.iors[CU_IO_SREG] & (~(auint)(SREG_TM));
   flags = flags | (((cpu_state.iors[arg1] >> arg2) & 1U) << SREG_T);
   cpu_state.iors[CU_IO_SREG] = flags;
   goto cy1_tail;

  case 0x46U: /* SBRC */
   if ((cpu_state.iors[arg1] & arg2) != 0U){
    goto cy1_tail;
   }
   goto skip_tail;

  case 0x47U: /* SBRS */
   if ((cpu_state.iors[arg1] & arg2) == 0U){
    goto cy1_tail;
   }
skip_tail:
   if (((cpu_code[cpu_state.pc & 0x7FFFU] >> 7) & 1U) != 0U){
    cpu_state.pc += 2U;
    goto cy3_tail;
   }
   cpu_state.pc ++;
   goto cy2_tail;

  case 0x48U: /* LDI */
   cpu_state.iors[arg1] = arg2;
   goto cy1_tail;

  case 0x49U: /* PIXEL */
   /* Note: Normally should execute after UPDATE_HARDWARE, here it doesn't
   ** matter (just shifts visual output one cycle left) */
   cpu_state.iors[CU_IO_PORTC] = cpu_state.iors[arg2] &
                                 cpu_state.iors[CU_IO_DDRC];
   goto cy1_tail;

  default:
   /* Undefined op. error here, implement! */
   goto cy1_tail;

cy4_tail:
   UPDATE_HARDWARE;
cy3_tail:
   UPDATE_HARDWARE;
cy2_tail:
   UPDATE_HARDWARE;
cy1_tail:
   UPDATE_HARDWARE_IT;
cy0_tail:

   /* Enter interrupt if flagged so */
   if (event_it_enter){ cu_avr_interrupt(); }
   return;

 }

 /* Control should never reach here (no "break" in the switch) */
}
