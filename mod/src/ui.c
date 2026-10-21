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
#define TEXT_LINE_BREAK  0xE0
#define TEXT_PAGE_BREAK  0xF0
#define TEXT_PERIOD      0x10   // in digit mode
#define TEXT_COMMA       0x11   // in digit mode
#define TEXT_QUESTION    0x0B   // in digit mode
#define TEXT_APOSTROPHE  0x1A   // in lowercase mode

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

// Converts plain text to the game's encoding, without the color/line-start prefix or the end byte; returns
// the end of the output. Letters, digits, spaces, . , ? and ' are supported; '\n' is a line break and '\f'
// a page break (waits for A). Codes decoded from the game's own dialogue (research/npcs).
u8* eo_text_encode(u8* out, const char* text) {
    int mode = 0;
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
        } else if (c == '.' || c == ',' || c == '?') {
            want = TEXT_MODE_DIGITS;
            code = c == '.' ? TEXT_PERIOD : c == ',' ? TEXT_COMMA : TEXT_QUESTION;
        } else if (c == '\'') {
            want = TEXT_MODE_LOWER;
            code = TEXT_APOSTROPHE;
        } else if (c == '\n' || c == '\f') {
            *out++ = c == '\n' ? TEXT_LINE_BREAK : TEXT_PAGE_BREAK;
            mode = 0;  // the game's text restates the mode after a break
            continue;
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
    return out;
}

// A whole text in the game's encoding: black, line start, the text, end byte.
void eo_text_encode_full(u8* out, const char* text) {
    *out++ = TEXT_COLOR_BLACK;
    *out++ = TEXT_LINE_START;
    out = eo_text_encode(out, text);
    *out = TEXT_END;
}

// Draws plain text in the menu/dialogue font.
void eo_ui_text(s32 x, s32 y, const char* text) {
    u8 encoded[64];
    eo_text_encode_full(encoded, text);
    ui_draw_text(x, y, encoded);
}

void eo_format_amount(char* out, const char* label, u32 amount, int columns, const char* unit) {
    char number[12];
    eo_format_u32(number, "", amount, "");
    int used = 0;
    while (label[used] != '\0') {
        *out++ = label[used++];
    }
    int len = 0;
    while (number[len] != '\0') {
        len++;
    }
    for (int pad = columns - used - len; pad > 0; pad--) {
        *out++ = ' ';
    }
    for (int i = 0; i < len; i++) {
        *out++ = number[i];
    }
    while (*unit != '\0') {
        *out++ = *unit++;
    }
    *out = '\0';
}
