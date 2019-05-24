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
#ifndef DISPLAY_H_INCLUDED
#define DISPLAY_H_INCLUDED

#define DISPLAY_HEIGHT  48
#define DISPLAY_WIDTH  84
#define DISPLAY_BYTE_HEIGHT  6
#define DISPLAY_BYTE_WIDTH  84

#define DISPLAY_BLACK  0xFF
#define DISPLAY_GRAY  0x55
#define DISPLAY_WHITE  0x00

typedef enum {
    DISPLAY_MODE_RAW,
    DISPLAY_MODE_USART,
    DISPLAY_MODE_EDMA,
} DISPLAY_MODE_t;

typedef enum {
    DISPLAY_BACKLIGHT_OFF,
    DISPLAY_BACKLIGHT_ON,
    DISPLAY_BACKLIGHT_TOGGLE,
    DISPLAY_BACKLIGHT_MAIN,
    DISPLAY_BACKLIGHT_AUX,
} DISPLAY_BACKLIGHT_t;

void DISPLAY_Init(void);
void DISPLAY_Mode(DISPLAY_MODE_t mode);
void DISPLAY_Clear(void);
void DISPLAY_Idle(void);
void DISPLAY_Send(void);
void DISPLAY_Loop(void);
void DISPLAY_CursorPosition(uint8_t x, uint8_t y);
void DISPLAY_MoveCursor(uint8_t offset);
void DISPLAY_PrintChar(uint8_t ch);
void DISPLAY_InvertLine(uint8_t top);
void DISPLAY_SelectLine(void);
void DISPLAY_Select(uint8_t top);
void DISPLAY_ProgressBar(uint8_t length);
void DISPLAY_ChartBar(int16_t value, uint8_t column, uint8_t pattern);
void DISPLAY_ClearBar(uint8_t column);
void DISPLAY_Image(const __flash uint8_t* image);
void DISPLAY_Icon(const __flash uint8_t* icon);
void DISPLAY_Icons(const __flash uint8_t* const __flash icons[]);
void DISPLAY_Backlight(DISPLAY_BACKLIGHT_t backlight);
void DISPLAY_Settings(void);
uint8_t DISPLAY_Update(void);

#endif // DISPLAY_H_INCLUDED
