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
#include <stdio.h>
#include "buffer.h"
#include "display.h"
#include "device.h"
#include "text.h"
#include "image.h"
#include "icon.h"
#include "chart.h"

static struct CHART_struct {
    uint8_t lock, scale, column, clear;
    int16_t max, min;
    uint16_t sample, count;
    struct {
        int16_t max;
        int16_t avg;
        int16_t min;
    } buffer[84];
} CHART;

void CHART_Init(void) {
    CHART.lock = 0;
    CHART_Clear();
}

void CHART_Clear(void) {
    CHART.column = 0xFF;
    CHART.max = 0;
    for(uint8_t i=0; i<DISPLAY_WIDTH; i++) {
        CHART.buffer[i].max = 0;
        CHART.buffer[i].avg = 0;
        CHART.buffer[i].min = 0;
    }
}

uint8_t CHART_Sample(void) {
    CHART.sample++;
    if(CHART.sample>=CHART.count) {
        CHART.sample = 0;
        CHART.column++;
        if(CHART.column>=DISPLAY_WIDTH) {
            CHART.column = 0;
        }
        return 1;
    }
    return 0;
}

void CHART_Update(void) {
    static uint8_t pattern = DISPLAY_GRAY;
    int16_t max = INT16_MIN;
    int16_t min = INT16_MAX;
    int16_t value;
    for(uint8_t i=0; i<DISPLAY_WIDTH; i++) {
        value = CHART.buffer[i].max;
        if(value>max) { max = value; }
        value = CHART.buffer[i].min;
        if(value<min) { min = value; }
    }
    CHART.max = max;
    CHART.min = min;
    uint8_t scale = (max/CHART_FULL_SCALE)+1;
    DISPLAY_CursorPosition(72, 0);
    if(CHART.lock) {
        scale = CHART.scale;
        DISPLAY_Icon(ICON_LOCK);
    } else {
        CHART.scale = scale;
        DISPLAY_Icon(ICON_SCALE);
    }
    DISPLAY_CursorPosition(55, 1);
    printf_P(TEXT_ADC_SCALE, scale);
    for(uint8_t i=0; i<DISPLAY_WIDTH; i++) {
        DISPLAY_ChartBar((CHART.buffer[i].max/scale), i, pattern);
        DISPLAY_ChartBar((CHART.buffer[i].avg/scale), i, DISPLAY_BLACK);
        pattern = ~pattern;
    }
    pattern = ~pattern;
}

void CHART_Value(int16_t max, int16_t avg, int16_t min) {
    uint8_t column = CHART.column;
    CHART.buffer[column].max = max;
    CHART.buffer[column].avg = avg;
    CHART.buffer[column].min = min;
}

void CHART_Marker(void) {
    DISPLAY_ClearBar(CHART.column);
}

uint16_t  CHART_Count(CHART_SPEED_t speed) {
    static const __flash uint16_t COUNT[] = {
        [CHART_SPEED_1] = 1000,
        [CHART_SPEED_2] = 500,
        [CHART_SPEED_3] = 200,
        [CHART_SPEED_4] = 100,
        [CHART_SPEED_5] = 50,
        [CHART_SPEED_6] = 20,
        [CHART_SPEED_7] = 10,
        [CHART_SPEED_8] = 5,
        [CHART_SPEED_9] = 2,
        [CHART_SPEED_10] = 1,
    };
    CHART.count = COUNT[speed];
    CHART.sample = 0;
    return CHART.count;
}

uint8_t CHART_Column(void) {
    return CHART.column;
}

int16_t CHART_Max(void) {
    return CHART.max;
}

int16_t CHART_Min(void) {
    return CHART.min;
}

void CHART_Lock(void) {
    CHART.lock = !CHART.lock;
}
