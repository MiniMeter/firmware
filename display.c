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
#include "avr/eeprom.h"
#include "main.h"
#include "text.h"
#include "font.h"
#include "stream.h"
#include "device.h"
#include "keypad.h"
#include "lcd.h"
#include "led.h"
#include "display.h"

#define DISPLAY_CONTRAST_MIN  50
#define DISPLAY_CONTRAST_DEF  60
#define DISPLAY_CONTRAST_MAX  80
#define DISPLAY_FREQ_MIN  10 // Hz
#define DISPLAY_FREQ_DEF  50 // Hz
#define DISPLAY_FREQ_MAX  90 // Hz

typedef struct {
    uint8_t contrast;
    uint8_t freq;
    uint8_t backlight;
} DISPLAY_SETTINGS_t;

typedef struct {
    uint8_t x;
    uint8_t y;
} DISPLAY_CURSOR_t;

static DISPLAY_SETTINGS_t DISPLAY_settings EEMEM;
static struct {
    uint8_t* frame;
    uint8_t update, select;
    volatile uint8_t send;
    DISPLAY_CURSOR_t cursor;
    DISPLAY_SETTINGS_t settings;
} DISPLAY;

static void DISPLAY_RefreshFreq(uint8_t freq);
static void DISPLAY_SettingsLoop(void);
static void DISPLAY_SettingsKeyUp(KEYPAD_KEY_t key);
static inline void DISPLAY_SaveSettings(void);
static inline void DISPLAY_LoadSettings(void);

void DISPLAY_Init(void) {
    DISPLAY_LoadSettings();
    if(DISPLAY.settings.backlight) {
        DISPLAY_Backlight(DISPLAY_BACKLIGHT_ON);
    } else {
        DISPLAY_Backlight(DISPLAY_BACKLIGHT_OFF);
    }
    STREAM_Init();
    DISPLAY.update = 0;
    DISPLAY.frame = LCD_Init();
    LCD_Contrast(DISPLAY.settings.contrast);
    DISPLAY_RefreshFreq(DISPLAY.settings.freq);
    CLK_RTCCTRL = CLK_RTCSRC_RCOSC_gc|CLK_RTCEN_bm;
    DEVICE_RTC_Sync();
    RTC_CTRL = RTC_PRESCALER_DIV1_gc;
    RTC_INTCTRL = RTC_OVFINTLVL_LO_gc;
}

void DISPLAY_Mode(DISPLAY_MODE_t mode) {
    switch(mode) {
        case DISPLAY_MODE_RAW: LCD_RAW_Mode(); break;
        case DISPLAY_MODE_USART: LCD_USART_Mode(); break;
        case DISPLAY_MODE_EDMA: LCD_EDMA_Mode(); break;
    }
}

static void DISPLAY_RefreshFreq(uint8_t freq) {
    if((freq<DISPLAY_FREQ_MIN)||(freq>DISPLAY_FREQ_MAX)) {
        freq = DISPLAY_FREQ_DEF;
    }
    DEVICE_RTC_Sync();
    RTC_PER = (1000/freq)-1;
    DEVICE_RTC_Sync();
    RTC_CNT = 0;
}

void DISPLAY_Clear(void) {
    uint8_t* frame = DISPLAY.frame;
    for(uint16_t i=0; i<504; i++) {
        frame[i] = 0x00;
    }
}

void DISPLAY_Idle(void) {
    static uint8_t pattern = DISPLAY_GRAY;
    uint8_t* frame = DISPLAY.frame;
    for(uint16_t i=0; i<504; i++) {
        if((i%6)==0) { pattern^=0xFF; }
        frame[i] |= pattern;
    }
    pattern ^= 0xFF;
}

void DISPLAY_CursorPosition(uint8_t x, uint8_t y) {
    DISPLAY.cursor.x = x;
    DISPLAY.cursor.y = y;
}

void DISPLAY_MoveCursor(uint8_t offset) {
    DISPLAY.cursor.x += offset;
}

void DISPLAY_InvertLine(uint8_t top) {
    if(top>39) { return; }
    uint8_t bottom = top+9;
    uint8_t y = top>>3;
    uint8_t mask_top = 0xFF<<(top&0x07);
    uint8_t mask_bottom = 0xFF>>((-bottom)&0x07);
    uint8_t* frame = DISPLAY.frame;
    for(uint8_t x=0; x<DISPLAY_WIDTH; x++) {
        uint16_t idx = (x*6)+y;
        frame[idx+0] ^= mask_top;
        frame[idx+1] ^= mask_bottom;
    }
}

void DISPLAY_SelectLine(void) {
    static uint8_t pattern = DISPLAY_GRAY;
    uint8_t top = DISPLAY.select;
    if(top>39) { return; }
    uint8_t bottom = top+9;
    uint8_t y = top>>3;
    uint8_t mask_top = 0xFF<<(top&0x07);
    uint8_t mask_bottom = 0xFF>>((-bottom)&0x07);
    uint8_t* frame = DISPLAY.frame;
    for(uint8_t i=0; i<2; i++) {
        uint8_t pattern_top = pattern&mask_top;
        uint8_t pattern_bottom = pattern&mask_bottom;
        for(uint8_t x=i; x<DISPLAY_WIDTH; x+=2) {
            uint16_t idx = (x*6)+y;
            frame[idx+0] |= pattern_top;
            frame[idx+1] |= pattern_bottom;
        }
        pattern ^= 0xFF;
    }
    pattern ^= 0xFF;
}

void DISPLAY_Select(uint8_t top) {
    DISPLAY.select = top;
}

void DISPLAY_ProgressBar(uint8_t length) {
    uint8_t* frame = DISPLAY.frame;
    if(length>DISPLAY_WIDTH) {
        for(uint8_t x=0; x<DISPLAY_WIDTH; x+=2) {
            uint16_t idx = (x*6)+5;
            frame[idx+0] |= 0x40;
            frame[idx+6] |= 0x80;
        }
    } else {
        for(uint8_t x=0; x<length; x++) {
            frame[(x*6)+5] |= 0xC0;
        }
    }
}

void DISPLAY_PrintChar(uint8_t ch) {
    if((DISPLAY.cursor.x>(DISPLAY_WIDTH-5))||(DISPLAY.cursor.y>(DISPLAY_HEIGHT-8))) {
        return;
    }
    ch -= 32;
    uint16_t idx = (DISPLAY.cursor.y/8)+(DISPLAY.cursor.x*6);
    uint8_t offset_top = (DISPLAY.cursor.y)&0x07;
    uint8_t offset_bottom = 8-offset_top;
    uint8_t* frame = DISPLAY.frame;
    for(uint8_t i=0; i<5; i++) {
        uint8_t data = FONT[ch][i];
        frame[idx++] |= (data<<offset_top);
        frame[idx++] |= (data>>offset_bottom);
        idx+=4;
    }
    DISPLAY.cursor.x += 6;
}

void DISPLAY_ChartBar(int16_t value, uint8_t column, uint8_t pattern) {
    if(value<0) { value = 0; }
    if(value>39) { value = 39; }
    uint8_t bar = (uint8_t)value;
    if(column>=DISPLAY_WIDTH) { return; }
    uint8_t* frame = DISPLAY.frame;
    uint16_t idx = (column+1)*6;
    uint8_t fraction, full;
    fraction = bar&0x07;
    full = bar>>3;
    if(full>5) {full = 5;}
    for(uint8_t i=0; i<full; i++) {
        frame[--idx] |= pattern;
    }
    if(fraction) {
        frame[--idx] |= pattern&~(0xFF>>fraction);
    }
}

void DISPLAY_ClearBar(uint8_t column) {
    if(column>=DISPLAY_WIDTH) { return; }
    uint8_t* frame = DISPLAY.frame;
    uint16_t idx = column*6;
    frame[++idx] &= 0x01;
    frame[++idx] = 0x00;
    frame[++idx] = 0x00;
    frame[++idx] = 0x00;
    frame[++idx] = 0x00;

}

/* Run-length decoding */
void DISPLAY_Image(const __flash uint8_t* image) {
    uint8_t* frame = DISPLAY.frame;
    uint16_t i = 0;
    int8_t length = 0;
    for(uint16_t j=0; j<504; j++) {
        if(length==0) {
            length = image[i++];
        }
        frame[j] = image[i];
        if(length<0) {
            length++;
            i++;
        } else {
            length--;
            if(!length) { i++; }
        }
    }
}

void DISPLAY_Icon(const __flash uint8_t* icon) {
    uint16_t idx = (DISPLAY.cursor.y/8)+(DISPLAY.cursor.x*6);
    uint8_t offset_top = (DISPLAY.cursor.y)&0x07;
    uint8_t offset_bottom = 8-offset_top;
    uint8_t* frame = DISPLAY.frame;
    for(uint8_t i=0; i<12; i++) {
        uint8_t data = icon[i];
        frame[idx++] |= (data<<offset_top);
        frame[idx++] |= (data>>offset_bottom);
        idx+=4;
    }
}

void DISPLAY_Icons(const __flash uint8_t* const __flash icons[]) {
    DISPLAY_CursorPosition(0, 40);
    DISPLAY_Icon(icons[0]);
    DISPLAY_CursorPosition(24, 40);
    DISPLAY_Icon(icons[1]);
    DISPLAY_CursorPosition(48, 40);
    DISPLAY_Icon(icons[2]);
    DISPLAY_CursorPosition(72, 40);
    DISPLAY_Icon(icons[3]);
}

void DISPLAY_Send(void) {
    DISPLAY.frame = LCD_Send();
    while(LCD_Busy());
    DISPLAY.update = 1;
}

void DISPLAY_Loop(void) {
    uint8_t busy = LCD_Busy();
    if(DISPLAY.send && !DISPLAY.update) {
        if(busy) { while(LCD_Busy()); }
        DISPLAY.frame = LCD_Send();
        DISPLAY.send = 0;
        DISPLAY.update = 1;
    }
}

uint8_t DISPLAY_Update(void) {
    if(DISPLAY.update) {
        DISPLAY.update = 0;
        return 1;
    }
    return 0;
}

ISR(RTC_OVF_vect) {
    DISPLAY.send = 1;
}

void DISPLAY_Backlight(DISPLAY_BACKLIGHT_t backlight) {
    switch(backlight) {
    case DISPLAY_BACKLIGHT_MAIN:
        LED_AuxOff();
        LED_MainOn();
        return;
    case DISPLAY_BACKLIGHT_AUX:
        LED_AuxOn();
        LED_MainOff();
        return;
    case DISPLAY_BACKLIGHT_OFF:
        DISPLAY.settings.backlight = 0;
        break;
    case DISPLAY_BACKLIGHT_ON:
        DISPLAY.settings.backlight = 1;
        break;
    case DISPLAY_BACKLIGHT_TOGGLE:
        DISPLAY.settings.backlight = !DISPLAY.settings.backlight;
        break;
    }
    if(DISPLAY.settings.backlight) {
        LED_On();
    } else {
        LED_Off();
    }
    DISPLAY_SaveSettings();
}

void DISPLAY_Settings(void) {
    DISPLAY_Mode(DISPLAY_MODE_EDMA);
    MAIN_Loop(DISPLAY_SettingsLoop);
    KEYPAD_KeyUp(DISPLAY_SettingsKeyUp);
    DISPLAY_Select(-1);
}

static void DISPLAY_SettingsLoop(void) {
    if(!DISPLAY_Update()) { return; }
    DISPLAY_CursorPosition(4, 1);
    puts_P(TEXT_DISPLAY_SETUP);
    DISPLAY_CursorPosition(2, 15);
    printf_P(TEXT_CONTRAST, DISPLAY.settings.contrast);
    DISPLAY_CursorPosition(2, 24);
    printf_P(TEXT_REFRESH, DISPLAY.settings.freq);
    DISPLAY_MoveCursor(2);
    puts_P(TEXT_FREQ_Hz);
    DISPLAY_CursorPosition(2, 33);
    puts_P(TEXT_BACKLIGHT);
    if(DISPLAY.settings.backlight==DISPLAY_BACKLIGHT_OFF) {
        DISPLAY_MoveCursor(3);
        puts_P(TEXT_OFF);
    } else {
        DISPLAY_MoveCursor(6);
        puts_P(TEXT_ON);
    }
    DISPLAY_InvertLine(0);
    DISPLAY_SelectLine();
}

static void DISPLAY_SettingsKeyUp(KEYPAD_KEY_t key) {
    uint8_t contrast = DISPLAY.settings.contrast;
    switch(key) {
        case KEYPAD_KEY1:
            if(contrast>DISPLAY_CONTRAST_MIN) {
                DISPLAY.settings.contrast--;
            }
            LCD_Contrast(DISPLAY.settings.contrast);
            DISPLAY_Select(14);
            break;
        case KEYPAD_KEY2:
            if(contrast<DISPLAY_CONTRAST_MAX) {
                DISPLAY.settings.contrast++;
            }
            LCD_Contrast(DISPLAY.settings.contrast);
            DISPLAY_Select(14);
            break;
        case KEYPAD_KEY3:
            DISPLAY.settings.freq += 10;
            if(DISPLAY.settings.freq>DISPLAY_FREQ_MAX) {
                DISPLAY.settings.freq = DISPLAY_FREQ_MIN;
            }
            DISPLAY_RefreshFreq(DISPLAY.settings.freq);
            DISPLAY_Select(23);
            break;
        case KEYPAD_KEY4:
            DISPLAY_Backlight(DISPLAY_BACKLIGHT_TOGGLE);
            DISPLAY_Select(32);
            break;
        default: break;
    }
    DISPLAY_SaveSettings();
}

static inline void DISPLAY_SaveSettings(void) {
    EEPROM_update_block(&DISPLAY.settings, &DISPLAY_settings, sizeof(DISPLAY_SETTINGS_t));
}

static inline void DISPLAY_LoadSettings(void) {
    eeprom_read_block(&DISPLAY.settings, &DISPLAY_settings, sizeof(DISPLAY_SETTINGS_t));
    uint8_t contrast = DISPLAY.settings.contrast;
    if((contrast<DISPLAY_CONTRAST_MIN)||(contrast>DISPLAY_CONTRAST_MAX)) {
        DISPLAY.settings.contrast = DISPLAY_CONTRAST_DEF;
    }
    uint8_t freq = DISPLAY.settings.freq;
    if((freq<DISPLAY_FREQ_MIN)||(freq>DISPLAY_FREQ_MAX)) {
        DISPLAY.settings.freq = DISPLAY_FREQ_DEF;
    }
    DISPLAY_SaveSettings();
}
