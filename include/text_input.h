#ifndef TEXT_INPUT_H
#define TEXT_INPUT_H
#include <gbdk/platform.h>
#include "keyboard.h"

#define TEXT_INPUT_MAX_LENGTH 63u
#define TEXT_INPUT_NUMERIC 1u
#define TEXT_INPUT_ALPHANUMERIC 2u
#define TEXT_INPUT_UPPERCASE 4u
#define TEXT_INPUT_PASSWORD 8u
#define TEXT_INPUT_OSK 16u
#define TEXT_INPUT_BACKGROUND_TEXT 32u
#define TEXT_INPUT_ACTIVE 0u
#define TEXT_INPUT_CONFIRMED 1u
#define TEXT_INPUT_CANCELLED 2u

typedef struct text_input_t {
    UBYTE text[TEXT_INPUT_MAX_LENGTH + 1u];
    UBYTE length, cursor, maximum, flags;
} text_input_t;

typedef struct text_osk_t {
    UBYTE selected, page, uppercase;
} text_osk_t;

void text_input_begin(text_input_t * input, const UWORD * source, UWORD capacity, UBYTE maximum, UBYTE flags) BANKED;
UBYTE text_input_key(text_input_t * input, UWORD key, UBYTE character) BANKED;
void text_input_commit(const text_input_t * input, UWORD * destination, UWORD capacity) BANKED;
void text_array_format(const UWORD * source, UWORD capacity, UWORD * offset, UWORD * more, UBYTE * out) BANKED;
UBYTE text_osk_character(const text_input_t * input, const text_osk_t * osk, UBYTE key) BANKED;
void text_osk_move(const text_input_t * input, text_osk_t * osk, INT8 dx, INT8 dy) BANKED;
UBYTE text_osk_press(text_input_t * input, text_osk_t * osk) BANKED;
#endif
