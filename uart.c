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

typedef struct {
    USART_BAUD_t baud;
    USART_FRAME_t frame;
    USART_PARITY_t parity;
    DIGITAL_DISPLAY_t display;
} UART_SETTINGS_t;

static UART_SETTINGS_t UART_settings EEMEM;
static struct {
    uint8_t dir;
    UART_SETTINGS_t settings;
} UART;

static void UART_Decode(void);
static void UART_KeyUp(KEYPAD_KEY_t key);
static void UART_Setup(USART_t* const usart);
static void UART_SettingsLoop(void);
static void UART_SettingsKeyUp(KEYPAD_KEY_t key);
static inline void UART_SaveSettings(void);
static inline void UART_LoadSettings(void);

void UART_Init(void) {
    UART_LoadSettings();
    DISPLAY_Mode(DISPLAY_MODE_RAW);
    USART_Info();
    UART_Desc();
    DIGITAL_Init(UART_Decode);
    DIGITAL_Display(UART.settings.display);
    BUFFER_Init(BUFFER_MODE_USART_RX);
    KEYPAD_KeyUp(UART_KeyUp);
    PORTA_PIN1CTRL = PORT_OPC_BUSKEEPER_gc;
    PORTC_PIN6CTRL = PORT_OPC_BUSKEEPER_gc;
    PORTD_PIN6CTRL = PORT_OPC_BUSKEEPER_gc;
    PORTC_REMAP = PORT_USART0_bm;
    PORTD_REMAP = PORT_USART0_bm;
    UART_Setup(&USARTC0);
    UART_Setup(&USARTD0);
    /* Analog comparator is used to transfer signal from PORTA.PIN1 to PORTD.PIN6 */
    ACA.AC1MUXCTRL = AC_MUXPOS_PIN1_gc|AC_MUXNEG_SCALER_gc;
    ACA.CTRLA = AC_AC1OUT_bm;
    ACA.CTRLB = (31<<AC_SCALEFAC_gp)&AC_SCALEFAC_gm; // 1/2 AVCC
    ACA.AC1CTRL = AC_HYSMODE_SMALL_gc|AC_ENABLE_bm;
    PORTCFG_ACEVOUT = PORTCFG_ACOUT_PD_gc;
    UART.dir = 0;
}

static void UART_Setup(USART_t* const usart) {
    USART_SetBaudAsync(usart, USART_Baud(UART.settings.baud));
    (usart)->STATUS = 0xFF;
    USART_PMODE_t PMODE_gc = USART_PMODE(UART.settings.parity);
    USART_CHSIZE_t CHSIZE_gc = USART_CHSIZE(UART.settings.frame);
    (usart)->CTRLC = USART_CMODE_ASYNCHRONOUS_gc|PMODE_gc|CHSIZE_gc;
    (usart)->CTRLD = USART_LUTACT_OFF_gc|USART_PECACT_OFF_gc;
    (usart)->CTRLB = USART_RXEN_bm;
}

static void UART_Decode(void) {
    while(!BUFFER_Empty()) {
        uint8_t status = BUFFER_GetData();
        uint8_t dir = status&(USART_TXCIF_bm|USART_RXCIF_bm);
        if(dir) {
            if(UART.dir!=dir) {
                UART.dir = dir;
                DIGITAL_NewLine();
            }
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
            if(UART.dir==USART_TXCIF_bm) {
                DIGITAL_InvertLine();
            }
        }
    }
}

static void UART_KeyUp(KEYPAD_KEY_t key) {
    if(!DIGITAL_Counter()&&(key!=KEYPAD_KEY1)) {
        return;
    }
    switch(key) {
        case KEYPAD_KEY1:
            DIGITAL_Hold(1);
            KEYPAD_KeyUp(UART_SettingsKeyUp);
            MAIN_Loop(UART_SettingsLoop);
            DISPLAY_Select(-1);
            break;
        case KEYPAD_KEY2:
            if(DIGITAL_IsHold()) { return; }
            DIGITAL_NewLine();
            UART.settings.display = !UART.settings.display;
            DIGITAL_Display(UART.settings.display);
            UART_SaveSettings();
            break;
        case KEYPAD_KEY3: DIGITAL_Hold(!DIGITAL_IsHold()); break;
        case KEYPAD_KEY4: DIGITAL_Clear(); break;
        default: break;
    }
}

static void UART_SettingsLoop(void) {
    if(!DISPLAY_Update()) { return; }
    DISPLAY_CursorPosition(4, 1);
    puts_P(TEXT_UART_SETTINGS);
    DISPLAY_CursorPosition(4,15);
    printf_P(TEXT_BAUD, USART_Baud(UART.settings.baud));
    DISPLAY_CursorPosition(4,24);
    printf_P(TEXT_FRAME, USART_Frame(UART.settings.frame));
    DISPLAY_CursorPosition(4,33);
    printf_P(TEXT_PARITY, USART_Parity(UART.settings.parity));
    DISPLAY_InvertLine(0);
    DISPLAY_SelectLine();
}

static void UART_SettingsKeyUp(KEYPAD_KEY_t key) {
    switch(key) {
        case KEYPAD_KEY1:
            if((UART.settings.baud++)==USART_BAUD_2000000) {
                UART.settings.baud = USART_BAUD_1200;
            }
            DISPLAY_Select(14);
            break;
        case KEYPAD_KEY2:
            if((UART.settings.frame++)==USART_FRAME_8BIT) {
                UART.settings.frame = USART_FRAME_5BIT;
            }
            DISPLAY_Select(23);
            break;
        case KEYPAD_KEY3:
            if((UART.settings.parity++)==USART_PARITY_EVEN) {
                UART.settings.parity = USART_PARITY_NO;
            }
            DISPLAY_Select(32);
            break;
        case KEYPAD_KEY4:
            UART_SaveSettings();
            UART_Setup(&USARTC0);
            UART_Setup(&USARTD0);
            KEYPAD_KeyUp(UART_KeyUp);
            DIGITAL_Init(UART_Decode);
            DIGITAL_Display(UART.settings.display);
            DIGITAL_Hold(0);
            break;
        default: break;
    }
}

void UART_Intro(void) {
    DISPLAY_Image(IMAGE_UART);
    DISPLAY_Icons(USART_ICONS);
    DISPLAY_Send();
}

void UART_Desc(void) {
    DISPLAY_CursorPosition(7,7);
    puts_P(TEXT_PARITY_ERR);
    DISPLAY_CursorPosition(7,16);
    puts_P(TEXT_FRAME_ERR);
    DISPLAY_CursorPosition(7,25);
    puts_P(TEXT_OVERFLOW);
    DISPLAY_Icons(USART_ICONS);
    DISPLAY_Send();
    DELAY_Info();
}

static inline void UART_SaveSettings(void) {
    EEPROM_update_block(&UART.settings, &UART_settings, sizeof(UART_SETTINGS_t));
}

static inline void UART_LoadSettings(void) {
    eeprom_read_block(&UART.settings, &UART_settings, sizeof(UART_SETTINGS_t));
    if(UART.settings.baud>USART_BAUD_2000000) {
        UART.settings.baud = USART_BAUD_9600;
    }
    if(UART.settings.frame>USART_FRAME_8BIT) {
        UART.settings.frame = USART_FRAME_8BIT;
    }
    if(UART.settings.parity>USART_PARITY_EVEN) {
        UART.settings.parity = USART_PARITY_NO;
    }
    if(UART.settings.display>DIGITAL_DISPLAY_ASCII) {
        UART.settings.display = DIGITAL_DISPLAY_HEX;
    }
    UART_SaveSettings();
}
