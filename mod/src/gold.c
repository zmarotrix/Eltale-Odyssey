#include "eo.h"

// Shows gold in a small window for a few seconds whenever it changes (a won battle, a paid night),
// in the dialogue box's style. There's no permanent counter: you see your purse when it matters.

#define NOTICE_FRAMES 150
#define NOTICE_W (NOTICE_COLUMNS * SMALL_W + 2 * SMALL_PAD)  // small font
#define NOTICE_H (2 * SMALL_LINE + 2 * SMALL_PAD - 2)
#define NOTICE_X (320 - 16 - 3 - NOTICE_W)  // top right, shadow inside the safe area
#define NOTICE_Y 16

static u32 shown_gold;
static int synced = 0;
static int notice_timer = 0;
static char notice_line1[24];
static char notice_line2[24];
static int notice_coins = 0;  // a gold notice: a coin before the total
static int notice_spent = 0;  // ...and the change in red

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
    notice_coins = 0;
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
    // A coin and the total on top, the change below it: green for winnings, red with "-" for spending.
    eo_format_u32(line1, "", gold, "");
    eo_format_u32(line2, gold > shown_gold ? "" : "-", gold > shown_gold ? gold - shown_gold : shown_gold - gold, "");
    notice_spent = gold < shown_gold;
    shown_gold = gold;
    eo_gold_notice(line1, line2);
    notice_coins = 1;
}

// hud_draw_main draws the field and battle HUD when no menu or dialogue is open.
RECOMP_HOOK_RETURN("hud_draw_main") void eo_gold_draw(void) {
    watch_gold();
    if (notice_timer <= 0) {
        return;
    }
    notice_timer--;
    // The game's parchment window, with the HUD's small font; a gold notice is just wide enough for its
    // numbers and coin. Both lines right-aligned, against the screen's top right.
    s32 w = NOTICE_W;
    if (notice_coins) {
        s32 len = eo_strlen(notice_line1) > eo_strlen(notice_line2) ? eo_strlen(notice_line1) : eo_strlen(notice_line2);
        w = len * SMALL_W + ICON_W + 2 * SMALL_PAD;
    }
    s32 right = NOTICE_X + NOTICE_W - SMALL_PAD;
    eo_ui_window(right + SMALL_PAD - w, NOTICE_Y, w, NOTICE_H);
    s32 x1 = right - eo_strlen(notice_line1) * SMALL_W;
    eo_ui_small(x1, NOTICE_Y + SMALL_PAD, TEXT_NORMAL, notice_line1);
    eo_ui_small(right - eo_strlen(notice_line2) * SMALL_W, NOTICE_Y + SMALL_PAD + SMALL_LINE,
                notice_coins && notice_spent ? TEXT_WARN : TEXT_GOOD, notice_line2);
    if (notice_coins) {
        eo_icons_begin();
        eo_icon_draw(ICON_COIN, x1 - ICON_W, NOTICE_Y + SMALL_PAD + ICON_TEXT_DY);
    }
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

// Charms and sigils are found, not bought: bosses carry them (pacing plan, "Build pieces"; first pass).
static const s8 boss_piece[] = {
    EQ_GAMBLERS_COIN,  // 0 Solvaring (and the charm slot opens)
    EQ_LEECH_PILLAR,   // 1 Zelse (and the sigil slot opens)
    EQ_TIDAL_ECHO,     // 2 Nepty
    -1,                // 3 Shilf
    -1,                // 4 Fargo
    EQ_MIRROR_SHARD,   // 5 Guilty
    -1,                // 6 Beigis
    -1,                // 7 Mammon
};

RECOMP_HOOK("boss_give_reward") void eo_gold_on_boss_reward(s32 boss) {
    if (boss < 0 || boss >= (s32)(sizeof(boss_gold) / sizeof(boss_gold[0]))) {
        return;
    }
    GOLD += boss_gold[boss];
    int piece = boss_piece[boss];
    if (piece >= 0 && !eo_equipment_owned(piece)) {
        eo_equipment_give(piece);
        eo_notice("Found ", eo_equipment_info(piece)->name);
    }
}

// Defeat costs gold (game design document, "Old-school friction"): falling in battle sends Brian back to the
// last inn with half his gold, as in Dragon Quest. Seen as HP reaching 0, then coming back (the respawn).
static int defeated = 0;

RECOMP_HOOK_RETURN("frame_build") void eo_gold_check_defeat(void) {
    if (gGameMode != 1) {
        return;
    }
    if (((u16*)gPlayerMainData)[2] == 0) {
        defeated = 1;
        return;
    }
    if (!defeated) {
        return;
    }
    defeated = 0;
    u32 lost = GOLD / 2;
    GOLD -= lost;
    char line1[24];
    char line2[24];
    eo_gold_resync();
    eo_format_amount(line1, "Gold", GOLD, NOTICE_COLUMNS, "");
    eo_format_amount(line2, "Lost", lost, NOTICE_COLUMNS, "");
    eo_gold_notice(line1, line2);
}
