#include "eo.h"

// Shows gold in a small window for a few seconds whenever it changes (a won battle, a paid night),
// in the dialogue box's style. There's no permanent counter: you see your purse when it matters.

#define NOTICE_FRAMES 150
#define NOTICE_W (NOTICE_COLUMNS * 8 + 20)  // 8 px characters plus margins
#define NOTICE_H 36
#define NOTICE_X (320 - 16 - 3 - NOTICE_W)  // top right, shadow inside the safe area
#define NOTICE_Y 16

static u32 shown_gold;
static int synced = 0;
static int notice_timer = 0;
static char notice_line1[24];
static char notice_line2[24];

void eo_format_u32(char* out, const char* prefix, u32 value, const char* suffix) {
    char digits[11];
    int n = 0;
    do {
        digits[n++] = (char)('0' + value % 10);
        value /= 10;
    } while (value != 0);
    while (*prefix) {
        *out++ = *prefix++;
    }
    while (n > 0) {
        *out++ = digits[--n];
    }
    while (*suffix) {
        *out++ = *suffix++;
    }
    *out = '\0';
}

static void copy_text(char* out, const char* text) {
    int i = 0;
    while (text[i] != '\0' && i < 23) {
        out[i] = text[i];
        i++;
    }
    out[i] = '\0';
}

// Forget the last shown amount, so a loaded save's gold isn't shown as earned.
void eo_gold_resync(void) {
    synced = 0;
    notice_timer = 0;
}

void eo_gold_notice(const char* line1, const char* line2) {
    copy_text(notice_line1, line1);
    copy_text(notice_line2, line2);
    notice_timer = NOTICE_FRAMES;
}

static void watch_gold(void) {
    u32 gold = GOLD;
    if (!synced) {
        shown_gold = gold;
        synced = 1;
        return;
    }
    if (gold == shown_gold) {
        return;
    }
    char line1[24];
    char line2[24];
    eo_format_amount(line1, "Gold", gold, NOTICE_COLUMNS, "");
    if (gold > shown_gold) {
        eo_format_amount(line2, "Won", gold - shown_gold, NOTICE_COLUMNS, "");
    } else {
        eo_format_amount(line2, "Paid", shown_gold - gold, NOTICE_COLUMNS, "");
    }
    shown_gold = gold;
    eo_gold_notice(line1, line2);
}

// hud_draw_main draws the field and battle HUD when no menu or dialogue is open.
RECOMP_HOOK_RETURN("hud_draw_main") void eo_gold_draw(void) {
    watch_gold();
    if (notice_timer <= 0) {
        return;
    }
    notice_timer--;
    // The same parchment window and font as the game's dialogue box (the font is loaded once at boot).
    eo_ui_window(NOTICE_X, NOTICE_Y, NOTICE_W, NOTICE_H);
    eo_ui_text(NOTICE_X + 10, NOTICE_Y + 7, notice_line1);
    eo_ui_text(NOTICE_X + 10, NOTICE_Y + 21, notice_line2);
}

// Boss gold (pacing plan, "Gold and shops"). Only Solvaring carries gold in the ROM (200, paid through the
// normal battle reward); the others give none, so the overhaul adds theirs on top.
static const u16 boss_gold[] = {
    0,     // 0 Solvaring (pays its own 200)
    2000,  // 1 Zelse
    1500,  // 2 Nepty
    3000,  // 3 Shilf
    3000,  // 4 Fargo
    4500,  // 5 Guilty
    5000,  // 6 Beigis
    0,     // 7 Mammon (the ending follows)
};

RECOMP_HOOK("boss_give_reward") void eo_gold_on_boss_reward(s32 boss) {
    if (boss >= 0 && boss < (s32)(sizeof(boss_gold) / sizeof(boss_gold[0]))) {
        GOLD += boss_gold[boss];
    }
}
