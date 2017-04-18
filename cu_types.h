/*
 *  Emulator types
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



#ifndef CU_TYPES_H
#define CU_TYPES_H



#include "types.h"



/*
** Render structure used to generate a row of data. There are 262 rows per
** frame, each line taking 1820 cycles, producing 60Hz output, that is,
** normally. The pulse number can be used to identify the row to render (for
** rows 0 - 252 it corresponds the row number).
*/
typedef struct{
 uint8 pixels[2032];  /* Pixel data for the row (using Uzebox palette) */
 auint sample;        /* A 8 bit unsigned audio sample */
 auint pno;           /* Sync pulse number (0 - 270, VSync is at the end) */
}cu_row_t;


/*
** Sync signal rise and fall locations. 2's complement, they provide
** divergence from the proper values (Positive: Too late, Negative: Too
** early).
*/
typedef struct{
 auint rise;
 auint fall;
}cu_syncedge_t;


/* No sync for the sync signal rise & fall locations. This is used when rows
** are forcibly advanced without any sync signal. */
#define CU_NOSYNC 0x80000000U


/*
** Frame information structure. This provides information on the accuracy of
** the combined sync signal. Correct signal is as follows:
**
** Display pulses (0 - 250 & 270):
**
**  1820=>0
** _|0~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~|1684_______
**
** VSync pulses (251 - 269: 10 rows with 19 pulses):
**
**  1820=>0
** _|0~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~|1684_|1752~  251 / 252
** ~~~~~~~~~~~~~~~~~~|774__|842~~~~~~~~~~~~~~~~~~|1684_|1752~  253 / 254
** ~~~~~~~~~~~~~~~~~~|774__|842~~~~~~~~~~~~~~~~~~|1684_|1752~  255 / 256
** ~~~~~~~~~~~~~~~~~~|774__|842~~~~~~~~~~~~~~~~~~|1684_______  257
** ____________|638~~|774__________________|1548~|1684_______  258 / 259
** ____________|638~~|774__________________|1548~|1684_______  260 / 261
** ____________|638~~|774__________________|1548~|1684_|1752~  262 / 263 / 264
** ~~~~~~~~~~~~~~~~~~|774__|842~~~~~~~~~~~~~~~~~~|1684_|1752~  265 / 266
** ~~~~~~~~~~~~~~~~~~|774__|842~~~~~~~~~~~~~~~~~~|1684_|1752~  267 / 268
** ~~~~~~~~~~~~~~~~~~|774__|842~~~~~~~~~~~~~~~~~~|1684_______  269
*/
typedef struct{
 cu_syncedge_t pulse[271];
 auint rowcdif;       /* Row (or pulse) count difference */
}cu_frameinfo_t;


/*
** Emulator state structure: AVR CPU (ATMega644)
*/
typedef struct{
 uint8 crom[65536];   /* Code ROM */
 uint8 sram[4096];    /* Static RAM */
 uint8 eepr[2048];    /* EEPROM */
 uint8 iors[256];     /* IO Registers (including general purpose regs) */
 uint8 sbuf[256];     /* SPM page buffer */
 auint pc;            /* Program Counter */
 auint latch;         /* 16 bit I/O register high latch */
 auint cycle;         /* Current cycle (32 bits wrapping, used for timing) */
 boole crom_mod;      /* Whether code ROM was modified since the last reset */
 boole spi_tran;      /* SPI transfer in progress */
 auint spi_end;       /* SPI transfer end cycle */
 auint spi_rx;        /* SPI received data waiting to be clocked into the AVR */
 auint spi_tx;        /* SPI transmit data waiting to be clocked out of the AVR */
 auint wd_seed;       /* Watchdog seed to provide a deterministic WD timeout */
 auint wd_end;        /* Watchdog timeout end cycle */
 boole eep_wrte;      /* EEPROM Write enabled, timeout active */
 auint eep_end;       /* EEPROM activity end cycle */
 boole spm_prge;      /* SPM erasing or programming in progress */
 auint spm_mode;      /* SPM selected mode for the next SPM instruction */
 auint spm_end;       /* SPM enable end cycle */
}cu_state_cpu_t;


/*
** Definitions for accessing specific I/O registers
*/
#define CU_IO_PINA    0x20U
#define CU_IO_DDRA    0x21U
#define CU_IO_PORTA   0x22U
#define CU_IO_PINB    0x23U
#define CU_IO_DDRB    0x24U
#define CU_IO_PORTB   0x25U
#define CU_IO_PINC    0x26U
#define CU_IO_DDRC    0x27U
#define CU_IO_PORTC   0x28U
#define CU_IO_PIND    0x29U
#define CU_IO_DDRD    0x2AU
#define CU_IO_PORTD   0x2BU
#define CU_IO_TIFR0   0x35U
#define CU_IO_TIFR1   0x36U
#define CU_IO_TIFR2   0x37U
#define CU_IO_PCIFR   0x3BU
#define CU_IO_EIFR    0x3CU
#define CU_IO_EIMSK   0x3DU
#define CU_IO_GPIOR0  0x3EU
#define CU_IO_EECR    0x3FU
#define CU_IO_EEDR    0x40U
#define CU_IO_EEARL   0x41U
#define CU_IO_EEARH   0x42U
#define CU_IO_GTCCR   0x43U
#define CU_IO_TCCR0A  0x44U
#define CU_IO_TCCR0B  0x45U
#define CU_IO_TCNT0   0x46U
#define CU_IO_OCR0A   0x47U
#define CU_IO_OCR0B   0x48U
#define CU_IO_GPIOR1  0x4AU
#define CU_IO_GPIOR2  0x4BU
#define CU_IO_SPCR    0x4CU
#define CU_IO_SPSR    0x4DU
#define CU_IO_SPDR    0x4EU
#define CU_IO_ACSR    0x50U
#define CU_IO_OCDR    0x51U
#define CU_IO_SMCR    0x53U
#define CU_IO_MCUSR   0x54U
#define CU_IO_MCUCR   0x55U
#define CU_IO_SPMCSR  0x57U
#define CU_IO_SPL     0x5DU
#define CU_IO_SPH     0x5EU
#define CU_IO_SREG    0x5FU
#define CU_IO_WDTCSR  0x60U
#define CU_IO_CLKPR   0x61U
#define CU_IO_PRR     0x64U
#define CU_IO_OSCCAL  0x66U
#define CU_IO_PCICR   0x68U
#define CU_IO_EICRA   0x69U
#define CU_IO_PCMSK0  0x6BU
#define CU_IO_PCMSK1  0x6CU
#define CU_IO_PCMSK2  0x6DU
#define CU_IO_TIMSK0  0x6EU
#define CU_IO_TIMSK1  0x6FU
#define CU_IO_TIMSK2  0x70U
#define CU_IO_PCMSK3  0x73U
#define CU_IO_ADCL    0x78U
#define CU_IO_ADCH    0x79U
#define CU_IO_ADCSRA  0x7AU
#define CU_IO_ADCSRB  0x7BU
#define CU_IO_ADMUX   0x7CU
#define CU_IO_DIDR0   0x7EU
#define CU_IO_DIDR1   0x7FU
#define CU_IO_TCCR1A  0x80U
#define CU_IO_TCCR1B  0x81U
#define CU_IO_TCCR1C  0x82U
#define CU_IO_TCNT1L  0x84U
#define CU_IO_TCNT1H  0x85U
#define CU_IO_ICR1L   0x86U
#define CU_IO_ICR1H   0x87U
#define CU_IO_OCR1AL  0x88U
#define CU_IO_OCR1AH  0x89U
#define CU_IO_OCR1BL  0x8AU
#define CU_IO_OCR1BH  0x8BU
#define CU_IO_TCCR2A  0xB0U
#define CU_IO_TCCR2B  0xB1U
#define CU_IO_TCNT2   0xB2U
#define CU_IO_OCR2A   0xB3U
#define CU_IO_OCR2B   0xB4U
#define CU_IO_ASSR    0xB6U
#define CU_IO_TWBR    0xB8U
#define CU_IO_TWSR    0xB9U
#define CU_IO_TWAR    0xBAU
#define CU_IO_TWDR    0xBBU
#define CU_IO_TWCR    0xBCU
#define CU_IO_TWAMR   0xBDU
#define CU_IO_UCSR0A  0xC0U
#define CU_IO_UCSR0B  0xC1U
#define CU_IO_UCSR0C  0xC2U
#define CU_IO_UBRR0L  0xC4U
#define CU_IO_UBRR0H  0xC5U
#define CU_IO_UDR0    0xC6U


/* Return flag: Row completed. The row should be fetched for render. */
#define CU_GET_ROW    0x01U

/* Return flag: Frame completed. The frame information structure should be
** fetched for processing (or discarding). */
#define CU_GET_FRAME  0x02U

/* Return flag: 2000 cycles passed without sync signal. This is produced
** along with a CU_GET_ROW. */
#define CU_SYNCERR    0x04U

/* Return flag: Breakpoint. */
#define CU_BREAK      0x08U


/* Memory access info block: Read access flag */
#define CU_MEM_R      0x01U

/* Memory access info block: Write access flag */
#define CU_MEM_W      0x02U


#endif
