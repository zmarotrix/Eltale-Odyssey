#include "eo.h"

// Spirit Points, attunement tiers and spell discovery (game design document, "How growth works").
//
// - Collecting a spirit still raises an element level, and now also gives 1 Spirit Point (counted from the
//   game's own spirit flags, so saves from before this get points for the spirits they already found).
// - Each element's 15 spells sit in 5 tiers by their vanilla level requirement. Tier 1 is open; attuning the
//   next tier costs Spirit Points and needs a minimum element level. All four elements to tier 5 cost 92 points
//   and the game has 98 spirits: specialise through the mid-game, master everything only at the very end.
// - Spells in attuned tiers can be cast, but undiscovered ones show as "???" in the spell wheel until their
//   combination is cast once.
//
// How it hooks in: the spell wheel and casting ask spell_check_castable, which refuses a spell whose level
// requirement is above Brian's element level; spells of unattuned tiers get an unreachable requirement. The
// wheel reads names from per-element pointer tables (0x800C0690); undiscovered entries point at "???".
// The Spirits window (D-pad up in the field) shows levels, tiers and points, and attunes.

#define SCHOOLS 4
#define SPELLS 15
#define TIERS 5
#define SPELL_STAT_TABLES ((u8**)0x800C1B14)
#define SPELL_NAME_TABLES ((const char***)0x800C0690)
#define SPELL_STAT_SIZE 0x44
#define SPELL_LEVEL_REQ 0x00   // u16
#define REQ_OPEN 1
#define REQ_LOCKED 0xFF

#define SPIRIT_FLAGS ((u8*)0x80086AE8)  // 128 bits, one per spirit
#define SPIRIT_FLAG_BYTES 16
#define ELEMENT_LEVELS ((u8*)gPlayerMainData + 0x24)  // Fire, Earth, Water, Wind

static const u8 tier_min_level[TIERS + 1] = { 0, 1, 6, 14, 22, 30 };
static const u8 tier_cost[TIERS + 1] = { 0, 0, 2, 4, 7, 10 };
static const char* const element_names[SCHOOLS] = { "Fire", "Earth", "Water", "Wind" };
static const char unknown_name[] = "???";

static u8 spell_tier[SCHOOLS][SPELLS];
static u8 spell_vanilla_level[SCHOOLS][SPELLS];
static const char* spell_names[SCHOOLS][SPELLS];  // the game's own names
static int tables_ready = 0;
static int dirty = 1;

// Tier from the vanilla level requirement: below 7, 16, 25, 36, then the rest.
static u8 tier_for_level(u16 level) {
    return level < 7 ? 1 : level < 16 ? 2 : level < 25 ? 3 : level < 36 ? 4 : 5;
}

static int is_discovered(int school, int index) {
    int bit = school * SPELLS + index;
    return (gEo.discovered[bit / 8] >> (bit % 8)) & 1;
}

int eo_spirits_known(int school, int index) {
    return is_discovered(school, index);
}

static void set_discovered(int school, int index) {
    int bit = school * SPELLS + index;
    gEo.discovered[bit / 8] |= (u8)(1 << (bit % 8));
}

// From main.c at boot, before any save is loaded: remember the vanilla tiers and names.
void eo_spirits_init_tables(void) {
    for (int s = 0; s < SCHOOLS; s++) {
        for (int i = 0; i < SPELLS; i++) {
            u8* stats = SPELL_STAT_TABLES[s] + i * SPELL_STAT_SIZE;
            u16 level = *(u16*)(stats + SPELL_LEVEL_REQ);
            spell_vanilla_level[s][i] = (u8)level;
            spell_tier[s][i] = tier_for_level(level);
            spell_names[s][i] = SPELL_NAME_TABLES[s][i];
        }
    }
    tables_ready = 1;
}

static int count_spirits(void) {
    int n = 0;
    for (int i = 0; i < SPIRIT_FLAG_BYTES; i++) {
        for (u8 b = SPIRIT_FLAGS[i]; b != 0; b &= (u8)(b - 1)) {
            n++;
        }
    }
    return n;
}

// First time this save sees the spirit system: points for spirits already found, tier 1 everywhere, and the
// spells Brian could already cast (tier 1 and within his element level) count as known.
static void set_up_save(void) {
    gEo.spirits_counted = (u8)count_spirits();
    gEo.spirit_points = gEo.spirits_counted;
    for (int s = 0; s < SCHOOLS; s++) {
        gEo.tiers[s] = 1;
    }
    for (int i = 0; i < 8; i++) {
        gEo.discovered[i] = 0;
    }
    for (int s = 0; s < SCHOOLS; s++) {
        for (int i = 0; i < SPELLS; i++) {
            if (spell_tier[s][i] == 1 && ELEMENT_LEVELS[s] >= spell_vanilla_level[s][i]) {
                set_discovered(s, i);
            }
        }
    }
    gEo.spirits_ready = 1;
    dirty = 1;
}

// Writes the tier locks and the "???" names into the game's tables.
static void apply(void) {
    for (int s = 0; s < SCHOOLS; s++) {
        for (int i = 0; i < SPELLS; i++) {
            u8* stats = SPELL_STAT_TABLES[s] + i * SPELL_STAT_SIZE;
            *(u16*)(stats + SPELL_LEVEL_REQ) = spell_tier[s][i] <= gEo.tiers[s] ? REQ_OPEN : REQ_LOCKED;
            SPELL_NAME_TABLES[s][i] = is_discovered(s, i) ? spell_names[s][i] : unknown_name;
        }
    }
    dirty = 0;
}

void eo_spirits_mark_dirty(void) {
    dirty = 1;
}

// ---- Notices ----

#define NOTICE_FRAMES 120
static int notice_timer = 0;
static char notice[40];

// A short notice at the bottom of the screen: a followed by b (for example "Learned " + a spell name).
void eo_notice(const char* a, const char* b) {
    eo_append(eo_append(notice, a), b);
    notice_timer = NOTICE_FRAMES;
}

// ---- Discovery: the first cast of a spell teaches it ----

RECOMP_HOOK("projectile_init") void eo_spirits_on_projectile(void* caster, u16 spell) {
    u32 c = (u32)caster;
    if (c >= 0x8007C998 && c < 0x8007C998 + 6 * 0x128) {
        return;  // an enemy
    }
    int s = (spell >> 8) & 0xF;
    int i = spell & 0xFF;
    if (s >= SCHOOLS || i >= SPELLS || is_discovered(s, i)) {
        return;
    }
    set_discovered(s, i);
    SPELL_NAME_TABLES[s][i] = spell_names[s][i];
    char name[24];
    eo_title_case(name, spell_names[s][i], 20);
    eo_notice("Learned ", name);
    sfx_play(SFX_CONFIRM, 0xFF, 0x40);
}

// ---- Every frame: set up saves, count new spirits ----

RECOMP_HOOK_RETURN("frame_build") void eo_spirits_frame(void) {
    if (!tables_ready || gGameMode != 1) {
        return;
    }
    if (!gEo.spirits_ready) {
        set_up_save();
    }
    int spirits = count_spirits();
    if (spirits > gEo.spirits_counted) {
        gEo.spirit_points = (u8)(gEo.spirit_points + (spirits - gEo.spirits_counted));
        gEo.spirits_counted = (u8)spirits;
        eo_notice("Spirit Point gained", "");
    }
    if (dirty) {
        apply();
    }
}

// ---- The Spirits window ----

// Small font layout, 30 characters wide.
#define WIN_W (30 * SMALL_W + 2 * SMALL_PAD)
#define WIN_X ((320 - WIN_W) / 2)
#define WIN_Y 40
#define ROW_H SMALL_LINE
#define LIST_Y (WIN_Y + SMALL_PAD + SMALL_LINE + 3)
#define INFO_Y (LIST_Y + SCHOOLS * ROW_H + 3)
#define WIN_H (INFO_Y + SMALL_LINE + SMALL_PAD - 2 - WIN_Y)
#define BTN_A 0x8000
#define BTN_B 0x4000
#define BTN_DUP 0x0800
#define BTN_DDOWN 0x0400
#define STICK_THRESHOLD 40

extern u16 gPadButtonsNew;
extern u16 gPadButtonPressed;
extern s8 gPadStickX;
extern s8 gPadStickY;
extern u16 gBattleState;

static int window_open = 0;
static int cursor = 0;
static u16 pressed = 0;
static int stick_held = 0;

int eo_spirits_window_open(void) {
    return window_open;
}

static int field_idle(void) {
    return gGameMode == 1 && !(gBattleState & 0x1) && !(gGameState & 0x3) && !eo_shop_is_open() &&
           !eo_equipment_window_open();
}

RECOMP_HOOK_RETURN("pad_read") void eo_spirits_read_input(void) {
    if (!window_open) {
        if (gEo.spirits_ready && field_idle() && (gPadButtonsNew & BTN_DUP)) {
            window_open = 1;
            cursor = 0;
            pressed = 0;
            gPadButtonsNew = 0;
            sfx_play(SFX_CONFIRM, 0xFF, 0x40);
        }
        return;
    }
    pressed |= gPadButtonsNew;
    if (gPadStickY > STICK_THRESHOLD || gPadStickY < -STICK_THRESHOLD) {
        if (!stick_held) {
            pressed |= gPadStickY > 0 ? BTN_DUP : BTN_DDOWN;
        }
        stick_held = 1;
    } else {
        stick_held = 0;
    }
    gPadButtonsNew = 0;
    gPadButtonPressed = 0;
    gPadStickX = 0;
    gPadStickY = 0;
}

static void try_attune(int s) {
    int next = gEo.tiers[s] + 1;
    if (next > TIERS || ELEMENT_LEVELS[s] < tier_min_level[next] || gEo.spirit_points < tier_cost[next]) {
        sfx_play(SFX_CANCEL, 0xFF, 0x40);
        return;
    }
    gEo.spirit_points = (u8)(gEo.spirit_points - tier_cost[next]);
    gEo.tiers[s] = (u8)next;
    dirty = 1;
    sfx_play(SFX_CONFIRM, 0xFF, 0x40);
}

static void handle_input(void) {
    u16 p = pressed;
    pressed = 0;
    if (p & BTN_B) {
        window_open = 0;
        sfx_play(SFX_CANCEL, 0xFF, 0x40);
    } else if (p & BTN_DUP) {
        cursor = (cursor + SCHOOLS - 1) % SCHOOLS;
    } else if (p & BTN_DDOWN) {
        cursor = (cursor + 1) % SCHOOLS;
    } else if (p & BTN_A) {
        try_attune(cursor);
    }
}

static void draw_window(void) {
    char line[40];
    eo_ui_window(WIN_X, WIN_Y, WIN_W, WIN_H);
    eo_ui_small(WIN_X + SMALL_PAD, WIN_Y + SMALL_PAD, TEXT_NORMAL, "SPIRITS");
    eo_format_amount(line, "POINTS", gEo.spirit_points, 10, "");
    eo_ui_small(WIN_X + WIN_W - SMALL_PAD - 10 * SMALL_W, WIN_Y + SMALL_PAD, TEXT_NORMAL, line);
    s32 x = WIN_X + SMALL_PAD + 2 * SMALL_W;
    for (int s = 0; s < SCHOOLS; s++) {
        s32 y = LIST_Y + s * ROW_H;
        int palette = s == cursor ? TEXT_GOOD : TEXT_NORMAL;
        eo_ui_small(x, y, palette, element_names[s]);
        eo_format_amount(line, "LV", ELEMENT_LEVELS[s], 5, "");
        eo_ui_small(x + 7 * SMALL_W, y, palette, line);
        eo_format_amount(line, "TIER", gEo.tiers[s], 6, "");
        eo_ui_small(x + 14 * SMALL_W, y, palette, line);
    }

    // What the next tier of the selected element needs.
    int next = gEo.tiers[cursor] + 1;
    if (next > TIERS) {
        eo_ui_small(WIN_X + SMALL_PAD, INFO_Y, TEXT_DIM, "FULLY ATTUNED.");
    } else {
        char num[8];
        char* out = eo_append(line, "TIER ");
        eo_format_u32(num, "", (u32)next, "");
        out = eo_append(out, num);
        out = eo_append(out, ": LV ");
        eo_format_u32(num, "", tier_min_level[next], "");
        out = eo_append(out, num);
        out = eo_append(out, " AND ");
        eo_format_u32(num, "", tier_cost[next], "");
        out = eo_append(out, num);
        eo_append(out, " POINTS");
        int ready = ELEMENT_LEVELS[cursor] >= tier_min_level[next] && gEo.spirit_points >= tier_cost[next];
        eo_ui_small(WIN_X + SMALL_PAD, INFO_Y, ready ? TEXT_GOOD : TEXT_NORMAL, line);
    }
    eo_icons_begin();
    eo_icon_draw(ICON_POINTER, WIN_X + SMALL_PAD - 1, LIST_Y + cursor * ROW_H - 2);
}

#define NOTICE_Y 182

RECOMP_HOOK_RETURN("hud_draw_main") void eo_spirits_draw(void) {
    if (window_open) {
        handle_input();
        if (window_open) {
            draw_window();
        }
    }
    if (notice_timer > 0) {
        notice_timer--;
        s32 w = hud_strlen(notice) * SMALL_W + 2 * SMALL_PAD;
        s32 x = (320 - w) / 2;
        eo_ui_window(x, NOTICE_Y, w, SMALL_LINE + 2 * SMALL_PAD - 2);
        eo_ui_small(x + SMALL_PAD, NOTICE_Y + SMALL_PAD, TEXT_GOOD, notice);
    }
}
