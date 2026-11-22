#pragma bank 255

#include <string.h>

#include "events.h"
#include "input.h"
#ifdef PS2_KEYBOARD
#include "keyboard.h"
script_event_t keyboard_events[MAX_KEYBOARD_SCRIPTS];
UWORD keyboard_event_keys[MAX_KEYBOARD_SCRIPTS];
#endif

script_event_t input_events[8];
UBYTE input_slots[8];

script_event_t timer_events[MAX_CONCURRENT_TIMERS];
timer_time_t timer_values[MAX_CONCURRENT_TIMERS];

void events_init(UBYTE preserve) BANKED {
    if (preserve) {
        for (UBYTE i = 0; i < 8; i++)
            input_events[i].handle = 0;
    } else {
        memset(input_slots, 0, sizeof(input_slots));
        memset(input_events, 0, sizeof(input_events));
    }
#ifdef PS2_KEYBOARD
    if (preserve) {
        for (UBYTE i = 0; i != MAX_KEYBOARD_SCRIPTS; i++)
            keyboard_events[i].handle = 0;
    } else {
        memset(keyboard_events, 0, sizeof(keyboard_events));
        memset(keyboard_event_keys, 0, sizeof(keyboard_event_keys));
    }
#endif
}

#ifdef PS2_KEYBOARD
void keyboard_script_attach(UWORD key, UBYTE bank, void * pc) BANKED {
    UBYTE slot = MAX_KEYBOARD_SCRIPTS;
    if (key >= 512u) return;
    for (UBYTE i = 0; i != MAX_KEYBOARD_SCRIPTS; i++) {
        if (keyboard_events[i].script_addr && keyboard_event_keys[i] == key) {
            slot = i;
            break;
        }
        if (!keyboard_events[i].script_addr && slot == MAX_KEYBOARD_SCRIPTS) slot = i;
    }
    if (slot == MAX_KEYBOARD_SCRIPTS) {
        keyboard_errors |= KB_ERROR_OVERFLOW;
        return;
    }
    keyboard_event_keys[slot] = key;
    keyboard_events[slot].script_bank = bank;
    keyboard_events[slot].script_addr = pc;
}

void keyboard_script_detach(UWORD key) BANKED {
    for (UBYTE i = 0; i != MAX_KEYBOARD_SCRIPTS; i++) {
        if (keyboard_event_keys[i] == key) keyboard_events[i].script_addr = NULL;
    }
}

void keyboard_events_update(void) BANKED {
    for (UBYTE i = 0; i != MAX_KEYBOARD_SCRIPTS; i++) {
        script_event_t * event = &keyboard_events[i];
        if (event->script_addr && keyboard_key_pressed(keyboard_event_keys[i]) &&
            ((event->handle == 0) || (event->handle & SCRIPT_TERMINATED)))
            script_execute(event->script_bank, event->script_addr, &event->handle, 1, (int)keyboard_event_keys[i]);
    }
}
#endif

void events_update(void) NONBANKED {
    UBYTE * slot_ptr = input_slots;
    for (UBYTE tmp = joy, key = 1; (tmp); tmp = tmp >> 1, key = key << 1, slot_ptr++) {
        if (tmp & 1) {
            if (*slot_ptr == 0) continue;
            script_event_t * event = &input_events[(*slot_ptr & 0x0f) - 1u];
            if (!event->script_addr) continue;
            if (*slot_ptr & 0x80) joy ^= key;     // reset key bit
            if (last_joy & key) continue;
            if ((event->handle == 0) || ((event->handle & SCRIPT_TERMINATED) != 0))
                script_execute(event->script_bank, event->script_addr, &event->handle, 1, (int)key);
        }
    }
    recent_joy = recent_joy & joy;
}

void timers_init(UBYTE preserve) BANKED {
    if (preserve) {
        for (UBYTE i = 0; i != MAX_CONCURRENT_TIMERS; i++)
            timer_events[i].handle = 0;
    } else {
        memset(timer_values, 0, sizeof(timer_values));
        memset(timer_events, 0, sizeof(timer_events));
    }
}

void timers_update(void) NONBANKED {
    timer_time_t * ctimer = timer_values;
    for (UBYTE i = 0; i != MAX_CONCURRENT_TIMERS; i++) {
        if (ctimer->value) {
            if (--ctimer->remains == 0) {
                ctimer->remains = ctimer->value;
                script_event_t * event = &timer_events[i];
                if (!event->script_addr) continue;
                if ((event->handle == 0) || ((event->handle & SCRIPT_TERMINATED) != 0)) {
                    script_execute(event->script_bank, event->script_addr, &event->handle, 0, 0);
                }
            }

        }
        ctimer++;
    }
}
