/*
 *  AVR microcontroller emulation
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



#include "cu_avr.h"
#include "cu_avrc.h"
#include "cu_avrfg.h"
#include "cu_ctr.h"
#include "cu_spi.h"



/* Marker for this compilation unit for sub-includes */
#define CU_AVR_C



/* CPU state */
cu_state_cpu_t  cpu_state;

/* Compiled AVR instructions */
uint32          cpu_code[32768];

/* Access info structure for SRAM */
uint8           access_mem[4096U];

/* Access info structure for I/O */
uint8           access_io[256U];

/* Precalculated flags */
uint8           cpu_pflags[CU_AVRFG_SIZE];

/* Whether the flags were already precalculated */
boole           pflags_done = FALSE;

/* Row generation structure */
cu_row_t        video_row;

/* Frame information structure */
cu_frameinfo_t  video_frame;

/* Sync pulse counter (0 - 270) */
auint           video_pulsectr;

/* Cycle of previous edge of sync signal */
auint           video_pedge;

/* Cycle of previous rising edge of sync signal (used to find VSync) */
auint           video_prise;

/* Row generation flag (passes when a row has to be signalled) */
boole           video_rowflag;

/* Cycle counter within row. At most approx. 2010 */
auint           video_cycle;

/* A tiny sample FIFO to align sample output with rows cleanly */
auint           audio_samples[4U];
auint           audio_rp;
auint           audio_wp;

/* Next hardware event's cycle. It can be safely set to
** WRAP32(cpu_state.cycle + 1U) to force full processing next time. */
auint           cycle_next_event;

/* Timer1 TCNT1 adjustment value: WRAP32(cpu_state.cycle - timer1_base)
** gives the correct TCNT1 any time. */
auint           timer1_base;

/* Interrupt might be waiting to be serviced flag. This is set nonzero by
** any event which should trigger an IT including setting the I flag in the
** status register. It can be safely set nonzero to ask for a certain IT
** check. */
boole           event_it;

/* Interrupt entry necessary if set. */
boole           event_it_enter;

/* Vector to call when entering interrupt */
auint           event_it_vect;

/* EEPROM change indicator */
boole           eeprom_changed;

/* Watchdog-based debug counter: Time of last WDR execution */
auint           wd_last;

/* Watchdog-based debug counter: PC of last WDR execution */
auint           wd_last_pc;

/* Watchdog-based debug counter: Smallest interval between two WDR calls in a
** frame. Index 0 is the work value, Index 1 is the latched value. */
auint           wd_interval_min[2];

/* Watchdog-based debug counter: PC of baginning WDR of interval. Index 0 is
** the work value, Index 1 is the latched value. */
auint           wd_interval_beg[2];

/* Watchdog-based debug counter: PC of ending WDR of interval. Index 0 is the
** work value, Index 1 is the latched value. */
auint           wd_interval_end[2];



/* Watchdog 16 millisecond timer base tick count */
#define WD_16MS_BASE (458176U - 1024U)

/* Watchdog timing seed mask base, used to mask for the 16 ms timer */
#define WD_SEED_MASK 2048U

/* EEPROM programming time base, assume ~1.75ms */
#define EEPROM_EWR_TIM 50000U

/* SPM programming time base, assuming ~4ms */
#define SPM_PROG_TIM 100000U

/* Maximal cycles in a video scanline (above which sync error is returned) */
#define VIDEO_CY_MAX 1920U

/* Watchdog-based debug counter: Maximal interval to display */
#define WD_INTERVAL_MAX (VIDEO_CY_MAX * 262U / 2U)


/* Flags in CU_IO_SREG */
#define SREG_I  7U
#define SREG_T  6U
#define SREG_H  5U
#define SREG_S  4U
#define SREG_V  3U
#define SREG_N  2U
#define SREG_Z  1U
#define SREG_C  0U
#define SREG_IM 0x80U
#define SREG_TM 0x40U
#define SREG_HM 0x20U
#define SREG_SM 0x10U
#define SREG_VM 0x08U
#define SREG_NM 0x04U
#define SREG_ZM 0x02U
#define SREG_CM 0x01U


/* Macros for managing the flags */

/* Clear flags by mask */
#define SREG_CLR(fl, xm) (fl &= (auint)(~((auint)(xm))))

/* Set flags by mask */
#define SREG_SET(fl, xm) (fl |= (xm))

/* Set Zero if (at most) 16 bit "val" is zero */
#define SREG_SET_Z(fl, val) (fl |= SREG_ZM & (((auint)(val) - 1U) >> 16))

/* Set Carry by bit 15 (for multiplications) */
#define SREG_SET_C_BIT15(fl, val) (fl |= ((val) >> 15) & 1U)

/* Set Carry by bit 16 (for float multiplications and adiw & sbiw) */
#define SREG_SET_C_BIT16(fl, val) (fl |= ((val) >> 16) & 1U)

/* Combine N and V into S (for all ops updating N or V) */
#define SREG_COM_NV(fl) (fl |= (((fl) << 1) ^ ((fl) << 2)) & 0x10U)

/* Get carry flag (for carry overs) */
#define SREG_GET_C(fl) ((fl) & 1U)


/* Macro for calculating a multiplication's flags */
#define PROCFLAGS_MUL(fl, res) \
 do{ \
  SREG_CLR(fl, SREG_CM | SREG_ZM); \
  SREG_SET_C_BIT15(fl, res); \
  SREG_SET_Z(fl, res & 0xFFFFU); \
 }while(0)

/* Macro for calculating a floating multiplication's flags */
#define PROCFLAGS_FMUL(fl, res) \
 do{ \
  SREG_CLR(fl, SREG_CM | SREG_ZM); \
  SREG_SET_C_BIT16(fl, res); \
  SREG_SET_Z(fl, res & 0xFFFFU); \
 }while(0)


/* Macro for updating hardware from within instructions */
#define UPDATE_HARDWARE \
 do{ \
  cpu_state.cycle = WRAP32(cpu_state.cycle + 1U); \
  if (cycle_next_event == cpu_state.cycle){ cu_avr_hwexec(); } \
  video_row.pixels[video_cycle] = cpu_state.iors[CU_IO_PORTC]; \
  video_cycle ++; \
 }while(0)

/* Macro for consuming last instruction cycle before which ITs are triggered */
#define UPDATE_HARDWARE_IT \
 do{ \
  if (event_it){ cu_avr_itcheck(); } \
  UPDATE_HARDWARE; \
 }while(0)


/* Vectors base when IVSEL is 1 (MCUCR.1; Boot loader base) */
#define VBASE_BOOT     0x7800U


/* Various vectors */

/* Reset */
#define VECT_RESET     0x0000U
/* Watchdog */
#define VECT_WDT       0x0010U
/* Timer1 Comparator A */
#define VECT_T1COMPA   0x001AU
/* Timer1 Comparator B */
#define VECT_T1COMPB   0x001CU
/* Timer1 Overflow */
#define VECT_T1OVF     0x001EU
/* SPI */
#define VECT_SPISTC    0x0026U



/*
** Get Watchdog timeout ticks (WDP3 ignored, should be zero)
*/
static auint cu_avr_getwdto(void)
{
 auint presc = cpu_state.iors[CU_IO_WDTCSR] & 7U;
 return ( ((auint)(WD_16MS_BASE) << presc) +
          (cpu_state.wd_seed & (((auint)(WD_SEED_MASK) << presc) - 1U)) );
}



/*
** Emulates cycle-precise hardware tasks. This is called through the
** UPDATE_HARDWARE macro if cycle_next_event matches the cycle counter (a new
** HW event is to be processed).
*/
static void cu_avr_hwexec(void)
{
 auint nextev = ~0U;
 auint t0;
 auint t1;
 auint t2;

 /* Timer 1 */

 if ((cpu_state.iors[CU_IO_TCCR1B] & 0x07U) != 0U){ /* Timer 1 started */

  t0 = (cpu_state.cycle - timer1_base) & 0xFFFFU;   /* Current TCNT1 value */
  t1 = ( ( ((auint)(cpu_state.iors[CU_IO_OCR1AL])     ) |
           ((auint)(cpu_state.iors[CU_IO_OCR1AH]) << 8) ) + 1U) & 0xFFFFU;
  t2 = ( ( ((auint)(cpu_state.iors[CU_IO_OCR1BL])     ) |
           ((auint)(cpu_state.iors[CU_IO_OCR1BH]) << 8) ) + 1U) & 0xFFFFU;

  if ((cpu_state.iors[CU_IO_TCCR1B] & 0x08U) != 0U){ /* Timer 1 in CTC mode: Counts to OCR1A, then resets */

   if (t0 == 0x0000U){                    /* Timer overflow (might happen if it starts above Comp. A, or Comp. A is 0) */
    cpu_state.iors[CU_IO_TIFR1] |= 0x01U;
    event_it = TRUE;
   }

   if (t0 == t2){
    cpu_state.iors[CU_IO_TIFR1] |= 0x04U; /* Comparator B interrupt */
    event_it = TRUE;
   }

   if (t0 == t1){
    cpu_state.iors[CU_IO_TIFR1] |= 0x02U; /* Comparator A interrupt */
    event_it = TRUE;
    timer1_base = cpu_state.cycle;        /* Reset timer to zero */
    t0 = 0U;                              /* Also reset for event calculation */
   }

   if ( (t0 != t2) &&
        (nextev > (t2 - t0)) ){ nextev = t2 - t0; } /* Next Comp. B match */
   if ( (nextev > (t1 - t0)) ){ nextev = t1 - t0; } /* Next Comp. A match */
   if ( (nextev > (0x10000U - t0)) ){ nextev = 0x10000U - t0; } /* Next Overflow (if timer was set above Comp. A) */

  }else{ /* Timer 1 in normal mode: wrapping 16 bit counter */

   if (t0 == 0x0000U){
    cpu_state.iors[CU_IO_TIFR1] |= 0x01U; /* Overflow interrupt */
    event_it = TRUE;
    timer1_base = cpu_state.cycle;        /* Reset timer to zero */
   }

   if (nextev > (0x10000U - t0)){ nextev = 0x10000U - t0; }

  }

 }

 /* Watchdog (in Uzebox used to seed random number generators) */

 if ((cpu_state.iors[CU_IO_WDTCSR] & 0x48U) != 0U){ /* Watchdog is operational */

  /* Assume interrupt mode (this is used for random number generator seeding) */

  if (cpu_state.cycle == cpu_state.wd_end){
   cpu_state.iors[CU_IO_WDTCSR] |= 0x80U; /* Watchdog interrupt */
   event_it = TRUE;
   cpu_state.wd_end  = WRAP32(cu_avr_getwdto() + cpu_state.cycle);
  }

  t0 = WRAP32(cpu_state.wd_end - cpu_state.cycle);
  if (nextev > t0){ nextev = t0; }

 }

 /* SPI peripherals (SD card, SPI RAM) */

 if (cpu_state.spi_tran){

  if (cpu_state.cycle == cpu_state.spi_end){

   cpu_state.spi_tran = FALSE;
   cpu_state.iors[CU_IO_SPSR] |= 0x80U; /* SPI interrupt */
   event_it = TRUE;
   cpu_state.iors[CU_IO_SPDR] = cpu_state.spi_rx;
   cu_spi_send(cpu_state.spi_tx, cpu_state.cycle);

  }else{

   t0 = WRAP32(cpu_state.spi_end - cpu_state.cycle);
   if (nextev > t0){ nextev = t0; }

  }

 }

 /* EEPROM */

 if (cpu_state.eep_wrte){

  if (cpu_state.cycle == cpu_state.eep_end){

   cpu_state.eep_wrte = FALSE;
   t0 = cpu_state.iors[CU_IO_EECR] & 0x30U; /* EEPROM write mode */
   t1 = ( ((auint)(cpu_state.iors[CU_IO_EEARH]) << 8) |
          ((auint)(cpu_state.iors[CU_IO_EEARL])     ) ) & 0x7FFU;
   t2 = cpu_state.eepr[t1];
   if       (t0 == 0x00U){ /* Erase and Write */
    cpu_state.eepr[t1]  = cpu_state.iors[CU_IO_EEDR];
   }else if (t0 == 0x10U){ /* Erase only */
    cpu_state.eepr[t1]  = 0xFFU;
   }else if (t0 == 0x20U){ /* Write only */
    cpu_state.eepr[t1] |= cpu_state.iors[CU_IO_EEDR];
   }else{                  /* Reserved: Do nothing */
   }
   if (t2 != t1){ eeprom_changed = TRUE; }
   cpu_state.iors[CU_IO_EECR] &= ~0x02U; /* Clear EEPE (programming completed) */

  }else{

   t0 = WRAP32(cpu_state.eep_end - cpu_state.cycle);
   if (nextev > t0){ nextev = t0; }

  }

 }else if ((cpu_state.iors[CU_IO_EECR] & 0x04U) != 0U){

  if (cpu_state.cycle == cpu_state.eep_end){
   cpu_state.iors[CU_IO_EECR] &= ~0x04U; /* Clear EEMPE (disable programming) */
  }else{
   t0 = WRAP32(cpu_state.eep_end - cpu_state.cycle);
   if (nextev > t0){ nextev = t0; }
  }

 }else{}

 /* SPM */

 if (cpu_state.spm_prge){

  if (cpu_state.cycle == cpu_state.spm_end){

   cpu_state.spm_prge = FALSE; /* Completed erasing / programming */
   cpu_state.iors[CU_IO_SPMCSR] &= ~0x1FU;

  }else{

   t0 = WRAP32(cpu_state.spm_end - cpu_state.cycle);
   if (nextev > t0){ nextev = t0; }

  }

 }else if ((cpu_state.iors[CU_IO_SPMCSR] & 0x01U) != 0U){

  if (cpu_state.cycle == cpu_state.spm_end){
   cpu_state.iors[CU_IO_SPMCSR] &= ~0x01U; /* Cancel SPM instruction */
  }else{
   t0 = WRAP32(cpu_state.spm_end - cpu_state.cycle);
   if (nextev > t0){ nextev = t0; }
  }

 }else{}

 /* Calculate next event's cycle */

 cycle_next_event = WRAP32(cpu_state.cycle + nextev);
}



/*
** Enters requested interrupt (event_it_enter must be true)
*/
static void cu_avr_interrupt(void)
{
 auint tmp;

 event_it_enter = FALSE; /* Requested IT entry performed */

 SREG_CLR(cpu_state.iors[CU_IO_SREG], SREG_IM);

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

 cpu_state.pc = event_it_vect;

 UPDATE_HARDWARE;
 UPDATE_HARDWARE;
 UPDATE_HARDWARE;
}



/*
** Checks for interrupts and triggers if any is pending. Clears event_it when
** there are no more interrupts waiting for servicing.
*/
static void cu_avr_itcheck(void)
{
 auint vbase;

 /* Global interrupt enable? */

 if ((cpu_state.iors[CU_IO_SREG] & SREG_IM) == 0U){
  event_it = FALSE;
  return;
 }

 /* Interrupts are enabled, so check them */

 vbase = ((cpu_state.iors[CU_IO_MCUCR] >> 1) & 1U) * VBASE_BOOT;

 if       ( (cpu_state.iors[CU_IO_SPCR] &
             cpu_state.iors[CU_IO_SPSR] & 0x80U)   !=    0U ){ /* SPI */

  cpu_state.iors[CU_IO_SPSR] ^= 0x80U;
  event_it_enter = TRUE;
  event_it_vect  = vbase + VECT_SPISTC;

 }else if ( (cpu_state.iors[CU_IO_WDTCSR] & 0xC0U) == 0xC0U ){ /* Watchdog */

  cpu_state.iors[CU_IO_WDTCSR] ^= 0x80U; /* Assume interrupt mode (only clearing the WDIF flag) */
  event_it_enter = TRUE;
  event_it_vect  = vbase + VECT_WDT;

 }else if ( (cpu_state.iors[CU_IO_TIFR1] &
             cpu_state.iors[CU_IO_TIMSK1] & 0x02U) !=    0U ){ /* Timer 1 Comparator A */

  cpu_state.iors[CU_IO_TIFR1] ^= 0x02U;
  event_it_enter = TRUE;
  event_it_vect  = vbase + VECT_T1COMPA;

 }else if ( (cpu_state.iors[CU_IO_TIFR1] &
             cpu_state.iors[CU_IO_TIMSK1] & 0x04U) !=    0U ){ /* Timer 1 Comparator B */

  cpu_state.iors[CU_IO_TIFR1] ^= 0x04U;
  event_it_enter = TRUE;
  event_it_vect  = vbase + VECT_T1COMPB;

 }else if ( (cpu_state.iors[CU_IO_TIFR1] &
             cpu_state.iors[CU_IO_TIMSK1] & 0x01U) !=    0U ){ /* Timer 1 Overflow */

  cpu_state.iors[CU_IO_TIFR1] ^= 0x01U;
  event_it_enter = TRUE;
  event_it_vect  = vbase + VECT_T1OVF;

 }else{ /* No interrupts are pending */

  event_it = FALSE;

 }
}



/*
** Writes an I/O port
*/
static void  cu_avr_write_io(auint port, auint val)
{
 auint pval = cpu_state.iors[port]; /* Previous value */
 auint cval = val & 0xFFU;          /* Current (requested) value */
 auint pio;
 auint cio;
 auint t0;
 auint t1;

 access_io[port] |= CU_MEM_W;

 switch (port){

  case CU_IO_PORTA:   /* Controller inputs, SPI RAM Chip Select */
  case CU_IO_DDRA:

   if (port == CU_IO_PORTA){
    pio = pval & cpu_state.iors[CU_IO_DDRA];
    cio = cval & cpu_state.iors[CU_IO_DDRA];
   }else{
    pio = pval & cpu_state.iors[CU_IO_PORTA];
    cio = cval & cpu_state.iors[CU_IO_PORTA];
   }
   cpu_state.iors[CU_IO_PINA] = cu_ctr_process(pio, cio);
   cu_spi_cs_set(CU_SPI_CS_RAM, (cio & 0x10U) == 0U, cpu_state.cycle);
   break;

  case CU_IO_PORTB:   /* Sync output */
  case CU_IO_DDRB:

   if (port == CU_IO_PORTB){
    pio = pval & cpu_state.iors[CU_IO_DDRB];
    cio = cval & cpu_state.iors[CU_IO_DDRB];
   }else{
    pio = pval & cpu_state.iors[CU_IO_PORTB];
    cio = cval & cpu_state.iors[CU_IO_PORTB];
   }

   if (((pio ^ cio) & 1U) != 0U){   /* Sync edge */

    t0 = WRAP32(cpu_state.cycle - video_pedge); /* Cycles elapsed since previous edge */
    video_pedge = cpu_state.cycle;

    if ((cio & 1U) == 1U){      /* Rising edge */

     if ( (video_pulsectr <= 268U) &&
          (video_pulsectr != 251U) ){
      video_pulsectr ++;
     }
     if (video_pulsectr <  252U){
      video_frame.rowcdif ++;
     }
     if (video_pulsectr == 270U){
      video_pulsectr      = 0U;
      video_frame.rowcdif = 0U - 252U;
     }
     t1 = WRAP32(cpu_state.cycle - video_prise);
     video_prise = cpu_state.cycle;
     if       ( (t1 >=  944U) &&
                (t1 <= 1012U) && /* Sync to first normal pulse (978 cycles apart from last rise) */
                (video_pulsectr >= 252U) ){
      video_pulsectr = 270U;
     }else if ( (t1 >= 1718U) &&
                (t1 <= 1786U) ){ /* Sync to first VSync pulse (1752 cycles apart from last rise) */
      video_pulsectr = 252U;
     }else{}

     if ( (video_pulsectr < 252U) ||
          (video_pulsectr == 270U) ){ /* 0 - 251 & 270 are normal rises 136 cycles after the fall */
      video_frame.pulse[video_pulsectr].rise = t0 - 136U;
     }else{
      switch (video_pulsectr){
       case 252U:
       case 253U:
       case 254U:
       case 255U:
       case 256U:
       case 257U:                /* 251 - 257 come 68 cycles after the fall */
        video_frame.pulse[video_pulsectr].rise = t0 - 68U;
        break;
       case 258U:
       case 259U:
       case 260U:
       case 261U:
       case 262U:
       case 263U:                /* 258 - 263 come 774 cycles after the fall */
        video_frame.pulse[video_pulsectr].rise = t0 - 774U;
        break;
       case 264U:
       case 265U:
       case 266U:
       case 267U:
       case 268U:
       case 269U:                /* 264 - 269 come 68 cycles after the fall */
        video_frame.pulse[video_pulsectr].rise = t0 - 68U;
        break;
       default:                  /* Out of sync */
        break;
      }
     }

    }else{                       /* Falling edge */

     if ( (video_pulsectr < 252U) ||
          (video_pulsectr == 270U) ){ /* 0 - 251 & 270 are normal falls 1684 cycles after the rise */
      video_frame.pulse[video_pulsectr].fall = t0 - 1684U;
      video_rowflag = TRUE;      /* Trigger new row */
      video_cycle   = VIDEO_CY_MAX; /* Also flags new row */
     }else{
      switch (video_pulsectr){
       case 252U:
       case 253U:
       case 254U:
       case 255U:
       case 256U:
       case 257U:                /* 252 - 257 come 842 cycles after the rise */
        video_frame.pulse[video_pulsectr].fall = t0 - 842U;
        break;
       case 258U:
       case 259U:
       case 260U:
       case 261U:
       case 262U:
       case 263U:                /* 258 - 263 come 136 cycles after the rise */
        video_frame.pulse[video_pulsectr].fall = t0 - 136U;
        break;
       case 264U:
       case 265U:
       case 266U:
       case 267U:
       case 268U:
       case 269U:                /* 264 - 269 come 842 cycles after the rise */
        video_frame.pulse[video_pulsectr].fall = t0 - 842U;
        break;
       default:                  /* Out of sync */
        break;
      }
      if ((video_pulsectr & 1U) != 0U){
       video_rowflag = TRUE;    /* Odd pulses trigger new row */
       video_cycle   = VIDEO_CY_MAX; /* Also flags new row */
      }
     }

    }

   }

   break;

  case CU_IO_PORTC:   /* Pixel output */

   /* Special shortcut masking with the DDR register. This port tolerates a
   ** bit of inaccuracy since it is only graphics and is most frequently
   ** written "normally" (the DDR is used for fade effects on it) */
   cval &= cpu_state.iors[CU_IO_DDRC];
   break;

  case CU_IO_PORTD:   /* SD card Chip Select */
  case CU_IO_DDRD:

   if (port == CU_IO_PORTD){
    cio = cval & cpu_state.iors[CU_IO_DDRD];
   }else{
    cio = cval & cpu_state.iors[CU_IO_PORTD];
   }
   cu_spi_cs_set(CU_SPI_CS_SD, (cio & 0x40U) == 0U, cpu_state.cycle);
   break;

  case CU_IO_OCR2A:   /* PWM audio output */

   if (cpu_state.iors[CU_IO_TCCR2B] != 0U){
    audio_samples[audio_wp] = cval;
    if (audio_rp == audio_wp){ audio_rp = (audio_rp + 1U) & 0x3U; }
    audio_wp = (audio_wp + 1U) & 0x3U;
   }
   break;

  case CU_IO_TCNT1H:  /* Timer1 counter, high */

   cpu_state.latch = cval; /* Write into latch (value written to the port itself is ignored) */
   break;

  case CU_IO_TCNT1L:  /* Timer1 counter, low */

   t0    = (cpu_state.latch << 8) | cval;
   timer1_base = WRAP32(cpu_state.cycle - t0);
   cycle_next_event = WRAP32(cpu_state.cycle + 1U); /* Request HW processing */
   break;

  case CU_IO_TIFR1:   /* Timer1 interrupt flags */

   cval  = pval & (~cval);
   break;

  case CU_IO_TCCR1B:  /* Timer1 control */
  case CU_IO_OCR1AH:  /* Timer1 comparator A, high */
  case CU_IO_OCR1AL:  /* Timer1 comparator A, low */
  case CU_IO_OCR1BH:  /* Timer1 comparator B, high */
  case CU_IO_OCR1BL:  /* Timer1 comparator B, low */

   cycle_next_event = WRAP32(cpu_state.cycle + 1U); /* Request HW processing */
   break;

  case CU_IO_SPDR:    /* SPI data */

   cpu_state.iors[CU_IO_SPSR] &= ~0x80U;
   /* Note: By the doc first a read would be necessary for clearing SPIF here,
   ** but that's a very unusual use case, so ignored */
   if ((cpu_state.iors[CU_IO_SPCR] & 0x40U) != 0U){ /* SPI enabled */
    if (cpu_state.spi_tran){                /* Already sending */
     cpu_state.iors[CU_IO_SPSR] |= 0x40U;   /* Signal write collision */
    }else{
     cpu_state.spi_tran = TRUE;
     cpu_state.spi_end  = WRAP32( cpu_state.cycle +
          (16U << ( ((cpu_state.iors[CU_IO_SPCR] & 0x3U) << 1) |
                    ((cpu_state.iors[CU_IO_SPSR] & 0x1U) ^ 1U) )) );
     if ( (WRAP32(cycle_next_event  - cpu_state.cycle)) >
          (WRAP32(cpu_state.spi_end - cpu_state.cycle)) ){
      cycle_next_event = cpu_state.spi_end; /* Set SPI HW processing target */
     }
     cpu_state.spi_rx = cu_spi_recv(cpu_state.cycle);
     cpu_state.spi_tx = cval;
    }
   }
   break;

  case CU_IO_SPCR:    /* SPI control */

   break;

  case CU_IO_SPSR:    /* SPI status */

   break;

  case CU_IO_EECR:    /* EEPROM control */

   if ((pval & 0x04U) == 0U){
    cval &= ~0x02U;   /* Without EEMPE, programming (EEPE) can not start */
   }else{
    cpu_state.eep_end  = WRAP32(cpu_state.cycle + 4U); /* Open EEPE window (4 cycles) */
    if ( (WRAP32(cycle_next_event  - cpu_state.cycle)) >
         (WRAP32(cpu_state.eep_end - cpu_state.cycle)) ){
     cycle_next_event = cpu_state.eep_end; /* Set EEPROM HW processing target */
    }
   }

   if ( ((pval & 0x02U) == 0U) &&
        ((cval & 0x02U) != 0U) ){ /* Programming started */
    cpu_state.eep_wrte = TRUE;
    t0 = cpu_state.iors[CU_IO_EECR] & 0x30U;  /* EEPROM write mode */
    cpu_state.eep_end  = EEPROM_EWR_TIM;
    if (t0 == 0U){ cpu_state.eep_end *= 2U; } /* Erase + Write */
    cpu_state.eep_end  = WRAP32(cpu_state.eep_end + cpu_state.cycle);
    if ( (WRAP32(cycle_next_event  - cpu_state.cycle)) >
         (WRAP32(cpu_state.eep_end - cpu_state.cycle)) ){
     cycle_next_event = cpu_state.eep_end; /* Set EEPROM HW processing target */
    }
    UPDATE_HARDWARE;  /* 2 cycles write stall. */
    UPDATE_HARDWARE;  /* Note: IT checks are slightly off due to this, but this inaccuracy is tolerable. */
    cval &= ~0x04U;   /* Turn off EEMPE (succesfully entered programming) */
   }

   if (cval & 0x01U){ /* EEPROM read (EERE) strobe */
    if (!cpu_state.eep_wrte){ /* During writing it can't be done */
     t0 = ( ((auint)(cpu_state.iors[CU_IO_EEARH]) << 8) |
            ((auint)(cpu_state.iors[CU_IO_EEARL])     ) ) & 0x7FFU;
     cpu_state.iors[CU_IO_EEDR] = cpu_state.eepr[t0];
     UPDATE_HARDWARE;
     UPDATE_HARDWARE;
     UPDATE_HARDWARE; /* 4 cycles read stall. */
     UPDATE_HARDWARE; /* Note: IT checks are slightly off due to this, but this inaccuracy is tolerable. */
    }
    cval &= ~0x01U;
    /* Note: The EERE bit is a little hazy, it is not described whether it is
    ** cleared after write or not, the SBI / CBI instructions might work
    ** differently on this port than read + mask + write. Clearing it however
    ** works for the documented usage (the bit is never read anyway). */
   }
   break;

  case CU_IO_EEARH:   /* EEPROM address & data registers */
  case CU_IO_EEARL:
  case CU_IO_EEDR:

   if (cpu_state.eep_wrte){
    cval = pval;      /* During EEPROM programming, these can't be modified */
   }
   break;

  case CU_IO_WDTCSR:  /* Watchdog timer control */

   if ( ((pval & 0x48U) == 0U) &&
        ((cval & 0x48U) != 0U) ){ /* Watchdog becomes enabled, so start it */
    cpu_state.wd_end = WRAP32(cu_avr_getwdto() + cpu_state.cycle);
    if ( (WRAP32(cycle_next_event - cpu_state.cycle)) >
         (WRAP32(cpu_state.wd_end - cpu_state.cycle)) ){
     cycle_next_event = cpu_state.wd_end; /* Set Watchdog timeout HW processing target */
    }
   }
   break;

  case CU_IO_SPMCSR:  /* Store program memory control & status */

   cval = (cval & (~0x40U)) | (pval & 0x40U); /* RRWSB bit can not be written */
   if ( ((pval & 0x01U) == 0U) &&
        ((cval & 0x01U) != 0U) && /* SPM enable just turned on */
        (!cpu_state.spm_prge) ){
    t0 = cval & 0x1EU;    /* SPM mode select bits */
    if ( (t0 == 0x00U) || /* Page load (filling temp buffer) */
         (t0 == 0x02U) || /* Page erase */
         (t0 == 0x04U) || /* Page write */
         (t0 == 0x08U) || /* Boot lock bit set */
         (t0 == 0x10U) ){ /* RWW section read enable */
     cpu_state.spm_mode = t0;
     cpu_state.spm_end  = WRAP32(cpu_state.cycle + 4U);
     if ( (WRAP32(cycle_next_event  - cpu_state.cycle)) >
          (WRAP32(cpu_state.spm_end - cpu_state.cycle)) ){
      cycle_next_event = cpu_state.spm_end; /* Set SPM HW processing target */
     }
    }
   }
   break;

  case CU_IO_SREG:    /* Status register */

   if ((((~pval) & cval) & SREG_IM) != 0U){
    event_it = TRUE;  /* Interrupts become enabled, so check them */
   }
   break;

  default:

   break;

 }

 cpu_state.iors[port] = cval;
}



/*
** Reads from an I/O port
*/
static auint cu_avr_read_io(auint port)
{
 auint t0;
 auint ret;

 access_io[port] |= CU_MEM_R;

 switch (port){

  case CU_IO_TCNT1L:
   t0  = WRAP32(cpu_state.cycle - timer1_base); /* Current TCNT1 value */
   cpu_state.latch = (t0 >> 8) & 0xFFU;
   ret = t0 & 0xFFU;
   break;

  case CU_IO_TCNT1H:
   ret = cpu_state.latch;
   break;

  default:
   ret = cpu_state.iors[port];
   break;
 }

 return ret;
}



/*
** Emulates a single (compiled) AVR instruction and any associated hardware
** tasks.
*/
#ifdef __EMSCRIPTEN__
#ifndef FLAG_NATIVE
#include "cu_avr_e.h"
#else
#include "cu_avr_n.h"
#endif
#else
#include "cu_avr_n.h"
#endif



/*
** Resets the CPU as if it was power-cycled. It properly initializes
** everything from the state as if cu_avr_crom_update() and cu_avr_io_update()
** was called.
*/
void  cu_avr_reset(void)
{
 auint i;

 for (i = 0U; i < 4096U; i++){
  access_mem[i] = 0U;
 }

 for (i = 0U; i < 256U; i++){
  access_io[i] = 0U;
 }

 for (i = 0U; i < 271U; i++){
  video_frame.pulse[i].rise = CU_NOSYNC;
  video_frame.pulse[i].fall = CU_NOSYNC;
 }

 for (i = 0U; i < 4U; i++){
  audio_samples[i] = 0x80U;
 }

 for (i = 0U; i < 256U; i++){ /* Most I/O regs are reset to zero */
  cpu_state.iors[i] = 0U;
 }
 cpu_state.iors[CU_IO_SPL] = 0xFFU;
 cpu_state.iors[CU_IO_SPH] = 0x10U;

 cpu_state.latch = 0U;

 /* Check whether something is loaded. Normally the AVR is fused to use the
 ** boot loader, so if anything is present there, use that. If there is no
 ** boot loader (only an application is loaded), boot that instead. */

 if ( (cpu_state.crom[(VBASE_BOOT * 2U) + 0U] != 0U) ||
      (cpu_state.crom[(VBASE_BOOT * 2U) + 1U] != 0U) ){ cpu_state.iors[CU_IO_MCUCR] |= 2U; }

 cpu_state.pc = (((cpu_state.iors[CU_IO_MCUCR] >> 1) & 1U) * VBASE_BOOT) + VECT_RESET;

 cpu_state.cycle    = 0U;
 video_pulsectr     = 0U;
 video_pedge        = cpu_state.cycle;
 video_prise        = cpu_state.cycle;
 video_rowflag      = FALSE;
 video_cycle        = 0U;
 audio_rp           = 0U;
 audio_wp           = 0U;
 cycle_next_event   = WRAP32(cpu_state.cycle + 1U);
 timer1_base        = cpu_state.cycle;
 event_it           = TRUE;
 event_it_enter     = FALSE;
 wd_last            = cpu_state.cycle;
 wd_last_pc         = 0U;
 cpu_state.spi_tran = FALSE;
 cpu_state.wd_end   = WRAP32(cu_avr_getwdto() + cpu_state.cycle);
 cpu_state.eep_wrte = FALSE;
 cpu_state.spm_prge = FALSE;

 wd_interval_min[0] = WD_INTERVAL_MAX;
 wd_interval_min[1] = 0U;
 wd_interval_beg[0] = 0U;
 wd_interval_beg[1] = 0U;
 wd_interval_end[0] = 0U;
 wd_interval_end[1] = 0U;

 if (!pflags_done){
  cu_avrfg_fill(&cpu_pflags[0]);
  pflags_done = TRUE;
 }

 cu_avr_crom_update(0U, 65536U);
 cu_avr_io_update();

 cu_ctr_reset();
 cu_spi_reset(cpu_state.cycle);

 cpu_state.crom_mod = FALSE; /* Initial code ROM state: not modified. */
}



/*
** Run emulation. Returns according to the return values defined in cu_types
** (emulating up to about 2050 cycles).
*/
auint cu_avr_run(void)
{
 auint ret = 0U;
 auint i;

 video_rowflag = FALSE;

 while (video_cycle < VIDEO_CY_MAX){ /* Also signals proper row end */
  cu_avr_exec();       /* Note: This inlines as only this single call exists */
 }
 video_cycle -= VIDEO_CY_MAX; /* Next line pixels */

 if (video_rowflag){

  ret |= CU_GET_ROW;

  audio_rp = (audio_rp + 1U) & 0x3U;
  if (audio_rp == audio_wp){ audio_rp = (audio_rp - 1U) & 0x3U; }
  video_row.sample = audio_samples[audio_rp];

  video_row.pno = video_pulsectr;

  if (video_pulsectr == 270U){ /* Frame completed, can return it */

   ret |= CU_GET_FRAME;

   if (wd_interval_min[0] < WD_INTERVAL_MAX){ /* Latch WDR interval debug counter */
    wd_interval_min[1] = wd_interval_min[0];
    wd_interval_end[1] = wd_interval_end[0];
    wd_interval_beg[1] = wd_interval_beg[0];
   }
   wd_interval_min[0] = WD_INTERVAL_MAX; /* Clear for next frame */
   wd_interval_beg[0] = 0U;
   wd_interval_end[0] = 0U;

  }else if (video_pulsectr == 0U){

   for (i = 1U; i < 271U; i++){ /* Clear sync info for next frame (pulse 0 is already produced here) */
    video_frame.pulse[i].rise = CU_NOSYNC;
    video_frame.pulse[i].fall = CU_NOSYNC;
   }

  }

 }else{ /* (Note: No CU_BREAK support yet) */

  ret |= CU_GET_ROW;
  ret |= CU_SYNCERR;

 }

 return ret;
}



/*
** Returns emulator's cycle counter. It may be used to time emulation when it
** doesn't generate proper video signal. This is the cycle member of the CPU
** state (32 bits wrapping).
*/
auint cu_avr_getcycle(void)
{
 return cpu_state.cycle;
}



/*
** Return current row. Note that continuing emulation will modify the returned
** structure's contents.
*/
cu_row_t const* cu_avr_get_row(void)
{
 return &video_row;
}



/*
** Return frame info. Note that continuing emulation will modify the returned
** structure's contents.
*/
cu_frameinfo_t const* cu_avr_get_frameinfo(void)
{
 return &video_frame;
}


/*
** Returns memory access info block. It can be written (with zeros) to clear
** flags which are only set by the emulator. Note that the highest 256 bytes
** of the RAM come first here! (so address 0x0100 corresponds to AVR address
** 0x0100)
*/
uint8* cu_avr_get_meminfo(void)
{
 return &access_mem[0];
}


/*
** Returns I/O register access info block. It can be written (with zeros) to
** clear flags which are only set by the emulator.
*/
uint8* cu_avr_get_ioinfo(void)
{
 return &access_io[0];
}


/*
** Returns whether the EEPROM changed since the last clear of this indicator.
** Calling cu_avr_io_update() clears this indicator (as well as resetting by
** cu_avr_reset()). Passing TRUE also clears it.
*/
boole cu_avr_eeprom_ischanged(boole clear)
{
 boole ret = eeprom_changed;
 if (clear){ eeprom_changed = FALSE; }
 return ret;
}



/*
** Returns whether the Code ROM was modified since reset. This can be used to
** determine if it is necessary to include the Code ROM in a save state.
** Internal Code ROM writes and the cu_avr_crom_update() function can set it.
*/
boole cu_avr_crom_ismod(void)
{
 return cpu_state.crom_mod;
}



/*
** Returns AVR CPU state structure. It may be written, the Code ROM must be
** recompiled (by cu_avr_crom_update()) if anything in that area was updated
** or freshly written.
*/
cu_state_cpu_t* cu_avr_get_state(void)
{
 auint t0 = WRAP32(cpu_state.cycle - timer1_base); /* Current TCNT1 value */

 cpu_state.iors[CU_IO_TCNT1H] = (t0 >> 8) & 0xFFU;
 cpu_state.iors[CU_IO_TCNT1L] = (t0     ) & 0xFFU;

 return &cpu_state;
}



/*
** Updates a section of the Code ROM. This must be called after writing into
** the Code ROM so the emulator recompiles the affected instructions. The
** "base" and "len" parameters specify the range to update in bytes.
*/
void  cu_avr_crom_update(auint base, auint len)
{
 auint wbase = base >> 1;
 auint wlen  = (len + (base & 1U) + 1U) >> 1;
 auint i;

 if (wbase > 0x7FFFU){ wbase = 0x7FFFU; }
 if ((wbase + wlen) > 0x8000U){ wlen = 0x8000U - wbase; }

 for (i = wbase; i < (wbase + wlen); i++){
  cpu_code[i] = cu_avrc_compile(
      ((auint)(cpu_state.crom[((i << 1) + 0U) & 0xFFFFU])     ) |
      ((auint)(cpu_state.crom[((i << 1) + 1U) & 0xFFFFU]) << 8),
      ((auint)(cpu_state.crom[((i << 1) + 2U) & 0xFFFFU])     ) |
      ((auint)(cpu_state.crom[((i << 1) + 3U) & 0xFFFFU]) << 8) );
 }

 cpu_state.crom_mod = TRUE;
}



/*
** Updates the I/O area. If any change is performed in the I/O register
** contents (iors, 0x20 - 0xFF), this have to be called to update internal
** emulator state over it. It also updates state related to additional
** variables in the structure (such as the watchdog timer).
*/
void  cu_avr_io_update(void)
{
 auint t0;

 eeprom_changed = FALSE;

 t0    = (cpu_state.iors[CU_IO_TCNT1H] << 8) |
         (cpu_state.iors[CU_IO_TCNT1L]     );
 timer1_base = WRAP32(cpu_state.cycle - t0);

 cycle_next_event = WRAP32(cpu_state.cycle + 1U); /* Request HW processing */
 event_it         = TRUE; /* Request interrupt processing */
}



/*
** Returns last measured interval between WDR calls. Returns begin and end
** (word) addresses of WDR instructions into beg and end.
*/
auint cu_avr_get_lastwdrinterval(auint* beg, auint* end)
{
 *beg = wd_interval_beg[1];
 *end = wd_interval_end[1];
 return wd_interval_min[1];
}
