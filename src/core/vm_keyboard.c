#pragma bank 255
#include "vm_keyboard.h"
#include "input.h"
#include "text_input.h"
#include "ui.h"
#include "vm_ui.h"
#include "scroll.h"
#include "system.h"
#include <string.h>
#ifdef PS2_KEYBOARD
#include "keyboard.h"
#include "events.h"
#endif

static text_input_t text_entry;
static text_osk_t text_osk;
static UBYTE osk_previous_joy, osk_repeat;
static UBYTE osk_glyph_count;
// The OSK renders cells directly, leaving the dialogue text buffer available
// for its prompt and glyph cache without reserving more persistent WRAM.
#define osk_prompt (ui_text_data)
#define osk_glyphs (ui_text_data + 32)

// Background text uses 16x4 cells, enough for 63 characters and the cursor.
// Back up their tilemap and attributes in unused dialogue-buffer space. The
// prompt and expanded glyph cache occupy bytes 0..125 while the OSK is active.
#define note_tiles (ui_text_data + 128)
#define note_attributes (ui_text_data + 192)
static UBYTE note_x, note_y;

static UBYTE * note_address(UBYTE i) {
    return GetBkgAddr() + ((UWORD)((note_y + i / 16u) & 31u) << 5) + ((note_x + i % 16u) & 31u);
}

static void note_save(void) {
    UBYTE i;
    note_x = ((SCX_REG >> 3) + 2u) & 31u;
    note_y = ((SCY_REG >> 3) + 4u) & 31u;
    for (i = 0; i != 64u; i++) {
        UBYTE * addr = note_address(i);
#ifdef CGB
        if (_is_CGB) {
            VBK_REG = VBK_ATTRIBUTES;
            note_attributes[i] = get_vram_byte(addr);
            VBK_REG = VBK_TILES;
        }
#endif
        note_tiles[i] = get_vram_byte(addr);
    }
}

static void note_restore(void) {
    UBYTE i;
    for (i = 0; i != 64u; i++) {
        UBYTE * addr = note_address(i);
#ifdef CGB
        if (_is_CGB) {
            VBK_REG = VBK_ATTRIBUTES;
            set_vram_byte(addr, note_attributes[i]);
            VBK_REG = VBK_TILES;
        }
#endif
        set_vram_byte(addr, note_tiles[i]);
    }
}

static void discard_text_input(void) {
#ifdef PS2_KEYBOARD
    keyboard_event_t event;
    while (keyboard_read_event(&event)) {}
    while (keyboard_read_character()) {}
#endif
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

// Cache repeated glyphs instead of allocating one tile for every key and label.
// Two tiles are reserved for the selected key's inverse highlight (including OK).
static UBYTE osk_glyph_tile(UBYTE i) {
    // Background-preview scenes reserve bank-0 tiles 148..191 in addition to
    // the usual 50 glyph slots. That fits every printable ASCII glyph without
    // borrowing scene graphics or the two selected-key highlight tiles.
    return i < 50u ? TEXT_BUFFER_START + i : 148u + i - 50u;
}

static UBYTE osk_tile(UBYTE ch) {
    UBYTE i;
    if (ch == ' ') return ui_white_tile;
    for (i = 0; i < osk_glyph_count; i++) if (osk_glyphs[i] == ch) return osk_glyph_tile(i);
    if (osk_glyph_count == ((text_entry.flags & TEXT_INPUT_BACKGROUND_TEXT) ? 94u : 50u)) return 0;
    osk_glyphs[osk_glyph_count] = ch;
    ui_load_text_cell(osk_glyph_tile(osk_glyph_count), ch, FALSE);
    return osk_glyph_tile(osk_glyph_count++);
}

static void draw_note(void) {
    UBYTE i, source = 0, cursor_drawn = FALSE, ch;
    for (i = 0; i != 64u; i++) {
        UBYTE * addr = note_address(i);
        if (!cursor_drawn && source == text_entry.cursor) {
            ch = '_';
            cursor_drawn = TRUE;
        } else if (source < text_entry.length) {
            ch = (text_entry.flags & TEXT_INPUT_PASSWORD) ? '*' : text_entry.text[source];
            source++;
        } else ch = ' ';
#ifdef CGB
        if (_is_CGB) {
            VBK_REG = VBK_ATTRIBUTES;
            set_vram_byte(addr, text_palette & 7u);
            VBK_REG = VBK_TILES;
        }
#endif
        set_vram_byte(addr, osk_tile(ch));
    }
}

static void draw_osk(SCRIPT_CTX * THIS) {
    static const char * const actions[] = { "Aa", "#!", "<", ">", "OK", "X" };
    static const UBYTE action_x[] = {1, 4, 7, 9, 12, 16};
    UBYTE i, j, ch, tile, column = 1;
    UBYTE first = text_entry.cursor > 10u ? text_entry.cursor - 10u : 0;
    UBYTE end = first + 11u;
    vm_overlay_clear(THIS, 0, 0, 20, 8, 1, UI_DRAW_FRAME);
    osk_glyph_count = 0;
    for (i = 0; i < 30u; i++) {
        ch = text_osk_character(&text_entry, &text_osk, i);
        if (!ch) continue;
        if (ch == ' ') ch = '_';
        tile = osk_tile(ch);
        if (text_osk.selected == i) {
            tile = TEXT_BUFFER_START + 50u;
            ui_load_text_cell(tile, ch, TRUE);
        }
        ui_place_text_cell(5u + i % 10u, 3u + i / 10u, tile);
    }
    for (i = 0; i < 6u; i++) for (j = 0; actions[i][j]; j++) {
        ch = actions[i][j];
        tile = osk_tile(ch);
        if (text_osk.selected == 30u + i) {
            tile = TEXT_BUFFER_START + 50u + j;
            ui_load_text_cell(tile, ch, TRUE);
        }
        ui_place_text_cell(action_x[i] + j, 6, tile);
    }
    osk_tile('.'); osk_tile('_');
    if (text_entry.flags & TEXT_INPUT_BACKGROUND_TEXT) draw_note();
    else for (i = first; i <= text_entry.length && i <= end; i++) {
        if (i == text_entry.cursor) ui_place_text_cell(column++, 2, osk_tile('_'));
        if (i < text_entry.length && i < end)
            ui_place_text_cell(column++, 2, osk_tile((text_entry.flags & TEXT_INPUT_PASSWORD) ? '*' : text_entry.text[i]));
    }
    // Prioritize editable text. Unusually diverse prompts are ellipsized if the
    // monochrome tile cache fills. Background previews reserve extra glyph tiles.
    for (i = 0; i < 18u && osk_prompt[i]; i++) {
        tile = osk_tile(osk_prompt[i]);
        if (!tile) {
            if (i > 15u) i = 15u;
            for (j = 0; j < 3u; j++) ui_place_text_cell(1u + i + j, 1, osk_tile('.'));
            break;
        }
        ui_place_text_cell(1u + i, 1, tile);
    }
}

UBYTE keyboard_text_input(void * context, UBYTE start, UWORD * stack_frame) OLDCALL BANKED {
    SCRIPT_CTX * THIS = context;
    UWORD * result = VM_REF_TO_PTR((INT16)stack_frame[4]);
    UWORD * destination = VM_REF_TO_PTR((INT16)stack_frame[0]);
    UBYTE status = TEXT_INPUT_ACTIVE, dirty = FALSE, pressed, direction, previous;
#ifdef PS2_KEYBOARD
    keyboard_event_t event;
#endif
    if (!(stack_frame[3] & TEXT_INPUT_OSK)) {
#ifdef PS2_KEYBOARD
        if (!keyboard_enabled) { *result = 2; return TRUE; }
#else
        *result = 2; return TRUE;
#endif
    }
    if (start) {
        text_input_begin(&text_entry, destination, stack_frame[1], stack_frame[2], stack_frame[3]);
        memset(&text_osk, 0, sizeof(text_osk));
        osk_prompt[18] = 0;
        if ((text_entry.flags & (TEXT_INPUT_OSK | TEXT_INPUT_BACKGROUND_TEXT)) == (TEXT_INPUT_OSK | TEXT_INPUT_BACKGROUND_TEXT)) note_save();
        osk_previous_joy = joy; osk_repeat = 0;
        discard_text_input();
        dirty = TRUE;
    } else if (text_drawn) {
#ifdef PS2_KEYBOARD
        while (keyboard_read_event(&event)) {
            if (!(event.flags & KB_EVENT_DOWN)) continue;
            dirty = TRUE;
            status = text_input_key(&text_entry, event.key, event.character);
            if (status != TEXT_INPUT_ACTIVE) break;
        }
        while (keyboard_read_character()) {}
#endif
        pressed = joy & ~osk_previous_joy;
        osk_previous_joy = joy;
        if ((text_entry.flags & TEXT_INPUT_OSK) && status == TEXT_INPUT_ACTIVE) {
            if (pressed & J_START) status = TEXT_INPUT_CONFIRMED;
            else if (pressed & J_B) { text_input_key(&text_entry, PS2_KEY_BACKSPACE, 0); dirty = TRUE; }
            else if (pressed & J_A) { status = text_osk_press(&text_entry, &text_osk); dirty = TRUE; }
            else if (pressed & J_SELECT) {
                previous = text_osk.selected;
                text_osk.selected = 31;
                text_osk_press(&text_entry, &text_osk);
                text_osk.selected = previous;
                if (previous < 30u && !text_osk_character(&text_entry, &text_osk, previous)) text_osk.selected = 0;
                dirty = TRUE;
            }
            direction = joy & INPUT_DPAD;
            if (direction) {
                if (pressed & INPUT_DPAD) osk_repeat = 0;
                if (!osk_repeat) {
                    text_osk_move(&text_entry, &text_osk,
                        (direction & J_LEFT) ? -1 : ((direction & J_RIGHT) ? 1 : 0),
                        (direction & J_UP) ? -1 : ((direction & J_DOWN) ? 1 : 0));
                    dirty = TRUE;
                    osk_repeat = (pressed & INPUT_DPAD) ? 20 : 4;
                } else osk_repeat--;
            } else osk_repeat = 0;
        }
    }
    if (status != TEXT_INPUT_ACTIVE) {
        if (status == TEXT_INPUT_CONFIRMED) text_input_commit(&text_entry, destination, stack_frame[1]);
        *result = status == TEXT_INPUT_CONFIRMED ? 1 : 0;
        if ((text_entry.flags & (TEXT_INPUT_OSK | TEXT_INPUT_BACKGROUND_TEXT)) == (TEXT_INPUT_OSK | TEXT_INPUT_BACKGROUND_TEXT)) note_restore();
        discard_text_input();
        return TRUE;
    }
    if (dirty) {
        if (text_entry.flags & TEXT_INPUT_OSK) draw_osk(THIS);
        else draw_text_input(THIS);
    }
    THIS->waitable = TRUE;
    return FALSE;
}

UBYTE keyboard_text_wait(void * context, UBYTE start, UWORD * stack_frame) OLDCALL BANKED {
    SCRIPT_CTX * THIS = context;
    start; stack_frame;
    if (INPUT_A_PRESSED || INPUT_B_PRESSED) return TRUE;
#ifdef PS2_KEYBOARD
    if (keyboard_key_pressed(PS2_KEY_ENTER) || keyboard_key_pressed(PS2_KEY_NUMPAD_ENTER) || keyboard_key_pressed(PS2_KEY_ESCAPE)) return TRUE;
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
