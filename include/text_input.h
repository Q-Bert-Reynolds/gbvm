#ifndef TEXT_INPUT_H
#define TEXT_INPUT_H
#include <gbdk/platform.h>

#define TEXT_INPUT_MAX_LENGTH 63u
#define TEXT_INPUT_NUMERIC 1u
#define TEXT_INPUT_ALPHANUMERIC 2u
#define TEXT_INPUT_UPPERCASE 4u
#define TEXT_INPUT_PASSWORD 8u
#define TEXT_INPUT_ACTIVE 0u
#define TEXT_INPUT_CONFIRMED 1u
#define TEXT_INPUT_CANCELLED 2u

typedef struct text_input_t {
    UBYTE text[TEXT_INPUT_MAX_LENGTH + 1u];
    UBYTE length, cursor, maximum, flags;
} text_input_t;

void text_input_begin(text_input_t * input, const UWORD * source, UWORD capacity, UBYTE maximum, UBYTE flags) BANKED;
UBYTE text_input_key(text_input_t * input, UWORD key, UBYTE character) BANKED;
void text_input_commit(const text_input_t * input, UWORD * destination, UWORD capacity) BANKED;
void text_array_format(const UWORD * source, UWORD capacity, UWORD * offset, UWORD * more, UBYTE * out) BANKED;
#endif
