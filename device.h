/***************************************************************************
Copyright (c) 2019, Mateusz Panuś

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 ***************************************************************************/
#ifndef DEVCE_H_INCLUDED
#define DEVCE_H_INCLUDED

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>

void DEVICE_Init(void);

static inline void DEVICE_ClockFreq_2MHz(void) {
    OSC_CTRL |= OSC_RC2MEN_bm;
    while(!(OSC_STATUS&OSC_RC2MRDY_bm));
    CPU_CCP = CCP_IOREG_gc;
    CLK_CTRL = CLK_SCLKSEL_RC2M_gc;
    OSC_CTRL &= ~(OSC_RC8MEN_bm|OSC_RC32MEN_bm);
}

static inline void DEVICE_ClockFreq_8MHz(void) {
    OSC_CTRL |= OSC_RC8MEN_bm;
    while(!(OSC_STATUS&OSC_RC8MRDY_bm));
    CPU_CCP = CCP_IOREG_gc;
    CLK_CTRL = CLK_SCLKSEL_RC8M_gc;
    OSC_CTRL &= ~(OSC_RC2MEN_bm|OSC_RC32MEN_bm);
}

static inline void DEVICE_ClockFreq_32MHz(void) {
    OSC_CTRL |= OSC_RC32MEN_bm|OSC_RC32KEN_bm;
    while(!(OSC_STATUS&OSC_RC32KRDY_bm));
    while(!(OSC_STATUS&OSC_RC32MRDY_bm));
    /* use DFLL to calibrate 32MHz clock from more accurate 32kHz clock */
    DFLLRC32M_CTRL = DFLL_ENABLE_bm;
    CPU_CCP = CCP_IOREG_gc;
    CLK_CTRL = CLK_SCLKSEL_RC32M_gc;
    OSC_CTRL &= ~(OSC_RC2MEN_bm|OSC_RC8MEN_bm);
}

static inline void DEVICE_RTC_Sync(void) {
    while(RTC_STATUS&RTC_SYNCBUSY_bm);
}

static inline uint8_t DEVICE_ReadCalibrationByte(uint8_t index) {
    NVM_CMD = NVM_CMD_READ_CALIB_ROW_gc;
    uint8_t result = pgm_read_byte(index);
    NVM_CMD = NVM_CMD_NO_OPERATION_gc;
    return result;
}

static inline void DEVICE_InterruptEnable(void) {
    PMIC_CTRL = PMIC_LOLVLEN_bm;
    sei();
}

#endif // DEVCE_H_INCLUDED
