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
#ifndef DIGITAL_H_INCLUDED
#define DIGITAL_H_INCLUDED

typedef enum {
    DIGITAL_DISPLAY_HEX,
    DIGITAL_DISPLAY_ASCII,
} DIGITAL_DISPLAY_t;

typedef void (*DIGITAL_Decode_t)(void);

void DIGITAL_Init(DIGITAL_Decode_t Decode);
void DIGITAL_Display(DIGITAL_DISPLAY_t display);
void DIGITAL_Print(uint8_t data);
void DIGITAL_PrintChar(uint8_t ch);
void DIGITAL_PrintSymbol(uint8_t sym);
void DIGITAL_PrintHex(uint8_t hex);
void DIGITAL_PrintTab(void);
void DIGITAL_EndLine(void);
void DIGITAL_InvertLine(void);
void DIGITAL_Clear(void);
void DIGITAL_Blackout(void);
uint8_t DIGITAL_Lock(void);
void DIGITAL_Hold(uint8_t hold);
uint8_t DIGITAL_IsHold(void);
void DIGITAL_Update(void);

static inline void DIGITAL_CheckClockPeriod(void) {
    TCC5.CTRLB = TC45_BYTEM_NORMAL_gc|TC45_WGMODE_NORMAL_gc;
    TCC5.CTRLD = TC45_EVACT_PWF_gc|TC45_EVSEL_CH1_gc;
    TCC5.CTRLE = TC45_CCAMODE_CAPT_gc;
    TCC5.CTRLA = TC45_CLKSEL_DIV1_gc;
    TCC5.CCA = UINT16_MAX;
}

static inline uint16_t DIGITAL_ClockPeriod(void) {
    return TCC5_CCA; //start measurement
}

static inline void DIGITAL_ClockPeriodReset(void) {
    TCC5_CCA = UINT16_MAX;
}

#endif // DIGITAL_H_INCLUDED
