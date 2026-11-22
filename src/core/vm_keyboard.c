#pragma bank 255
#include "vm_keyboard.h"
#include "input.h"
#ifdef PS2_KEYBOARD
#include "keyboard.h"
#include "events.h"
#include "text_input.h"
#include "ui.h"
#include "vm_ui.h"

static text_input_t text_entry;

static void discard_text_input(void) {
    keyboard_event_t event;
    while (keyboard_read_event(&event)) {}
    while (keyboard_read_character()) {}
}

static void draw_text_input(SCRIPT_CTX * THIS) {
    UBYTE * out = ui_text_data;
    UBYTE first = text_entry.cursor > 15u ? text_entry.cursor - 15u : 0;
    UBYTE end = first + 16u, i;
    vm_overlay_clear(THIS, 1, 3, 18, 1, 1, 0);
    *out++ = 1; *out++ = 1; // Instant text.
    *out++ = 3; *out++ = 2; *out++ = 4; // Entry row below the prompt.
    for (i = first; i <= text_entry.length && i <= end; i++) {
        if (i == text_entry.cursor) *out++ = '_';
        if (i < text_entry.length && i < end)
            *out++ = (text_entry.flags & TEXT_INPUT_PASSWORD) ? '*' : text_entry.text[i];
    }
    *out = 0;
    // Prompt uses the first 20 tiles; editing reuses only the remaining tiles.
    vm_display_text(THIS, 0, 20);
}
#endif

UBYTE keyboard_text_input(void * context, UBYTE start, UWORD * stack_frame) OLDCALL BANKED {
    SCRIPT_CTX * THIS = context;
    UWORD * result = VM_REF_TO_PTR((INT16)stack_frame[4]);
#ifdef PS2_KEYBOARD
    UWORD * destination = VM_REF_TO_PTR((INT16)stack_frame[0]);
    keyboard_event_t event;
    UBYTE status, dirty = FALSE;
    if (!keyboard_enabled) { *result = 2; return TRUE; }
    if (start) {
        text_input_begin(&text_entry, destination, stack_frame[1], stack_frame[2], stack_frame[3]);
        discard_text_input();
        draw_text_input(THIS);
    } else if (text_drawn) {
        while (keyboard_read_event(&event)) {
            if (!(event.flags & KB_EVENT_DOWN)) continue;
            dirty = TRUE;
            status = text_input_key(&text_entry, event.key, event.character);
            if (status != TEXT_INPUT_ACTIVE) {
                if (status == TEXT_INPUT_CONFIRMED) text_input_commit(&text_entry, destination, stack_frame[1]);
                *result = status == TEXT_INPUT_CONFIRMED ? 1 : 0;
                discard_text_input();
                return TRUE;
            }
        }
        while (keyboard_read_character()) {}
        if (dirty) draw_text_input(THIS);
    }
    THIS->waitable = TRUE;
    return FALSE;
#else
    start;
    *result = 2;
    return TRUE;
#endif
}

UBYTE keyboard_text_wait(void * context, UBYTE start, UWORD * stack_frame) OLDCALL BANKED {
    SCRIPT_CTX * THIS = context;
    start; stack_frame;
    if (INPUT_A_PRESSED || INPUT_B_PRESSED) return TRUE;
#ifdef PS2_KEYBOARD
    if (keyboard_key_pressed(0x5a) || keyboard_key_pressed(0x15a) || keyboard_key_pressed(0x76)) return TRUE;
#endif
    THIS->waitable = TRUE;
    return FALSE;
}

#define OUTPUT(arg) (*(UWORD *)VM_REF_TO_PTR(*(INT16 *)VM_REF_TO_PTR(arg)))

UBYTE keyboard_wait(void * THIS, UBYTE start, UWORD * stack_frame) OLDCALL BANKED {
    start;
#ifdef PS2_KEYBOARD
    if (keyboard_key_pressed(stack_frame[0])) return TRUE;
#else
    stack_frame;
#endif
    ((SCRIPT_CTX *)THIS)->waitable = TRUE;
    return FALSE;
}

void keyboard_attach_native(SCRIPT_CTX * THIS) OLDCALL BANKED {
#ifdef PS2_KEYBOARD
    keyboard_script_attach(*(UWORD *)VM_REF_TO_PTR(FN_ARG0),
                           *(UWORD *)VM_REF_TO_PTR(FN_ARG1),
                           (void *)*(UWORD *)VM_REF_TO_PTR(FN_ARG2));
#else
    THIS;
#endif
}

void keyboard_detach_native(SCRIPT_CTX * THIS) OLDCALL BANKED {
#ifdef PS2_KEYBOARD
    keyboard_script_detach(*(UWORD *)VM_REF_TO_PTR(FN_ARG0));
#else
    THIS;
#endif
}

void keyboard_enable_native(SCRIPT_CTX * THIS) OLDCALL BANKED {
#ifdef PS2_KEYBOARD
    keyboard_set_enabled(*(UWORD *)VM_REF_TO_PTR(FN_ARG0));
#else
    THIS;
#endif
}

void keyboard_clear_native(SCRIPT_CTX * THIS) OLDCALL BANKED {
    THIS;
#ifdef PS2_KEYBOARD
    keyboard_clear();
#endif
}

void keyboard_read_event_native(SCRIPT_CTX * THIS) OLDCALL BANKED {
#ifdef PS2_KEYBOARD
    keyboard_event_t event;
    keyboard_update();
    keyboard_read_event(&event);
    OUTPUT(FN_ARG0) = event.key;
    OUTPUT(FN_ARG1) = event.flags;
    OUTPUT(FN_ARG2) = event.modifiers;
    OUTPUT(FN_ARG3) = event.character;
#else
    OUTPUT(FN_ARG0) = OUTPUT(FN_ARG1) = OUTPUT(FN_ARG2) = OUTPUT(FN_ARG3) = 0;
#endif
}

void keyboard_read_character_native(SCRIPT_CTX * THIS) OLDCALL BANKED {
#ifdef PS2_KEYBOARD
    keyboard_update();
    OUTPUT(FN_ARG0) = keyboard_read_character();
#else
    OUTPUT(FN_ARG0) = 0;
#endif
}

void keyboard_held_native(SCRIPT_CTX * THIS) OLDCALL BANKED {
#ifdef PS2_KEYBOARD
    keyboard_update();
    OUTPUT(FN_ARG1) = keyboard_key_held(*(UWORD *)VM_REF_TO_PTR(FN_ARG0));
#else
    OUTPUT(FN_ARG1) = 0;
#endif
}

void keyboard_status_native(SCRIPT_CTX * THIS) OLDCALL BANKED {
#ifdef PS2_KEYBOARD
    keyboard_update();
    OUTPUT(FN_ARG0) = keyboard_enabled;
    OUTPUT(FN_ARG1) = keyboard_modifiers;
    OUTPUT(FN_ARG2) = keyboard_errors;
#else
    OUTPUT(FN_ARG0) = OUTPUT(FN_ARG1) = OUTPUT(FN_ARG2) = 0;
#endif
}
