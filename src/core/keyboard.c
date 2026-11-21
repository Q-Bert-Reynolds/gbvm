#pragma bank 255
#include <string.h>
#include "keyboard.h"
#include "sio.h"

UBYTE keyboard_modifiers, keyboard_errors;

volatile UBYTE kb_raw[KB_RAW_SIZE], kb_raw_errors[KB_RAW_SIZE];
volatile UBYTE kb_raw_read, kb_raw_write, kb_raw_fault;
static UBYTE held[64];
static keyboard_event_t events[16];
static UBYTE characters[32];
static UBYTE event_read, event_write, char_read, char_write;
static UBYTE prefix, pause_skip;

static const UBYTE ascii_normal[128] =
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\t`\0"
    "\0\0\0\0\0q1\0\0\0zsaw2\0"
    "\0cxde43\0\0 vftr5\0"
    "\0nbhgy6\0\0\0mju78\0"
    "\0,kio09\0\0./l;p-\0"
    "\0\0'\0[=\0\0\0\0\n]\0\\\0\0"
    "\0\0\0\0\0\0\b\0\0001\00047\0\0\0" 
    "0.2568\033\0\0+3-*9\0";

static const UBYTE ascii_shifted[96] =
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\t~\0"
    "\0\0\0\0\0Q!\0\0\0ZSAW@\0"
    "\0CXDE$#\0\0 VFTR%\0"
    "\0NBHGY^\0\0\0MJU&*\0"
    "\0<KIO)(\0\0>?L:P_\0"
    "\0\0\"\0{+\0\0\0\0\n}\0|\0";

static UBYTE is_held(UWORD key) {
    return key < 512u && (held[key >> 3] & (1u << (key & 7u))) != 0;
}

static void reset_decoder(void) {
    memset(held, 0, sizeof(held));
    keyboard_modifiers = prefix = pause_skip = 0;
    event_read = event_write = char_read = char_write = 0;
}

static UBYTE character_for(UWORD key) {
    UBYTE code = (UBYTE)key, ch;
    if (key & 0x100u) {
        if (code == 0x5a) return '\n';
        if (code == 0x4a) return '/';
        if (code == 0x71) return 127;
        return 0;
    }
    if (code == 0x66) return 8;
    if (code == 0x76) return 27;
    if (code >= 128) return 0;
    if (code >= 0x60 && code != 0x79 && code != 0x7b && code != 0x7c &&
        !(keyboard_modifiers & KB_MOD_NUM)) return 0;
    ch = ascii_normal[code];
    if (ch >= 'a' && ch <= 'z') {
        if (!!(keyboard_modifiers & KB_MOD_SHIFT) != !!(keyboard_modifiers & KB_MOD_CAPS))
            ch -= 'a' - 'A';
    } else if ((keyboard_modifiers & KB_MOD_SHIFT) && code < 0x60) ch = ascii_shifted[code];
    return ch;
}

static void decode(UBYTE code) {
    UWORD key;
    UBYTE down, repeat, mask, next, character;
    keyboard_event_t * event;
    if (pause_skip) { --pause_skip; return; }
    if (code == 0xe1) { pause_skip = 7; prefix = 0; return; }
    if (code == 0xe0) { prefix |= 2; return; }
    if (code == 0xf0) { prefix |= 1; return; }
    if (code == 0xfa) { prefix = 0; return; }
    if (!code || code == 0xff || code > 0x83) {
        keyboard_errors |= (!code || code == 0xff) ? KB_ERROR_DEVICE : KB_ERROR_CODE;
        reset_decoder();
        return;
    }
    key = code | ((prefix & 2u) ? 0x100u : 0);
    down = !(prefix & 1u);
    prefix = 0;
    if (key == 0x112 || key == 0x159) return; //Print Screen's fake extended Shift bytes must not alter real Shift.
    repeat = is_held(key);
    mask = 1u << (key & 7u);
    if (down) held[key >> 3] |= mask;
    else held[key >> 3] &= ~mask;
    if (down && !repeat) {
        if (key == 0x58) keyboard_modifiers ^= KB_MOD_CAPS;
        if (key == 0x77) keyboard_modifiers ^= KB_MOD_NUM;
        if (key == 0x7e) keyboard_modifiers ^= KB_MOD_SCROLL;
    }
    keyboard_modifiers &= KB_MOD_CAPS | KB_MOD_NUM | KB_MOD_SCROLL;
    if (is_held(0x12) || is_held(0x59)) keyboard_modifiers |= KB_MOD_SHIFT;
    if (is_held(0x14) || is_held(0x114)) keyboard_modifiers |= KB_MOD_CTRL;
    if (is_held(0x11) || is_held(0x111)) keyboard_modifiers |= KB_MOD_ALT;
    if (is_held(0x11f) || is_held(0x127)) keyboard_modifiers |= KB_MOD_SUPER;
    character = down ? character_for(key) : 0;
    next = (event_write + 1u) & 15u;
    if (next != event_read) {
        event = &events[event_write];
        event->key = key;
        event->flags = KB_EVENT_VALID | (down ? KB_EVENT_DOWN : 0) |
                       ((down && repeat) ? KB_EVENT_REPEAT : 0);
        event->modifiers = keyboard_modifiers;
        event->character = character;
        event_write = next;
    } else keyboard_errors |= KB_ERROR_OVERFLOW;
    if (character) {
        next = (char_write + 1u) & 31u;
        if (next != char_read) { characters[char_write] = character; char_write = next; }
        else keyboard_errors |= KB_ERROR_OVERFLOW;
    }
}

// interrupts must be disabled when called
static void clear_locked(void) {
    if (keyboard_enabled) SC_REG = 0;
    kb_raw_read = kb_raw_write = kb_raw_fault = 0;
    keyboard_errors = 0;
    reset_decoder();
    if (keyboard_enabled) {
        IF_REG &= ~SIO_IFLAG;
        SB_REG = 0;
        SC_REG = 0x80;
    }
}

void keyboard_clear(void) BANKED {
    CRITICAL {
        clear_locked();
    }
}

UBYTE keyboard_set_enabled(UBYTE enabled) BANKED {
    if (!!enabled == !!keyboard_enabled) return TRUE;
    if (enabled && link_operation_mode != LINK_MODE_NONE) {
        keyboard_errors |= KB_ERROR_PORT_BUSY;
        return FALSE;
    }
    CRITICAL {
        SC_REG = 0;
        keyboard_enabled = FALSE;
        IF_REG &= ~SIO_IFLAG;
        clear_locked();
        SIO_init();
        SIO_status = IO_IDLE;
        link_byte_sent = FALSE;
        link_next_mode = IO_IDLE;
        keyboard_enabled = !!enabled;
        /* Match GBDK .DT_IDLE when returning the port to the link driver. */
        SB_REG = keyboard_enabled ? 0 : 0x66;
        SC_REG = 0x80;
        IE_REG |= SIO_IFLAG;
    }
    return TRUE;
}

void keyboard_init(void) BANKED {
    keyboard_enabled = keyboard_modifiers = keyboard_errors = 0;
    keyboard_set_enabled(TRUE);
}

void keyboard_update(void) BANKED {
    UBYTE code = 0, error = 0, fault = 0, available;
    if (!keyboard_enabled) return;
    do {
        available = FALSE;
        CRITICAL {
            fault = kb_raw_fault;
            kb_raw_fault = 0;
            if (fault) kb_raw_read = kb_raw_write;
            if (kb_raw_read != kb_raw_write) {
                code = kb_raw[kb_raw_read];
                error = kb_raw_errors[kb_raw_read];
                kb_raw_read = (kb_raw_read + 1u) & (KB_RAW_SIZE - 1u);
                available = TRUE;
            }
        }
        if (fault) { keyboard_errors |= fault; reset_decoder(); }
        if (available) {
            if (error) { keyboard_errors |= error; reset_decoder(); }
            else decode(code);
        }
    } while (available);
}

UBYTE keyboard_read_event(keyboard_event_t * event) BANKED {
    memset(event, 0, sizeof(*event));
    if (event_read != event_write) {
        *event = events[event_read];
        event_read = (event_read + 1u) & 15u;
        return TRUE;
    }
    return FALSE;
}

UBYTE keyboard_read_character(void) BANKED {
    UBYTE ch;
    if (char_read != char_write) {
        ch = characters[char_read];
        char_read = (char_read + 1u) & 31u;
        return ch;
    }
    return 0;
}

UBYTE keyboard_key_held(UWORD key) BANKED {
    return is_held(key);
}
