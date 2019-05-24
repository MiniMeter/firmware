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
#ifndef LED_H_INCLUDED
#define LED_H_INCLUDED

#define LED_MAIN1_bm  PIN2_bm
#define LED_MAIN2_bm  PIN7_bm
#define LED_MAIN3_bm  PIN4_bm
#define LED_MAIN4_bm  PIN1_bm
#define LED_AUX1_bm  PIN3_bm
#define LED_AUX2_bm  PIN0_bm

static inline void LED_AuxOn(void) {
    VPORT0_OUT |= (LED_AUX1_bm);
    VPORT3_OUT |= (LED_AUX2_bm);
}

static inline void LED_AuxOff(void) {
    VPORT0_OUT &= ~(LED_AUX1_bm);
    VPORT3_OUT &= ~(LED_AUX2_bm);
}

static inline void LED_MainOn(void) {
    VPORT0_OUT |= (LED_MAIN1_bm|LED_MAIN3_bm);
    VPORT2_OUT |= (LED_MAIN2_bm);
    VPORT3_OUT |= (LED_MAIN4_bm);
}

static inline void LED_MainOff(void) {
    VPORT0_OUT &= ~(LED_MAIN1_bm|LED_MAIN3_bm);
    VPORT2_OUT &= ~(LED_MAIN2_bm);
    VPORT3_OUT &= ~(LED_MAIN4_bm);
}

static inline void LED_On(void) {
    VPORT0_DIR |= (LED_AUX1_bm|LED_MAIN1_bm|LED_MAIN3_bm);
    VPORT2_DIR |= (LED_MAIN2_bm);
    VPORT3_DIR |= (LED_AUX2_bm|LED_MAIN4_bm);
}

static inline void LED_Off(void) {
    VPORT0_DIR &= ~(LED_AUX1_bm|LED_MAIN1_bm|LED_MAIN3_bm);
    VPORT2_DIR &= ~(LED_MAIN2_bm);
    VPORT3_DIR &= ~(LED_AUX2_bm|LED_MAIN4_bm);
}

#endif // LED_H_INCLUDED
