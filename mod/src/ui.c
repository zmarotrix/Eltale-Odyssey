#include "eo.h"

// Native-looking UI pieces built from the game's own routines (research/menus).

// The window palette the game's dialogue box loads before drawing rectangles.
#define WINDOW_PALETTE 0x8005FB80

// Text control bytes in the game's encoding (research/modding-notes-text, "Text Information").
#define TEXT_COLOR_BLACK 0xA0
#define TEXT_LINE_START  0xC0
#define TEXT_MODE_DIGITS 0x80
#define TEXT_MODE_UPPER  0x81
#define TEXT_MODE_LOWER  0x82
#define TEXT_SPACE       0x7F
#define TEXT_END         0xFF

// Same commands the dialogue box sends before ui_draw_rect: palette mode on, load the 256-color window palette.
static void load_window_palette(void) {
    eo_gfx(0xE7000000, 0x00000000);      // pipe sync
    eo_gfx(0xBA000E02, 0x00008000);      // texture LUT: RGBA16
    eo_gfx(0xE7000000, 0x00000000);
    eo_gfx(0xFD100000, WINDOW_PALETTE);  // set texture image: the palette
    eo_gfx(0xE8000000, 0x00000000);      // tile sync
    eo_gfx(0xF5000100, 0x07000000);      // set tile 7 at TMEM 0x100
    eo_gfx(0xE6000000, 0x00000000);      // load sync
    eo_gfx(0xF0000000, 0x073FC000);      // load TLUT, 256 colors
    eo_gfx(0xE7000000, 0x00000000);
}

// Same pieces as the game's ui_draw_window: background, 3 px drop shadow, 1 px border.
void eo_ui_window(s32 x, s32 y, s32 w, s32 h) {
    load_window_palette();
    ui_draw_rect(0, x, y, w, h);
    ui_draw_rect(3, x + w, y + 3, 3, h);
    ui_draw_rect(3, x + 3, y + h, w, 3);
    ui_draw_rect(3, x, y, w, 1);
    ui_draw_rect(3, x, y + h - 1, w, 1);
    ui_draw_rect(3, x, y, 1, h);
    ui_draw_rect(3, x + w - 1, y, 1, h);
}

// Converts plain text (letters, digits, spaces) to the game's encoding, in black like dialogue text.
static void encode(u8* out, const char* text) {
    int mode = 0;
    *out++ = TEXT_COLOR_BLACK;
    *out++ = TEXT_LINE_START;
    for (; *text != '\0'; text++) {
        char c = *text;
        int want;
        u8 code;
        if (c >= 'A' && c <= 'Z') {
            want = TEXT_MODE_UPPER;
            code = (u8)(c - 'A');
        } else if (c >= 'a' && c <= 'z') {
            want = TEXT_MODE_LOWER;
            code = (u8)(c - 'a');
        } else if (c >= '0' && c <= '9') {
            want = TEXT_MODE_DIGITS;
            code = (u8)(c - '0');
        } else {
            *out++ = TEXT_SPACE;
            continue;
        }
        if (mode != want) {
            *out++ = (u8)want;
            mode = want;
        }
        *out++ = code;
    }
    *out = TEXT_END;
}

// Draws plain text in the menu/dialogue font.
void eo_ui_text(s32 x, s32 y, const char* text) {
    u8 encoded[64];
    encode(encoded, text);
    ui_draw_text(x, y, encoded);
}
