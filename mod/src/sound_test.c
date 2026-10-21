#include "eo.h"

// Sound browser, for picking sounds by ear (dev tool, 2026-10-21). Hold L and tap D-pad right / left to play the
// next / previous sound ID, D-pad up to play the current one again. The ID shows at the top left while L is held.
// D-pad presses made while L is held don't reach the game or the overhaul's own D-pad controls.

#define BTN_L 0x0020
#define BTN_DUP 0x0800
#define BTN_DLEFT 0x0200
#define BTN_DRIGHT 0x0100
#define MAX_SOUND 0x7F
#define TEST_X 20
#define TEST_Y 120

extern u16 gPadButtonsNew;
extern u16 gPadButtonPressed;   // held

static int sound = 0x18;        // the game's hit sound
static int showing = 0;

RECOMP_HOOK_RETURN("pad_read") void eo_sound_test_input(void) {
    showing = (gPadButtonPressed & BTN_L) != 0;
    if (!showing) {
        return;
    }
    u16 pressed = gPadButtonsNew;
    int play = 0;
    if (pressed & BTN_DRIGHT) {
        sound = sound >= MAX_SOUND ? 0 : sound + 1;
        play = 1;
    } else if (pressed & BTN_DLEFT) {
        sound = sound <= 0 ? MAX_SOUND : sound - 1;
        play = 1;
    } else if (pressed & BTN_DUP) {
        play = 1;
    }
    if (play) {
        sfx_play(sound, 0xFF, 0x40);
    }
    gPadButtonsNew &= (u16)~(BTN_DUP | BTN_DLEFT | BTN_DRIGHT | 0x0400);
}

// The dev panel's Sounds tab (Quest64-Recomp assets/devpanel) asks for a sound by writing SOUND_REQUEST_TAG | id
// here; it's played on the next frame and cleared. In the console's spare 4 MB (see icons.c), never used by the game.
#define SOUND_MAILBOX ((volatile u32*)0x807F1F00)
#define SOUND_REQUEST_TAG 0x534E0000   // "SN"

RECOMP_HOOK_RETURN("frame_build") void eo_sound_test_mailbox(void) {
    u32 request = *SOUND_MAILBOX;
    if ((request & 0xFFFF0000) == SOUND_REQUEST_TAG) {
        *SOUND_MAILBOX = 0;
        sound = (int)(request & 0xFF);
        sfx_play(sound, 0xFF, 0x40);
    }
}

RECOMP_HOOK_RETURN("hud_draw_main") void eo_sound_test_draw(void) {
    if (!showing) {
        return;
    }
    char line[24];
    char hex[3];
    const char digits[] = "0123456789ABCDEF";
    hex[0] = digits[(sound >> 4) & 0xF];
    hex[1] = digits[sound & 0xF];
    hex[2] = '\0';
    char* out = eo_append(line, "SOUND ");
    eo_append(out, hex);
    s32 w = eo_strlen(line) * SMALL_W + 2 * SMALL_PAD;
    eo_ui_window(TEST_X, TEST_Y, w, SMALL_LINE + 2 * SMALL_PAD - 2);
    eo_ui_small(TEST_X + SMALL_PAD, TEST_Y + SMALL_PAD, TEXT_NORMAL, line);
}
