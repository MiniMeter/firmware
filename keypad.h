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
#ifndef KEYPAD_H_INCLUDED
#define KEYPAD_H_INCLUDED

typedef enum {
    KEYPAD_KEY0,
    KEYPAD_KEY1,
    KEYPAD_KEY2,
    KEYPAD_KEY3,
    KEYPAD_KEY4,
    KEYPAD_KEY12,
    KEYPAD_KEY23,
    KEYPAD_KEY34,
    KEYPAD_KEY14,
    KEYPAD_KEY13,
    KEYPAD_KEY24,
    KEYPAD_KEY123,
    KEYPAD_KEY124,
    KEYPAD_KEY134,
    KEYPAD_KEY234,
    KEYPAD_KEY1234
} KEYPAD_KEY_t;

typedef void (*KEYPAD_Callback_t)(KEYPAD_KEY_t key);

void KEYPAD_Init(void);
void KEYPAD_Loop(void);
void KEYPAD_KeyDown(KEYPAD_Callback_t KeyDown);
void KEYPAD_KeyUp(KEYPAD_Callback_t KeyUp);
void KEYPAD_KeyLongPress(KEYPAD_Callback_t KeyLongPress);

#endif // KEYPAD_H_INCLUDED
