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
#ifndef IOX32E5_H_INCLUDED
#define IOX32E5_H_INCLUDED

/* Fix XMEGA header files */

// USART.CTRLC  bit masks and bit positions
#ifndef USART_UCPHA_bm
#  define USART_UCPHA_bm (1<<1)
#  define USART_UCPHA_bp 1
#endif
#ifndef USART_DORD_bm
#  define USART_DORD_bm (1<<2)
#  define USART_DORD_bp 2
#endif

/* EDMA Channel Error Interrupt Level */
typedef enum EDMA_CH_ERRINTLVL_enum {
    EDMA_CH_ERRINTLVL_OFF_gc = (0x00<<2),  /* Interrupt disabled */
    EDMA_CH_ERRINTLVL_LO_gc = (0x01<<2),  /* Low level */
    EDMA_CH_ERRINTLVL_MED_gc = (0x02<<2),  /* Medium level */
    EDMA_CH_ERRINTLVL_HI_gc = (0x03<<2),  /* High level */
} EDMA_CH_ERRINTLVL_t;

/* EDMA Channel Transaction Complete Interrupt Level */
typedef enum EDMA_CH_TRNINTLVL_enum {
    EDMA_CH_TRNINTLVL_OFF_gc = (0x00<<0),  /* Interrupt disabled */
    EDMA_CH_TRNINTLVL_LO_gc = (0x01<<0),  /* Low level */
    EDMA_CH_TRNINTLVL_MED_gc = (0x02<<0),  /* Medium level */
    EDMA_CH_TRNINTLVL_HI_gc = (0x03<<0),  /* High level */
} EDMA_CH_TRNINTLVL_t;

/* XCL Timer/Counter Mode */
#define XCL_TCMODE_1SHOT_gc  (0x03<<0)

/* XCL BTC1 Timer/Counter Event Action Selection */
typedef enum XCL_EVACT1_enum {
    XCL_EVACT1_INPUT_gc = (0x00<<5),  /* Input Capture */
    XCL_EVACT1_FREQ_gc = (0x01<<5),  /* Frequency Capture */
    XCL_EVACT1_PW_gc = (0x02<<5),  /* Pulse Width Capture */
    XCL_EVACT1_STOPSTART_gc = (0x02<<5),  /* 1SHOT stop-start mode */
    XCL_EVACT1_RESTART_gc = (0x03<<5),  /* Restart timer/counter */
} XCL_EVACT1_t;

/* XCL BTC0 Timer/Counter Event Action Selection */
typedef enum XCL_EVACT0_enum {
    XCL_EVACT0_INPUT_gc = (0x00<<3),  /* Input Capture */
    XCL_EVACT0_FREQ_gc = (0x01<<3),  /* Frequency Capture */
    XCL_EVACT0_PW_gc = (0x02<<3),  /* Pulse Width Capture */
    XCL_EVACT0_STOPSTART_gc = (0x02<<5),  /* 1SHOT stop-start mode */
    XCL_EVACT0_RESTART_gc = (0x03<<3),  /* Restart timer/counter */
} XCL_EVACT0_t;

/* XCL LUT1 Delay Configuration on LUT */
typedef enum XCL_DLY1CONF_enum {
    XCL_DLY1CONF_NO_gc = (0x00<<2),  /* Delay element disabled */
    XCL_DLY1CONF_IN_gc = (0x01<<2),  /* Delay enabled on LUT input */
    XCL_DLY1CONF_OUT_gc = (0x02<<2),  /* Delay enabled on LUT output */
} XCL_DLY1CONF_t;

/* XCL LUT0 Delay Configuration on LUT */
typedef enum XCL_DLY0CONF_enum {
    XCL_DLY0CONF_NO_gc = (0x00<<0),  /* Delay element disabled */
    XCL_DLY0CONF_IN_gc = (0x01<<0),  /* Delay enabled on LUT input */
    XCL_DLY0CONF_OUT_gc = (0x02<<0),  /* Delay enabled on LUT output */
} XCL_DLY0CONF_t;

/* XCL LUT0 IN0 Selection */
typedef enum XCL_IN0SEL_enum {
    XCL_IN0SEL_EVSYS_gc = (0x00<<0),  /* Event system selected as source */
    XCL_IN0SEL_XCL_gc = (0x01<<0),  /* XCL selected as source */
    XCL_IN0SEL_PINL_gc = (0x02<<0),  /* LSB port pin selected as source */
    XCL_IN0SEL_PINH_gc = (0x03<<0),  /* MSB port pin selected as source */
} XCL_IN0SEL_t;

/* XCL LUT0 IN1 Selection */
typedef enum XCL_IN1SEL_enum {
    XCL_IN1SEL_EVSYS_gc = (0x00<<2),  /* Event system selected as source */
    XCL_IN1SEL_XCL_gc = (0x01<<2),  /* XCL selected as source */
    XCL_IN1SEL_PINL_gc = (0x02<<2),  /* LSB port pin selected as source */
    XCL_IN1SEL_PINH_gc = (0x03<<2),  /* MSB port pin selected as source */
} XCL_IN1SEL_t;

/* XCL LUT1 IN2 Selection */
typedef enum XCL_IN2SEL_enum {
    XCL_IN2SEL_EVSYS_gc = (0x00<<4),  /* Event system selected as source */
    XCL_IN2SEL_XCL_gc = (0x01<<4),  /* XCL selected as source */
    XCL_IN2SEL_PINL_gc = (0x02<<4),  /* LSB port pin selected as source */
    XCL_IN2SEL_PINH_gc = (0x03<<4),  /* MSB port pin selected as source */
} XCL_IN2SEL_t;

/* XCL LUT1 IN3 Selection */
typedef enum XCL_IN3SEL_enum {
    XCL_IN3SEL_EVSYS_gc = (0x00<<6),  /* Event system selected as source */
    XCL_IN3SEL_XCL_gc = (0x01<<6),  /* XCL selected as source */
    XCL_IN3SEL_PINL_gc = (0x02<<6),  /* LSB port pin selected as source */
    XCL_IN3SEL_PINH_gc = (0x03<<6),  /* MSB port pin selected as source */
} XCL_IN3SEL_t;

#endif // IOX32E5_H_INCLUDED
