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
#include "text.h"
#include "display.h"
#include "image.h"
#include "analog.h"
#include "chart.h"
#include "buffer.h"
#include "current.h"

static ANALOG_SETTINGS_t CURRENT_settings EEMEM;

static void CURRENT_Result(int16_t value) {
    value += 1; //round to closest integer
    value /= 2;
    if((value>(-250))&&(value<1000)) {
        printf_P(TEXT_CURRENT_VALUE, value);
    } else {
        const char sign = (value<0) ? '-' : ' ';
        printf_P(TEXT_OVERLOAD, sign);
    }
    DISPLAY_MoveCursor(2);
    puts_P(TEXT_CURRENT_UNIT);
}

void CURRENT_Setup(void) {
    ADCA.CH0.CTRL = ADC_CH_GAIN_1X_gc|ADC_CH_INPUTMODE_DIFFWGAINH_gc;
    ADCA.CH0.MUXCTRL = ADC_CH_MUXPOS_PIN7_gc|ADC_CH_MUXNEGH_PIN6_gc;
    ADCA.CH0.AVGCTRL = (3<<ADC_CH_RIGHTSHIFT_gp)|ADC_SAMPNUM_8X_gc; // 12-bit mode (signed)
    ANALOG_Init(&CURRENT_settings);
    ANALOG_Result(CURRENT_Result);
}

void CURRENT_Init(void) {
    ANALOG_Info();
    CURRENT_Setup();
}

void CURRENT_Calibration(void) {
    CURRENT_Setup();
    ANALOG_Calibration();
}

void CURRENT_Intro(void) {
    DISPLAY_Image(IMAGE_CURRENT);
    DISPLAY_Icons(ANALOG_ICONS);
    DISPLAY_Send();
}
