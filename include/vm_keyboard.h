#ifndef VM_KEYBOARD_H
#define VM_KEYBOARD_H
#include "vm.h"

void keyboard_enable_native(SCRIPT_CTX * THIS) OLDCALL BANKED;
void keyboard_clear_native(SCRIPT_CTX * THIS) OLDCALL BANKED;
void keyboard_read_event_native(SCRIPT_CTX * THIS) OLDCALL BANKED;
void keyboard_read_character_native(SCRIPT_CTX * THIS) OLDCALL BANKED;
void keyboard_held_native(SCRIPT_CTX * THIS) OLDCALL BANKED;
void keyboard_status_native(SCRIPT_CTX * THIS) OLDCALL BANKED;
UBYTE keyboard_wait(void * THIS, UBYTE start, UWORD * stack_frame) OLDCALL BANKED;
void keyboard_attach_native(SCRIPT_CTX * THIS) OLDCALL BANKED;
void keyboard_detach_native(SCRIPT_CTX * THIS) OLDCALL BANKED;
#endif
