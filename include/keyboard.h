#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <gbdk/platform.h>
#include "compat.h"

#define KB_ERROR_STOP       0x01u
#define KB_ERROR_PARITY     0x02u
#define KB_ERROR_START      0x04u
#define KB_ERROR_DEVICE     0x08u
#define KB_ERROR_CODE       0x10u
#define KB_ERROR_TIMEOUT    0x20u
#define KB_ERROR_PORT_BUSY  0x40u
#define KB_ERROR_OVERFLOW   0x80u

#define KB_MOD_SCROLL 0x01u
#define KB_MOD_NUM    0x02u
#define KB_MOD_CAPS   0x04u
#define KB_MOD_SUPER  0x08u
#define KB_MOD_ALT    0x10u
#define KB_MOD_CTRL   0x20u
#define KB_MOD_SHIFT  0x40u

#define KB_EVENT_DOWN   0x01u
#define KB_EVENT_REPEAT 0x02u
#define KB_EVENT_VALID  0x80u
#define KB_RAW_SIZE     32u

typedef struct keyboard_event_t {
    UWORD key; /* Set 2 scan code; bit 8 marks an E0 extended key. */
    UBYTE flags;
    UBYTE modifiers;
    UBYTE character; /* ASCII on keydown, or zero for a non-character key. */
} keyboard_event_t;

extern volatile UBYTE keyboard_enabled;
extern UBYTE keyboard_modifiers, keyboard_errors;
extern volatile UBYTE kb_raw[KB_RAW_SIZE], kb_raw_errors[KB_RAW_SIZE];
extern volatile UBYTE kb_raw_read, kb_raw_write, kb_raw_fault;

void keyboard_init(void) BANKED;
UBYTE keyboard_set_enabled(UBYTE enabled) BANKED;
void keyboard_clear(void) BANKED;
void keyboard_update(void) BANKED;
void keyboard_begin_frame(void) BANKED;
UBYTE keyboard_read_event(keyboard_event_t * event) BANKED;
UBYTE keyboard_read_character(void) BANKED;
UBYTE keyboard_key_held(UWORD key) BANKED;
UBYTE keyboard_key_pressed(UWORD key) BANKED;
void keyboard_serial_receive(void) NONBANKED NAKED;

#endif
