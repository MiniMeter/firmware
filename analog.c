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
#include <stdio.h>
#include "avr/iox32e5.h"
#include "avr/eeprom.h"
#include "main.h"
#include "text.h"
#include "image.h"
#include "icon.h"
#include "buffer.h"
#include "device.h"
#include "display.h"
#include "delay.h"
#include "keypad.h"
#include "chart.h"
#include "analog.h"

#define OFFSET_MID  0
#define OFFSET_MAX (100)
#define OFFSET_MIN (-OFFSET_MAX)
#define GAIN_MID  0x0800
#define GAIN_MIN (GAIN_MID-(100<<1))
#define GAIN_MAX (GAIN_MID+(100<<1))
#define RESULT_REFRESH  250 // ms

static ANALOG_SETTINGS_t* ANALOG_settings;
static struct ANALOG_struct {
    volatile uint8_t update;
    uint8_t hold;
    int16_t trigger, value, max, min, last;
    uint16_t count;
    int32_t total;
    ANALOG_Result_t Result;
    ANALOG_SETTINGS_t settings;
} ANALOG;

static void ANALOG_Loop(void);
static void ANALOG_Flush(void);
static void ANALOG_KeyUp(KEYPAD_KEY_t key);
static void ANALOG_Hold(void);
static inline void ANALOG_ChangeCount(void);
static inline void ANALOG_CalibrationSetup(void);
static void ANALOG_CalibrationKeyUp(KEYPAD_KEY_t key);
static void ANALOG_CalibrationUpdate(void);
static inline void ANALOG_SaveSettings(void);
static inline void ANALOG_LoadSettings(void);

void ANALOG_Init(ANALOG_SETTINGS_t* settings) {
    ANALOG_settings = settings;
    ANALOG_LoadSettings();
    CHART_Init();
    DISPLAY_Mode(DISPLAY_MODE_RAW);
    MAIN_Loop(ANALOG_Flush);
    KEYPAD_KeyUp(ANALOG_KeyUp);
    PORTA.PIN5CTRL = PORT_ISC_INPUT_DISABLE_gc;
    PORTA.PIN6CTRL = PORT_ISC_INPUT_DISABLE_gc;
    PORTA.PIN7CTRL = PORT_ISC_INPUT_DISABLE_gc;
    PORTD_PIN0CTRL = PORT_ISC_INPUT_DISABLE_gc;
    EVSYS_CH6MUX = EVSYS_CHMUX_PRESCALER_8192_gc;
    EVSYS_CH2MUX = EVSYS_CHMUX_ADCA_CH0_gc;
    XCL.INTCTRL = XCL_UNF0IE_bm|XCL_UNF_INTLVL_LO_gc;
    XCL.PERCAPTL = 0xFF;
    XCL.PERCAPTH = 0xFF; // needed for BTC0 to work properly (hardware bug?)
    XCL.CTRLG = XCL_EVACTEN_bm|XCL_EVACT0_RESTART_gc|XCL_EVSRC_EVCH4_gc;
    XCL.CTRLF = XCL_TCMODE_1SHOT_gc;
    XCL.CTRLE = XCL_TCSEL_BTC0_gc|XCL_CLKSEL_EVCH6_gc;
    TCC5.CTRLB = TC45_BYTEM_NORMAL_gc|TC45_WGMODE_NORMAL_gc;
    TCC5.INTCTRLA = TC45_OVFINTLVL_LO_gc;
    TCC5.PER = (RESULT_REFRESH*125)-1;
    TCC5.CTRLA = TC45_CLKSEL_DIV256_gc;
    ANALOG_CalibrationSetup();
    ADCA.CH0.INTCTRL = ADC_CH_INTMODE_COMPLETE_gc|ADC_CH_INTLVL_OFF_gc;
    ADCA.REFCTRL = ADC_REFSEL_AREFA_gc;
    ADCA.CTRLB = ADC_CURRLIMIT_NO_gc|ADC_CONMODE_bm|ADC_FREERUN_bm|ADC_RESOLUTION_MT12BIT_gc;
    ADCA.PRESCALER = ADC_PRESCALER_DIV32_gc;
    ADCA.SAMPCTRL = (1<<ADC_SAMPVAL_gp)&ADC_SAMPVAL_gm;
    ADCA.CALL = DEVICE_ReadCalibrationByte(offsetof(NVM_PROD_SIGNATURES_t, ADCACAL0));
    ADCA.CALH = DEVICE_ReadCalibrationByte(offsetof(NVM_PROD_SIGNATURES_t, ADCACAL1));
    ADCA.CTRLA = ADC_FLUSH_bm|ADC_ENABLE_bm;
    BUFFER_Init(BUFFER_MODE_ADCA_RES);
    ANALOG.Result = NULL;
    ANALOG.update = 0;
    ANALOG.hold = 0;
}

static void ANALOG_Flush(void) {
    if(BUFFER_Flush()) {
        ANALOG.max = 0;
        ANALOG.value = 0;
        ANALOG.min = 0;
        ANALOG_ChangeCount();
        MAIN_Loop(ANALOG_Loop);
    }
}

static void ANALOG_Loop(void) {
    static int16_t avg;
    while(!BUFFER_Empty()) {
        int16_t sample = BUFFER_GetSample();
        if(sample>ANALOG.max) { ANALOG.max = sample; }
        if(sample<ANALOG.min) { ANALOG.min = sample; }
        ANALOG.total += sample;
        if(CHART_Sample()) {
            avg = ANALOG.total/ANALOG.count;
            CHART_Value(ANALOG.max, avg, ANALOG.min);
            if((ANALOG.max-ANALOG.last)>((ANALOG.max/CHART_FULL_SCALE)+100)) {
                TCC5_CTRLGSET = TC45_CMD_RESTART_gc;
                ANALOG.value = ANALOG.max;
                ANALOG.update = 0;
            }
            ANALOG.last = (ANALOG.max>0) ? ANALOG.max : 0;
            ANALOG.total = 0;
            ANALOG.max = 0;
            ANALOG.min = INT16_MAX;
            if((ANALOG.settings.speed>CHART_SPEED_4)&&(CHART_Column()==(DISPLAY_WIDTH-1))) {
                int16_t max = CHART_Max();
                int16_t min = CHART_Min();
                int16_t dif = max-min;
                if(dif>=((max/CHART_FULL_SCALE)+4)) {
                    EVSYS_CH2MUX = EVSYS_CHMUX_OFF_gc;
                    ADCA_CMP = min+(dif/4);
                    ANALOG.trigger = min+(dif/2);
                    ADCA_CH0_INTCTRL = ADC_CH_INTMODE_BELOW_gc|ADC_CH_INTLVL_LO_gc;
                    if(!ANALOG.hold) {
                        EVSYS_STROBE = EVSYS_CHMUX4_bm;
                    }
                    BUFFER_Clear();
                }
            }
        }
    }
    if(ANALOG.update) {
        ANALOG.update = 0;
        ANALOG.value = avg;
    }
    if(DISPLAY_Update()){
        CHART_Update();
        DISPLAY_CursorPosition(8,1);
        if(ANALOG.Result) { ANALOG.Result(ANALOG.value); }
        if(ANALOG.hold) { DISPLAY_InvertLine(0); }
        if(ANALOG.settings.speed<=CHART_SPEED_4) {
            CHART_Marker();
        }
    }
}

static inline void ANALOG_ChangeCount(void) {
    ANALOG.count = CHART_Count(ANALOG.settings.speed);
    ANALOG.total = 0;
}

static void ANALOG_KeyUp(KEYPAD_KEY_t key) {
    if(ANALOG.hold && key!=KEYPAD_KEY3) { return; }
    switch(key) {
    case KEYPAD_KEY1:
        if(ANALOG.settings.speed>CHART_SPEED_1) {
            ANALOG.settings.speed--;
            ANALOG_ChangeCount();
            ANALOG_SaveSettings();
        }
        break;
    case KEYPAD_KEY2:
        if(ANALOG.settings.speed<CHART_SPEED_10) {
            ANALOG.settings.speed++;
            ANALOG_ChangeCount();
            ANALOG_SaveSettings();
        }
        break;
    case KEYPAD_KEY3:
        ANALOG_Hold();
        break;
    case KEYPAD_KEY4:
        CHART_Lock();
        break;
    default:
        break;
    }
}

static void ANALOG_Hold(void) {
    ANALOG.hold = !ANALOG.hold;
    if(ANALOG.hold) {
        EVSYS_CH2MUX = EVSYS_CHMUX_OFF_gc;
        DISPLAY_Backlight(DISPLAY_BACKLIGHT_AUX);
    } else {
        EVSYS_CH2MUX = EVSYS_CHMUX_ADCA_CH0_gc;
        DISPLAY_Backlight(DISPLAY_BACKLIGHT_MAIN);
    }
}

void ANALOG_Result(ANALOG_Result_t Result) {
    ANALOG.Result = Result;
}

ISR(TCC5_OVF_vect) {
    TCC5_INTFLAGS = TC5_OVFIF_bm;
    ANALOG.update = 1;
}

ISR(ADCA_CH0_vect) {
    static uint8_t idx = 0;
    switch(ADCA_CH0_INTCTRL&ADC_CH_INTMODE_gm) {
    case ADC_CH_INTMODE_BELOW_gc:
        if(++idx>2) {
            idx = 0;
            ADCA_CMP = ANALOG.trigger;
            ADCA_CH0_INTCTRL = ADC_CH_INTMODE_ABOVE_gc|ADC_CH_INTLVL_LO_gc;
        }
        break;
    case ADC_CH_INTMODE_ABOVE_gc:
        ADCA_CH0_INTCTRL = ADC_CH_INTMODE_COMPLETE_gc|ADC_CH_INTLVL_OFF_gc;
        BUFFER_Clear();
        if(!ANALOG.hold) {
            EVSYS_CH2MUX = EVSYS_CHMUX_ADCA_CH0_gc;
        }
        break;
    }
}

ISR(XCL_UNF_vect) {
    XCL_INTFLAGS = XCL_UNF0IF_bm;
    ADCA_CH0_INTCTRL = ADC_CH_INTMODE_COMPLETE_gc|ADC_CH_INTLVL_OFF_gc;
    if(!ANALOG.hold) {
        EVSYS_CH2MUX = EVSYS_CHMUX_ADCA_CH0_gc;
    }
}

void ANALOG_Calibration(void) {
    DISPLAY_Mode(DISPLAY_MODE_EDMA);
    MAIN_Loop(ANALOG_CalibrationUpdate);
    KEYPAD_KeyUp(ANALOG_CalibrationKeyUp);
    ANALOG.update = 1;
    DISPLAY_Select(-1);
}

static void ANALOG_CalibrationUpdate(void) {
    static int16_t sample;
    if(!DISPLAY_Update()) { return; }
    if(ANALOG.update) {
        ANALOG.update = 0;
        sample = ADCA_CH0RES;
    }
    DISPLAY_CursorPosition(15, 1);
    puts_P(TEXT_ADC_CALIBRATION);
    DISPLAY_CursorPosition(8, 15);
    puts_P(TEXT_ADC_VALUE);
    if(ANALOG.Result) { ANALOG.Result(sample); }
    DISPLAY_CursorPosition(2, 24);
    printf_P(TEXT_ADC_OFFSET, ANALOG.settings.offset);
    DISPLAY_CursorPosition(14, 33);
    uint16_t gain = ANALOG.settings.gain;
    uint8_t integer = gain>>11;
    uint16_t fraction = (gain>>1)&0x03FF;
    if(fraction>0x0200) { fraction = 999-(0x03FF-fraction); }
    printf_P(TEXT_ADC_GAIN, integer, fraction);
    DISPLAY_InvertLine(0);
    DISPLAY_SelectLine();
}

static void ANALOG_CalibrationKeyUp(KEYPAD_KEY_t key) {
    int16_t offset = ANALOG.settings.offset;
    uint16_t gain = ANALOG.settings.gain;
    switch(key) {
    case KEYPAD_KEY1:
        if(offset>OFFSET_MIN) { ANALOG.settings.offset--; }
        DISPLAY_Select(23);
        break;
    case KEYPAD_KEY2:
        if(offset<OFFSET_MAX) { ANALOG.settings.offset++; }
        DISPLAY_Select(23);
        break;
    case KEYPAD_KEY3:
        if(gain>GAIN_MIN) { ANALOG.settings.gain-=2; }
        DISPLAY_Select(32);
        break;
    case KEYPAD_KEY4:
        if(gain<GAIN_MAX) { ANALOG.settings.gain+=2; }
        DISPLAY_Select(32);
        break;
    default:
        break;
    }
    ANALOG_SaveSettings();
    ANALOG_CalibrationSetup();
}

static inline void ANALOG_CalibrationSetup(void) {
    int16_t offset = ANALOG.settings.offset;
    ADCA.CH0.OFFSETCORR0 = 0xFF&(((uint16_t)offset)>>0);
    ADCA.CH0.OFFSETCORR1 = 0x0F&(((uint16_t)offset)>>8);
    uint16_t gain = ANALOG.settings.gain;
    ADCA.CH0.GAINCORR0 = 0xFF&(gain>>0);
    ADCA.CH0.GAINCORR1 = 0x0F&(gain>>8);
    ADCA.CH0.CORRCTRL = ADC_CH_CORREN_bm;
}

void ANALOG_Info(void) {
    DISPLAY_CursorPosition(14, 1);
    DISPLAY_Icon(ICON_SLOWER);
    DISPLAY_CursorPosition(29, 2);
    puts_P(TEXT_SLOWER);
    DISPLAY_CursorPosition(15, 10);
    DISPLAY_Icon(ICON_FASTER);
    DISPLAY_CursorPosition(29, 11);
    puts_P(TEXT_FASTER);
    DISPLAY_CursorPosition(15, 19);
    DISPLAY_Icon(ICON_HOLD_RUN);
    DISPLAY_CursorPosition(29, 20);
    puts_P(TEXT_HOLD_RUN);
    DISPLAY_CursorPosition(13, 28);
    DISPLAY_Icon(ICON_LOCK);
    DISPLAY_CursorPosition(29, 29);
    puts_P(TEXT_SCALE);
    DISPLAY_Icons(ANALOG_ICONS);
    DISPLAY_Send();
    DELAY_Info();
}

static inline void ANALOG_SaveSettings(void) {
    EEPROM_update_block(&ANALOG.settings, ANALOG_settings, sizeof(ANALOG_SETTINGS_t));
}

static inline void ANALOG_LoadSettings(void) {
    eeprom_read_block(&ANALOG.settings, ANALOG_settings, sizeof(ANALOG_SETTINGS_t));
    if((ANALOG.settings.offset<OFFSET_MIN)||(ANALOG.settings.offset>OFFSET_MAX)) {
        ANALOG.settings.offset = OFFSET_MID;
    }
    if((ANALOG.settings.gain<GAIN_MIN)||(ANALOG.settings.gain>GAIN_MAX)) {
        ANALOG.settings.gain = GAIN_MID;
    }
    if(ANALOG.settings.speed>CHART_SPEED_10) {
        ANALOG.settings.speed = CHART_SPEED_1;
    }
    ANALOG_SaveSettings();
}

const __flash uint8_t* const __flash ANALOG_ICONS[] = {
    ICON_SLOWER,
    ICON_FASTER,
    ICON_HOLD_RUN,
    ICON_LOCK,
};
