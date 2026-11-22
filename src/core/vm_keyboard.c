#pragma bank 255
#include "vm_keyboard.h"
#ifdef PS2_KEYBOARD
#include "keyboard.h"
#include "events.h"
#endif

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
