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
#ifndef BUFFER_H_INCLUDED
#define BUFFER_H_INCLUDED

#include <util/atomic.h>

#define BUFFER_BIT_SIZE  11 // 2^11 = 2048
#define BUFFER_SIZE  (1<<BUFFER_BIT_SIZE)
#define BUFFER_MAX  (BUFFER_SIZE-1)
#define BUFFER_MARGIN  16
#define BUFFER_OVERFLOW  (BUFFER_SIZE-BUFFER_MARGIN)

typedef enum {
    BUFFER_MODE_USART_RX,
    BUFFER_MODE_PORTC_IN,
    BUFFER_MODE_ADCA_RES,
    BUFFER_MODE_TCC5_CCA,
    BUFFER_MODE_TCC5_CNT,
} BUFFER_MODE_t;

struct {
    uint16_t first;
    volatile uint16_t last;
    uint8_t flush;
    volatile uint8_t page;
    BUFFER_MODE_t mode;
    union {
        uint8_t data[BUFFER_SIZE];
        int16_t sample[BUFFER_SIZE/sizeof(int16_t)];
    };
} BUFFER;

void BUFFER_Init(BUFFER_MODE_t mode);
void BUFFER_Start(void);
void BUFFER_Stop(void);
void BUFFER_Clear(void);
uint8_t BUFFER_Flush(void);
uint8_t BUFFER_Empty(void);
uint8_t BUFFER_Overflow(void);
void BUFFER_Reduce(void);

static inline uint8_t BUFFER_GetData(void) {
    uint8_t data;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        data = BUFFER.data[BUFFER.first];
        BUFFER.first++;
        BUFFER.first &= BUFFER_MAX;
        if(BUFFER.first==0 && BUFFER.page>0) { BUFFER.page--; }
    }
    return data;
}

static inline int16_t BUFFER_GetSample(void) {
    int16_t data;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        data = BUFFER.sample[BUFFER.first/sizeof(uint16_t)];
        BUFFER.first += sizeof(uint16_t);
        BUFFER.first &= BUFFER_MAX;
        if(BUFFER.first==0 && BUFFER.page>0) { BUFFER.page--; }
    }
    return data;
}

#endif // BUFFER_H_INCLUDED
