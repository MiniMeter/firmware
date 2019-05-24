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
#include <avr/wdt.h>
#include <stdio.h>
#include "text.h"
#include "display.h"
#include "image.h"
#include "analog.h"
#include "chart.h"
#include "voltage.h"

ANALOG_SETTINGS_t VOLTAGE_settings EEMEM;

static void VOLTAGE_Result(int16_t value) {
    value += 5; //round to closest integer
    value /= 10;
    const char sign = (value<0) ? '-' : ' ';
    if(value<0) { value = -value; }
    if(value<700) {
        printf_P(TEXT_VOLTAGE_VALUE, sign, (int)(value/100), (int)(value%100));
    } else {
        printf_P(TEXT_OVERLOAD, sign);
        DISPLAY_MoveCursor(6);
    }
    DISPLAY_MoveCursor(2);
    puts_P(TEXT_VOLTAGE_UNIT);
}

static void VOLTAGE_Setup(void) {
    ADCA.CH0.CTRL = ADC_CH_GAIN_DIV2_gc|ADC_CH_INPUTMODE_DIFFWGAINH_gc;
    ADCA.CH0.MUXCTRL = ADC_CH_MUXPOS_PIN8_gc|ADC_CH_MUXNEGH_PIN5_gc;
    ADCA.CH0.AVGCTRL = (1<<ADC_CH_RIGHTSHIFT_gp)|ADC_SAMPNUM_8X_gc; // 14-bit mode (signed)
    ANALOG_Init(&VOLTAGE_settings);
    ANALOG_Result(VOLTAGE_Result);
}

void VOLTAGE_Init(void) {
    ANALOG_Info();
    VOLTAGE_Setup();
}

void VOLTAGE_Calibration(void) {
    VOLTAGE_Setup();
    ANALOG_Calibration();
}

void VOLTAGE_Intro(void) {
    DISPLAY_Image(IMAGE_VOLTAGE);
    DISPLAY_Icons(ANALOG_ICONS);
    DISPLAY_Send();
}
