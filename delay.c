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
#include <util/delay.h>
#include "avr/eeprom.h"
#include "main.h"
#include "text.h"
#include "keypad.h"
#include "display.h"
#include "device.h"
#include "delay.h"

#define DELAY_INFO_DEFAULT 1 // 1s
#define DELAY_INFO_MAX     3 // 3s
#define DELAY_IDLE_DEFAULT 4 // 8s
#define DELAY_IDLE_MIN     1 // 1s
#define DELAY_IDLE_MAX    60 // 60s

typedef struct {
    uint8_t info, idle;
} DELAY_SETTINGS_t;

static DELAY_SETTINGS_t DELAY_settings EEMEM;
static struct {
    DELAY_SETTINGS_t settings;
} DELAY;

static void DELAY_SettingsLoop(void);
static void DELAY_SettingsKeyUp(KEYPAD_KEY_t key);
static inline void DELAY_SaveSettings(void);
static inline void DELAY_LoadSettings(void);

void DELAY_Init(void) {
    DELAY_LoadSettings();
}

void DELAY_Info(void) {
    uint8_t info = DELAY.settings.info*10;
    for(uint8_t i=0; i<info; i++) {
        wdt_reset();
        _delay_ms(100);
    }
}

uint16_t DELAY_Idle(void) {
    return DELAY.settings.idle*100;
}

void DELAY_Message(void) {
    for(uint8_t i=0; i<5; i++) {
        wdt_reset();
        _delay_ms(100);
    }
}

void DELAY_Settings(void) {
    DISPLAY_Mode(DISPLAY_MODE_EDMA);
    MAIN_Loop(DELAY_SettingsLoop);
    KEYPAD_KeyUp(DELAY_SettingsKeyUp);
    DISPLAY_Select(-1);
}

static void DELAY_SettingsLoop(void) {
    if(!DISPLAY_Update()) { return; }
    DISPLAY_CursorPosition(9, 1);
    puts_P(TEXT_DELAY_SETUP);
    DISPLAY_CursorPosition(18, 19);
    printf_P(TEXT_DELAY_INFO, DELAY.settings.info);
    DISPLAY_CursorPosition(18, 28);
    printf_P(TEXT_DELAY_IDLE, DELAY.settings.idle);
    DISPLAY_InvertLine(0);
    DISPLAY_SelectLine();
}

static void DELAY_SettingsKeyUp(KEYPAD_KEY_t key) {
    uint8_t info = DELAY.settings.info;
    uint8_t idle = DELAY.settings.idle;
    switch(key) {
        case KEYPAD_KEY1:
            if(info>0) {
                DELAY.settings.info--;
            }
            DISPLAY_Select(18);
            break;
        case KEYPAD_KEY2:
            if(info<DELAY_INFO_MAX) {
                DELAY.settings.info++;
            }
            DISPLAY_Select(18);
            break;
        case KEYPAD_KEY3:
            if(idle>DELAY_IDLE_MIN) {
                DELAY.settings.idle--;
            }
            DISPLAY_Select(27);
            break;
        case KEYPAD_KEY4:
            if(idle<DELAY_IDLE_MAX) {
                DELAY.settings.idle++;
            }
            DISPLAY_Select(27);
            break;
        default: break;
    }
    DELAY_SaveSettings();
}

static inline void DELAY_SaveSettings(void) {
    EEPROM_update_block(&DELAY.settings, &DELAY_settings, sizeof(DELAY_SETTINGS_t));
}

static inline void DELAY_LoadSettings(void) {
    eeprom_read_block(&DELAY.settings, &DELAY_settings, sizeof(DELAY_SETTINGS_t));
    if(DELAY.settings.info>DELAY_INFO_MAX) {
        DELAY.settings.info = DELAY_INFO_DEFAULT;
    }
    uint8_t idle = DELAY.settings.idle;
    if((idle<DELAY_IDLE_MIN)||(idle>DELAY_IDLE_MAX)) {
        DELAY.settings.idle = DELAY_IDLE_DEFAULT;
    }
    DELAY_SaveSettings();
}
