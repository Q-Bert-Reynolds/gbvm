#pragma bank 255
#include "text_input.h"
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
        case PS2_KEY_ENTER: 
        case PS2_KEY_NUMPAD_ENTER: return TEXT_INPUT_CONFIRMED;
        case PS2_KEY_ESCAPE: return TEXT_INPUT_CANCELLED;
        case PS2_KEY_LEFT: if (input->cursor) input->cursor--; return TEXT_INPUT_ACTIVE;
        case PS2_KEY_RIGHT: if (input->cursor < input->length) input->cursor++; return TEXT_INPUT_ACTIVE;
        case PS2_KEY_HOME: input->cursor = 0; return TEXT_INPUT_ACTIVE;
        case PS2_KEY_END: input->cursor = input->length; return TEXT_INPUT_ACTIVE;
        case PS2_KEY_BACKSPACE:
            if (!input->cursor) return TEXT_INPUT_ACTIVE;
            input->cursor--;
            // Fall through to delete the character at the cursor.
        case PS2_KEY_DELETE:
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

static const UBYTE osk_keys[3][31] = {
    "qwertyuiopasdfghjkl'zxcvbnm .-",
    "1234567890!@#$%^&*()_+-=[]{}:;",
    "`~\\|/<>?,.\"'!@#$%^&*()_+-=[]{}"
};

UBYTE text_osk_character(const text_input_t * input, const text_osk_t * osk, UBYTE key) BANKED {
    UBYTE ch;
    if (key >= 30u) return 0;
    if (input->flags & TEXT_INPUT_NUMERIC) ch = key < 10u ? osk_keys[1][key] : 0;
    else ch = osk_keys[osk->page][key];
    if (!accepted(ch, input->flags)) return 0;
    if ((osk->uppercase || (input->flags & TEXT_INPUT_UPPERCASE)) && ch >= 'a' && ch <= 'z') ch -= 'a' - 'A';
    return ch;
}

void text_osk_move(const text_input_t * input, text_osk_t * osk, INT8 dx, INT8 dy) BANKED {
    UBYTE row = osk->selected / 10u, col = osk->selected % 10u, candidate, i, width;
    if (dx) {
        width = row == 3u ? 6u : 10u;
        for (i = 0; i < width; i++) {
            col = dx < 0 ? (col ? col - 1u : width - 1u) : (col + 1u) % width;
            candidate = row * 10u + col;
            if (row == 3u || text_osk_character(input, osk, candidate)) { osk->selected = candidate; return; }
        }
    }
    if (dy) {
        if (row == 3u) col = col * 2u > 9u ? 9u : col * 2u;
        for (i = 0; i < 4u; i++) {
            row = dy < 0 ? (row ? row - 1u : 3u) : (row + 1u) % 4u;
            if (row == 3u) { osk->selected = 30u + (col * 5u / 9u); return; }
            candidate = row * 10u + col;
            // Short filtered rows use the nearest available key to the left.
            while (!text_osk_character(input, osk, candidate) && candidate > row * 10u) candidate--;
            if (text_osk_character(input, osk, candidate)) { osk->selected = candidate; return; }
        }
    }
}

UBYTE text_osk_press(text_input_t * input, text_osk_t * osk) BANKED {
    UBYTE pages;
    if (osk->selected < 30u) return text_input_key(input, 0, text_osk_character(input, osk, osk->selected));
    switch (osk->selected) {
        case 30:
            if (!(input->flags & TEXT_INPUT_UPPERCASE)) osk->uppercase = !osk->uppercase;
            break;
        case 31:
            pages = (input->flags & TEXT_INPUT_NUMERIC) ? 1 : ((input->flags & TEXT_INPUT_ALPHANUMERIC) ? 2 : 3);
            osk->page = (osk->page + 1u) % pages;
            break;
        case 32: return text_input_key(input, PS2_KEY_LEFT, 0);
        case 33: return text_input_key(input, PS2_KEY_RIGHT, 0);
        case 34: return TEXT_INPUT_CONFIRMED;
        case 35: return TEXT_INPUT_CANCELLED;
    }
    return TEXT_INPUT_ACTIVE;
}

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
