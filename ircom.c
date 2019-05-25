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
#include "avr/eeprom.h"
#include "main.h"
#include "text.h"
#include "font.h"
#include "device.h"
#include "usart.h"
#include "keypad.h"
#include "buffer.h"
#include "digital.h"
#include "display.h"
#include "delay.h"
#include "image.h"
#include "uart.h"
#include "ircom.h"

#ifdef IRCOM
    #undef IRCOM
#endif // IRCOM

typedef enum {
    IRCOM_INVERT_NO,
    IRCOM_INVERT_YES
} IRCOM_INVERT_t;

typedef struct {
    USART_BAUD_t baud;
    USART_PARITY_t parity;
    DIGITAL_DISPLAY_t display;
    IRCOM_INVERT_t invert;
} IRCOM_SETTINGS_t;

static IRCOM_SETTINGS_t IRCOM_settings EEMEM;
static struct {
    IRCOM_SETTINGS_t settings;
} IRCOM;

static void IRCOM_Decode(void);
static void IRCOM_KeyUp(KEYPAD_KEY_t key);
static void IRCOM_Setup(USART_t* const usart);
static void IRCOM_SettingsLoop(void);
static void IRCOM_SettingsKeyUp(KEYPAD_KEY_t key);
static inline void IRCOM_SaveSettings(void);
static inline void IRCOM_LoadSettings(void);

void IRCOM_Init(void) {
    IRCOM_LoadSettings();
    DISPLAY_Mode(DISPLAY_MODE_RAW);
    USART_Info();
    UART_Desc();
    DIGITAL_Init(IRCOM_Decode);
    DIGITAL_Display(IRCOM.settings.display);
    BUFFER_Init(BUFFER_MODE_USART_RX);
    KEYPAD_KeyUp(IRCOM_KeyUp);
    PORTC_REMAP = PORT_USART0_bm;
    IRCOM_Setup(&USARTC0);
}

static void IRCOM_Decode(void) {
    while(!BUFFER_Empty()) {
        uint8_t status = BUFFER_GetData();
        if(status&USART_RXCIF_bm) {
            uint8_t data = BUFFER_GetData();
            if(status&(USART_FERR_bm|USART_PERR_bm)) {
                data = FONT_SYMBOL_PERR;
                if(status&USART_FERR_bm) {
                    data = FONT_SYMBOL_FERR;
                }
                DIGITAL_PrintSymbol(data);
            } else {
                DIGITAL_Print(data);
            }
        }
    }
}

static void IRCOM_KeyUp(KEYPAD_KEY_t key) {
    if(DIGITAL_Lock()&&(key!=KEYPAD_KEY1)) {
        return;
    }
    switch(key) {
        case KEYPAD_KEY1:
            DIGITAL_Hold(1);
            KEYPAD_KeyUp(IRCOM_SettingsKeyUp);
            MAIN_Loop(IRCOM_SettingsLoop);
            DISPLAY_Select(-1);
            break;
        case KEYPAD_KEY2:
            if(DIGITAL_IsHold()) { return; }
            DIGITAL_EndLine();
            IRCOM.settings.display = !IRCOM.settings.display;
            IRCOM_SaveSettings();
            break;
        case KEYPAD_KEY3: DIGITAL_Hold(!DIGITAL_IsHold()); break;
        case KEYPAD_KEY4: DIGITAL_Clear(); break;
        default: break;
    }
}

static void IRCOM_Setup(USART_t* const usart) {
    if(IRCOM.settings.invert) {
        PORTC_PIN6CTRL = PORT_OPC_BUSKEEPER_gc;
    } else {
        PORTC_PIN6CTRL = PORT_OPC_BUSKEEPER_gc|PORT_INVEN_bm;
    }
    USART_SetBaudAsync(usart, USART_Baud(IRCOM.settings.baud));
    (usart)->STATUS = 0xFF;
    USART_PMODE_t PMODE_gc = USART_PMODE(IRCOM.settings.parity);
    (usart)->CTRLC = USART_CMODE_IRDA_gc|PMODE_gc|USART_CHSIZE_8BIT_gc;
    (usart)->CTRLD = USART_LUTACT_OFF_gc|USART_PECACT_OFF_gc;
    (usart)->CTRLB = USART_RXEN_bm;
}

static void IRCOM_SettingsLoop(void) {
    if(!DISPLAY_Update()) { return; }
    DISPLAY_CursorPosition(0, 1);
    puts_P(TEXT_IRCOM_SETTINGS);
    DISPLAY_CursorPosition(6,15);
    printf_P(TEXT_BAUD, USART_Baud(IRCOM.settings.baud));
    DISPLAY_CursorPosition(6,24);
    printf_P(TEXT_PARITY, USART_Parity(IRCOM.settings.parity));
    DISPLAY_CursorPosition(6,33);
    puts_P(TEXT_INVERT);
    DISPLAY_MoveCursor(6);
    if(IRCOM.settings.invert==IRCOM_INVERT_NO) {
        puts_P(TEXT_NO);
    } else {
        puts_P(TEXT_YES);
    }
    DISPLAY_InvertLine(0);
    DISPLAY_SelectLine();
}

static void IRCOM_SettingsKeyUp(KEYPAD_KEY_t key) {
    switch(key) {
        case KEYPAD_KEY1:
            if((IRCOM.settings.baud++)==USART_BAUD_2000000) {
                IRCOM.settings.baud = USART_BAUD_1200;
            }
            DISPLAY_Select(14);
            break;
        case KEYPAD_KEY2:
            if((IRCOM.settings.parity++)==USART_PARITY_EVEN) {
                IRCOM.settings.parity = USART_PARITY_NO;
            }
            DISPLAY_Select(23);
            break;
        case KEYPAD_KEY3:
            IRCOM.settings.invert = !IRCOM.settings.invert;
            DISPLAY_Select(32);
            break;
        case KEYPAD_KEY4:
            IRCOM_SaveSettings();
            IRCOM_Setup(&USARTC0);
            KEYPAD_KeyUp(IRCOM_KeyUp);
            DIGITAL_Init(IRCOM_Decode);
            DIGITAL_Display(IRCOM.settings.display);
            DIGITAL_Hold(0);
            break;
        default: break;
    }
}

void IRCOM_Intro(void) {
    DISPLAY_Image(IMAGE_IRCOM);
    DISPLAY_Icons(USART_ICONS);
    DISPLAY_Send();
}

static inline void IRCOM_SaveSettings(void) {
    EEPROM_update_block(&IRCOM.settings, &IRCOM_settings, sizeof(IRCOM_SETTINGS_t));
}

static inline void IRCOM_LoadSettings(void) {
    eeprom_read_block(&IRCOM.settings, &IRCOM_settings, sizeof(IRCOM_SETTINGS_t));
    if(IRCOM.settings.baud>USART_BAUD_2000000) {
        IRCOM.settings.baud = USART_BAUD_9600;
    }
    if(IRCOM.settings.parity>USART_PARITY_EVEN) {
        IRCOM.settings.parity = USART_PARITY_NO;
    }
    if(IRCOM.settings.display>DIGITAL_DISPLAY_ASCII) {
        IRCOM.settings.display = DIGITAL_DISPLAY_HEX;
    }
    if(IRCOM.settings.invert>IRCOM_INVERT_YES) {
        IRCOM.settings.invert = IRCOM_INVERT_YES;
    }
    IRCOM_SaveSettings();
}
