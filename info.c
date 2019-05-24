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
#include <stdio.h>
#include "avr/ram.h"
#include "avr/flash.h"
#include "avr/eeprom.h"
#include "keypad.h"
#include "display.h"
#include "text.h"
#include "info.h"

static void INFO_KeyUp(KEYPAD_KEY_t key);
static void INFO_Firmware(void);
static void INFO_Ram(void);
static void INFO_Flash(void);
static void INFO_EEProm(void);

void INFO_Init(void) {
    DISPLAY_Clear();
    INFO_Firmware();
    DISPLAY_Send();
    KEYPAD_KeyUp(INFO_KeyUp);
    DISPLAY_Backlight(DISPLAY_BACKLIGHT_TOGGLE);
}

static void INFO_KeyUp(KEYPAD_KEY_t key) {
    DISPLAY_Clear();
    switch(key) {
        case KEYPAD_KEY1: INFO_Firmware(); break;
        case KEYPAD_KEY2: INFO_Ram(); break;
        case KEYPAD_KEY3: INFO_Flash(); break;
        case KEYPAD_KEY4: INFO_EEProm(); break;
        default: return;
    }
    DISPLAY_Send();
}

static void INFO_Firmware(void) {
    DISPLAY_CursorPosition(18, 14);
    puts_P(TEXT_INFO_FIRMWARE);
    DISPLAY_CursorPosition(24, 24);
    puts_P(TEXT_INFO_VERSION);
}

static uint8_t INFO_Percent(uint16_t usage, uint16_t size) {
    return (((uint32_t)usage*100)+(size/2))/size;
}

static void INFO_Ram(void) {
    DISPLAY_CursorPosition(15, 14);
    puts_P(TEXT_INFO_RAM);
    DISPLAY_CursorPosition(9, 24);
    uint16_t usage = RAM_Usage();
    printf_P(TEXT_INFO_USAGE, usage);
    DISPLAY_MoveCursor(3);
    uint8_t percent = INFO_Percent(usage, RAM_SIZE);
    printf_P(TEXT_INFO_PERCENT, percent);
}

static void INFO_Flash(void) {
    DISPLAY_CursorPosition(9, 14);
    puts_P(TEXT_INFO_FLASH);
    DISPLAY_CursorPosition(6, 24);
    uint16_t usage = FLASH_Usage();
    printf_P(TEXT_INFO_USAGE, usage);
    DISPLAY_MoveCursor(3);
    uint8_t percent = INFO_Percent(usage, FLASH_SIZE);
    printf_P(TEXT_INFO_PERCENT, percent);
}

static void INFO_EEProm(void) {
    DISPLAY_CursorPosition(6, 14);
    puts_P(TEXT_INFO_EEPROM);
    DISPLAY_CursorPosition(18, 24);
    uint16_t usage = EEPROM_Usage();
    printf_P(TEXT_INFO_USAGE, usage);
    DISPLAY_MoveCursor(3);
    uint8_t percent = INFO_Percent(usage, EEPROM_SIZE);
    printf_P(TEXT_INFO_PERCENT, percent);
}
