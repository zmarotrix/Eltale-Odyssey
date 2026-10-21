#include "eo.h"

// Spirit Points, attunement tiers and spell discovery (game design document, "How growth works").
//
// - Collecting a spirit still raises an element level, and now also gives 1 Spirit Point (counted from the
//   game's own spirit flags, so saves from before this get points for the spirits they already found).
// - Each element's 15 spells sit in 5 tiers by their vanilla level requirement. Tier 1 is open; attuning the
//   next tier costs Spirit Points and needs the element level vanilla asked for the tier's first spell, so no
//   spell comes earlier than its tier would in vanilla (balance pass, 2026-10-21: gates of 6/14/22/30 handed a
//   focused Brian level-36 spells at 30). One element to tier 5 costs 34 points, all four 136; the game has 98
//   spirits and talents spend the same points: master one or two elements, never everything.
// - Tier 1 spells are known as soon as Brian's element level reaches their vanilla requirement (playtest,
//   2026-10-21: "only new tiers need discovering"). Spells in tiers attuned later can be cast, but show as "???"
//   in the spell wheel until their combination is cast once.
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

static const u8 tier_min_level[TIERS + 1] = { 0, 1, 7, 16, 25, 36 };   // matches tier_for_level
static const u8 tier_cost[TIERS + 1] = { 0, 0, 3, 6, 10, 15 };
static const char* const element_names[SCHOOLS] = { "Fire", "Earth", "Water", "Wind" };
static const char unknown_name[] = "???";

static u8 spell_tier[SCHOOLS][SPELLS];
static u8 spell_vanilla_level[SCHOOLS][SPELLS];
static const char* spell_names[SCHOOLS][SPELLS];  // each spell's name (spell_text.c)
static int tables_ready = 0;
static int dirty = 1;

// Tier from the vanilla level requirement: below 7, 16, 25, 36, then the rest.
static u8 tier_for_level(u16 level) {
    return level < 7 ? 1 : level < 16 ? 2 : level < 25 ? 3 : level < 36 ? 4 : 5;
}

static int is_discovered(int school, int index) {
    int bit = school * SPELLS + index;
    if (spell_tier[school][index] == 1 && ELEMENT_LEVELS[school] >= spell_vanilla_level[school][index]) {
        return 1;   // a tier 1 spell Brian's level allows
    }
    return (gEo.discovered[bit / 8] >> (bit % 8)) & 1;
}

int eo_spirits_known(int school, int index) {
    return is_discovered(school, index);
}

static void set_discovered(int school, int index) {
    int bit = school * SPELLS + index;
    gEo.discovered[bit / 8] |= (u8)(1 << (bit % 8));
}

// From main.c at boot, before any save is loaded: remember the vanilla tiers, and use the overhaul's spell names
// (spell_text.c) in place of the game's "... LV1" ones.
void eo_spirits_init_tables(void) {
    for (int s = 0; s < SCHOOLS; s++) {
        for (int i = 0; i < SPELLS; i++) {
            u8* stats = SPELL_STAT_TABLES[s] + i * SPELL_STAT_SIZE;
            u16 level = *(u16*)(stats + SPELL_LEVEL_REQ);
            spell_vanilla_level[s][i] = (u8)level;
            spell_tier[s][i] = tier_for_level(level);
            spell_names[s][i] = eo_spell_name(s, i);
            SPELL_NAME_TABLES[s][i] = spell_names[s][i];
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
    eo_sfx(SFX_CONFIRM);
}

// ---- Hint scrolls (shop.c; pacing plan: the Elementalist's starter, sold by every shop from beat 1) ----

#define SPELL_INPUT_COUNT 0x04   // u16, 1-3 presses
#define SPELL_PRESSES 0x06       // u8 x 3: 0 Fire, 1 Earth, 2 Water, 3 Wind

// Teaches one spell Brian can already cast (its tier is attuned) but hasn't discovered, picked at random, lowest
// tier first so scrolls lead the way through a new tier. Writes how to cast it into out ("CAST IT: ...", 31
// characters at most) and shows "Learned <name>". Returns 0, writing nothing, when there's nothing left to teach.
int eo_spirits_hint(char* out, u32 seed) {
    static const char* const press_names[SCHOOLS] = { "FIRE", "EARTH", "WATER", "WIND" };
    for (int tier = 1; tier <= TIERS; tier++) {
        int candidates = 0;
        for (int s = 0; s < SCHOOLS; s++) {
            for (int i = 0; i < SPELLS; i++) {
                if (spell_tier[s][i] == tier && tier <= gEo.tiers[s] && !is_discovered(s, i)) {
                    candidates++;
                }
            }
        }
        if (candidates == 0) {
            continue;
        }
        int pick = (int)(seed % (u32)candidates);
        for (int s = 0; s < SCHOOLS; s++) {
            for (int i = 0; i < SPELLS; i++) {
                if (spell_tier[s][i] != tier || tier > gEo.tiers[s] || is_discovered(s, i) || pick-- != 0) {
                    continue;
                }
                u8* stats = SPELL_STAT_TABLES[s] + i * SPELL_STAT_SIZE;
                u16 count = *(u16*)(stats + SPELL_INPUT_COUNT);
                out[0] = '\0';
                char* end = eo_append(out, "CAST IT:");
                for (u16 k = 0; k < count && k < 3; k++) {
                    u8 press = stats[SPELL_PRESSES + k];
                    end = eo_append(eo_append(end, " "), press < SCHOOLS ? press_names[press] : "?");
                }
                set_discovered(s, i);
                SPELL_NAME_TABLES[s][i] = spell_names[s][i];
                char name[24];
                eo_title_case(name, spell_names[s][i], 20);
                eo_notice("Learned ", name);
                return 1;
            }
        }
    }
    return 0;
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
    // A level-up can make tier 1 spells known: refresh the names when any element level changes.
    static u8 seen_levels[SCHOOLS];
    for (int s = 0; s < SCHOOLS; s++) {
        if (ELEMENT_LEVELS[s] != seen_levels[s]) {
            seen_levels[s] = ELEMENT_LEVELS[s];
            dirty = 1;
        }
    }
    if (dirty) {
        apply();
    }
}

// ---- The Spirits window ----

// Small font layout, 38 characters wide. Rows are taller than text lines to fit the element gems.
#define WIN_W (38 * SMALL_W + 2 * SMALL_PAD)
#define WIN_X ((320 - WIN_W) / 2)
#define WIN_Y 40
#define ROW_H 13
#define GEM_X (WIN_X + SMALL_PAD + ICON_W)
#define NAME_X (GEM_X + ICON_W + 2)
#define LEVEL_X (NAME_X + 7 * SMALL_W)
#define PIPS_X (LEVEL_X + 6 * SMALL_W + 4)
#define PIP_STEP 8
#define LIST_Y (WIN_Y + SMALL_PAD + SMALL_LINE + 3)
#define INFO_Y (LIST_Y + SCHOOLS * ROW_H + 3)
#define WIN_H (INFO_Y + SMALL_LINE + SMALL_PAD - 2 - WIN_Y)
#define BTN_A 0x8000
#define BTN_B 0x4000
#define BTN_DUP 0x0800
#define BTN_DDOWN 0x0400
#define BTN_DLEFT 0x0200
#define BTN_DRIGHT 0x0100
// Pages (D-pad left / right): attunement, then one talent tree per element, one 12-pixel row per talent.
#define PAGES 5
#define TALENT_ROW_H 12
#define TALENT_LIST_Y LIST_Y
#define TALENT_INFO_Y (TALENT_LIST_Y + TALENTS_PER_SCHOOL * TALENT_ROW_H + 3)
#define TALENT_WIN_H (TALENT_INFO_Y + SMALL_LINE + SMALL_PAD - 2 - WIN_Y)
#define RANK_X (NAME_X + 15 * SMALL_W)     // rank pips
#define RANK_STEP 8
#define TALENT_GATE_X (RANK_X + 3 * RANK_STEP + 6)
#define STICK_THRESHOLD 40

extern u16 gPadButtonsNew;
extern u16 gPadButtonPressed;
extern s8 gPadStickX;
extern s8 gPadStickY;
extern u16 gBattleState;

static int window_open = 0;
static int page = 0;          // 0 attunement, 1-4 the Fire, Earth, Water and Wind talent trees
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
            page = 0;
            cursor = 0;
            pressed = 0;
            gPadButtonsNew = 0;
            eo_sfx(SFX_CONFIRM);
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
        eo_sfx(SFX_CANCEL);
        return;
    }
    gEo.spirit_points = (u8)(gEo.spirit_points - tier_cost[next]);
    gEo.tiers[s] = (u8)next;
    dirty = 1;
    eo_sfx(SFX_CONFIRM);
}

static void handle_input(void) {
    u16 p = pressed;
    pressed = 0;
    int rows = page == 0 ? SCHOOLS : TALENTS_PER_SCHOOL;
    if (p & BTN_B) {
        window_open = 0;
        eo_sfx(SFX_CANCEL);
    } else if (p & (BTN_DLEFT | BTN_DRIGHT)) {
        page = (page + ((p & BTN_DRIGHT) ? 1 : PAGES - 1)) % PAGES;
        cursor = 0;
        eo_sfx(SFX_CONFIRM);
    } else if (p & BTN_DUP) {
        cursor = (cursor + rows - 1) % rows;
    } else if (p & BTN_DDOWN) {
        cursor = (cursor + 1) % rows;
    } else if (p & BTN_A) {
        if (page == 0) {
            try_attune(cursor);
        } else if (eo_talent_learn((page - 1) * TALENTS_PER_SCHOOL + cursor)) {
            eo_sfx(SFX_CONFIRM);
        } else {
            eo_sfx(SFX_CANCEL);
        }
    }
}

// The window's top line: the page names (the open one green) and the unspent points at the right.
static const char* const page_names[PAGES] = { "ATTUNE", "FIRE", "EARTH", "WATER", "WIND" };

static s32 draw_header(s32 h) {
    char line[12];
    eo_ui_window(WIN_X, WIN_Y, WIN_W, h);
    s32 x = WIN_X + SMALL_PAD;
    for (int i = 0; i < PAGES; i++) {
        eo_ui_small(x, WIN_Y + SMALL_PAD, page == i ? TEXT_GOOD : TEXT_DIM, page_names[i]);
        x += (eo_strlen(page_names[i]) + 2) * SMALL_W;
    }
    eo_format_u32(line, "", gEo.spirit_points, "");
    s32 points_x = WIN_X + WIN_W - SMALL_PAD - eo_strlen(line) * SMALL_W;
    eo_ui_small(points_x, WIN_Y + SMALL_PAD, TEXT_NORMAL, line);
    return points_x;
}

// A talent tree: each talent with its rank pips, then MAX, its level gate (red until met) or its cost per rank
// (green when affordable, red when not); the bottom line says what the selected one does.
static void draw_talents(void) {
    char line[16];
    int school = page - 1;
    int first = school * TALENTS_PER_SCHOOL;
    s32 points_x = draw_header(TALENT_WIN_H);
    for (int i = 0; i < TALENTS_PER_SCHOOL; i++) {
        const TalentInfo* t = eo_talent_info(first + i);
        int rank = eo_talent(first + i);
        int open = ELEMENT_LEVELS[school] >= t->level;
        s32 y = TALENT_LIST_Y + i * TALENT_ROW_H + 2;
        eo_ui_small(NAME_X, y, i == cursor ? TEXT_GOOD : (rank > 0 || open) ? TEXT_NORMAL : TEXT_DIM, t->name);
        if (rank >= t->ranks) {
            eo_ui_small(TALENT_GATE_X, y, TEXT_GOOD, "MAX");
        } else if (!open) {
            eo_format_u32(line, "LV ", t->level, "");
            eo_ui_small(TALENT_GATE_X, y, TEXT_WARN, line);
        } else {
            eo_format_u32(line, "", t->cost, "");
            eo_ui_small(TALENT_GATE_X + ICON_W, y, gEo.spirit_points >= t->cost ? TEXT_GOOD : TEXT_WARN, line);
        }
    }
    eo_ui_small(WIN_X + SMALL_PAD, TALENT_INFO_Y, TEXT_NORMAL, eo_talent_info(first + cursor)->effect);

    eo_icons_begin();
    eo_icon_draw(ICON_PIP_FULL, points_x - ICON_W, WIN_Y + SMALL_PAD + ICON_TEXT_DY);
    for (int i = 0; i < TALENTS_PER_SCHOOL; i++) {
        const TalentInfo* t = eo_talent_info(first + i);
        int rank = eo_talent(first + i);
        s32 y = TALENT_LIST_Y + i * TALENT_ROW_H + 2 + ICON_TEXT_DY;
        eo_icon_draw(ICON_FIRE + school, GEM_X, y);
        for (int r = 0; r < t->ranks; r++) {   // learned ranks filled
            eo_icon_draw(r < rank ? ICON_PIP_FULL : ICON_PIP_EMPTY, RANK_X + r * RANK_STEP, y);
        }
        if (rank < t->ranks && ELEMENT_LEVELS[school] >= t->level) {
            eo_icon_draw(ICON_PIP_FULL, TALENT_GATE_X, y);
        }
    }
    eo_icon_draw(ICON_POINTER, WIN_X + SMALL_PAD - 1, TALENT_LIST_Y + cursor * TALENT_ROW_H + 2 + ICON_TEXT_DY);
}

// Text y of an attunement row: centered on the 13-pixel row.
static s32 row_text_y(int s) {
    return LIST_Y + s * ROW_H + 3;
}

static void draw_window(void) {
    char line[40];
    if (page > 0) {
        draw_talents();
        return;
    }
    s32 points_x = draw_header(WIN_H);

    for (int s = 0; s < SCHOOLS; s++) {
        s32 y = row_text_y(s);
        int palette = s == cursor ? TEXT_GOOD : TEXT_NORMAL;
        eo_ui_small(NAME_X, y, palette, element_names[s]);
        eo_format_u32(line, "LV ", ELEMENT_LEVELS[s], "");
        eo_ui_small(LEVEL_X, y, palette, line);
    }

    // The next tier of the selected element: its level gate and point cost, each green once met, red if not.
    int next = gEo.tiers[cursor] + 1;
    s32 cost_x = 0;
    if (next > TIERS) {
        eo_ui_small(WIN_X + SMALL_PAD, INFO_Y, TEXT_DIM, "FULLY ATTUNED.");
    } else {
        eo_format_u32(line, "NEXT TIER: LV ", tier_min_level[next], "");
        int level_ok = ELEMENT_LEVELS[cursor] >= tier_min_level[next];
        eo_ui_small(WIN_X + SMALL_PAD, INFO_Y, level_ok ? TEXT_GOOD : TEXT_WARN, line);
        cost_x = WIN_X + SMALL_PAD + (eo_strlen(line) + 1) * SMALL_W;
        eo_format_u32(line, "", tier_cost[next], "");
        int points_ok = gEo.spirit_points >= tier_cost[next];
        eo_ui_small(cost_x + ICON_W, INFO_Y, points_ok ? TEXT_GOOD : TEXT_WARN, line);
    }

    eo_icons_begin();
    eo_icon_draw(ICON_PIP_FULL, points_x - ICON_W, WIN_Y + SMALL_PAD + ICON_TEXT_DY);
    for (int s = 0; s < SCHOOLS; s++) {
        s32 y = row_text_y(s) + ICON_TEXT_DY;
        eo_icon_draw(ICON_FIRE + s, GEM_X, y);
        for (int t = 1; t <= TIERS; t++) {   // attuned tiers filled
            eo_icon_draw(t <= gEo.tiers[s] ? ICON_PIP_FULL : ICON_PIP_EMPTY, PIPS_X + (t - 1) * PIP_STEP, y);
        }
    }
    if (cost_x != 0) {
        eo_icon_draw(ICON_PIP_FULL, cost_x, INFO_Y + ICON_TEXT_DY);
    }
    eo_icon_draw(ICON_POINTER, WIN_X + SMALL_PAD - 1, row_text_y(cursor) + ICON_TEXT_DY);
}

#define NOTICE_Y 182

static void eo_spirits_draw_body(void);

// Drawn into the mod's own display list (ui.c).
RECOMP_HOOK_RETURN("hud_draw_main") void eo_spirits_draw(void) {
    eo_ui_begin();
    eo_spirits_draw_body();
    eo_ui_end();
}

static void eo_spirits_draw_body(void) {
    if (window_open) {
        handle_input();
        if (window_open) {
            draw_window();
        }
    }
    if (notice_timer > 0) {
        notice_timer--;
        s32 w = eo_strlen(notice) * SMALL_W + 2 * SMALL_PAD;
        s32 x = (320 - w) / 2;
        eo_ui_window(x, NOTICE_Y, w, SMALL_LINE + 2 * SMALL_PAD - 2);
        eo_ui_small(x + SMALL_PAD, NOTICE_Y + SMALL_PAD, TEXT_GOOD, notice);
    }
}
