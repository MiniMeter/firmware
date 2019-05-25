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
#include "avr/iox32e5.h"
#include "avr/eeprom.h"
#include "main.h"
#include "text.h"
#include "font.h"
#include "keypad.h"
#include "buffer.h"
#include "digital.h"
#include "display.h"
#include "delay.h"
#include "image.h"
#include "icon.h"
#include "twi.h"

#define TWI_SDA_bm  PIN0_bm
#define TWI_SCL_bm  PIN1_bm
#define TWI_START_STOP_bm  PIN7_bm
#define TWI_START '<'
#define TWI_STOP  '>'
#define TWI_ACK  '+'
#define TWI_NACK '-'
#define TWI_MIN_CLOCK_PERIOD 38

typedef struct {
    uint8_t ack_nack;
    uint8_t start_stop;
} TWI_SETTINGS_t;

static TWI_SETTINGS_t TWI_settings EEMEM;
static struct {
    TWI_SETTINGS_t settings;
} TWI;

static void TWI_Decode(void);
static void TWI_KeyUp(KEYPAD_KEY_t key);
static void TWI_Info(void);
static void TWI_Icons(void);
static void TWI_Desc(void);
static inline void TWI_SaveSettings(void);
static inline void TWI_LoadSettings(void);

void TWI_Init(void) {
    TWI_LoadSettings();
    DISPLAY_Mode(DISPLAY_MODE_USART);
    TWI_Info();
    TWI_Desc();
    KEYPAD_KeyUp(TWI_KeyUp);
    BUFFER_Init(BUFFER_MODE_PORTC_IN);
    DIGITAL_Init(TWI_Decode);
    DIGITAL_CheckClockPeriod();
    PORTC_PIN0CTRL = PORT_OPC_BUSKEEPER_gc|PORT_ISC_BOTHEDGES_gc; // SDA
    PORTC_PIN1CTRL = PORT_OPC_BUSKEEPER_gc|PORT_ISC_RISING_gc; // SCL
    EVSYS.CH7MUX = EVSYS_CHMUX_PORTC_PIN0_gc; // LUT1 IN3
    EVSYS.CH0MUX = EVSYS_CHMUX_PORTC_PIN1_gc; // LUT0 IN1
    EVSYS.CH1MUX = EVSYS_CHMUX_PORTC_PIN1_gc; // SCL period check
    EVSYS.CH5MUX = EVSYS_CHMUX_XCL_UNF0_gc; // TWI start/stop pulse
    EVSYS.CH6MUX = EVSYS_CHMUX_XCL_LUT1_gc; // LUT0 IN0
    EVSYS.CH2MUX = EVSYS_CHMUX_XCL_LUT0_gc; // Buffer EDMA trigger
    /* LUT1(EVSYS.CH6) = (EVSYS.CH7) AND (PORTC.PIN1) */
    /* LUT0(EVSYS.CH2) = (EVSYS.CH6) OR (EVSYS.CH0) */
    XCL.CTRLA = XCL_LUTOUTEN_DISABLE_gc|XCL_PORTSEL_PC_gc|XCL_LUTCONF_2LUT2IN_gc;
    XCL.CTRLB = XCL_IN3SEL_EVSYS_gc|XCL_IN2SEL_PINL_gc|XCL_IN1SEL_EVSYS_gc|XCL_IN0SEL_EVSYS_gc;
    XCL.CTRLC = XCL_DLYSEL_DLY22_gc|XCL_DLY1CONF_NO_gc|XCL_DLY0CONF_OUT_gc;
    XCL.CTRLD = (0x8<<XCL_TRUTH1_gp)|(0xE<<XCL_TRUTH0_gp);
    /* Timer BTC0 generate single low level pulse on every TWI start/stop event */
    XCL.PERCAPTL = 2;
    XCL.PERCAPTH = 2; // needed for BTC0 to work properly (hardware bug?)
    XCL.CTRLF = XCL_TCMODE_1SHOT_gc;
    XCL.CTRLG = XCL_EVACTEN_bm|XCL_EVACT0_RESTART_gc|XCL_EVSRC_EVCH6_gc;//
    XCL.INTCTRL = XCL_UNF_INTLVL_OFF_gc|XCL_CC_INTLVL_OFF_gc;
    XCL.CTRLE = XCL_TCSEL_BTC0_gc|XCL_CLKSEL_DIV2_gc;
    EVSYS_STROBE = EVSYS_CHMUX6_bm; // Restart BTC0
    /* TWI start/stop event pulse is exposed on PORTC.PIN7 */
    PORTC_DIRSET = PIN7_bm;
    PORTC_PIN7CTRL = PORT_OPC_TOTEM_gc;
    PORTCFG_ACEVOUT = PORTCFG_EVOUT_PC7_gc|PORTCFG_EVOUTSEL_5_gc;
    BUFFER_Clear();
}

static void TWI_Decode(void) {
    static uint8_t byte, bit;
    while(!BUFFER_Empty()) {
        uint8_t data = BUFFER_GetData();
        if((~data)&TWI_START_STOP_bm) {
            if(bit>1) {
                DIGITAL_PrintSymbol(FONT_SYMBOL_ERROR);
            }
            if(TWI.settings.start_stop) {
                if(data&TWI_SDA_bm) {
                    DIGITAL_PrintChar(TWI_STOP);
                } else {
                    DIGITAL_PrintChar(TWI_START);
                }
            }
            if(data&TWI_SDA_bm) {
                DIGITAL_EndLine();
            }
            byte = 0x00;
            bit = 0;
        } else {
            if(bit<8) {
                byte <<= 1;
                if(data&TWI_SDA_bm) {
                    byte |= 0x01;
                }
                bit++;
            } else {
                DIGITAL_PrintHex(byte>>4);
                DIGITAL_PrintHex(byte>>0);
                byte = 0x00;
                bit = 0;
                if(TWI.settings.ack_nack) {
                    if(data&TWI_SDA_bm) {
                        DIGITAL_PrintChar(TWI_NACK);
                    } else {
                        DIGITAL_PrintChar(TWI_ACK);
                    }
                }
            }
        }
    }
    if(DIGITAL_ClockPeriod()<TWI_MIN_CLOCK_PERIOD) {
        DIGITAL_Blackout();
        DIGITAL_ClockPeriodReset();
    }
}

static void TWI_KeyUp(KEYPAD_KEY_t key) {
    if(DIGITAL_Lock()) {
        return;
    }
    switch(key) {
        case KEYPAD_KEY1:
            if(DIGITAL_IsHold()) { break; }
            TWI.settings.start_stop = !TWI.settings.start_stop;
            TWI_SaveSettings();
            break;
        case KEYPAD_KEY2:
            if(DIGITAL_IsHold()) { break; }
            TWI.settings.ack_nack = !TWI.settings.ack_nack;
            TWI_SaveSettings();
            break;
        case KEYPAD_KEY3:
            DIGITAL_Hold(!DIGITAL_IsHold());
            break;
        case KEYPAD_KEY4:
            DIGITAL_Clear();
            break;
        default: break;
    }
}

void TWI_Intro(void) {
    DISPLAY_Image(IMAGE_TWI);
    TWI_Icons();
    DISPLAY_Send();
}

static void TWI_Info(void) {
    DISPLAY_CursorPosition(8, 1);
    DISPLAY_Icon(ICON_START_STOP);
    DISPLAY_CursorPosition(22, 2);
    puts_P(TEXT_START_STOP);
    DISPLAY_CursorPosition(8, 10);
    DISPLAY_Icon(ICON_ACK_NACK);
    DISPLAY_CursorPosition(22, 11);
    puts_P(TEXT_ACK_NACK);
    DISPLAY_CursorPosition(7, 19);
    DISPLAY_Icon(ICON_HOLD_RUN);
    DISPLAY_CursorPosition(22, 20);
    puts_P(TEXT_HOLD_RUN);
    DISPLAY_CursorPosition(7, 28);
    DISPLAY_Icon(ICON_CLEAR);
    DISPLAY_CursorPosition(22, 29);
    puts_P(TEXT_CLEAR);
    TWI_Icons();
    DISPLAY_Send();
    DELAY_Info();
}

static void TWI_Desc(void) {
    DISPLAY_CursorPosition(10,7);
    puts_P(TEXT_DATA_ERR);
    DISPLAY_CursorPosition(10,16);
    puts_P(TEXT_OVERFLOW);
    DISPLAY_CursorPosition(10,25);
    puts_P(TEXT_FREQ_ERR);
    TWI_Icons();
    DISPLAY_Send();
    DELAY_Info();
}

static void TWI_Icons(void) {
    static const __flash uint8_t* const __flash TWI_ICONS[] = {
        ICON_START_STOP,
        ICON_ACK_NACK,
        ICON_HOLD_RUN,
        ICON_CLEAR,
    };
    DISPLAY_Icons(TWI_ICONS);
}

static inline void TWI_SaveSettings(void) {
    EEPROM_update_block(&TWI.settings, &TWI_settings, sizeof(TWI_SETTINGS_t));
}

static inline void TWI_LoadSettings(void) {
    eeprom_read_block(&TWI.settings, &TWI_settings, sizeof(TWI_SETTINGS_t));
}
