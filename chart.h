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
#ifndef CHART_H_INCLUDED
#define CHART_H_INCLUDED

#define CHART_FULL_SCALE  39

typedef enum {
    CHART_SPEED_1,
    CHART_SPEED_2,
    CHART_SPEED_3,
    CHART_SPEED_4,
    CHART_SPEED_5,
    CHART_SPEED_6,
    CHART_SPEED_7,
    CHART_SPEED_8,
    CHART_SPEED_9,
    CHART_SPEED_10,
} CHART_SPEED_t;

void CHART_Init(void);
void CHART_Clear(void);
uint8_t CHART_Sample(void);
void CHART_Update(void);
void CHART_Value(int16_t max, int16_t avg, int16_t min);
void CHART_Marker(void);
uint16_t CHART_Count(CHART_SPEED_t speed);
uint8_t CHART_Column(void);
int16_t CHART_Max(void);
int16_t CHART_Min(void);
void CHART_Lock(void);

#endif // CHART_H_INCLUDED
