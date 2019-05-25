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
#include "main.h"
#include "text.h"
#include "font.h"
#include "keypad.h"
#include "device.h"
#include "display.h"
#include "delay.h"
#include "image.h"
#include "buffer.h"
#include "digital.h"

#define DIGITAL_INVERT  (1<<15)

static struct {
    uint8_t row, column, roll, lock, end_line, hold, counter;
    DIGITAL_DISPLAY_t display;
    uint16_t idle;
    DIGITAL_Decode_t Decode;
    struct {
        uint8_t text[14];
        uint16_t control;
    } buffer[5];
} DIGITAL;

static void DIGITAL_Ready(void);
static void DIGITAL_Loop(void);
static void DIGITAL_NewLine(void);

void DIGITAL_Init(DIGITAL_Decode_t Decode) {
    DIGITAL.Decode = Decode;
    MAIN_Loop(DIGITAL_Ready);
    BUFFER_Clear();
    DIGITAL_Clear();
    DIGITAL.counter = 1;
    DIGITAL.hold = 0;
    DIGITAL.idle = 0;
    DIGITAL.lock = 1;
    DIGITAL.display = DIGITAL_DISPLAY_ASCII;
}

static void DIGITAL_Ready(void) {
    if(!DISPLAY_Update()) { return; }
    DISPLAY_CursorPosition(28,20);
    puts_P(TEXT_READY);
    DISPLAY_Idle();
    if(!BUFFER_Empty()) {
        MAIN_Loop(DIGITAL_Loop);
        DIGITAL.lock = 0;
    }
}

static void DIGITAL_Loop(void) {
    uint8_t overflow = 0;
    if(BUFFER_Overflow()) {
        overflow = 1;
        DIGITAL_PrintSymbol(FONT_SYMBOL_OVERFLOW);
        BUFFER_Stop();
        BUFFER_Reduce();
    }
    if(DIGITAL.Decode) {
        DIGITAL.Decode();
    }
    if(overflow) {
        DIGITAL_PrintSymbol(FONT_SYMBOL_OVERFLOW);
        DIGITAL_Hold(DIGITAL.hold);
    }
    if(DISPLAY_Update()) {
        DIGITAL_Update();
    }
}

void DIGITAL_Display(DIGITAL_DISPLAY_t display) {
    DIGITAL.display = display;
}

void DIGITAL_Print(uint8_t data) {
    if(DIGITAL.display==DIGITAL_DISPLAY_HEX) {
        DIGITAL_PrintHex(data>>4);
        DIGITAL_PrintHex(data>>0);
        DIGITAL_PrintTab();
    } else {
        if(data=='\n') {
            DIGITAL_NewLine();
        } else {
            DIGITAL_PrintChar(data);
        }
    }
}

void DIGITAL_PrintChar(uint8_t ch) {
    if((DIGITAL.end_line)||(DIGITAL.column>=14)) {
        DIGITAL.end_line = 0;
        DIGITAL_NewLine();
    }
    DIGITAL.buffer[DIGITAL.row].text[DIGITAL.column] = ch;
    DIGITAL.column++;
    DIGITAL.idle = 0;
}

void DIGITAL_PrintSymbol(uint8_t sym) {
    DIGITAL_PrintChar(sym);
    uint16_t mask = (1<<DIGITAL.column);
    if(DIGITAL.display==DIGITAL_DISPLAY_HEX) {
        DIGITAL_PrintChar(sym);
        mask |= (1<<DIGITAL.column);
        DIGITAL_PrintTab();
    }
    DIGITAL.buffer[DIGITAL.row].control |= mask;
}

void DIGITAL_PrintHex(uint8_t hex) {
    hex &= 0x0F;
    hex += '0'+((hex>9)*7);
    DIGITAL_PrintChar(hex);
}

void DIGITAL_PrintTab(void) {
    uint8_t tab_length = 3-(DIGITAL.column%3);
    if((DIGITAL.column+tab_length)>14) { return; }
    for(uint8_t i=0; i<tab_length; i++) {
        DIGITAL_PrintChar(' ');
    }
}

void DIGITAL_EndLine(void) {
    DIGITAL.end_line = 1;
}

static void DIGITAL_NewLine(void) {
    DIGITAL.column = 0;
    if(++DIGITAL.row>=5) {
        DIGITAL.row = 0;
        DIGITAL.roll = 1;
    }
    for(uint8_t i=0; i<14; i++) {
        DIGITAL.buffer[DIGITAL.row].text[i] = ' ';
    }
    DIGITAL.buffer[DIGITAL.row].control = 0;
    if(++DIGITAL.counter>DISPLAY_WIDTH) {
        DIGITAL.counter = 1;
    }
}

void DIGITAL_InvertLine(void) {
    DIGITAL.buffer[DIGITAL.row].control |= DIGITAL_INVERT;
}

void DIGITAL_Clear(void) {
    for(uint8_t y=0; y<5; y++) {
        for(uint8_t x=0; x<14; x++) {
            DIGITAL.buffer[y].text[x] = ' ';
        }
        DIGITAL.buffer[y].control = 0;
    }
    DIGITAL.row = 0;
    DIGITAL.column = 0;
    DIGITAL.roll = 0;
}

void DIGITAL_Blackout(void) {
    for(uint8_t y=0; y<5; y++) {
        uint16_t mask = (1<<1);
        for(uint8_t x=0; x<14; x++) {
            if(DIGITAL.buffer[y].text[x]!=' ') {
                DIGITAL.buffer[y].text[x] = FONT_SYMBOL_BLACK_BOX;
            }
            DIGITAL.buffer[y].control |= mask;
            mask<<=1;
        }
    }
}

uint8_t DIGITAL_Lock(void) {
    return DIGITAL.lock;
}

void DIGITAL_Hold(uint8_t hold) {
    DIGITAL.hold = hold;
    if(hold) {
        BUFFER_Stop();
        DISPLAY_Backlight(DISPLAY_BACKLIGHT_AUX);
    } else {
        BUFFER_Start();
        DISPLAY_Backlight(DISPLAY_BACKLIGHT_MAIN);
    }
    BUFFER_Clear();
    if(DIGITAL.idle<DELAY_Idle()) {
        DIGITAL.idle = 0;
    }
}

uint8_t DIGITAL_IsHold(void) {
    return DIGITAL.hold;
}

void DIGITAL_Update(void) {
    DISPLAY_Clear();
    uint8_t offset = 0;
    if(DIGITAL.roll) { offset = DIGITAL.row+1; }
    for(uint8_t i=0; i<5; i++) {
        uint8_t y = (offset+i)%5;
        DISPLAY_CursorPosition(1, (i*9)+1);
        uint16_t mask = (1<<1);
        for(uint8_t x=0; x<14; x++) {
            uint8_t ch = DIGITAL.buffer[y].text[x];
            uint16_t control = DIGITAL.buffer[y].control;
            if(!(control&mask)) {
                if((ch<32)||(ch>127)) { ch = 127; }
            }
            DISPLAY_PrintChar(ch);
            mask<<=1;
        }
        if(DIGITAL.buffer[y].control&DIGITAL_INVERT) {
            DISPLAY_InvertLine(i*9);
        }
    }
    if(DIGITAL.hold) {
        DISPLAY_ProgressBar(-1);
    } else {
        DISPLAY_ProgressBar(DIGITAL.counter);
    }
    if(DIGITAL.idle>=DELAY_Idle()) {
        DISPLAY_Idle();
    } else if(!DIGITAL.hold) {
        DIGITAL.idle++;
    }
}
