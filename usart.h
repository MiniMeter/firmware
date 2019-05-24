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
#ifndef USART_H_INCLUDED
#define USART_H_INCLUDED

typedef enum {
    USART_BAUD_1200,
    USART_BAUD_2400,
    USART_BAUD_4800,
    USART_BAUD_9600,
    USART_BAUD_14400,
    USART_BAUD_19200,
    USART_BAUD_28800,
    USART_BAUD_38400,
    USART_BAUD_57600,
    USART_BAUD_76800,
    USART_BAUD_115200,
    USART_BAUD_230400,
    USART_BAUD_460800,
    USART_BAUD_921600,
    USART_BAUD_1382000,
    USART_BAUD_1843000,
    USART_BAUD_2000000
} USART_BAUD_t;

typedef enum {
    USART_FRAME_5BIT,
    USART_FRAME_6BIT,
    USART_FRAME_7BIT,
    USART_FRAME_8BIT
} USART_FRAME_t;

typedef enum {
    USART_PARITY_NO,
    USART_PARITY_ODD,
    USART_PARITY_EVEN
} USART_PARITY_t;

void USART_Info(void);
uint32_t USART_Baud(USART_BAUD_t baud);
uint8_t USART_Frame(USART_FRAME_t frame);
const __flash char* USART_Parity(USART_PARITY_t parity);
USART_CHSIZE_t USART_CHSIZE(USART_FRAME_t frame);
USART_PMODE_t USART_PMODE(USART_PARITY_t parity);
void USART_SetBaudAsync(USART_t* const usart, uint32_t baud);

inline void USART_SetBaudSync(USART_t* const usart, uint32_t baud) {
    uint16_t bsel = 0;
    uint8_t bscale = 0;
    /* Check if baudrate is less than the maximum limit specified in
     * datasheet */
    if (baud<(F_CPU/2)) {
        bsel = (F_CPU/(baud * 2))-1;
    }
    while(bsel>0x0FFF) {
        bsel /= 2;
        bscale++;
    }
    (usart)->BAUDCTRLB = (uint8_t)((bscale<<4)|(bsel>>8));
    (usart)->BAUDCTRLA = (uint8_t)(bsel);
}

extern const __flash uint8_t* const __flash USART_ICONS[];

#endif // USART_H_INCLUDED
