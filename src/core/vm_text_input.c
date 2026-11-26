#pragma bank 255
#include "vm_text_input.h"
#include "input.h"
#include "text_input.h"
#include "ui.h"
#include "vm_ui.h"
#include "scroll.h"
#include "system.h"
#include <string.h>

static text_input_t text_entry;
static text_osk_t text_osk;
static UBYTE osk_previous_joy, osk_repeat;
static UBYTE osk_glyph_count;
// Fixed cells reuse the dialogue buffer for the glyph cache and tile backups.
#define osk_glyphs (ui_text_data + 32)

// Background text uses 16x4 cells, enough for 63 characters and the cursor.
// Back up their tilemap and attributes in unused dialogue-buffer space. The
// expanded glyph cache occupies bytes 32..125 while the OSK is active.
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

// Cache repeated glyphs instead of allocating one tile for every key and label.
// Two tiles are reserved for the selected key's highlight and the return icon.
static UBYTE osk_glyph_tile(UBYTE i) {
    // Background-preview scenes reserve bank-0 tiles 148..191 in addition to
    // the usual 50 glyph slots. That fits every printable ASCII glyph without
    // borrowing scene graphics or the two icon/highlight tiles. Window mode can
    // reuse the nine hidden frame tiles for its scrolling entry row.
    return i < 50u ? TEXT_BUFFER_START + i : ((text_entry.flags & TEXT_INPUT_BACKGROUND_TEXT) ? 148u : 0xC0u) + i - 50u;
}

static UBYTE osk_tile(UBYTE ch) {
    UBYTE i;
    if (ch == ' ') return ui_white_tile;
    for (i = 0; i < osk_glyph_count; i++) if (osk_glyphs[i] == ch) return osk_glyph_tile(i);
    if (osk_glyph_count == ((text_entry.flags & TEXT_INPUT_BACKGROUND_TEXT) ? 94u : 59u)) return 0;
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
    static const UBYTE actions[TEXT_OSK_ACTION_KEYS] = {'^', '#', 'X', '_', '<', '>', '\r'};
    static const UBYTE action_x[TEXT_OSK_ACTION_KEYS] = {0, 2, 4, 6, 14, 16, 18};
    UBYTE i, j, ch, tile, column = 3;
    UBYTE background = text_entry.flags & TEXT_INPUT_BACKGROUND_TEXT;
    UBYTE key_y = background ? 0u : 1u;
    UBYTE first = text_entry.cursor > 12u ? text_entry.cursor - 12u : 0;
    UBYTE end = first + 13u;
    vm_overlay_clear(THIS, 0, 0, 20, background ? 5u : 6u, 1, 0);
    osk_glyph_count = 0;
    // The final text tile holds the return icon; the preceding one highlights
    // the selected key. This leaves all 50 normal overlay glyph slots intact.
    ui_load_text_cell(TEXT_BUFFER_START + 51u, '\r', FALSE);
    for (i = 0; i < TEXT_OSK_CHARACTER_KEYS; i++) {
        ch = text_osk_character(&text_entry, &text_osk, i);
        if (!ch) continue;
        tile = osk_tile(ch);
        if (text_osk.selected == i) {
            tile = TEXT_BUFFER_START + 50u;
            ui_load_text_cell(tile, ch, TRUE);
        }
        ui_place_text_cell((i % TEXT_OSK_COLUMNS) * 2u, key_y + i / TEXT_OSK_COLUMNS, tile);
    }
    for (i = 0; i < TEXT_OSK_ACTION_KEYS; i++) {
        ch = actions[i];
        tile = ch == '\r' ? TEXT_BUFFER_START + 51u : osk_tile(ch);
        if (text_osk.selected == TEXT_OSK_CHARACTER_KEYS + i) {
            tile = TEXT_BUFFER_START + 50u;
            ui_load_text_cell(tile, ch, TRUE);
        }
        // Four key columns form one continuous space bar.
        for (j = 0; j < (i == 3u ? 8u : 1u); j++)
            ui_place_text_cell(action_x[i] + j, key_y + 4u, tile);
    }
    if (background) draw_note();
    else for (i = first; i <= text_entry.length && i <= end; i++) {
        if (i == text_entry.cursor) ui_place_text_cell(column++, 0, osk_tile('_'));
        if (i < text_entry.length && i < end)
            ui_place_text_cell(column++, 0, osk_tile((text_entry.flags & TEXT_INPUT_PASSWORD) ? '*' : text_entry.text[i]));
    }
}

UBYTE text_input_run(void * context, UBYTE start, UWORD * stack_frame) OLDCALL BANKED {
    SCRIPT_CTX * THIS = context;
    UWORD * result = VM_REF_TO_PTR((INT16)stack_frame[4]);
    UWORD * destination = VM_REF_TO_PTR((INT16)stack_frame[0]);
    UBYTE status = TEXT_INPUT_ACTIVE, dirty = FALSE, pressed, direction, previous;
    if (start) {
        text_input_begin(&text_entry, destination, stack_frame[1], stack_frame[2], stack_frame[3]);
        memset(&text_osk, 0, sizeof(text_osk));
        if (text_entry.flags & TEXT_INPUT_BACKGROUND_TEXT) note_save();
        osk_previous_joy = joy; osk_repeat = 0;
        dirty = TRUE;
    } else if (text_drawn) {
        pressed = joy & ~osk_previous_joy;
        osk_previous_joy = joy;
        if (status == TEXT_INPUT_ACTIVE) {
            if (pressed & J_START) status = TEXT_INPUT_CONFIRMED;
            else if (pressed & J_B) { text_input_key(&text_entry, TEXT_KEY_BACKSPACE, 0); dirty = TRUE; }
            else if (pressed & J_A) { status = text_osk_press(&text_entry, &text_osk); dirty = TRUE; }
            else if (pressed & J_SELECT) {
                previous = text_osk.selected;
                text_osk.selected = TEXT_OSK_SYMBOLS;
                text_osk_press(&text_entry, &text_osk);
                text_osk.selected = previous;
                if (previous < TEXT_OSK_CHARACTER_KEYS && !text_osk_character(&text_entry, &text_osk, previous)) text_osk.selected = 0;
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
        if (text_entry.flags & TEXT_INPUT_BACKGROUND_TEXT) note_restore();
        else ui_load_tiles(); // Restore frame tiles temporarily used by the entry row.
        return TRUE;
    }
    if (dirty) {
        draw_osk(THIS);
    }
    THIS->waitable = TRUE;
    return FALSE;
}

UBYTE text_input_wait(void * context, UBYTE start, UWORD * stack_frame) OLDCALL BANKED {
    SCRIPT_CTX * THIS = context;
    start; stack_frame;
    if (INPUT_A_PRESSED || INPUT_B_PRESSED) return TRUE;
    THIS->waitable = TRUE;
    return FALSE;
}
