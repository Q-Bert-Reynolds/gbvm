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

UBYTE text_input_key(text_input_t * input, text_input_key_t key, UBYTE character) BANKED {
    switch (key) {
        case TEXT_KEY_NONE: break;
        case TEXT_KEY_CONFIRM: return TEXT_INPUT_CONFIRMED;
        case TEXT_KEY_CANCEL: return TEXT_INPUT_CANCELLED;
        case TEXT_KEY_LEFT: if (input->cursor) input->cursor--; return TEXT_INPUT_ACTIVE;
        case TEXT_KEY_RIGHT: if (input->cursor < input->length) input->cursor++; return TEXT_INPUT_ACTIVE;
        case TEXT_KEY_HOME: input->cursor = 0; return TEXT_INPUT_ACTIVE;
        case TEXT_KEY_END: input->cursor = input->length; return TEXT_INPUT_ACTIVE;
        case TEXT_KEY_BACKSPACE:
            if (!input->cursor) return TEXT_INPUT_ACTIVE;
            input->cursor--;
            // fall through
        case TEXT_KEY_DELETE:
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

static const UBYTE osk_keys[3][TEXT_OSK_CHARACTER_KEYS + 1u] = {
    "1234567890qwertyuiopasdfghjkl;zxcvbnm,./",
    "!@#$%^&*()QWERTYUIOPASDFGHJKL:ZXCVBNM<>?",
    "`~-_=+[]{}\\|'\"!?@#$%^&*()<>:;/1234567890"
};

UBYTE text_osk_character(const text_input_t * input, const text_osk_t * osk, UBYTE key) BANKED {
    UBYTE ch;
    if (key >= TEXT_OSK_CHARACTER_KEYS) return 0;
    if (input->flags & TEXT_INPUT_NUMERIC) ch = key < 10u ? osk_keys[0][key] : 0;
    else ch = osk_keys[osk->page ? 2u : (osk->uppercase ? 1u : 0u)][key];
    if (!accepted(ch, input->flags)) return 0;
    if ((input->flags & TEXT_INPUT_UPPERCASE) && ch >= 'a' && ch <= 'z') ch -= 'a' - 'A';
    return ch;
}

void text_osk_move(const text_input_t * input, text_osk_t * osk, INT8 dx, INT8 dy) BANKED {
    static const UBYTE action_columns[TEXT_OSK_ACTION_KEYS] = {0, 1, 2, 4, 7, 8, 9};
    UBYTE row = osk->selected < TEXT_OSK_CHARACTER_KEYS ? osk->selected / TEXT_OSK_COLUMNS : 4u;
    UBYTE col = row == 4u ? osk->selected - TEXT_OSK_CHARACTER_KEYS : osk->selected % TEXT_OSK_COLUMNS;
    UBYTE candidate, i, width;
    if (dx) {
        width = row == 4u ? TEXT_OSK_ACTION_KEYS : TEXT_OSK_COLUMNS;
        for (i = 0; i < width; i++) {
            col = dx < 0 ? (col ? col - 1u : width - 1u) : (col + 1u) % width;
            candidate = row == 4u ? TEXT_OSK_CHARACTER_KEYS + col : row * TEXT_OSK_COLUMNS + col;
            if (row == 4u || text_osk_character(input, osk, candidate)) { osk->selected = candidate; return; }
        }
    }
    if (dy) {
        if (row == 4u) col = action_columns[col];
        for (i = 0; i < 5u; i++) {
            row = dy < 0 ? (row ? row - 1u : 4u) : (row + 1u) % 5u;
            if (row == 4u) {
                osk->selected = TEXT_OSK_CHARACTER_KEYS + (col < 3u ? col : (col < 7u ? 3u : col - 3u));
                return;
            }
            candidate = row * TEXT_OSK_COLUMNS + col;
            while (!text_osk_character(input, osk, candidate) && candidate > row * TEXT_OSK_COLUMNS) candidate--;
            if (text_osk_character(input, osk, candidate)) { osk->selected = candidate; return; }
        }
    }
}

UBYTE text_osk_press(text_input_t * input, text_osk_t * osk) BANKED {
    if (osk->selected < TEXT_OSK_CHARACTER_KEYS) return text_input_key(input, TEXT_KEY_NONE, text_osk_character(input, osk, osk->selected));
    switch (osk->selected) {
        case TEXT_OSK_SHIFT:
            if (!(input->flags & TEXT_INPUT_UPPERCASE)) osk->uppercase = !osk->uppercase;
            break;
        case TEXT_OSK_SYMBOLS:
            if (!(input->flags & (TEXT_INPUT_NUMERIC | TEXT_INPUT_ALPHANUMERIC))) osk->page = !osk->page;
            break;
        case TEXT_OSK_CANCEL: return TEXT_INPUT_CANCELLED;
        case TEXT_OSK_SPACE: return text_input_key(input, TEXT_KEY_NONE, ' ');
        case TEXT_OSK_LEFT: return text_input_key(input, TEXT_KEY_LEFT, 0);
        case TEXT_OSK_RIGHT: return text_input_key(input, TEXT_KEY_RIGHT, 0);
        case TEXT_OSK_CONFIRM: return TEXT_INPUT_CONFIRMED;
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
