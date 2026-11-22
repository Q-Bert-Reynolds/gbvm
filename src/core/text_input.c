#pragma bank 255
#include "text_input.h"
#ifdef PS2_KEYBOARD
#include <string.h>

static UBYTE accepted(UBYTE ch, UBYTE flags) {
    if (ch < 32u || ch > 126u) return FALSE;
    if (flags & TEXT_INPUT_NUMERIC) return ch >= '0' && ch <= '9';
    if (flags & TEXT_INPUT_ALPHANUMERIC)
        return (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
    return TRUE;
}

void text_input_begin(text_input_t * input, const UWORD * source, UWORD capacity, UBYTE maximum, UBYTE flags) BANKED {
    UWORD i;
    memset(input, 0, sizeof(*input));
    input->flags = flags;
    input->maximum = maximum > TEXT_INPUT_MAX_LENGTH ? TEXT_INPUT_MAX_LENGTH : maximum;
    if (!capacity) { input->maximum = 0; return; }
    if (input->maximum >= capacity) input->maximum = capacity - 1u;
    for (i = 0; i < capacity && source[i] && input->length < input->maximum; i++) {
        UWORD ch = source[i];
        if (ch > 126u || !accepted(ch, flags)) continue;
        if ((flags & TEXT_INPUT_UPPERCASE) && ch >= 'a' && ch <= 'z') ch -= 'a' - 'A';
        input->text[input->length++] = ch;
    }
    input->cursor = input->length;
}

UBYTE text_input_key(text_input_t * input, UWORD key, UBYTE character) BANKED {
    switch (key) {
        case 0x5a: case 0x15a: return TEXT_INPUT_CONFIRMED;
        case 0x76: return TEXT_INPUT_CANCELLED;
        case 0x16b: if (input->cursor) input->cursor--; return TEXT_INPUT_ACTIVE;
        case 0x174: if (input->cursor < input->length) input->cursor++; return TEXT_INPUT_ACTIVE;
        case 0x16c: input->cursor = 0; return TEXT_INPUT_ACTIVE;
        case 0x169: input->cursor = input->length; return TEXT_INPUT_ACTIVE;
        case 0x66:
            if (!input->cursor) return TEXT_INPUT_ACTIVE;
            input->cursor--;
            // Fall through to delete the character at the cursor.
        case 0x171:
            if (input->cursor < input->length) {
                memmove(input->text + input->cursor, input->text + input->cursor + 1u, input->length - input->cursor);
                input->length--;
            }
            return TEXT_INPUT_ACTIVE;
    }
    if (input->length == input->maximum || !accepted(character, input->flags)) return TEXT_INPUT_ACTIVE;
    if ((input->flags & TEXT_INPUT_UPPERCASE) && character >= 'a' && character <= 'z') character -= 'a' - 'A';
    memmove(input->text + input->cursor + 1u, input->text + input->cursor, input->length - input->cursor + 1u);
    input->text[input->cursor++] = character;
    input->length++;
    return TEXT_INPUT_ACTIVE;
}

void text_input_commit(const text_input_t * input, UWORD * destination, UWORD capacity) BANKED {
    UWORD i;
    for (i = 0; i < capacity; i++) destination[i] = i < input->length ? input->text[i] : 0;
}
#endif

void text_array_format(const UWORD * source, UWORD capacity, UWORD * offset, UWORD * more, UBYTE * out) BANKED {
    UBYTE i, count = 0, column = 0;
    if (capacity > 64u) capacity = 64u;
    // At most 36 glyphs per page fit the monochrome renderer's 52 text tiles.
    for (i = *offset < 63u ? *offset : 63u; i < capacity && i < 63u && source[i] && count < 36u; i++) {
        if (column == 18u) { *out++ = '\n'; column = 0; }
        *out++ = (source[i] >= 32u && source[i] <= 126u) ? source[i] : '?';
        column++;
        count++;
    }
    *out = 0;
    *offset = i;
    *more = i < capacity && i < 63u && source[i] != 0;
}
