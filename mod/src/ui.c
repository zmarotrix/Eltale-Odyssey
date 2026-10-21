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
#define TEXT_EXCLAMATION 0x0A   // in digit mode
#define TEXT_COLON       0x14   // in digit mode
#define TEXT_DASH        0x15   // in digit mode
#define TEXT_AMPERSAND   0x1A   // in uppercase mode
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
    if (!eo_gfx_fits(0x200)) {
        return;
    }
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
// the end of the output. Letters, digits, spaces and . , ? ! : - ' & are supported; '\n' is a line break and '\f'
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
        } else if (c == '.' || c == ',' || c == '?' || c == '!' || c == ':' || c == '-') {
            want = TEXT_MODE_DIGITS;
            code = c == '.' ? TEXT_PERIOD : c == ',' ? TEXT_COMMA : c == '?' ? TEXT_QUESTION :
                   c == '!' ? TEXT_EXCLAMATION : c == ':' ? TEXT_COLON : TEXT_DASH;
        } else if (c == '&') {
            want = TEXT_MODE_UPPER;
            code = TEXT_AMPERSAND;
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
    if (!eo_gfx_fits(0x80 + eo_strlen(text) * 0x30)) {
        return;
    }
    u8 encoded[64];
    eo_text_encode_full(encoded, text);
    ui_draw_text(x, y, encoded);
}

void eo_title_case(char* out, const char* name, int max_chars) {
    int start = 1;
    for (int n = 0; name != 0 && *name != '\0' && n < max_chars; name++, n++) {
        char c = *name;
        if (c >= 'A' && c <= 'Z' && !start) {
            c = (char)(c - 'A' + 'a');
        }
        start = c == ' ' || c == '-';
        *out++ = c;
    }
    *out = '\0';
}

char* eo_append(char* out, const char* text) {
    while (*text != '\0') {
        *out++ = *text++;
    }
    *out = '\0';
    return out;
}

// Fills a line: %w becomes word, %a and %b the numbers a and b; everything else is copied. Returns out.
s32 eo_strlen(const char* text) {
    s32 n = 0;
    while (text[n] != '\0') {
        n++;
    }
    return n;
}

char* eo_text_fill(char* out, const char* line, const char* word, u32 a, u32 b) {
    char* o = out;
    for (; *line != '\0'; line++) {
        if (line[0] == '%' && (line[1] == 'w' || line[1] == 'a' || line[1] == 'b')) {
            line++;
            if (*line == 'w') {
                o = eo_append(o, word != 0 ? word : "");
            } else {
                eo_format_u32(o, "", *line == 'a' ? a : b, "");
                while (*o != '\0') {
                    o++;
                }
            }
        } else {
            *o++ = *line;
        }
    }
    *o = '\0';
    return out;
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

// ---- Small text: the HUD's own 6 x 8 font ----
// Crisp at native size and about a quarter smaller than the dialogue font (playtest, 2026-10-20: windows can be
// smaller on a modern screen). It has uppercase letters, digits and : ' - . ? = only, so other characters are
// mapped: lowercase to uppercase, ! and , to ., & to -; anything else becomes a space.

void eo_ui_small(s32 x, s32 y, int palette, const char* text) {
    if (!eo_gfx_fits(0x80 + eo_strlen(text) * 0x30)) {   // font setup, then a few commands a letter
        return;
    }
    char buf[64];
    int n = 0;
    for (; *text != '\0' && n < 63; text++) {
        char c = *text;
        if (c >= 'a' && c <= 'z') {
            c = (char)(c - 'a' + 'A');
        } else if (c == '!' || c == ',') {
            c = '.';
        } else if (c == '&') {
            c = '-';
        } else if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ':' || c == '\'' || c == '-' ||
                     c == '.' || c == '?' || c == '=' || c == ' ')) {
            c = ' ';
        }
        buf[n++] = c;
    }
    buf[n] = '\0';
    hud_draw_text(x, y, palette, buf);
}

// ---- Sounds ----

static u32 sfx_frame = 0;      // counts frames (frame_build)
static u32 sfx_last_frame = 0xFFFFFFFF;

RECOMP_HOOK_RETURN("frame_build") void eo_sfx_frame(void) {
    sfx_frame++;
}

void eo_sfx(s32 sound) {
    if (sfx_last_frame == sfx_frame) {
        return;
    }
    sfx_last_frame = sfx_frame;
    sfx_play_scaled(0, sound, 0xFF);
}

// ---- The mod's own display list ----

static u32* ui_pos = 0;     // next free command in this frame's UI list
static u32* ui_end = 0;
static u32* game_pos = 0;   // where the game's list resumes while drawing into ours
static int ui_depth = 0;

RECOMP_HOOK("frame_build") void eo_ui_frame_start(u8* context) {
    int n = (u32)context >= GFX_CONTEXT_BASE + GFX_CONTEXT_SIZE ? 1 : 0;
    ui_pos = (u32*)(UI_LIST_BASE + n * UI_LIST_SIZE);
    ui_end = ui_pos + UI_LIST_SIZE / 4;
    ui_depth = 0;
    game_pos = 0;
}

void eo_ui_begin(void) {
    if (ui_depth++ > 0) {
        return;
    }
    if (ui_pos == 0 || ui_end - ui_pos < 0x100 / 4 || eo_gfx_room() < 0x40) {
        return;   // draw into the game's list as before (still room-checked)
    }
    u32* g = gMasterGfxPos;
    g[0] = 0x06000000;          // gSPDisplayList: call ours, come back
    g[1] = (u32)ui_pos;
    game_pos = g + 2;
    gMasterGfxPos = ui_pos;
}

void eo_ui_end(void) {
    if (ui_depth == 0 || --ui_depth > 0 || game_pos == 0) {
        return;
    }
    u32* g = gMasterGfxPos;
    g[0] = 0xB8000000;          // gSPEndDisplayList
    g[1] = 0;
    ui_pos = g + 2;
    gMasterGfxPos = game_pos;
    game_pos = 0;
}
