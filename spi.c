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
#include "keypad.h"
#include "buffer.h"
#include "digital.h"
#include "display.h"
#include "delay.h"
#include "image.h"
#include "icon.h"
#include "spi.h"

#define SPI_SS_bm  PIN0_bm
#define SPI_MISO_bm  PIN1_bm
#define SPI_MOSI_bm  PIN6_bm
#define SPI_SCK_bm  PIN1_bm
#define SPI_START '<'
#define SPI_STOP  '>'
#define SPI_MIN_CLOCK_PERIOD 28 //<1us (1MHz)

typedef enum {
    SPI_INPUT_MOSI,
    SPI_INPUT_MISO
} SPI_INPUT_t;

typedef enum {
    SPI_DATA_LSB,
    SPI_DATA_MSB
} SPI_DATA_t;

typedef enum {
    SPI_CLOCK_FALLING,
    SPI_CLOCK_RISING
} SPI_CLOCK_t;

typedef enum {
    SPI_SELECT_LOW,
    SPI_SELECT_HIGH
} SPI_SELECT_t;

typedef struct {
    SPI_INPUT_t input;
    SPI_DATA_t data;
    SPI_CLOCK_t clock;
    SPI_SELECT_t select;
} SPI_SETTINGS_t;

static SPI_SETTINGS_t SPI_settings EEMEM;
static struct {
    uint8_t input, select;
    SPI_SETTINGS_t settings;
} SPI;

static void SPI_Decode(void);
static void SPI_KeyUp(KEYPAD_KEY_t key);
static void SPI_SettingsLoop(void);
static void SPI_SettingsKeyUp(KEYPAD_KEY_t key);
static void SPI_Info(void);
static void SPI_Desc(void);
static void SPI_Icons(void);
static inline void SPI_ClockEdge(void);
static inline void SPI_SaveSettings(void);
static inline void SPI_LoadSettings(void);

void SPI_Init(void) {
    SPI_LoadSettings();
    DISPLAY_Mode(DISPLAY_MODE_USART);
    SPI_Info();
    SPI_Desc();
    KEYPAD_KeyUp(SPI_KeyUp);
    BUFFER_Init(BUFFER_MODE_PORTC_IN);
    DIGITAL_Init(SPI_Decode);
    DIGITAL_CheckClockPeriod();
    SPI_ClockEdge(); // SCK
    PORTC.PIN6CTRL = PORT_OPC_BUSKEEPER_gc; // MOSI
    PORTC.PIN0CTRL = PORT_OPC_BUSKEEPER_gc|PORT_ISC_BOTHEDGES_gc; // SS
    PORTC.PIN1CTRL = PORT_OPC_BUSKEEPER_gc; // MISO
    PORTD_PIN6CTRL = PORT_OPC_BUSKEEPER_gc|PORT_ISC_RISING_gc; // SCK period check
    EVSYS.CH0MUX = EVSYS_CHMUX_PORTA_PIN1_gc; // LUT0 IN1
    EVSYS.CH1MUX = EVSYS_CHMUX_PORTD_PIN6_gc; // SCK period check
    EVSYS.CH6MUX = EVSYS_CHMUX_PORTC_PIN0_gc; // LUT0 IN0
    EVSYS.CH2MUX = EVSYS_CHMUX_XCL_LUT0_gc; // Buffer EDMA trigger
    /* Analog comparator is used to transfer signal from PORTA.PIN1 to PORTD.PIN6 */
    ACA.AC1MUXCTRL = AC_MUXPOS_PIN1_gc|AC_MUXNEG_SCALER_gc;
    ACA.CTRLA = AC_AC1OUT_bm;
    ACA.CTRLB = (31<<AC_SCALEFAC_gp)&AC_SCALEFAC_gm; // 1/2 AVCC
    ACA.AC1CTRL = AC_HYSMODE_SMALL_gc|AC_ENABLE_bm;
    PORTCFG_ACEVOUT = PORTCFG_ACOUT_PD_gc;
    /* LUT0(EVSYS.CH2) = (EVSYS.CH6) OR (EVSYS.CH0) */
    XCL.CTRLA = XCL_LUTOUTEN_DISABLE_gc|XCL_PORTSEL_PC_gc|XCL_LUTCONF_2LUT2IN_gc;
    XCL.CTRLB = XCL_IN3SEL_EVSYS_gc|XCL_IN2SEL_EVSYS_gc|XCL_IN1SEL_EVSYS_gc|XCL_IN0SEL_EVSYS_gc;
    XCL.CTRLC = XCL_DLY1CONF_NO_gc|XCL_DLY0CONF_NO_gc;
    XCL.CTRLD = (0x0<<XCL_TRUTH1_gp)|(0xE<<XCL_TRUTH0_gp);
    if(SPI.settings.input==SPI_INPUT_MISO) {
        SPI.input = SPI_MISO_bm;
    } else {
        SPI.input = SPI_MOSI_bm;
    }
    if(SPI.settings.select==SPI_SELECT_HIGH) {
        SPI.select = 0;
    } else {
        SPI.select = SPI_SS_bm;
    }
}

static inline void SPI_ClockEdge(void) {
    if(SPI.settings.clock==SPI_CLOCK_FALLING) {
        PORTA_PIN1CTRL = PORT_OPC_BUSKEEPER_gc|PORT_ISC_FALLING_gc; // SCK
    } else {
        PORTA_PIN1CTRL = PORT_OPC_BUSKEEPER_gc|PORT_ISC_RISING_gc; // SCK
    }
}

static void SPI_Decode(void) {
    static uint8_t byte, bit;
    while(!BUFFER_Empty()) {
        uint8_t data = BUFFER_GetData();
        if((data&SPI_SS_bm)^SPI.select) {
            if(bit>0) {
                if(bit>3) {
                    DIGITAL_PrintHex(byte>>4);
                }
                DIGITAL_PrintChar('?');
            }
            if(SPI.settings.select==SPI_SELECT_HIGH) {
                data^=SPI_SS_bm;
            }
            if(data&SPI_SS_bm) {
                DIGITAL_PrintChar(SPI_STOP);
                DIGITAL_NewLine();
            } else {
                DIGITAL_PrintChar(SPI_START);
            }
            if(SPI.input==SPI_MISO_bm) {
                DIGITAL_InvertLine();
            }
            byte = 0x00;
            bit = 0;
        } else {
            if(bit==0) {
                if(SPI.settings.input) {
                    SPI.input = SPI_MISO_bm;
                } else {
                    SPI.input = SPI_MOSI_bm;
                }
            }
            if(SPI.settings.data==SPI_DATA_LSB) {
                byte>>=1;
                if(data&SPI.input) {
                    byte|=0x80;
                }
            } else {
                byte<<=1;
                if(data&SPI.input) {
                    byte|=0x01;
                }
            }
            bit++;
            if(bit>7) {
                DIGITAL_PrintHex(byte>>4);
                DIGITAL_PrintHex(byte>>0);
                byte = 0x00;
                bit = 0;
                if(SPI.input==SPI_MISO_bm) {
                    DIGITAL_InvertLine();
                }
            }
        }
        SPI.select = data&SPI_SS_bm;
    }
    if(DIGITAL_ClockPeriod()<SPI_MIN_CLOCK_PERIOD) {
        DIGITAL_Blackout();
        DIGITAL_ClockPeriodReset();
    }
}

static void SPI_KeyUp(KEYPAD_KEY_t key) {
    if(!DIGITAL_Counter()&&(key!=KEYPAD_KEY1)) {
        return;
    }
    switch(key) {
        case KEYPAD_KEY1:
            DIGITAL_Hold(1);
            KEYPAD_KeyUp(SPI_SettingsKeyUp);
            MAIN_Loop(SPI_SettingsLoop);
            DISPLAY_Select(-1);
            break;
        case KEYPAD_KEY2:
            if(DIGITAL_IsHold()) { break; }
            DIGITAL_NewLine();
            SPI.settings.input = !SPI.settings.input;
            SPI_SaveSettings();
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

static inline const __flash char* SPI_DATA(void) {
    static const __flash char* const __flash DATA[] = {
        [SPI_DATA_LSB] = TEXT_LSB_MSB,
        [SPI_DATA_MSB] = TEXT_MSB_LSB,
    };
    return DATA[SPI.settings.data];
}

static inline const __flash char* SPI_CLOCK(void) {
    static const __flash char* const __flash CLOCK[] = {
        [SPI_CLOCK_FALLING] = TEXT_FALLING_EDGE,
        [SPI_CLOCK_RISING] = TEXT_RISING_EDGE,
    };
    return CLOCK[SPI.settings.clock];
}

static inline const __flash char* SPI_SELECT(void) {
    static const __flash char* const __flash SELECT[] = {
        [SPI_SELECT_LOW] = TEXT_LOW,
        [SPI_SELECT_HIGH] = TEXT_HIGH,
    };
    return SELECT[SPI.settings.select];
}

static void SPI_SettingsLoop(void) {
    if(!DISPLAY_Update()) { return; }
    DISPLAY_CursorPosition(7, 1);
    puts_P(TEXT_SPI_SETTINGS);
    DISPLAY_CursorPosition(3,15);
    printf_P(TEXT_DATA, SPI_DATA());
    DISPLAY_CursorPosition(3,24);
    printf_P(TEXT_CLOCK, SPI_CLOCK());
    DISPLAY_CursorPosition(3,33);
    printf_P(TEXT_SELECT, SPI_SELECT());
    DISPLAY_InvertLine(0);
    DISPLAY_SelectLine();
}

static void SPI_SettingsKeyUp(KEYPAD_KEY_t key) {
    switch(key) {
        case KEYPAD_KEY1:
            SPI.settings.data = !SPI.settings.data;
            DISPLAY_Select(14);
            break;
        case KEYPAD_KEY2:
            SPI.settings.clock = !SPI.settings.clock;
            DISPLAY_Select(23);
            break;
        case KEYPAD_KEY3:
            SPI.settings.select = !SPI.settings.select;
            DISPLAY_Select(32);
            break;
        case KEYPAD_KEY4:
            SPI_SaveSettings();
            SPI_ClockEdge();
            KEYPAD_KeyUp(SPI_KeyUp);
            DIGITAL_Init(SPI_Decode);
            DIGITAL_Hold(0);
            break;
        default: break;
    }
}

void SPI_Intro(void) {
    DISPLAY_Image(IMAGE_SPI);
    SPI_Icons();
    DISPLAY_Send();
}

static void SPI_Info(void) {
    DISPLAY_CursorPosition(11, 1);
    DISPLAY_Icon(ICON_SETTINGS);
    DISPLAY_CursorPosition(25, 2);
    puts_P(TEXT_SETTINGS);
    DISPLAY_CursorPosition(11, 10);
    DISPLAY_Icon(ICON_INPUT);
    DISPLAY_CursorPosition(25, 11);
    puts_P(TEXT_INPUT);
    DISPLAY_CursorPosition(10, 19);
    DISPLAY_Icon(ICON_HOLD_RUN);
    DISPLAY_CursorPosition(25, 20);
    puts_P(TEXT_HOLD_RUN);
    DISPLAY_CursorPosition(10, 28);
    DISPLAY_Icon(ICON_CLEAR);
    DISPLAY_CursorPosition(25, 29);
    puts_P(TEXT_CLEAR);
    SPI_Icons();
    DISPLAY_Send();
    DELAY_Info();
}

static void SPI_Desc(void) {
    DISPLAY_CursorPosition(10, 2);
    puts_P(TEXT_DESC_START);
    DISPLAY_CursorPosition(10,11);
    puts_P(TEXT_DESC_STOP);
    DISPLAY_CursorPosition(10,20);
    puts_P(TEXT_OVERFLOW);
    DISPLAY_CursorPosition(10,29);
    puts_P(TEXT_FREQ_ERR);
    SPI_Icons();
    DISPLAY_Send();
    DELAY_Info();
}

static void SPI_Icons(void) {
    static const __flash uint8_t* const __flash SPI_ICONS[] = {
        ICON_SETTINGS,
        ICON_INPUT,
        ICON_HOLD_RUN,
        ICON_CLEAR,
    };
    DISPLAY_Icons(SPI_ICONS);
}

static inline void SPI_SaveSettings(void) {
    EEPROM_update_block(&SPI.settings, &SPI_settings, sizeof(SPI_SETTINGS_t));
}

static inline void SPI_LoadSettings(void) {
    eeprom_read_block(&SPI.settings, &SPI_settings, sizeof(SPI_SETTINGS_t));
    if(SPI.settings.select>SPI_SELECT_HIGH) {
        SPI.settings.select = SPI_SELECT_LOW;
    }
    if(SPI.settings.clock>SPI_CLOCK_RISING) {
        SPI.settings.clock = SPI_CLOCK_FALLING;
    }
    if(SPI.settings.data>SPI_DATA_MSB) {
        SPI.settings.data = SPI_DATA_LSB;
    }
    if(SPI.settings.input>SPI_INPUT_MISO) {
        SPI.settings.input = SPI_INPUT_MOSI;
    }
    SPI_SaveSettings();
}
