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
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <util/atomic.h>
#include <stdio.h>
#include "avr/eeprom.h"
#include "main.h"
#include "text.h"
#include "keypad.h"
#include "buffer.h"
#include "display.h"
#include "delay.h"
#include "analog.h"
#include "image.h"
#include "icon.h"
#include "current.h"
#include "chart.h"
#include "charge.h"

#define CALIB_TIME  4 // 4s
#define GAIN 2 // Input signal gain (x2)

typedef struct {
    CHART_SPEED_t speed;
} CHARGE_SETTINGS_t;

static CHARGE_SETTINGS_t CHARGE_settings EEMEM;
static struct CHARGE_struct {
    volatile uint8_t update;
    uint16_t count, period;
    uint8_t timer;
    int16_t max;
    int32_t total, accum, value, offset;
    struct {
        volatile uint8_t hour, minute, second;
    } time;
    CHARGE_SETTINGS_t settings;
} CHARGE;

static void CHARGE_Flush(void);
static void CHARGE_Calibration(void);
static void CHARGE_Clear(void);
static void CHARGE_Loop(void);
static void CHARGE_KeyUp(KEYPAD_KEY_t key);
static void CHARGE_Info(void);
static void CHARGE_Icons(void);
static inline void CHARGE_Result(void);
static inline void CHARGE_Timer(void);
static inline void CHARGE_ChangeCount(void);
static inline void CHARGE_SaveSettings(void);
static inline void CHARGE_LoadSettings(void);

void CHARGE_Init(void) {
    CHARGE_LoadSettings();
    CURRENT_Setup();
    CHARGE_Info();
    MAIN_Loop(CHARGE_Flush);
    KEYPAD_KeyUp(NULL);
    PORTD_PIN6CTRL = PORT_ISC_RISING_gc;
    PORTD_DIRSET = PIN6_bm;
    PORTCFG_CLKOUT = PORTCFG_RTCCLKOUT_PD6_gc;
    EVSYS_CH5MUX = EVSYS_CHMUX_PORTD_PIN6_gc;
    TCC5.CTRLB = TC45_BYTEM_NORMAL_gc|TC45_WGMODE_NORMAL_gc;
    TCC5.INTCTRLA = TC45_OVFINTLVL_OFF_gc;
    TCC5.INTCTRLB = TC45_CCAINTLVL_OFF_gc|TC45_CCBINTLVL_OFF_gc;
    XCL_INTCTRL = XCL_UNF_INTLVL_OFF_gc;
    XCL_CTRLE = XCL_CLKSEL_OFF_gc;
    ADCA_CH0_MUXCTRL = ADC_CH_MUXPOS_PIN6_gc|ADC_CH_MUXNEGH_PIN6_gc;
    CHARGE.timer = 0;
}

static void CHARGE_Flush(void) {
    if(BUFFER_Flush()) {
        CHARGE.value = 0;
        CHARGE.max = 0;
        CHARGE.offset = 0;
        CHARGE_ChangeCount();
        EVSYS_CH2MUX = EVSYS_CHMUX_OFF_gc;
        BUFFER_Clear();
        TCC5.CTRLA = TC45_CLKSEL_OFF_gc;
        TCC5.PER = 0xFFFF;
        TCC5.CCB = (CALIB_TIME*1024)-1;
        TCC5.CNT = 0;
        TCC5.INTFLAGS = TC5_CCBIF_bm;
        TCC5.INTCTRLB = TC45_CCBINTLVL_LO_gc;
        TCC5.CTRLA = TC45_CLKSEL_EVCH5_gc;
        ADCA_CTRLA |= ADC_FLUSH_bm;
        EVSYS_CH2MUX = EVSYS_CHMUX_ADCA_CH0_gc;
        MAIN_Loop(CHARGE_Calibration);
    }
}

static void CHARGE_Calibration(void) {
    while(!BUFFER_Empty()) {
        int16_t sample = BUFFER_GetSample();
        CHARGE.offset += sample;
    }
    if(DISPLAY_Update()){
        CHART_Update();
        DISPLAY_CursorPosition(1,1);
        CHARGE_Result();
        DISPLAY_CursorPosition(9,25);
        puts_P(TEXT_CALIBRATION);
    }
    if(TCC5_INTCTRLB==TC45_CCBINTLVL_OFF_gc) {
        CHARGE.offset /= CALIB_TIME;
        TCC5.CTRLA = TC45_CLKSEL_OFF_gc;
        TCC5.PER = 1024-1;
        TCC5.CNT = 0;
        TCC5.INTCTRLB = TC45_CCAINTLVL_LO_gc;
        TCC5.CTRLA = TC45_CLKSEL_EVCH5_gc;
        ADCA_CH0_MUXCTRL = ADC_CH_MUXPOS_PIN7_gc|ADC_CH_MUXNEGH_PIN6_gc;
        ADCA_CTRLA |= ADC_FLUSH_bm;
        EVSYS_CH2MUX = EVSYS_CHMUX_ADCA_CH0_gc;
        MAIN_Loop(CHARGE_Clear);
        KEYPAD_KeyUp(CHARGE_KeyUp);
        CHARGE.update = 0;
    }
}

static void CHARGE_Clear(void) {
    if(CHARGE.update) {
        CHARGE.value = 0;
        CHARGE.accum = 0;
        CHARGE.period = 0;
        CHARGE.update = 0;
        CHARGE.time.hour = 0;
        CHARGE.time.minute = 0;
        CHARGE.time.second = 0;
        BUFFER_Clear();
        MAIN_Loop(CHARGE_Loop);
    }
}

static void CHARGE_Loop(void) {
    while(!BUFFER_Empty()) {
        int16_t sample = BUFFER_GetSample();
        if(sample>CHARGE.max) { CHARGE.max = sample; }
        CHARGE.total += sample;
        CHARGE.accum += sample;
        CHARGE.period++;
        if(CHART_Sample()) {
            int16_t avg = CHARGE.total/CHARGE.count;
            CHART_Value(CHARGE.max, avg, 0);
            CHARGE.total = 0;
            CHARGE.max = 0;
        }
    }
    if(CHARGE.update) {
        CHARGE.accum += (CHARGE.period/2)-CHARGE.offset;
        CHARGE.value += CHARGE.accum/CHARGE.period;
        CHARGE.accum = 0;
        CHARGE.period = 0;
        CHARGE.update = 0;
    }
    if(DISPLAY_Update()){
        CHART_Update();
        if(CHARGE.timer) {
            DISPLAY_CursorPosition(3,1);
            CHARGE_Timer();
        } else {
            DISPLAY_CursorPosition(1,1);
            CHARGE_Result();
        }
        CHART_Marker();
    }
}

static inline void CHARGE_ChangeCount(void) {
    CHARGE.count = CHART_Count(CHARGE.settings.speed);
    CHARGE.total = 0;
}

static inline void CHARGE_Result(void) {
    int32_t value = CHARGE.value;
    value += (18*GAIN); //round to closest integer
    char sign = (value<0) ? '-' : ' ';
    if(value<0) { value = -value; }
    uint16_t milli = value/(3600*GAIN);
    uint16_t micro = (value%(3600*GAIN))/(36*GAIN);
    if(milli<10) {
        if((milli==0)&&(micro==0)) { sign = ' '; }
        printf_P(TEXT_CHARGE_muu, sign, milli, micro);
    } else if(milli<100) {
        printf_P(TEXT_CHARGE_mmu, sign, milli, micro/10);
    } else if(milli<10000) {
        printf_P(TEXT_CHARGE_mmm, sign, milli);
    } else {
        DISPLAY_MoveCursor(6);
        printf_P(TEXT_OVERLOAD, sign);
    }
    DISPLAY_MoveCursor(2);
    puts_P(TEXT_CHARGE_UNIT);
}

static inline void CHARGE_Timer(void) {
    uint8_t hour, minute, second;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        hour = CHARGE.time.hour;
        minute = CHARGE.time.minute;
        second = CHARGE.time.second;
    }
    printf_P(TEXT_CHARGE_TIME, hour, minute, second);
}

ISR(TCC5_CCA_vect) {
    CHARGE.update = 1;
    if(++CHARGE.time.second>=60) {
        CHARGE.time.second = 0;
        if(++CHARGE.time.minute>=60) {
            CHARGE.time.minute = 0;
            if(++CHARGE.time.hour>=100) {
                CHARGE.time.hour = 0;
            }
        }
    }
}

ISR(TCC5_CCB_vect) {
    EVSYS_CH2MUX = EVSYS_CHMUX_OFF_gc;
    TCC5_INTCTRLB = TC45_CCBINTLVL_OFF_gc;
}

static void CHARGE_KeyUp(KEYPAD_KEY_t key) {
    switch(key) {
        case KEYPAD_KEY1:
            if(CHARGE.settings.speed>CHART_SPEED_1) {
                CHARGE.settings.speed--;
                CHARGE_ChangeCount();
                CHARGE_SaveSettings();
            }
            break;
        case KEYPAD_KEY2:
            if(CHARGE.settings.speed<CHART_SPEED_4) {
                CHARGE.settings.speed++;
                CHARGE_ChangeCount();
                CHARGE_SaveSettings();
            }
            break;
        case KEYPAD_KEY3: CHARGE.timer = !CHARGE.timer; break;
        case KEYPAD_KEY4: CHART_Lock(); break;
        default: break;
    }
}

void CHARGE_Intro(void) {
    DISPLAY_Image(IMAGE_CHARGE);
    CHARGE_Icons();
    DISPLAY_Send();
}

static void CHARGE_Info(void) {
    DISPLAY_CursorPosition(14, 1);
    DISPLAY_Icon(ICON_SLOWER);
    DISPLAY_CursorPosition(29, 2);
    puts_P(TEXT_SLOWER);
    DISPLAY_CursorPosition(15, 10);
    DISPLAY_Icon(ICON_FASTER);
    DISPLAY_CursorPosition(29, 11);
    puts_P(TEXT_FASTER);
    DISPLAY_CursorPosition(14, 19);
    DISPLAY_Icon(ICON_TIMER);
    DISPLAY_CursorPosition(29, 20);
    puts_P(TEXT_TIMER);
    DISPLAY_CursorPosition(13, 28);
    DISPLAY_Icon(ICON_LOCK);
    DISPLAY_CursorPosition(29, 29);
    puts_P(TEXT_SCALE);
    CHARGE_Icons();
    DISPLAY_Send();
    DELAY_Info();
}

static void CHARGE_Icons(void) {
    static const __flash uint8_t* const __flash CHARGE_ICONS[] = {
        ICON_SLOWER,
        ICON_FASTER,
        ICON_TIMER,
        ICON_LOCK,
    };
    DISPLAY_Icons(CHARGE_ICONS);
}

static inline void CHARGE_SaveSettings(void) {
    EEPROM_update_block(&CHARGE.settings, &CHARGE_settings, sizeof(CHARGE_SETTINGS_t));
}

static inline void CHARGE_LoadSettings(void) {
    eeprom_read_block(&CHARGE.settings, &CHARGE_settings, sizeof(CHARGE_SETTINGS_t));
    if(CHARGE.settings.speed>CHART_SPEED_4) {
        CHARGE.settings.speed = CHART_SPEED_1;
    }
    CHARGE_SaveSettings();
}
