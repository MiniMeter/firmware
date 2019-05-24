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
#include <util/atomic.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include "avr/iox32e5.h"
#include "buffer.h"

void BUFFER_Init(BUFFER_MODE_t mode) {
    BUFFER.first = 0;
    BUFFER.last = 0;
    BUFFER.page = 0;
    BUFFER.flush = 0;
    BUFFER.mode = mode;
    if(mode==BUFFER_MODE_USART_RX) {
        USARTC0_STATUS = USART_RXCIF_bm;
        USARTD0_STATUS = USART_RXCIF_bm;
        USARTC0_CTRLA = USART_RXCINTLVL_LO_gc;
        USARTD0_CTRLA = USART_RXCINTLVL_LO_gc;
        return;
    }
    EDMA.CTRL &= ~EDMA_ENABLE_bm;
    EDMA.CTRL = EDMA_CHMODE_STD02_gc|EDMA_DBUFMODE_BUF0123_gc|EDMA_PRIMODE_CH0123_gc;
    EDMA.CH0.CTRLB = EDMA_CH_TRNINTLVL_LO_gc;
    EDMA.CH0.TRIGSRC = EDMA_CH_TRIGSRC_EVSYS_CH2_gc;
    EDMA.CH0.TRFCNT = sizeof(BUFFER.data);
    EDMA.CH0.DESTADDR = (uint16_t)&BUFFER.data;
    EDMA.CH0.DESTADDRCTRL = EDMA_CH_RELOAD_BLOCK_gc|EDMA_CH_DESTDIR_INC_gc;
    EDMA.CH0.ADDRCTRL = EDMA_CH_RELOAD_BURST_gc|EDMA_CH_DIR_INC_gc;
    EDMA.CH0.CTRLA = EDMA_CH_ENABLE_bm|EDMA_CH_SINGLE_bm|EDMA_CH_BURSTLEN_bm;
    EDMA.CH2.CTRLB = EDMA_CH_TRNINTLVL_LO_gc;
    EDMA.CH2.TRIGSRC = EDMA_CH_TRIGSRC_EVSYS_CH2_gc;
    EDMA.CH2.TRFCNT = sizeof(BUFFER.data);
    EDMA.CH2.DESTADDR = (uint16_t)&BUFFER.data;
    EDMA.CH2.DESTADDRCTRL = EDMA_CH_RELOAD_BLOCK_gc|EDMA_CH_DESTDIR_INC_gc;
    EDMA.CH2.ADDRCTRL = EDMA_CH_RELOAD_BURST_gc|EDMA_CH_DIR_INC_gc;
    EDMA.CH2.CTRLA = EDMA_CH_REPEAT_bm|EDMA_CH_SINGLE_bm|EDMA_CH_BURSTLEN_bm;
    switch(mode) {
    case BUFFER_MODE_PORTC_IN:
        EDMA.CH0.ADDR = (uint16_t)&PORTC_IN;
        EDMA.CH0.ADDRCTRL = EDMA_CH_RELOAD_NONE_gc|EDMA_CH_DIR_FIXED_gc;
        EDMA.CH0.CTRLA = EDMA_CH_ENABLE_bm|EDMA_CH_SINGLE_bm;
        EDMA.CH2.ADDR = (uint16_t)&PORTC_IN;
        EDMA.CH2.ADDRCTRL = EDMA_CH_RELOAD_NONE_gc|EDMA_CH_DIR_FIXED_gc;
        EDMA.CH2.CTRLA = EDMA_CH_REPEAT_bm|EDMA_CH_SINGLE_bm;
        break;
    case BUFFER_MODE_ADCA_RES:
        EDMA.CH0.ADDR = (uint16_t)&ADCA_CH0RES;
        EDMA.CH2.ADDR = (uint16_t)&ADCA_CH0RES;
        break;
    case BUFFER_MODE_TCC5_CCA:
        EDMA.CH0.ADDR = (uint16_t)&TCC5_CCA;
        EDMA.CH2.ADDR = (uint16_t)&TCC5_CCA;
        break;
    case BUFFER_MODE_TCC5_CNT:
        EDMA.CH0.ADDR = (uint16_t)&TCC5_CNT;
        EDMA.CH2.ADDR = (uint16_t)&TCC5_CNT;
        break;
    default:
        break;
    }
    EDMA.CTRL |= EDMA_ENABLE_bm;
}

void BUFFER_Start(void) {
    if(BUFFER.mode==BUFFER_MODE_USART_RX) {
        USARTC0_STATUS = USART_RXCIF_bm;
        USARTD0_STATUS = USART_RXCIF_bm;
        USARTC0_CTRLA = USART_RXCINTLVL_LO_gc;
        USARTD0_CTRLA = USART_RXCINTLVL_LO_gc;
    } else {
        EVSYS_CH2MUX = EVSYS_CHMUX_XCL_LUT0_gc;
    }
}

void BUFFER_Stop(void) {
    if(BUFFER.mode==BUFFER_MODE_USART_RX) {
        USARTC0_CTRLA = USART_RXCINTLVL_OFF_gc;
        USARTD0_CTRLA = USART_RXCINTLVL_OFF_gc;
    } else {
        EVSYS_CH2MUX = EVSYS_CHMUX_OFF_gc;
    }
}

static inline uint16_t BUFFER_EDMA_Last(void) {
    if(EDMA_STATUS&EDMA_CH0BUSY_bm) {
        return (-EDMA.CH0.TRFCNT)&BUFFER_MAX;
    } else {
        return (-EDMA.CH2.TRFCNT)&BUFFER_MAX;
    }
}

void BUFFER_Clear(void) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        if(BUFFER.mode!=BUFFER_MODE_USART_RX) {
            BUFFER.last = BUFFER_EDMA_Last();
        }
        BUFFER.first = BUFFER.last;
        BUFFER.page = 0;
    }
}

uint8_t BUFFER_Flush(void) {
    while(!BUFFER_Empty()) {
        BUFFER_GetSample();
        BUFFER.flush++;
    }
    if(BUFFER.flush>=8) {
        BUFFER.flush = 0;
        return 1;
    }
    return 0;
}

uint8_t BUFFER_Empty(void) {
    uint8_t empty = 0;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        if(BUFFER.mode!=BUFFER_MODE_USART_RX) {
            BUFFER.last = BUFFER_EDMA_Last();
        }
        if(BUFFER.first==BUFFER.last) {
            empty = 1;
        }
    }
    return empty;
}

void BUFFER_Reduce(void) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        if(BUFFER.mode!=BUFFER_MODE_USART_RX) {
            BUFFER.last = BUFFER_EDMA_Last();
        }
        BUFFER.first = BUFFER.last+2;
        BUFFER.first &= BUFFER_MAX;
        BUFFER.page = 0;
    }
}

uint8_t BUFFER_Overflow(void) {
    uint16_t use = 0;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        if(BUFFER.mode!=BUFFER_MODE_USART_RX) {
            BUFFER.last = BUFFER_EDMA_Last();
        }
        if(BUFFER.page!=0) {
            use = (uint16_t)BUFFER.page*sizeof(BUFFER.data);
            use += BUFFER.last-BUFFER.first;
        }
    }
    return (use>BUFFER_OVERFLOW);
}

static inline void BUFFER_NewData(uint8_t data) {
    BUFFER.data[BUFFER.last] = data;
    BUFFER.last++;
    BUFFER.last &= BUFFER_MAX;
    if(BUFFER.last==0) { BUFFER.page++; }
}

ISR(USARTC0_RXC_vect) {
    uint8_t status = USARTC0_STATUS&(USART_FERR_bm|USART_PERR_bm);
    BUFFER_NewData(status|USART_RXCIF_bm);
    BUFFER_NewData(USARTC0_DATA);
}

ISR(USARTD0_RXC_vect) {
    uint8_t status = USARTD0_STATUS&(USART_FERR_bm|USART_PERR_bm);
    BUFFER_NewData(status|USART_TXCIF_bm);
    BUFFER_NewData(USARTD0_DATA);
}

ISR(EDMA_CH0_vect) {
    EDMA_INTFLAGS = EDMA_CH0TRNFIF_bm;
    EDMA_CH0_CTRLA |= EDMA_CH_REPEAT_bm;
    BUFFER.page++;
}

ISR(EDMA_CH2_vect) {
    EDMA_INTFLAGS = EDMA_CH2TRNFIF_bm;
    EDMA_CH2_CTRLA |= EDMA_CH_REPEAT_bm;
    BUFFER.page++;
}
