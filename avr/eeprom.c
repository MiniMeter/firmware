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
#include "eeprom.h"

static struct {
    uint8_t *scr;
    uint8_t *dst;
    volatile size_t len;
} EEPROM;

void EEPROM_update_block(void* src, void* dst, size_t n) {
    while(EEPROM.len);
    EEPROM.dst = dst;
    EEPROM.scr = src;
    EEPROM.len = n-1;
    eeprom_update_byte(EEPROM.dst, *(EEPROM.scr));
    if(n<2) { return; }
    EEPROM.dst++;
    EEPROM.scr++;
    NVM_INTCTRL = NVM_EELVL_LO_gc;
}

ISR(NVM_EE_vect) {
    eeprom_update_byte(EEPROM.dst, *(EEPROM.scr));
    EEPROM.dst++;
    EEPROM.scr++;
    EEPROM.len--;
    if(EEPROM.len==0) {
        NVM_INTCTRL = NVM_EELVL_OFF_gc;
        return;
    }
}

uint16_t EEPROM_Usage(void) {
    uint16_t count = 0;
    for(uint16_t i=EEPROM_START; i<=EEPROM_END; i++) {
        if(eeprom_read_byte((uint8_t*)i)!=0xFF) {
            count++;
        }
    }
    return count;
}
