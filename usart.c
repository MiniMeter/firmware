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
#include <stdio.h>
#include "text.h"
#include "image.h"
#include "display.h"
#include "delay.h"
#include "font.h"
#include "icon.h"
#include "usart.h"

void USART_Info(void) {
    DISPLAY_CursorPosition(11, 1);
    DISPLAY_Icon(ICON_SETTINGS);
    DISPLAY_CursorPosition(25, 2);
    puts_P(TEXT_SETTINGS);
    DISPLAY_CursorPosition(11, 10);
    DISPLAY_Icon(ICON_DISPLAY);
    DISPLAY_CursorPosition(25, 11);
    puts_P(TEXT_HEX_ASCII);
    DISPLAY_CursorPosition(10, 19);
    DISPLAY_Icon(ICON_HOLD_RUN);
    DISPLAY_CursorPosition(25, 20);
    puts_P(TEXT_HOLD_RUN);
    DISPLAY_CursorPosition(10, 28);
    DISPLAY_Icon(ICON_CLEAR);
    DISPLAY_CursorPosition(25, 29);
    puts_P(TEXT_CLEAR);
    DISPLAY_Icons(USART_ICONS);
    DISPLAY_Send();
    DELAY_Info();
}

uint32_t USART_Baud(USART_BAUD_t baud) {
    static const __flash uint32_t BAUD[] = {
        [USART_BAUD_1200] = 1200,
        [USART_BAUD_2400] = 2400,
        [USART_BAUD_4800] = 4800,
        [USART_BAUD_9600] = 9600,
        [USART_BAUD_14400] = 14400,
        [USART_BAUD_19200] = 19200,
        [USART_BAUD_28800] = 28800,
        [USART_BAUD_38400] = 38400,
        [USART_BAUD_57600] = 57600,
        [USART_BAUD_76800] = 76800,
        [USART_BAUD_115200] = 115200,
        [USART_BAUD_230400] = 230400,
        [USART_BAUD_460800] = 460800,
        [USART_BAUD_921600] = 921600,
        [USART_BAUD_1382000] = 1382000,
        [USART_BAUD_1843000] = 1843000,
        [USART_BAUD_2000000] = 2000000
    };
    return BAUD[baud];
}

uint8_t USART_Frame(USART_FRAME_t frame) {
    static const __flash uint8_t FRAME[] = {
        [USART_FRAME_5BIT] = 5,
        [USART_FRAME_6BIT] = 6,
        [USART_FRAME_7BIT] = 7,
        [USART_FRAME_8BIT] = 8
    };
    return FRAME[frame];
}

const __flash char* USART_Parity(USART_PARITY_t parity) {
    static const __flash char* const __flash PARITY[] = {
        [USART_PARITY_NO] = TEXT_NO,
        [USART_PARITY_ODD] = TEXT_ODD,
        [USART_PARITY_EVEN] = TEXT_EVEN
    };
    return PARITY[parity];
}

USART_CHSIZE_t USART_CHSIZE(USART_FRAME_t frame) {
    static const __flash uint8_t CHSIZE[] = {
        [USART_FRAME_5BIT] = USART_CHSIZE_5BIT_gc,
        [USART_FRAME_6BIT] = USART_CHSIZE_6BIT_gc,
        [USART_FRAME_7BIT] = USART_CHSIZE_7BIT_gc,
        [USART_FRAME_8BIT] = USART_CHSIZE_8BIT_gc
    };
    return CHSIZE[frame];
}

USART_PMODE_t USART_PMODE(USART_PARITY_t parity) {
    static const __flash uint8_t PMODE[] = {
        [USART_PARITY_NO]  = USART_PMODE_DISABLED_gc,
        [USART_PARITY_ODD] = USART_PMODE_ODD_gc,
        [USART_PARITY_EVEN] = USART_PMODE_EVEN_gc
    };
    return PMODE[parity];
}

void USART_SetBaudAsync(USART_t* const usart, uint32_t baud) {
    int8_t exp;
    uint32_t div;
    uint32_t limit;
    uint32_t ratio;
    uint32_t min_rate;
    uint32_t max_rate;
    uint32_t cpu_hz = F_CPU;
    /*
     * Check if the hardware supports the given baud rate
     */
    /* 8 = (2^0) * 8 * (2^0) = (2^BSCALE_MIN) * 8 * (BSEL_MIN) */
    max_rate = cpu_hz / 8;
    /* 4194304 = (2^7) * 8 * (2^12) = (2^BSCALE_MAX) * 8 * (BSEL_MAX+1) */
    min_rate = cpu_hz / 4194304;
    if (!((usart)->CTRLB & USART_CLK2X_bm)) {
        max_rate /= 2;
        min_rate /= 2;
    }
    if ((baud > max_rate) || (baud < min_rate)) {
        return;
    }
    /* Check if double speed is enabled. */
    if (!((usart)->CTRLB & USART_CLK2X_bm)) {
        baud *= 2;
    }
    /* Find the lowest possible exponent. */
    limit = 0xfffU >> 4;
    ratio = cpu_hz / baud;
    for (exp = -7; exp < 7; exp++) {
        if (ratio < limit) {
            break;
        }
        limit <<= 1;
        if (exp < -3) {
            limit |= 1;
        }
    }
    /*
     * Depending on the value of exp, scale either the input frequency or
     * the target baud rate. By always scaling upwards, we never introduce
     * any additional inaccuracy.
     *
     * We are including the final divide-by-8 (aka. right-shift-by-3) in
     * this operation as it ensures that we never exceeed 2**32 at any
     * point.
     *
     * The formula for calculating BSEL is slightly different when exp is
     * negative than it is when exp is positive.
     */
    if (exp < 0) {
        /* We are supposed to subtract 1, then apply BSCALE. We want to
         * apply BSCALE first, so we need to turn everything inside the
         * parenthesis into a single fractional expression.
         */
        cpu_hz -= 8 * baud;
        /* If we end up with a left-shift after taking the final
         * divide-by-8 into account, do the shift before the divide.
         * Otherwise, left-shift the denominator instead (effectively
         * resulting in an overall right shift.)
         */
        if (exp <= -3) {
            div = ((cpu_hz << (-exp - 3)) + baud / 2) / baud;
        } else {
            baud <<= exp + 3;
            div = (cpu_hz + baud / 2) / baud;
        }
    } else {
        /* We will always do a right shift in this case, but we need to
         * shift three extra positions because of the divide-by-8.
         */
        baud <<= exp + 3;
        div = (cpu_hz + baud / 2) / baud - 1;
    }
    (usart)->BAUDCTRLB = (uint8_t)(((div >> 8) & 0X0F) | (exp << 4));
    (usart)->BAUDCTRLA = (uint8_t)div;
}

const __flash uint8_t* const __flash USART_ICONS[] = {
    ICON_SETTINGS,
    ICON_DISPLAY,
    ICON_HOLD_RUN,
    ICON_CLEAR,
};
