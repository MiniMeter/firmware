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
#include <stddef.h>
#include "device.h"
#include "keypad.h"

#define KEY1_bm  PIN2_bm
#define KEY2_bm  PIN3_bm
#define KEY3_bm  PIN4_bm
#define KEY4_bm  PIN5_bm
#define KEY12_bm  (KEY1_bm|KEY2_bm)
#define KEY13_bm  (KEY1_bm|KEY3_bm)
#define KEY14_bm  (KEY1_bm|KEY4_bm)
#define KEY23_bm  (KEY2_bm|KEY3_bm)
#define KEY24_bm  (KEY2_bm|KEY4_bm)
#define KEY34_bm  (KEY3_bm|KEY4_bm)
#define KEY123_bm  (KEY1_bm|KEY2_bm|KEY3_bm)
#define KEY124_bm  (KEY1_bm|KEY2_bm|KEY4_bm)
#define KEY134_bm  (KEY1_bm|KEY3_bm|KEY4_bm)
#define KEY234_bm  (KEY2_bm|KEY3_bm|KEY4_bm)
#define KEY1234_bm  (KEY1_bm|KEY2_bm|KEY3_bm|KEY4_bm)

#define KEY_UP(key) (KEYPAD_EVENT_t){key, KEYPAD_UP}
#define KEY_DOWN(key) (KEYPAD_EVENT_t){key, KEYPAD_DOWN}
#define KEY_LONG_PRESS(key) (KEYPAD_EVENT_t){key, KEYPAD_LONG_PRESS}

typedef enum {
    KEYPAD_DOWN,
    KEYPAD_UP,
    KEYPAD_LONG_PRESS
} KEYPAD_TYPE_t;

typedef struct {
    KEYPAD_KEY_t key:6;
    KEYPAD_TYPE_t type:2;
} KEYPAD_EVENT_t;

typedef struct {
    KEYPAD_EVENT_t event[8];
    volatile uint8_t first;
    volatile uint8_t last;
} KEYPAD_BUFFER_t;

static struct {
    KEYPAD_BUFFER_t buffer;
    uint8_t state;
    uint8_t changed;
    uint8_t flush;
    KEYPAD_Callback_t KeyDown;
    KEYPAD_Callback_t KeyUp;
    KEYPAD_Callback_t KeyLongPress;
} KEYPAD;

void KEYPAD_Init(void) {
    PORTCFG.MPCMASK = KEY1_bm|KEY2_bm|KEY3_bm|KEY4_bm;
    PORTC.PIN2CTRL = PORT_OPC_PULLUP_gc|PORT_ISC_BOTHEDGES_gc|PORT_INVEN_bm;
    PORTC.INTMASK = KEY1_bm|KEY2_bm|KEY3_bm|KEY4_bm;
    PORTC.INTCTRL = PORT_INTLVL_LO_gc;
    TCC4.CTRLA = TC45_CLKSEL_OFF_gc;
    TCC4.INTCTRLA = TC45_OVFINTLVL_LO_gc;
    TCC4.INTCTRLB = TC45_CCAINTLVL_LO_gc|TC45_CCBINTLVL_LO_gc|TC45_CCCINTLVL_LO_gc|TC45_CCDINTLVL_LO_gc;
    TCC4.CCA = 0x0100; // ~8ms
    TCC4.CCB = 0x0200; //~16ms
    TCC4.CCC = 0x0300; //~24ms
    TCC4.CCD = 0x0400; //~32ms
    TCC4.PER = (uint16_t)15*3125; //1.5s (long press)
}

static inline uint8_t KEYPAD_BufferEmpty(void) {
    return KEYPAD.buffer.first == KEYPAD.buffer.last;
}

static inline void KEYPAD_NewEvent(KEYPAD_EVENT_t event) {
    KEYPAD.buffer.last = (KEYPAD.buffer.last+1)&0x07;
    KEYPAD.buffer.event[KEYPAD.buffer.last] = event;
}

static inline KEYPAD_EVENT_t KEYPAD_GetEvent(void) {
    KEYPAD.buffer.first = (KEYPAD.buffer.first+1)&0x07;
    return KEYPAD.buffer.event[KEYPAD.buffer.first];
}

static void KEYPAD_Debounce() {
    static struct {
        uint8_t low;
        uint8_t high;
    } vcount = {0xFF, 0xFF};
    KEYPAD.changed = (PORTC_IN^KEYPAD.state)&(KEY1_bm|KEY2_bm|KEY3_bm|KEY4_bm);
    vcount.low = ~(vcount.low&KEYPAD.changed);
    vcount.high = vcount.low^(vcount.high&KEYPAD.changed);
    KEYPAD.changed &= vcount.high&vcount.low;
    KEYPAD.state ^= KEYPAD.changed;
}

static inline uint8_t KEYPAD_DownKey(void) {
    return KEYPAD.state&KEYPAD.changed;
}

static inline uint8_t KEYPAD_UpKey(void) {
    return (~KEYPAD.state)&KEYPAD.changed;
}

static inline uint8_t KEYPAD_LongPressKey(void) {
    return PORTC_IN&(KEY1_bm|KEY2_bm|KEY3_bm|KEY4_bm);
}

void KEYPAD_Loop(void) {
    if(KEYPAD_BufferEmpty()) { return; }
    KEYPAD_EVENT_t event = KEYPAD_GetEvent();
    switch(event.type) {
    case KEYPAD_DOWN:
        if(KEYPAD.KeyDown) { KEYPAD.KeyDown(event.key); }
        break;
    case KEYPAD_UP:
        if(KEYPAD.KeyUp) { KEYPAD.KeyUp(event.key); }
        break;
    case KEYPAD_LONG_PRESS:
        if(KEYPAD.KeyLongPress) { KEYPAD.KeyLongPress(event.key); }
        break;
    }
}

ISR(PORTC_INT_vect) {
    PORTC_INTFLAGS = 0xFF;
    TCC4_CTRLGSET = TC45_CMD_RESTART_gc;
    TCC4_CTRLA = TC4_UPSTOP_bm|TC45_CLKSEL_DIV1024_gc;
}

ISR(TCC4_CCA_vect, ISR_ALIASOF(TCC4_CCD_vect));
ISR(TCC4_CCB_vect, ISR_ALIASOF(TCC4_CCD_vect));
ISR(TCC4_CCC_vect, ISR_ALIASOF(TCC4_CCD_vect));
ISR(TCC4_CCD_vect) {
    KEYPAD_Debounce();
    if(KEYPAD.flush) {
        if(!KEYPAD.state) {
            KEYPAD.flush = 0;
            KEYPAD_NewEvent(KEY_UP(KEYPAD_KEY0));
        }
        return;
    }
    uint8_t down = KEYPAD_DownKey();
    if(down&KEY1_bm) { KEYPAD_NewEvent(KEY_DOWN(KEYPAD_KEY1)); }
    if(down&KEY2_bm) { KEYPAD_NewEvent(KEY_DOWN(KEYPAD_KEY2)); }
    if(down&KEY3_bm) { KEYPAD_NewEvent(KEY_DOWN(KEYPAD_KEY3)); }
    if(down&KEY4_bm) { KEYPAD_NewEvent(KEY_DOWN(KEYPAD_KEY4)); }
    uint8_t up = KEYPAD_UpKey();
    if(up&KEY1_bm) { KEYPAD_NewEvent(KEY_UP(KEYPAD_KEY1)); }
    if(up&KEY2_bm) { KEYPAD_NewEvent(KEY_UP(KEYPAD_KEY2)); }
    if(up&KEY3_bm) { KEYPAD_NewEvent(KEY_UP(KEYPAD_KEY3)); }
    if(up&KEY4_bm) { KEYPAD_NewEvent(KEY_UP(KEYPAD_KEY4)); }
}

ISR(TCC4_OVF_vect) {
    TCC4_INTFLAGS = TC4_OVFIF_bm;
    switch(KEYPAD_LongPressKey()) {
        case KEY1_bm: KEYPAD_NewEvent(KEY_LONG_PRESS(KEYPAD_KEY1)); return;
        case KEY2_bm: KEYPAD_NewEvent(KEY_LONG_PRESS(KEYPAD_KEY2)); return;
        case KEY3_bm: KEYPAD_NewEvent(KEY_LONG_PRESS(KEYPAD_KEY3)); return;
        case KEY4_bm: KEYPAD_NewEvent(KEY_LONG_PRESS(KEYPAD_KEY4)); return;
        case KEY12_bm: KEYPAD_NewEvent(KEY_LONG_PRESS(KEYPAD_KEY12)); break;
        case KEY13_bm: KEYPAD_NewEvent(KEY_LONG_PRESS(KEYPAD_KEY13)); break;
        case KEY14_bm: KEYPAD_NewEvent(KEY_LONG_PRESS(KEYPAD_KEY14)); break;
        case KEY23_bm: KEYPAD_NewEvent(KEY_LONG_PRESS(KEYPAD_KEY23)); break;
        case KEY24_bm: KEYPAD_NewEvent(KEY_LONG_PRESS(KEYPAD_KEY24)); break;
        case KEY34_bm: KEYPAD_NewEvent(KEY_LONG_PRESS(KEYPAD_KEY34)); break;
        case KEY123_bm: KEYPAD_NewEvent(KEY_LONG_PRESS(KEYPAD_KEY123)); break;
        case KEY124_bm: KEYPAD_NewEvent(KEY_LONG_PRESS(KEYPAD_KEY124)); break;
        case KEY134_bm: KEYPAD_NewEvent(KEY_LONG_PRESS(KEYPAD_KEY134)); break;
        case KEY234_bm: KEYPAD_NewEvent(KEY_LONG_PRESS(KEYPAD_KEY234)); break;
        case KEY1234_bm: KEYPAD_NewEvent(KEY_LONG_PRESS(KEYPAD_KEY1234)); break;
        default: break;
    }
    if(KEYPAD.state) { KEYPAD.flush = 1; }
}

void KEYPAD_KeyDown(KEYPAD_Callback_t KeyDown) {
    KEYPAD.KeyDown = KeyDown;
    KEYPAD.buffer.first = KEYPAD.buffer.last;
}

void KEYPAD_KeyUp(KEYPAD_Callback_t KeyUp) {
    KEYPAD.KeyUp = KeyUp;
    KEYPAD.buffer.first = KEYPAD.buffer.last;
}

void KEYPAD_KeyLongPress(KEYPAD_Callback_t KeyLongPress) {
    KEYPAD.KeyLongPress = KeyLongPress;
    KEYPAD.buffer.first = KEYPAD.buffer.last;
}
