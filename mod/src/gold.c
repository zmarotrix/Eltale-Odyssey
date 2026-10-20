#include "eo.h"

// Shows gold on the field HUD for a few seconds whenever it changes (a won battle, a paid night),
// in the game's own HUD font. There's no permanent counter: you see your purse when it matters.

#define NOTICE_FRAMES 150
#define NOTICE_X 214
#define NOTICE_Y 18

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
    eo_format_u32(line1, "GOLD ", gold, "");
    if (gold > shown_gold) {
        eo_format_u32(line2, "+", gold - shown_gold, "");
    } else {
        eo_format_u32(line2, "-", shown_gold - gold, "");
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
    hud_draw_text(NOTICE_X, NOTICE_Y, 0, notice_line1);
    hud_draw_text(NOTICE_X, NOTICE_Y + 10, 2, notice_line2);
}
