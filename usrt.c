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
#include "avr/iox32e5.h"
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
#include "usrt.h"

#define USRT_RxD_bm  PIN6_bm
#define USRT_MIN_CLOCK_PERIOD 28 //<1us (1MHz)

typedef enum {
    USRT_CLOCK_FALLING,
    USRT_CLOCK_RISING
} USRT_CLOCK_t;

typedef struct {
    USRT_CLOCK_t clock;
    USART_FRAME_t frame;
    USART_PARITY_t parity;
    DIGITAL_DISPLAY_t display;
} USRT_SETTINGS_t;

static USRT_SETTINGS_t USRT_settings EEMEM;
static struct {
    USRT_SETTINGS_t settings;
} USRT;

static void USRT_Decode(void);
static void USRT_KeyUp(KEYPAD_KEY_t key);
static void USRT_SettingsLoop(void);
static void USRT_SettingsKeyUp(KEYPAD_KEY_t key);
static void USRT_Desc(void);
static inline void USRT_ClockEdge(void);
static inline void USRT_SaveSettings(void);
static inline void USRT_LoadSettings(void);

void USRT_Init(void) {
    USRT_LoadSettings();
    DISPLAY_Mode(DISPLAY_MODE_USART);
    USART_Info();
    USRT_Desc();
    KEYPAD_KeyUp(USRT_KeyUp);
    BUFFER_Init(BUFFER_MODE_PORTC_IN);
    DIGITAL_Init(USRT_Decode);
    DIGITAL_Display(USRT.settings.display);
    DIGITAL_CheckClockPeriod();
    USRT_ClockEdge(); // XCK
    PORTC_PIN6CTRL = PORT_OPC_BUSKEEPER_gc|PORT_ISC_FALLING_gc; // RxD
    PORTD_PIN6CTRL = PORT_OPC_BUSKEEPER_gc|PORT_ISC_RISING_gc; // XCK period check
    EVSYS.CH0MUX = EVSYS_CHMUX_PORTA_PIN1_gc; // LUT0 IN1
    EVSYS.CH1MUX = EVSYS_CHMUX_PORTD_PIN6_gc; // XCK period check
    EVSYS.CH5MUX = EVSYS_CHMUX_PORTC_PIN6_gc; // Restart BTC0
    EVSYS.CH6MUX = EVSYS_CHMUX_XCL_UNF0_gc; // LUT0 IN0
    EVSYS.CH2MUX = EVSYS_CHMUX_XCL_LUT0_gc; // Buffer EDMA trigger
    /* Analog comparator is used to transfer signal from PORTA.PIN1 to PORTD.PIN6 */
    ACA.AC1MUXCTRL = AC_MUXPOS_PIN1_gc|AC_MUXNEG_SCALER_gc;
    ACA.CTRLA = AC_AC1OUT_bm;
    ACA.CTRLB = (31<<AC_SCALEFAC_gp)&AC_SCALEFAC_gm; // 1/2 AVCC
    ACA.AC1CTRL = AC_HYSMODE_SMALL_gc|AC_ENABLE_bm;
    PORTCFG_ACEVOUT = PORTCFG_ACOUT_PD_gc;
    /* LUT0(EVSYS.CH2) = NOT(EVSYS.CH6) AND (EVSYS.CH0) */
    XCL.CTRLA = XCL_LUTOUTEN_DISABLE_gc|XCL_PORTSEL_PC_gc|XCL_LUTCONF_2LUT2IN_gc;
    XCL.CTRLB = XCL_IN3SEL_EVSYS_gc|XCL_IN2SEL_EVSYS_gc|XCL_IN1SEL_EVSYS_gc|XCL_IN0SEL_EVSYS_gc;
    XCL.CTRLC = XCL_DLYSEL_DLY11_gc|XCL_DLY1CONF_NO_gc|XCL_DLY0CONF_NO_gc;
    XCL.CTRLD = (0x0<<XCL_TRUTH1_gp)|(0x4<<XCL_TRUTH0_gp);
    /* Timer BTC0 counts only relevant XCK clock cycles */
    XCL.PERCAPTL = 0x0F;
    XCL.PERCAPTH = 0x0F; // needed for BTC0 to work properly (hardware bug?)
    XCL.CTRLF = XCL_TCMODE_1SHOT_gc;
    XCL.INTCTRL = XCL_UNF_INTLVL_OFF_gc|XCL_CC_INTLVL_OFF_gc;
    XCL.CTRLG = XCL_EVACTEN_bm|XCL_EVACT0_RESTART_gc|XCL_EVSRC_EVCH5_gc;
    XCL.CTRLE = XCL_TCSEL_BTC0_gc|XCL_CLKSEL_EVCH0_gc;
    EVSYS_STROBE = EVSYS_CHMUX5_bm; // Restart BTC0
    /* Force BTC0 to underflow */
    XCL.CNTL = 0x01;
    EVSYS_STROBE = EVSYS_CHMUX0_bm;
    EVSYS_STROBE = EVSYS_CHMUX0_bm;
    BUFFER_Clear();
}

static inline void USRT_ClockEdge(void) {
    if(USRT.settings.clock==USRT_CLOCK_FALLING) {
        PORTA_PIN1CTRL = PORT_OPC_BUSKEEPER_gc|PORT_ISC_FALLING_gc; // SCK
    } else {
        PORTA_PIN1CTRL = PORT_OPC_BUSKEEPER_gc|PORT_ISC_RISING_gc; // SCK
    }
}

static void USRT_Decode(void) {
    static uint8_t start, byte, bit, counter;
    const uint8_t frame = USART_Frame(USRT.settings.frame);
    const uint8_t parity = USRT.settings.parity;
    while(!BUFFER_Empty()) {
        uint8_t data = BUFFER_GetData();
        if(start) {
            if(bit<frame) {
                bit++;
                byte>>=1;
                if(data&USRT_RxD_bm) {
                    byte |= 0x80;
                    counter++;
                }
            } else if(parity&&(bit==frame)) {
                if(data&USRT_RxD_bm) { counter++; }
                if((parity+counter)&0x01) {
                    DIGITAL_PrintSymbol(FONT_SYMBOL_PERR);
                    start = 0;
                }
                bit++;
            } else {
                byte>>=(8-frame);
                if(data&USRT_RxD_bm) {
                    DIGITAL_Print(byte);
                } else {
                    DIGITAL_PrintSymbol(FONT_SYMBOL_FERR);
                }
                start = 0;
            }
        } else if(!(data&USRT_RxD_bm)) {
            start = 1;
            byte = 0x00;
            bit = 0;
            counter = 0;
        }
    }
    if(DIGITAL_ClockPeriod()<USRT_MIN_CLOCK_PERIOD) {
        DIGITAL_Blackout();
        DIGITAL_ClockPeriodReset();
    }
}

static void USRT_KeyUp(KEYPAD_KEY_t key) {
    if(!DIGITAL_Counter()&&(key!=KEYPAD_KEY1)) {
        return;
    }
    switch(key) {
        case KEYPAD_KEY1:
            DIGITAL_Hold(1);
            KEYPAD_KeyUp(USRT_SettingsKeyUp);
            MAIN_Loop(USRT_SettingsLoop);
            DISPLAY_Select(-1);
            break;
        case KEYPAD_KEY2:
            if(DIGITAL_IsHold()) { break; }
            DIGITAL_NewLine();
            USRT.settings.display = !USRT.settings.display;
            DIGITAL_Display(USRT.settings.display);
            USRT_SaveSettings();
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

static inline const __flash char* USRT_CLOCK(void) {
    static const __flash char* const __flash CLOCK[] = {
        [USRT_CLOCK_FALLING] = TEXT_FALLING_EDGE,
        [USRT_CLOCK_RISING] = TEXT_RISING_EDGE,
    };
    return CLOCK[USRT.settings.clock];
}

static void USRT_SettingsLoop(void) {
    if(!DISPLAY_Update()) { return; }
    DISPLAY_CursorPosition(3, 1);
    puts_P(TEXT_USRT_SETTINGS);
    DISPLAY_CursorPosition(3,15);
    printf_P(TEXT_CLOCK, USRT_CLOCK());
    DISPLAY_CursorPosition(3,24);
    printf_P(TEXT_FRAME, USART_Frame(USRT.settings.frame));
    DISPLAY_CursorPosition(3,33);
    printf_P(TEXT_PARITY, USART_Parity(USRT.settings.parity));
    DISPLAY_InvertLine(0);
    DISPLAY_SelectLine();
}

static void USRT_SettingsKeyUp(KEYPAD_KEY_t key) {
    switch(key) {
        case KEYPAD_KEY1:
            USRT.settings.clock = !USRT.settings.clock;
            DISPLAY_Select(14);
            break;
        case KEYPAD_KEY2:
            if((USRT.settings.frame++)==USART_FRAME_8BIT) {
                USRT.settings.frame = USART_FRAME_5BIT;
            }
            DISPLAY_Select(23);
            break;
        case KEYPAD_KEY3:
            if((USRT.settings.parity++)==USART_PARITY_EVEN) {
                USRT.settings.parity = USART_PARITY_NO;
            }
            DISPLAY_Select(32);
            break;
        case KEYPAD_KEY4:
            USRT_SaveSettings();
            USRT_ClockEdge();
            KEYPAD_KeyUp(USRT_KeyUp);
            DIGITAL_Init(USRT_Decode);
            DIGITAL_Display(USRT.settings.display);
            DIGITAL_Hold(0);
            break;
        default: break;
    }
}

void USRT_Intro(void) {
    DISPLAY_Image(IMAGE_USRT);
    DISPLAY_Icons(USART_ICONS);
    DISPLAY_Send();
}

static void USRT_Desc(void) {
    DISPLAY_CursorPosition(7, 2);
    puts_P(TEXT_PARITY_ERR);
    DISPLAY_CursorPosition(7,11);
    puts_P(TEXT_FRAME_ERR);
    DISPLAY_CursorPosition(7,20);
    puts_P(TEXT_OVERFLOW);
    DISPLAY_CursorPosition(7,29);
    puts_P(TEXT_FREQ_ERR);
    DISPLAY_Icons(USART_ICONS);
    DISPLAY_Send();
    DELAY_Info();
}

static inline void USRT_SaveSettings(void) {
    EEPROM_update_block(&USRT.settings, &USRT_settings, sizeof(USRT_SETTINGS_t));
}

static inline void USRT_LoadSettings(void) {
    eeprom_read_block(&USRT.settings, &USRT_settings, sizeof(USRT_SETTINGS_t));
    if(USRT.settings.clock>USRT_CLOCK_RISING) {
        USRT.settings.clock = USRT_CLOCK_FALLING;
    }
    if(USRT.settings.frame>USART_FRAME_8BIT) {
        USRT.settings.frame = USART_FRAME_8BIT;
    }
    if(USRT.settings.parity>USART_PARITY_EVEN) {
        USRT.settings.parity = USART_PARITY_NO;
    }
    if(USRT.settings.display>DIGITAL_DISPLAY_ASCII) {
        USRT.settings.display = DIGITAL_DISPLAY_HEX;
    }
    USRT_SaveSettings();
}
