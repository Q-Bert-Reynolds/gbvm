#ifndef VM_TEXT_INPUT_H
#define VM_TEXT_INPUT_H
#include "vm.h"

UBYTE text_input_run(void * context, UBYTE start, UWORD * stack_frame) OLDCALL BANKED;
UBYTE text_input_wait(void * context, UBYTE start, UWORD * stack_frame) OLDCALL BANKED;
#endif
