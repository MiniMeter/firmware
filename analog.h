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
#ifndef ANALOG_H_INCLUDED
#define ANALOG_H_INCLUDED

#include "chart.h"

typedef struct {
    int16_t offset;
    uint16_t gain;
    CHART_SPEED_t speed;
} ANALOG_SETTINGS_t;

typedef void (*ANALOG_Result_t)(int16_t value);

void ANALOG_Init(ANALOG_SETTINGS_t* settings);
void ANALOG_Result(ANALOG_Result_t Result);
void ANALOG_Calibration(void);
void ANALOG_Info(void);

extern const __flash uint8_t* const __flash ANALOG_ICONS[];

#endif // ANALOG_H_INCLUDED
