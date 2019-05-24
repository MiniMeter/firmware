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
#include <avr/io.h>
#include <avr/eeprom.h>
#include <stdio.h>
#include "avr/iox32e5.h"
#include "avr/eeprom.h"
#include "main.h"
#include "device.h"
#include "text.h"
#include "buffer.h"
#include "image.h"
#include "display.h"
#include "analog.h"
#include "delay.h"
#include "keypad.h"
#include "chart.h"
#include "freq.h"

#define FREQ_PERIOD_CHANGE  11 // (11*1024) = ~11.3kHz
#define FREQ_PERIOD_READ  64 // ms
#define FREQ_PERIOD_BUFSIZE  8

typedef struct {
    CHART_SPEED_t speed;
} FREQ_SETTINGS_t;

static FREQ_SETTINGS_t FREQ_settings EEMEM;
static struct FREQ_struct {
    volatile uint8_t sync;
    uint8_t hold, clear, window, index;
    uint16_t count, max, last;
    volatile uint16_t period;
    __uint24 value, total;
    struct {
        volatile uint8_t index;
        uint16_t period[FREQ_PERIOD_BUFSIZE];
    } buffer;
    FREQ_SETTINGS_t settings;
} FREQ;

static void FREQ_Loop(void);
static void FREQ_Flush(void);
static void FREQ_KeyUp(KEYPAD_KEY_t key);
static void FREQ_Hold(void);
static inline void FREQ_Result(void);
static inline void FREQ_ChangeCount(void);
static inline void FREQ_SaveSettings(void);
static inline void FREQ_LoadSettings(void);

void FREQ_Init(void) {
    FREQ_LoadSettings();
    DISPLAY_Mode(DISPLAY_MODE_RAW);
    MAIN_Loop(FREQ_Flush);
    ANALOG_Info();
    CHART_Init();
    BUFFER_Init(BUFFER_MODE_TCC5_CCA);
    KEYPAD_KeyUp(FREQ_KeyUp);
    PORTA_PIN1CTRL = PORT_OPC_BUSKEEPER_gc|PORT_ISC_RISING_gc;
    PORTD_PIN6CTRL = PORT_ISC_RISING_gc;
    PORTD_DIRSET = PIN6_bm;
    PORTCFG_CLKOUT = PORTCFG_RTCCLKOUT_PD6_gc;
    EVSYS.CH5MUX = EVSYS_CHMUX_PORTA_PIN1_gc;
    EVSYS.CH6MUX = EVSYS_CHMUX_PORTD_PIN6_gc;
    EVSYS.CH2MUX = EVSYS_CHMUX_PORTD_PIN6_gc;
    EVSYS.CH7MUX = EVSYS_CHMUX_PRESCALER_32_gc;
    TCC5.CTRLB = TC45_BYTEM_NORMAL_gc|TC45_WGMODE_NORMAL_gc;
    TCC5.CTRLD = TC45_EVACT_PWF_gc|TC45_EVSEL_CH2_gc;
    TCC5.CTRLE = TC45_CCAMODE_CAPT_gc|TC45_CCBMODE_DISABLE_gc;
    TCC5.CTRLA = TC45_CLKSEL_EVCH5_gc;
    TCD5.INTCTRLB = TC45_CCAINTLVL_OFF_gc;
    TCD5.CTRLB = TC45_BYTEM_NORMAL_gc|TC45_WGMODE_NORMAL_gc;
    TCD5.CTRLD = TC45_EVACT_PWF_gc|TC45_EVSEL_CH5_gc;
    TCD5.CTRLE = TC45_CCAMODE_CAPT_gc|TC45_CCBMODE_DISABLE_gc;
    TCD5.CTRLA = TC45_CLKSEL_EVCH7_gc;
    XCL.INTCTRL = XCL_CC0IE_bm|XCL_CC_INTLVL_LO_gc;
    XCL.CTRLF = XCL_TCMODE_PWM_gc;
    XCL.PERCAPTL = FREQ_PERIOD_READ-1;
    XCL.PERCAPTH = FREQ_PERIOD_READ-1; // needed for BTC0 to work properly (hardware bug?)
    XCL.CNTL = FREQ_PERIOD_READ-1;
    XCL.CTRLE = XCL_TCSEL_BTC0_gc|XCL_CLKSEL_EVCH6_gc;
    FREQ.hold = 0;
}

static void FREQ_Flush(void) {
    if(BUFFER_Flush()) {
        FREQ.window = 1;
        FREQ.max = 0;
        FREQ.last = 0;
        FREQ_ChangeCount();
        MAIN_Loop(FREQ_Loop);
    }
}

static void FREQ_Loop(void) {
    static uint8_t counter, idx, show, skip, sign;
    while(!BUFFER_Empty()) {
        uint16_t sample = (uint16_t)BUFFER_GetSample();
        if(sample>FREQ.max) { FREQ.max = sample; }
        FREQ.total += sample;
        uint16_t tolerance, top, bottom;
        tolerance = FREQ.last/8;
        top = FREQ.last+tolerance;
        if(FREQ.last>tolerance) { bottom = FREQ.last-tolerance; }
        else { bottom = 0; }
        idx++;
        if((sample>top)||(sample<bottom)) {
            idx = 1;
            FREQ.window = 1;
            skip = 0;
            if(!show) {
                show = 2;
                sign = sample>FREQ.last;
            } else {
                if(sign^(sample>FREQ.last)) {
                    skip = 1;
                }
            }
        }
        FREQ.last = sample;
        if(FREQ.window<64) {
            if((idx/2)&FREQ.window) {
                FREQ.window <<= 1; // 1->2->4->8->16->32->64
                if(!skip) {
                    show = 2;
                }
            }
        }
        if((++counter==0)||(show==2)||(FREQ.sync>1)) {
            if(show>0) { show--; }
            counter = 0;
            FREQ.sync = 0;
            __uint24 freq = 0;
            if(sample>FREQ_PERIOD_CHANGE) {
                uint16_t first = BUFFER.first;
                for(uint8_t i=0; i<FREQ.window; i++) {
                    freq += (uint16_t)BUFFER.sample[first/sizeof(uint16_t)];
                    first -= sizeof(uint16_t);
                    first &= BUFFER_MAX;
                }
                freq *= (1024/FREQ.window);
            } else {
                uint16_t period = FREQ.period;
                if(period>0) {
                    freq = 1000000/period;
                }
            }
            FREQ.value = freq;
        }
        if(CHART_Sample()) {
            int16_t avg = FREQ.total/FREQ.count;
            CHART_Value(FREQ.max, avg, 0);
            FREQ.max = 0;
            FREQ.total = 0;
        }
    }
    if(DISPLAY_Update()) {
        CHART_Update();
        DISPLAY_CursorPosition(1,1);
        FREQ_Result();
        if(FREQ.hold) { DISPLAY_InvertLine(0); }
        if(FREQ.settings.speed<=CHART_SPEED_8) {
            CHART_Marker();
        }
    }
}


ISR(TCD5_CCA_vect) {
    uint8_t index = FREQ.buffer.index;
    if(FREQ.buffer.index<FREQ_PERIOD_BUFSIZE) {
        FREQ.buffer.period[index] = TCD5_CCA;
        FREQ.buffer.index++;
    } else {
        TCD5_INTCTRLB = TC45_CCAINTLVL_OFF_gc;
    }
}

ISR(XCL_CC_vect) {
    XCL_INTFLAGS = XCL_CC0IF_bm;
    uint8_t index = FREQ.buffer.index;
    if(index>1) {
        __uint24 period = 0;
        for(uint8_t i=1; i<index; i++) {
            period += FREQ.buffer.period[i];
        }
        period /= (index-1);
        if(period>(1000000/((FREQ_PERIOD_CHANGE+1)*1024))) {
            FREQ.period = period;
            FREQ.sync = 2;
        }
    } else {
        FREQ.period = 0;
        FREQ.sync++;
    }
    FREQ.buffer.index = 0;
    TCD5_INTCTRLB = TC45_CCAINTLVL_LO_gc;
}

static inline void FREQ_Result(void) {
    __uint24 value = FREQ.value;
    const __flash char* unit;
    if(value<1000) {
        printf_P(TEXT_FREQ_nnn, (uint16_t)value);
        unit = TEXT_FREQ_Hz;
    } else if(value<10000) {
        value += 5;
        printf_P(TEXT_FREQ_dnn, (uint8_t)(value/1000), (uint8_t)((value%1000)/10));
        unit = TEXT_FREQ_kHz;
    } else if(value<100000) {
        value += 50;
        printf_P(TEXT_FREQ_ddn, (uint8_t)(value/1000), (uint8_t)((value%1000)/100));
        unit = TEXT_FREQ_kHz;
    } else if(value<1000000) {
        value += 500;
        printf_P(TEXT_FREQ_nnn, (uint16_t)(value/1000));
        unit = TEXT_FREQ_kHz;
    } else if(value<4010000) {
        value += 5000;
        printf_P(TEXT_FREQ_dnn, (uint8_t)(value/1000000), (uint8_t)((value%1000000)/10000));
        unit = TEXT_FREQ_MHz;
    } else {
        printf_P(TEXT_OVERLOAD, ' ');
        CHART_Clear();
        BUFFER_Clear();
        unit = TEXT_FREQ_MHz;
    }
    DISPLAY_MoveCursor(2);
    puts_P(unit);
}

static inline void FREQ_ChangeCount(void) {
    FREQ.count = CHART_Count(FREQ.settings.speed);
    FREQ.total = 0;
}

static void FREQ_KeyUp(KEYPAD_KEY_t key) {
    if(FREQ.hold && key!=KEYPAD_KEY3) { return; }
    switch(key) {
        case KEYPAD_KEY1:
            if(FREQ.settings.speed>CHART_SPEED_4) { FREQ.settings.speed--; }
            FREQ_ChangeCount();
            FREQ_SaveSettings();
            break;
        case KEYPAD_KEY2:
            if(FREQ.settings.speed<CHART_SPEED_10) { FREQ.settings.speed++; }
            FREQ_ChangeCount();
            FREQ_SaveSettings();
            break;
        case KEYPAD_KEY3: FREQ_Hold(); break;
        case KEYPAD_KEY4: CHART_Lock(); break;
        default: break;
    }
}

static void FREQ_Hold(void) {
    FREQ.hold = !FREQ.hold;
    if(FREQ.hold) {
        EVSYS_CH2MUX = EVSYS_CHMUX_OFF_gc;
    } else {
        EVSYS_CH2MUX = EVSYS_CHMUX_PORTD_PIN6_gc;
        MAIN_Loop(FREQ_Flush);
    }
}

void FREQ_Intro(void) {
    DISPLAY_Image(IMAGE_FREQ);
    DISPLAY_Icons(ANALOG_ICONS);
    DISPLAY_Send();
}

static inline void FREQ_SaveSettings(void) {
    EEPROM_update_block(&FREQ.settings, &FREQ_settings, sizeof(FREQ_SETTINGS_t));
}

static inline void FREQ_LoadSettings(void) {
    eeprom_read_block(&FREQ.settings, &FREQ_settings, sizeof(FREQ_SETTINGS_t));
    CHART_SPEED_t speed = FREQ.settings.speed;
    if((speed>CHART_SPEED_10)||(speed<CHART_SPEED_4)) {
        FREQ.settings.speed = CHART_SPEED_4;
    }
    FREQ_SaveSettings();
}
