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
#include "main.h"
#include "text.h"
#include "avr/eeprom.h"
#include "image.h"
#include "icon.h"
#include "buffer.h"
#include "digital.h"
#include "display.h"
#include "delay.h"
#include "keypad.h"
#include "onewire.h"

#define TIMEOUT      0xB600 //1440us +1% (1456us)
#define RESET_MIN    0x3B60 // 480us -1% (475us)
#define RESET_MAX    0x7940 // 960us +1% (970us)
#define PRESENCE_MIN 0x0760 //  60us -1% (59us)
#define PRESENCE_MAX 0x1E60 // 240us +1% (243us)
#define BIT_ZERO_MIN 0x01DB //  15us -1% (14,8us)
#define BIT_ZERO_MAX 0x0F27 // 120us +1% (121,2us)
#define BIT_ONE_MIN  0x001F //   1us -1% (0,99us)
#define BIT_ONE_MAX  0x01C6 //  14us +1% (14,2us)

typedef struct {
    uint8_t tab;
} ONEWIRE_SETTINGS_t;

static ONEWIRE_SETTINGS_t ONEWIRE_settings EEMEM;
static struct {
    uint8_t reset;
    ONEWIRE_SETTINGS_t settings;
} ONEWIRE;

static void ONEWIRE_Decode(void);
static void ONEWIRE_KeyUp(KEYPAD_KEY_t key);
static void ONEWIRE_Info(void);
static void ONEWIRE_Icons(void);
static void ONEWIRE_InfoLoop(void);
static void ONEWIRE_InfoKeyUp(KEYPAD_KEY_t key);
static void ONEWIRE_Desc(void);
static inline void ONEWIRE_SaveSettings(void);
static inline void ONEWIRE_LoadSettings(void);

void ONEWIRE_Init(void) {
    ONEWIRE_LoadSettings();
    DISPLAY_Mode(DISPLAY_MODE_USART);
    ONEWIRE_Info();
    ONEWIRE_Desc();
    KEYPAD_KeyUp(ONEWIRE_KeyUp);
    DIGITAL_Init(ONEWIRE_Decode);
    DIGITAL_Display(ONEWIRE.settings.tab);
    BUFFER_Init(BUFFER_MODE_TCC5_CNT);
    EVSYS.CH0MUX = EVSYS_CHMUX_ACA_CH1_gc; // LUT0 IN1
    EVSYS.CH4MUX = EVSYS_CHMUX_ACA_CH0_gc; // Restart TCC5 (timestamp)
    EVSYS.CH1MUX = EVSYS_CHMUX_XCL_UNF0_gc; // LUT1 IN2
    EVSYS.CH7MUX = EVSYS_CHMUX_XCL_UNF0_gc; // LUT1 IN3
    EVSYS.CH6MUX = EVSYS_CHMUX_XCL_LUT1_gc; // LUT0 IN0
    EVSYS.CH2MUX = EVSYS_CHMUX_XCL_LUT0_gc; // Buffer EDMA trigger
    /* Analog comparator generates rising and falling edge events from PORTA.PIN1 (DATA) */
    ACA.CTRLB = (31<<AC_SCALEFAC_gp)&AC_SCALEFAC_gm; // 1/2 AVCC
    ACA.AC0MUXCTRL = AC_MUXPOS_PIN1_gc|AC_MUXNEG_SCALER_gc;
    ACA.AC1MUXCTRL = AC_MUXPOS_PIN1_gc|AC_MUXNEG_SCALER_gc;
    ACA.AC0CTRL = AC_INTMODE_FALLING_gc|AC_HYSMODE_SMALL_gc|AC_ENABLE_bm;
    ACA.AC1CTRL = AC_INTMODE_RISING_gc|AC_HYSMODE_SMALL_gc|AC_ENABLE_bm;
    /* LUT1(EVSYS.CH6) = RISING EDGE (BTC0_UNF) */
    /* LUT0(EVSYS.CH2) = (EVSYS.CH6) OR (EVSYS.CH0) */
    XCL.CTRLA = XCL_LUTOUTEN_DISABLE_gc|XCL_PORTSEL_PC_gc|XCL_LUTCONF_2LUT2IN_gc;
    XCL.CTRLB = XCL_IN3SEL_EVSYS_gc|XCL_IN2SEL_EVSYS_gc|XCL_IN1SEL_EVSYS_gc|XCL_IN0SEL_EVSYS_gc;
    XCL.CTRLC = XCL_DLYSEL_DLY11_gc|XCL_DLY1CONF_IN_gc|XCL_DLY0CONF_OUT_gc;
    XCL.CTRLD = (0x4<<XCL_TRUTH1_gp)|(0xE<<XCL_TRUTH0_gp);
    /* Timer BTC0 underflows at maximum slave response time (after 1-wire reset) */
    XCL.PERCAPTL = TIMEOUT>>8;
    XCL.PERCAPTH = TIMEOUT>>8; // needed for BTC0 to work properly (hardware bug?)
    XCL.CNTL = TIMEOUT>>8;
    XCL.CTRLF = XCL_TCMODE_1SHOT_gc;
    XCL.CTRLG = XCL_EVACTEN_bm|XCL_EVACT0_RESTART_gc|XCL_EVSRC_EVCH4_gc;
    XCL.INTCTRL = XCL_UNF_INTLVL_OFF_gc|XCL_CC_INTLVL_OFF_gc;
    XCL.CTRLE = XCL_TCSEL_BTC0_gc|XCL_CLKSEL_DIV256_gc;
    /* Timer TCC5 is used to timestamp (with EDMA) every 1-wire low pulse */
    TCC5.CTRLB = TC45_BYTEM_NORMAL_gc|TC45_WGMODE_NORMAL_gc;
    TCC5.CTRLD = TC45_EVACT_RESTART_gc|TC45_EVSEL_CH4_gc;
    TCC5.CTRLA = TC45_CLKSEL_DIV1_gc;
    BUFFER_Clear();
}

static void ONEWIRE_Decode(void) {
    static uint8_t byte, bit;
    while(!BUFFER_Empty()) {
        uint16_t sample = (uint16_t)BUFFER_GetSample();
        if(ONEWIRE.reset) {
            if(sample>PRESENCE_MIN && sample<PRESENCE_MAX) {
                DIGITAL_PrintChar('+');
            } else if(sample>TIMEOUT) {
                DIGITAL_PrintChar('-');
            } else {
                DIGITAL_PrintChar('?');
            }
            if(ONEWIRE.settings.tab) { DIGITAL_PrintTab(); }
            ONEWIRE.reset = 0;
        } else {
            if(sample>RESET_MIN && sample<RESET_MAX) {
                if(bit>0){
                    if(bit>3) {
                        DIGITAL_PrintHex(byte>>4);
                    }
                    DIGITAL_PrintChar('?');
                }
                DIGITAL_EndLine();
                DIGITAL_PrintChar('R');
                ONEWIRE.reset = 1;
                byte = 0x00;
                bit = 0;
            } else if(sample>BIT_ZERO_MIN && sample<BIT_ZERO_MAX) {
                byte >>= 1;
                bit++;
            } else if(sample>BIT_ONE_MIN && sample<BIT_ONE_MAX) {
                byte >>= 1;
                byte |= 0x80;
                bit++;
            }
            if(bit==8) {
                DIGITAL_PrintHex(byte>>4);
                DIGITAL_PrintHex(byte>>0);
                if(ONEWIRE.settings.tab) {
                    DIGITAL_PrintTab();
                }
                byte = 0x00;
                bit = 0;
            }
        }
    }
}

static void ONEWIRE_KeyUp(KEYPAD_KEY_t key) {
    if(DIGITAL_Lock()&&(key!=KEYPAD_KEY1)) {
        return;
    }
    switch(key) {
        case KEYPAD_KEY1:
            DIGITAL_Hold(1);
            KEYPAD_KeyUp(ONEWIRE_InfoKeyUp);
            MAIN_Loop(ONEWIRE_InfoLoop);
            break;
        case KEYPAD_KEY2:
            if(DIGITAL_IsHold()) { return; }
            ONEWIRE.settings.tab = !ONEWIRE.settings.tab;
            DIGITAL_Display(ONEWIRE.settings.tab);
            ONEWIRE_SaveSettings();
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

static void ONEWIRE_InfoLoop(void) {
    if(!DISPLAY_Update()) { return; }
    DISPLAY_CursorPosition(9, 1);
    puts_P(TEXT_1WIRE_INFO);
    DISPLAY_CursorPosition(15,15);
    puts_P(TEXT_RESET);
    DISPLAY_CursorPosition(15,26);
    puts_P(TEXT_RESPONSE);
    DISPLAY_CursorPosition(6,35);
    puts_P(TEXT_YES_NO);
    DISPLAY_InvertLine(0);
}

static void ONEWIRE_InfoKeyUp(KEYPAD_KEY_t key) {
    (void)key; //unused
    DIGITAL_Init(ONEWIRE_Decode);
    DIGITAL_Display(ONEWIRE.settings.tab);
    KEYPAD_KeyUp(ONEWIRE_KeyUp);
    DIGITAL_Hold(0);
}

void ONEWIRE_Intro(void) {
    DISPLAY_Image(IMAGE_ONEWIRE);
    ONEWIRE_Icons();
    DISPLAY_Send();
}

static void ONEWIRE_Info(void) {
    DISPLAY_CursorPosition(15, 1);
    DISPLAY_Icon(ICON_INFO);
    DISPLAY_CursorPosition(29, 2);
    puts_P(TEXT_INFO);
    DISPLAY_CursorPosition(15, 10);
    DISPLAY_Icon(ICON_DISPLAY);
    DISPLAY_CursorPosition(29, 11);
    puts_P(TEXT_SPACE);
    DISPLAY_CursorPosition(14, 19);
    DISPLAY_Icon(ICON_HOLD_RUN);
    DISPLAY_CursorPosition(29, 20);
    puts_P(TEXT_HOLD_RUN);
    DISPLAY_CursorPosition(14, 28);
    DISPLAY_Icon(ICON_CLEAR);
    DISPLAY_CursorPosition(29, 29);
    puts_P(TEXT_CLEAR);
    ONEWIRE_Icons();
    DISPLAY_Send();
    DELAY_Info();
}

static void ONEWIRE_Desc(void) {
    DISPLAY_CursorPosition(15,5);
    puts_P(TEXT_RESET);
    DISPLAY_CursorPosition(15,17);
    puts_P(TEXT_RESPONSE);
    DISPLAY_CursorPosition(6,26);
    puts_P(TEXT_YES_NO);
    ONEWIRE_Icons();
    DISPLAY_Send();
    DELAY_Info();
}

static void ONEWIRE_Icons(void) {
    static const __flash uint8_t* const __flash ONEWIRE_ICONS[] = {
        ICON_INFO,
        ICON_DISPLAY,
        ICON_HOLD_RUN,
        ICON_CLEAR,
    };
    DISPLAY_Icons(ONEWIRE_ICONS);
}

static inline void ONEWIRE_SaveSettings(void) {
    EEPROM_update_block(&ONEWIRE.settings, &ONEWIRE_settings, sizeof(ONEWIRE_SETTINGS_t));
}

static inline void ONEWIRE_LoadSettings(void) {
    eeprom_read_block(&ONEWIRE.settings, &ONEWIRE_settings, sizeof(ONEWIRE_SETTINGS_t));
}
