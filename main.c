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
#include <stdio.h>
#include "avr/eeprom.h"
#include "text.h"
#include "device.h"
#include "display.h"
#include "delay.h"
#include "keypad.h"
#include "image.h"
#include "current.h"
#include "voltage.h"
#include "uart.h"
#include "twi.h"
#include "onewire.h"
#include "usrt.h"
#include "spi.h"
#include "ircom.h"
#include "freq.h"
#include "charge.h"
#include "info.h"
#include "main.h"

typedef struct {
    uint8_t select;
} MAIN_SETTINGS_t;

static MAIN_SETTINGS_t MAIN_settings EEMEM;
static struct {
    uint8_t intro, page;
    MAIN_SETTINGS_t settings;
    MAIN_Loop_t Loop;
} MAIN;

static inline void MAIN_SaveSettings(void);
static inline void MAIN_LoadSettings(void);

static void MAIN_Run(KEYPAD_KEY_t key) {
    (void)key; //unused
    KEYPAD_KeyUp(NULL);
    switch(MAIN.settings.select) {
        case KEYPAD_KEY1: TWI_Init(); break;
        case KEYPAD_KEY2: VOLTAGE_Init(); break;
        case KEYPAD_KEY3: CURRENT_Init(); break;
        case KEYPAD_KEY4: UART_Init(); break;
        case KEYPAD_KEY12: ONEWIRE_Init(); break;
        case KEYPAD_KEY23: SPI_Init(); break;
        case KEYPAD_KEY34: USRT_Init(); break;
        case KEYPAD_KEY14: IRCOM_Init(); break;
        case KEYPAD_KEY13: FREQ_Init(); break;
        case KEYPAD_KEY24: CHARGE_Init(); break;
        default: break;
    }
}

static void MAIN_KeyLongPress(KEYPAD_KEY_t key) {
    if(((key>KEYPAD_KEY0)&&(key<KEYPAD_KEY123))||(key==KEYPAD_KEY1234)) {
        MAIN.settings.select = key;
        MAIN_SaveSettings();
    }
    DEVICE_Init();
    MAIN_Loop(NULL);
    DISPLAY_Mode(DISPLAY_MODE_RAW);
    DISPLAY_Backlight(DISPLAY_BACKLIGHT_MAIN);
    KEYPAD_KeyUp(NULL);
    KEYPAD_KeyDown(NULL);
    switch(key) {
        case KEYPAD_KEY1: TWI_Intro(); break;
        case KEYPAD_KEY2: VOLTAGE_Intro(); break;
        case KEYPAD_KEY3: CURRENT_Intro(); break;
        case KEYPAD_KEY4: UART_Intro(); break;
        case KEYPAD_KEY12: ONEWIRE_Intro(); break;
        case KEYPAD_KEY23: SPI_Intro(); break;
        case KEYPAD_KEY34: USRT_Intro(); break;
        case KEYPAD_KEY14: IRCOM_Intro(); break;
        case KEYPAD_KEY13: FREQ_Intro(); break;
        case KEYPAD_KEY24: CHARGE_Intro(); break;
        case KEYPAD_KEY123: DISPLAY_Settings(); return;
        case KEYPAD_KEY234: DELAY_Settings(); return;
        case KEYPAD_KEY134: VOLTAGE_Calibration(); return;
        case KEYPAD_KEY124: CURRENT_Calibration(); return;
        case KEYPAD_KEY1234: INFO_Init(); return;
        default: return;
    }
    KEYPAD_KeyUp(MAIN_Run);
}

static void MAIN_KeyDown(KEYPAD_KEY_t key) {
    (void)key; //unused
    MAIN.intro = -1;
}

static void MAIN_KeyUp(KEYPAD_KEY_t key) {
    (void)key; //unused
    MAIN.page++;
}

static void MAIN_Menu(void) {
    _delay_ms(20);
    uint8_t select = MAIN.settings.select;
    if(MAIN.intro==(DISPLAY_WIDTH+1)) {
        if(!select) {
            MAIN.intro = 0;
            MAIN.page++;
            return;
        }
        DEVICE_Init();
        DISPLAY_Mode(DISPLAY_MODE_RAW);
        MAIN_Loop(NULL);
        MAIN_Run(0);
        return;
    }
    DISPLAY_CursorPosition(6,1);
    puts_P(TEXT_PRESS_AND_HOLD);
    if(MAIN.page>3) { MAIN.page = 0; }
    switch(MAIN.page) {
    case 0:
        DISPLAY_CursorPosition(15,10);
        puts_P(TEXT_1_I2C_TWI);
        DISPLAY_CursorPosition(15,19);
        puts_P(TEXT_2_VOLTAGE);
        DISPLAY_CursorPosition(15,28);
        puts_P(TEXT_3_CURRENT);
        DISPLAY_CursorPosition(15,37);
        puts_P(TEXT_4_UART);
        break;
    case 1:
        DISPLAY_CursorPosition(12,10);
        puts_P(TEXT_12_1WIRE);
        DISPLAY_CursorPosition(12,19);
        puts_P(TEXT_23_SPI);
        DISPLAY_CursorPosition(12,28);
        puts_P(TEXT_34_USRT);
        DISPLAY_CursorPosition(12,37);
        puts_P(TEXT_14_IRCOM);
        if((select-5)<4) { select-=4; }
        break;
    case 2:
        DISPLAY_CursorPosition(15,10);
        puts_P(TEXT_13_FREQ);
        DISPLAY_CursorPosition(15,19);
        puts_P(TEXT_24_CHARGE);
        DISPLAY_CursorPosition(3,28);
        puts_P(TEXT_123_DISPLAY);
        DISPLAY_CursorPosition(3,37);
        puts_P(TEXT_234_DELAY);
        if((select-9)<2) { select-=8; }
        break;
    case 3:
        DISPLAY_CursorPosition(9,12);
        puts_P(TEXT_CALIBRATION);
        DISPLAY_CursorPosition(3,23);
        puts_P(TEXT_134_VOLTAGE);
        DISPLAY_CursorPosition(3,32);
        puts_P(TEXT_124_CURRENT);
        break;
    }
    if(MAIN.intro<=DISPLAY_WIDTH) {
        MAIN.intro++;
        if(select && select<5) {
            DISPLAY_InvertLine(select*9);
        }
    }
    DISPLAY_ProgressBar(MAIN.intro);
    DISPLAY_Send();
}

int main(void) {
    wdt_enable(WDT_PER_128CLK_gc);
    DEVICE_ClockFreq_32MHz();
    DEVICE_Init();
    DEVICE_InterruptEnable();
    DISPLAY_Init();
    DISPLAY_Mode(DISPLAY_MODE_USART);
    MAIN_Loop(MAIN_Menu);
    DISPLAY_Backlight(DISPLAY_BACKLIGHT_MAIN);
    DELAY_Init();
    KEYPAD_Init();
    KEYPAD_KeyDown(MAIN_KeyDown);
    KEYPAD_KeyUp(MAIN_KeyUp);
    KEYPAD_KeyLongPress(MAIN_KeyLongPress);
    MAIN_LoadSettings();
    while(1) {
        if(MAIN.Loop) { MAIN.Loop(); }
        DISPLAY_Loop();
        KEYPAD_Loop();
        wdt_reset();
    }
    return 0;
}

void MAIN_Loop(MAIN_Loop_t Loop) {
    MAIN.Loop = Loop;
    DISPLAY_Clear();
}

static inline void MAIN_SaveSettings(void) {
    EEPROM_update_block(&MAIN.settings, &MAIN_settings, sizeof(MAIN_SETTINGS_t));
}

static inline void MAIN_LoadSettings(void) {
    eeprom_read_block(&MAIN.settings, &MAIN_settings, sizeof(MAIN_SETTINGS_t));
    uint8_t select = MAIN.settings.select;
    if(select>KEYPAD_KEY24) { MAIN.settings.select = KEYPAD_KEY0; }
    if(select>KEYPAD_KEY4) { MAIN.page = 1; }
    if(select>KEYPAD_KEY14) { MAIN.page = 2; }
    MAIN_SaveSettings();
}
