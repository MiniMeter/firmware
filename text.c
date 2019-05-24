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
#include "font.h"
#include "text.h"

/* COMMON */
const __flash char TEXT_SETTINGS[] = "SETTINGS";
const __flash char TEXT_INPUT[] = "INPUT";
const __flash char TEXT_INFO[] = "INFO";
const __flash char TEXT_SPACE[] = "SPACE";
const __flash char TEXT_SLOWER[] = "SLOWER";
const __flash char TEXT_FASTER[] = "FASTER";
const __flash char TEXT_TIMER[] = "TIMER";
const __flash char TEXT_SCALE[] = "SCALE";
const __flash char TEXT_START_STOP[] = "START/STOP";
const __flash char TEXT_ACK_NACK[] = "ACK/NACK";
const __flash char TEXT_HOLD_RUN[] = "HOLD/RUN";
const __flash char TEXT_HEX_ASCII[] = "HEX/ASCII";
const __flash char TEXT_CLEAR[] = "CLEAR";
const __flash char TEXT_READY[] = "READY";
const __flash char TEXT_OVERLOAD[] = "%cOVL";
const __flash char TEXT_FALLING_EDGE[] = "F.EDGE";
const __flash char TEXT_RISING_EDGE[] = "R.EDGE";
const __flash char TEXT_LSB_MSB[] = "LSB-MSB";
const __flash char TEXT_MSB_LSB[] = "MSB-LSB";
const __flash char TEXT_LOW[] = "LOW";
const __flash char TEXT_HIGH[] = "HIGH";
const __flash char TEXT_NO[] = "NO";
const __flash char TEXT_YES[] = "YES";
const __flash char TEXT_OFF[] = "OFF";
const __flash char TEXT_ON[] = "ON";
const __flash char TEXT_ODD[] = "ODD";
const __flash char TEXT_EVEN[] = "EVEN";
/* MAIN */
const __flash char TEXT_PRESS_AND_HOLD[] = "PRESS & HOLD";
const __flash char TEXT_1_I2C_TWI[] = "1.I2C/TWI";
const __flash char TEXT_2_VOLTAGE[] = "2.VOLTAGE";
const __flash char TEXT_3_CURRENT[] = "3.CURRENT";
const __flash char TEXT_4_UART[] = "4.UART";
const __flash char TEXT_12_1WIRE[] = "1+2.1-WIRE";
const __flash char TEXT_23_SPI[] = "2+3.SPI";
const __flash char TEXT_34_USRT[] = "3+4.USRT";
const __flash char TEXT_14_IRCOM[] = "1+4.IRCOM";
const __flash char TEXT_13_FREQ[] = "1+3.FREQ.";
const __flash char TEXT_24_CHARGE[] = "2+4.CHARGE";
const __flash char TEXT_123_DISPLAY[] = "1+2+3.DISPLAY";
const __flash char TEXT_234_DELAY[] = "2+3+4.DELAY";
const __flash char TEXT_CALIBRATION[] = "CALIBRATION";
const __flash char TEXT_134_VOLTAGE[] = "1+3+4.VOLTAGE";
const __flash char TEXT_124_CURRENT[] = "1+2+4.CURRENT";
/* SPI */
const __flash char TEXT_SPI_SETTINGS[] = "SPI SETTINGS";
const __flash char TEXT_DATA[] = "DATA: %S";
const __flash char TEXT_CLOCK[] = "CLOCK: %S";
const __flash char TEXT_SELECT[] = "SELECT: %S";
/* USART */
const __flash char TEXT_UART_SETTINGS[] = "UART SETTINGS";
const __flash char TEXT_USRT_SETTINGS[] = "USRT SETTINGS";
const __flash char TEXT_BAUD[] = "BAUD: %lu";
const __flash char TEXT_FRAME[] = "FRAME: %u BIT";
const __flash char TEXT_PARITY[] = "PARITY: %S";
/* IRCOM */
const __flash char TEXT_IRCOM_SETTINGS[] = "IRCOM SETTINGS";
const __flash char TEXT_INVERT[] = "INVERT:";
/* 1-WIRE */
const __flash char TEXT_1WIRE_INFO[] = "1-WIRE INFO";
const __flash char TEXT_RESET[] = "RESET (R)";
const __flash char TEXT_RESPONSE[] = "RESPONSE:";
const __flash char TEXT_YES_NO[] = "YES(+) NO(-)";
/* DELAY */
const __flash char TEXT_DELAY_SETUP[] = "DELAY SETUP";
const __flash char TEXT_DELAY_INFO[] = "INFO: %ds";
const __flash char TEXT_DELAY_IDLE[] = "IDLE: %ds";
/* VOLTAGE */
const __flash char TEXT_VOLTAGE_UNIT[] = "V";
const __flash char TEXT_VOLTAGE_VALUE[] = "%c%u.%02u";
/* CURRENT */
const __flash char TEXT_CURRENT_UNIT[] = "mA";
const __flash char TEXT_CURRENT_VALUE[] = "%4d";
/* DISPLAY */
const __flash char TEXT_DISPLAY_SETUP[] = "DISPLAY SETUP";
const __flash char TEXT_CONTRAST[] = "CONTRAST: %2d";
const __flash char TEXT_REFRESH[] = "REFRESH: %2d";
const __flash char TEXT_BACKLIGHT[] = "BACKLIGHT:";
/* ANALOG */
const __flash char TEXT_ADC_SCALE[] = "%3u";
const __flash char TEXT_ADC_CALIBRATION[] = "ADC CALIB.";
const __flash char TEXT_ADC_VALUE[] = "VALUE:";
const __flash char TEXT_ADC_OFFSET[] = "OFFSET:%6d";
const __flash char TEXT_ADC_GAIN[] = "GAIN:%2d.%03d";
/* FREQ */
const __flash char TEXT_FREQ_nnn[] = "%5u";
const __flash char TEXT_FREQ_dnn[] = "%2u.%02u";
const __flash char TEXT_FREQ_ddn[] = "%3u.%u";
const __flash char TEXT_FREQ_Hz[] = "Hz";
const __flash char TEXT_FREQ_kHz[] = "kHz";
const __flash char TEXT_FREQ_MHz[] = "MHz";
/* CHARGE */
const __flash char TEXT_CHARGE_muu[] = "%c%u.%02u";
const __flash char TEXT_CHARGE_mmu[] = "%c%2u.%u";
const __flash char TEXT_CHARGE_mmm[] = "%c%4u";
const __flash char TEXT_CHARGE_UNIT[] = "mAh";
const __flash char TEXT_CHARGE_TIME[] = "%02u:%02u:%02u";
/* INFO */
const __flash char TEXT_INFO_FIRMWARE[] = "FIRMWARE:";
const __flash char TEXT_INFO_VERSION[] = "F1 v1.0";
const __flash char TEXT_INFO_RAM[] = "RAM USAGE:";
const __flash char TEXT_INFO_FLASH[] = "FLASH USAGE:";
const __flash char TEXT_INFO_EEPROM[] = "EEPROM USAGE:";
const __flash char TEXT_INFO_USAGE[] = "%u";
const __flash char TEXT_INFO_PERCENT[] = "B (%u%%)";
/* DESCRIPTION */
const __flash char TEXT_DESC_START[] = "< START";
const __flash char TEXT_DESC_STOP[] = "> STOP";
const __flash char TEXT_PARITY_ERR[] = "\x80 PARITY ERR.";
const __flash char TEXT_FRAME_ERR[] = "\x81 FRAME ERR.";
const __flash char TEXT_OVERFLOW[] = "\x82 OVERFLOW";
const __flash char TEXT_DATA_ERR[] = "\x83 DATA ERR.";
const __flash char TEXT_FREQ_ERR[] = "\x84 FREQ. ERR.";
