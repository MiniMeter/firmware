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
#include <util/delay.h>
#include "avr/iox32e5.h"
#include "display.h"
#include "usart.h"
#include "device.h"
#include "lcd.h"

#define LCD_CLK_bm  PIN1_bm // inverted
#define LCD_CLK_SET  VPORT2_OUT &= ~LCD_CLK_bm
#define LCD_CLK_CLR  VPORT2_OUT |= LCD_CLK_bm
#define LCD_RST_bm  PIN2_bm
#define LCD_RST_SET  VPORT2_OUT |= LCD_RST_bm
#define LCD_RST_CLR  VPORT2_OUT &= ~LCD_RST_bm
#define LCD_DIN_bm  PIN3_bm
#define LCD_DIN_SET  VPORT2_OUT |= LCD_DIN_bm
#define LCD_DIN_CLR  VPORT2_OUT &= ~LCD_DIN_bm
#define LCD_CE_bm   PIN4_bm // inverted
#define LCD_CE_SET  VPORT2_OUT &= ~LCD_CE_bm
#define LCD_CE_CLR  VPORT2_OUT |= LCD_CE_bm
#define LCD_DC_bm   PIN5_bm // inverted
#define LCD_DC_SET  VPORT2_OUT &= ~LCD_DC_bm
#define LCD_DC_CLR  VPORT2_OUT |= LCD_DC_bm

/* LCD driver PCD8544 (Nokia 5110 LCD) */

/* COMMAND */
#define NO_OPERATION  0x00
#define FUNCTION_SET  0x20

/* FUNCTION_SET */
#define CHIP_ACTIVE_MODE  0x00
#define CHIP_POWER_DOWN_MODE  0x04
#define HORIZONTAL_ADDRESSING  0x00
#define VERTICAL_ADDRESSING  0x02
#define BASIC_INSTRUCTION_SET  0x00
#define EXTENDED_INSTRUCTION_SET  0x01

/* BASIC_INSTRUCTION */
#define DISPLAY_CONTROL  0x08
#define SET_ROW  0x40
#define SET_COLUMN  0x80

/* EXTENDED_INSTRUCTION */
#define OPERATION_VOLTAGE  0x80
#define TEMPERATURE_CONTROL  0x04
#define BIAS_SYSTEM  0x10

/* DISPLAY_CONTROL */
#define DISPLAY_BLANK  0x00
#define ALL_SEGMENTS_ON  0x01
#define NORMAL_MODE  0x04
#define INVERSE_MODE  0x05

/* TEMPERATURE_CONTROL */
#define TEMPERATURE_COEFFICIENT_0  0x00
#define TEMPERATURE_COEFFICIENT_1  0x01
#define TEMPERATURE_COEFFICIENT_2  0x02
#define TEMPERATURE_COEFFICIENT_3  0x03

/* BIAS_SYSTEM */
#define MUX_RATE_8   0x07
#define MUX_RATE_16  0x06
#define MUX_RATE_24  0x05
#define MUX_RATE_34  0x04
#define MUX_RATE_48  0x03
#define MUX_RATE_65  0x02
#define MUX_RATE_80  0x01
#define MUX_RATE_100 0x00

typedef struct {
    uint8_t init[8];
    uint8_t frame[504];
} LCD_BUFFER_t;

static LCD_BUFFER_t LCD_buffer[2];
static struct {
    LCD_BUFFER_t* buffer;
    uint16_t counter;
    uint8_t contrast;
    uint8_t (*Busy)(void);
} LCD;

static void LCD_BufferInit(LCD_BUFFER_t* buffer);
static uint8_t LCD_RAW_Busy(void);
static uint8_t LCD_USART_Busy(void);
static uint8_t LCD_EDMA_Busy(void);

uint8_t* LCD_Init(void) {
    PORTD_DIRSET = LCD_CLK_bm|LCD_RST_bm|LCD_DIN_bm|LCD_CE_bm|LCD_DC_bm;
    PORTD.PIN1CTRL = PORT_ISC_FALLING_gc|PORT_INVEN_bm; // CLK
    PORTD.PIN4CTRL = PORT_INVEN_bm; // CE
    PORTD.PIN5CTRL = PORT_INVEN_bm; // DC
    LCD.buffer = &LCD_buffer[0];
    _delay_ms(1); //min 100ns reset pulse
    LCD_RST_SET;
    LCD_RAW_Mode();
    return LCD_buffer[1].frame;
}

static void LCD_BufferInit(LCD_BUFFER_t* buffer) {
    buffer->init[0] = FUNCTION_SET|CHIP_ACTIVE_MODE|VERTICAL_ADDRESSING|EXTENDED_INSTRUCTION_SET;
    buffer->init[1] = OPERATION_VOLTAGE|(0x7F&LCD.contrast);
    buffer->init[2] = TEMPERATURE_CONTROL|TEMPERATURE_COEFFICIENT_2;
    buffer->init[3] = BIAS_SYSTEM|MUX_RATE_48;
    buffer->init[4] = FUNCTION_SET|CHIP_ACTIVE_MODE|VERTICAL_ADDRESSING|BASIC_INSTRUCTION_SET;
    buffer->init[5] = DISPLAY_CONTROL|NORMAL_MODE;
    buffer->init[6] = SET_ROW|0; //0-5
    buffer->init[7] = SET_COLUMN|0; //0-83
}

void LCD_RAW_Mode(void) {
    EDMA_CTRL = 0x00;
    EDMA_CTRL = EDMA_RESET_bm;
    TCD5_CTRLA = TC45_CLKSEL_OFF_gc;
    TCD5_CTRLGSET = TC45_CMD_RESET_gc;
    USARTD0.CTRLA = 0x00;
    USARTD0.CTRLB = 0x00;
    USARTD0.CTRLC = 0x00;
    USARTD0.CTRLD = 0x00;
    PORTD_DIRSET = LCD_DIN_bm;
    LCD_CE_SET;
    LCD.Busy = LCD_RAW_Busy;
}

void LCD_USART_Mode(void) {
    LCD_RAW_Mode();
    USARTD0.CTRLA = USART_RXCINTLVL_OFF_gc|USART_TXCINTLVL_OFF_gc|USART_DREINTLVL_OFF_gc;
    USARTD0.CTRLC = USART_CMODE_MSPI_gc|USART_UCPHA_bm;
    USARTD0.CTRLD = USART_LUTACT_OFF_gc|USART_PECACT_OFF_gc;
    USART_SetBaudSync(&USARTD0, 1000000);
    USARTD0.CTRLB = USART_TXEN_bm;
    EVSYS_CH3MUX = EVSYS_CHMUX_PORTD_PIN1_gc;
    TCD5.CTRLE = TC45_CCBMODE_COMP_gc|TC45_CCAMODE_COMP_gc;
    TCD5.CTRLB = TC45_BYTEM_NORMAL_gc|TC45_WGMODE_SINGLESLOPE_gc;
    TCD5.CCB = (8*sizeof(LCD.buffer->init))-1;
    TCD5.CCA = (8*sizeof(LCD_BUFFER_t))-1;
    TCD5.CTRLA = TC45_CLKSEL_EVCH3_gc;
    LCD.Busy = LCD_USART_Busy;
}

void LCD_EDMA_Mode(void) {
    LCD_USART_Mode();
    EDMA.CTRL &= ~(EDMA_ENABLE_bm|EDMA_DBUFMODE_BUF23_gc);
    EDMA.CTRL |= EDMA_CHMODE_STD2_gc|EDMA_PRIMODE_CH0123_gc;
    EDMA.CH2.TRIGSRC = EDMA_CH_TRIGSRC_USARTD0_DRE_gc;
    EDMA.CH2.ADDRCTRL = EDMA_CH_RELOAD_NONE_gc|EDMA_CH_DIR_INC_gc;
    EDMA.CH2.DESTADDR = (uint16_t)&USARTD0_DATA;
    EDMA.CH2.DESTADDRCTRL = EDMA_CH_RELOAD_NONE_gc|EDMA_CH_DESTDIR_FIXED_gc;
    EDMA.CTRL |= EDMA_ENABLE_bm;
    LCD.Busy = LCD_EDMA_Busy;
}

static uint8_t LCD_RAW_Busy(void) {
    if(LCD.counter==0) {
        LCD_CE_CLR;
        LCD_DC_CLR;
        /* Disabling USARTD0 transmitter may change this pin to input */
        PORTD_DIRSET = LCD_DIN_bm;
    }
    uint8_t* buffer = (uint8_t*)LCD.buffer;
    uint8_t data = buffer[LCD.counter];
    for(uint8_t b=0; b<8; b++) {
        LCD_CLK_CLR;
        if(data&0x80) {
            LCD_DIN_SET;
        } else {
            LCD_DIN_CLR;
        }
        _delay_us(1);
        LCD_CLK_SET;
        _delay_us(1);
        data<<=1;
    }
    LCD.counter++;
    if(LCD.counter==sizeof(LCD.buffer->init)) {
        LCD_DC_SET;
    } else if(LCD.counter>=sizeof(LCD_BUFFER_t)) {
        LCD_CE_SET;
        return 0;
    }
    return 1;
}

static uint8_t LCD_USART_Busy(void) {
    if(USARTD0_STATUS&USART_DREIF_bm) {
        if(LCD.counter==0) {
            TCD5_CTRLGSET = TC45_CMD_RESTART_gc;
        }
        uint8_t* buffer = (uint8_t*)LCD.buffer;
        USARTD0_DATA = buffer[LCD.counter];
        LCD.counter++;
        if(LCD.counter>=sizeof(LCD_BUFFER_t)) {
            return 0;
        }
    }
    return 1;
}

static uint8_t LCD_EDMA_Busy(void) {
    if(!(EDMA_CH2_CTRLA&EDMA_CH_ENABLE_bm)) {
        if(LCD.counter>0) { return 0; }
        TCD5_CTRLGSET = TC45_CMD_RESTART_gc;
        EDMA.CH2.ADDR = (uint16_t)LCD.buffer;
        EDMA.CH2.TRFCNT = sizeof(LCD_BUFFER_t);
        EDMA.CH2.CTRLA = EDMA_CH_ENABLE_bm|EDMA_CH_SINGLE_bm;
        LCD.counter = sizeof(LCD_BUFFER_t);
    }
    return 1;
}

uint8_t LCD_Busy(void) {
    if(LCD.counter>=sizeof(LCD_BUFFER_t)) { return 0; }
    if(LCD.Busy) { return LCD.Busy(); }
    return 0;
}

uint8_t* LCD_Send(void) {
    static uint8_t active;
    uint8_t* frame = LCD.buffer->frame;
    active = !active; // 0 -> 1 -> 0 -> 1 ...
    LCD.buffer = &LCD_buffer[active];
    LCD.counter = 0;
    for(uint16_t i=0; i<504; i++) {
        frame[i] = 0x00;
    }
    return frame;
}

void LCD_Contrast(uint8_t contrast) {
    if(LCD.contrast==contrast) { return; }
    LCD.contrast = contrast;
    LCD_BufferInit(&LCD_buffer[0]);
    LCD_BufferInit(&LCD_buffer[1]);
}
