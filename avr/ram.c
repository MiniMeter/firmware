/***************************************************************************
Copyright (c) 2007, Michael C McTernan
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
#include "ram.h"

#define RAM_CANARY  0xA5

extern uint8_t _end;
extern uint8_t __stack;

/** Fill the stack space with a known pattern.
 * This fills all stack bytes with the 'canary' pattern to allow stack usage
 * to be later estimated.  This runs in the .init1 section, before normal
 * stack initialisation and unfortunately before __zero_reg__ has been
 * setup.  The C code is therefore replaced with inline assembly to ensure
 * the zero reg is not used by compiled code.
 *
 * \note This relies on the linker defining \a _end and \a __stack to define
 *        the bottom of the stack (\a __stack) and the top most address
 *        (\a _end).
 */
__attribute__ ((naked)) __attribute__ ((section (".init1")))
void RAM_Init(void) {
#if 0
    uint8_t *p = &_end;

    while(p <= &__stack)
    {
        *p = RAM_CANARY;
        p++;
    }
#else
    __asm volatile ("    ldi r30,lo8(_end)\n"
                    "    ldi r31,hi8(_end)\n"
                    "    ldi r24,lo8(0xA5)\n" /* RAM_CANARY = 0xA5 */
                    "    ldi r25,hi8(__stack)\n"
                    "    rjmp .cmp\n"
                    ".loop:\n"
                    "    st Z+,r24\n"
                    ".cmp:\n"
                    "    cpi r30,lo8(__stack)\n"
                    "    cpc r31,r25\n"
                    "    brlo .loop\n"
                    "    breq .loop"::);
#endif
}

/** Count unused stack space.
 * This examines the stack space, and determines how many bytes have never
 * been overwritten.
 *
 * \returns  The count of bytes likely to have never been used by the stack.
 */
uint16_t RAM_Usage(void) {
    const uint8_t *p = &_end;
    uint16_t count = 0;
    while((*p==RAM_CANARY)&&(p<=&__stack)) {
        p++;
        count++;
    }
    return RAM_SIZE-count;
}
