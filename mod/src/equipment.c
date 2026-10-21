#include "eo.h"

// Equipment (game design document, "Equipment" and "Charms and sigils"): a staff and a robe that add to
// Brian's stats, a charm that bends a rule of combat and a sigil that reshapes his magic (neither gives
// stats). Pieces are the overhaul's own, kept in the save block; staffs and robes are sold in shops, charms and
// sigils come from bosses (and one rare shop find). The charm slot opens after Solvaring, the sigil slot after
// Zelse. Equipment can be changed only at an inn: D-pad down in the field opens the window anywhere to look.
//
// The effects live where the game's numbers are handled (combat.c, status.c); this file holds the catalog,
// what Brian owns and wears, and the window.

enum { SLOT_STAFF, SLOT_ROBE, SLOT_CHARM, SLOT_SIGIL, SLOT_COUNT };

static const EquipmentInfo catalog[EQUIPMENT_COUNT] = {
    [EQ_EMBER_STAFF]    = { "Ember Staff",       SLOT_STAFF, 900,  "Staff 6 more, Fire 1.1x, Burns." },
    [EQ_HEAVY_STAFF]    = { "Heavy Staff",       SLOT_STAFF, 700,  "Staff 12 more." },
    [EQ_STONE_STAFF]    = { "Stone Staff",       SLOT_STAFF, 800,  "Staff 3 more, Earth 1.1x." },
    [EQ_TIDE_STAFF]     = { "Tide Staff",        SLOT_STAFF, 800,  "Staff 3 more, Water 1.1x." },
    [EQ_GALE_STAFF]     = { "Gale Staff",        SLOT_STAFF, 800,  "Staff 3 more, Wind 1.1x." },
    [EQ_TRAVELERS_ROBE] = { "Traveler's Robe",   SLOT_ROBE,  300,  "Defense 4 more." },
    [EQ_TIDECLOTH_ROBE] = { "Tidecloth Robe",    SLOT_ROBE,  1200, "Defense 8 more, no Poison." },
    [EQ_PILGRIMS_SOUL]  = { "Pilgrim's Soul",    SLOT_CHARM, 2500, "Half-cost spells, no staff MP." },
    [EQ_GAMBLERS_COIN]  = { "Gambler's Coin",    SLOT_CHARM, 0,    "Casts cost no MP or double." },
    [EQ_MIRROR_SHARD]   = { "Mirror Shard",      SLOT_CHARM, 0,    "First hit each fight reflects." },
    [EQ_TIDAL_ECHO]     = { "Tidal Echo",        SLOT_SIGIL, 0,    "Water spells cast twice, 0.75x." },
    [EQ_LEECH_PILLAR]   = { "Leech Pillar",      SLOT_SIGIL, 0,    "Water Pillar drains a quarter." },
};

const EquipmentInfo* eo_equipment_info(int piece) {
    return piece >= 0 && piece < EQUIPMENT_COUNT ? &catalog[piece] : 0;
}

int eo_equipment_owned(int piece) {
    return (gEo.owned_equipment >> piece) & 1;
}

int eo_equipment_worn(int piece) {
    const EquipmentInfo* e = eo_equipment_info(piece);
    return e != 0 && gEo.equipped[e->slot] == piece + 1;
}

static int worn_in(int slot) {
    return gEo.equipped[slot] - 1;  // -1 = none
}

void eo_equipment_give(int piece) {
    gEo.owned_equipment |= 1u << piece;
}

// ---- Effects the rest of the mod asks about ----

int eo_equip_staff_bonus(void) {
    switch (worn_in(SLOT_STAFF)) {
        case EQ_EMBER_STAFF: return 6;
        case EQ_HEAVY_STAFF: return 12;
        case EQ_STONE_STAFF:
        case EQ_TIDE_STAFF:
        case EQ_GALE_STAFF: return 3;
        default: return 0;
    }
}

int eo_equip_element_percent(int element) {
    int staff = worn_in(SLOT_STAFF);
    int favoured = staff == EQ_EMBER_STAFF ? 0 : staff == EQ_STONE_STAFF ? 1 : staff == EQ_TIDE_STAFF ? 2 :
                   staff == EQ_GALE_STAFF ? 3 : -1;
    return favoured == element ? 10 : 0;
}

int eo_equip_staff_burn_chance(void) {
    return worn_in(SLOT_STAFF) == EQ_EMBER_STAFF ? 10 : 0;
}

int eo_equip_defense(void) {
    switch (worn_in(SLOT_ROBE)) {
        case EQ_TRAVELERS_ROBE: return 4;
        case EQ_TIDECLOTH_ROBE: return 8;
        default: return 0;
    }
}

int eo_equip_poison_immune(void) {
    return worn_in(SLOT_ROBE) == EQ_TIDECLOTH_ROBE;
}

// ---- Robe defense: added to Brian's battle copy of Defense when a battle starts ----

#define BRIAN_BATTLE_DEFENSE (*(u16*)0x8007BBC0)

RECOMP_HOOK_RETURN("player_battle_init") void eo_equipment_on_battle_init(void) {
    BRIAN_BATTLE_DEFENSE = (u16)(BRIAN_BATTLE_DEFENSE + eo_equip_defense());
}

// ---- Slots open with the story ----

s32 boss_is_defeated(s32 boss);
#define BOSS_SOLVARING 0
#define BOSS_ZELSE 1

static int slot_open(int slot) {
    if (slot == SLOT_CHARM) {
        return boss_is_defeated(BOSS_SOLVARING) != 0;
    }
    if (slot == SLOT_SIGIL) {
        return boss_is_defeated(BOSS_ZELSE) != 0;
    }
    return 1;
}

// ---- The window: D-pad down in the field ----

extern u16 gPadButtonsNew;
extern u16 gPadButtonPressed;
extern s8 gPadStickX;
extern s8 gPadStickY;
extern u16 gBattleState;

#define BTN_A 0x8000
#define BTN_B 0x4000
#define BTN_DUP 0x0800
#define BTN_DDOWN 0x0400
#define STICK_THRESHOLD 40

// Small font layout, 32 characters wide (the longest effect line is 31).
#define WIN_W (32 * SMALL_W + 2 * SMALL_PAD)
#define WIN_X ((320 - WIN_W) / 2)
#define WIN_Y 32
#define ROW_H 12   // room for the slot icons
#define ICON_X (WIN_X + SMALL_PAD + ICON_W)
#define TEXT_X (ICON_X + ICON_W + 1)

static const char* const slot_names[SLOT_COUNT] = { "Staff", "Robe", "Charm", "Sigil" };

static int window_open = 0;
static int choosing = 0;      // 0: picking a slot, 1: picking a piece for it
static int cursor = 0;
static int slot_cursor = 0;
static u16 pressed = 0;
static int stick_held = 0;
static int choices[EQUIPMENT_COUNT + 1];
static int choice_count = 0;
static const char* message = 0;

static int field_idle(void) {
    return gGameMode == 1 && !(gBattleState & 0x1) && !(gGameState & 0x3) && !eo_shop_is_open() &&
           !eo_spirits_window_open();
}

int eo_equipment_window_open(void) {
    return window_open;
}

RECOMP_HOOK_RETURN("pad_read") void eo_equipment_read_input(void) {
    if (!window_open) {
        if (gEo.spirits_ready && field_idle() && (gPadButtonsNew & BTN_DDOWN)) {
            window_open = 1;
            choosing = 0;
            cursor = 0;
            pressed = 0;
            message = 0;
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

static void list_choices(int slot) {
    choice_count = 0;
    choices[choice_count++] = -1;  // "Nothing"
    for (int p = 0; p < EQUIPMENT_COUNT; p++) {
        if (catalog[p].slot == slot && eo_equipment_owned(p)) {
            choices[choice_count++] = p;
        }
    }
}

static void handle_input(void) {
    u16 p = pressed;
    pressed = 0;
    int count = choosing ? choice_count : SLOT_COUNT;
    if (p & BTN_B) {
        sfx_play(SFX_CANCEL, 0xFF, 0x40);
        if (choosing) {
            choosing = 0;
            cursor = slot_cursor;
        } else {
            window_open = 0;
        }
        message = 0;
        return;
    }
    if (p & BTN_DUP) {
        cursor = (cursor + count - 1) % count;
        message = 0;
    } else if (p & BTN_DDOWN) {
        cursor = (cursor + 1) % count;
        message = 0;
    } else if (p & BTN_A) {
        if (!choosing) {
            if (!slot_open(cursor)) {
                sfx_play(SFX_CANCEL, 0xFF, 0x40);
                message = "This slot isn't open yet.";
            } else if (!eo_inn_here()) {
                sfx_play(SFX_CANCEL, 0xFF, 0x40);
                message = "Change equipment at an inn.";
            } else {
                slot_cursor = cursor;
                list_choices(cursor);
                choosing = 1;
                cursor = 0;
                sfx_play(SFX_CONFIRM, 0xFF, 0x40);
            }
        } else {
            gEo.equipped[slot_cursor] = (u8)(choices[cursor] + 1);
            choosing = 0;
            cursor = slot_cursor;
            sfx_play(SFX_CONFIRM, 0xFF, 0x40);
        }
    }
}

static void draw_window(void) {
    int rows = choosing ? choice_count : SLOT_COUNT;
    s32 list_y = WIN_Y + SMALL_PAD + SMALL_LINE + 3;
    s32 info_y = list_y + rows * ROW_H + 3;
    eo_ui_window(WIN_X, WIN_Y, WIN_W, info_y + SMALL_LINE + SMALL_PAD - 2 - WIN_Y);
    // Rows: the slot's icon (a lock while it is closed), then the slot and the piece in it. Text sits 2 pixels
    // down a 12-pixel row.
    if (!choosing) {
        eo_ui_small(WIN_X + SMALL_PAD, WIN_Y + SMALL_PAD, TEXT_NORMAL, "EQUIPMENT");
        for (int s = 0; s < SLOT_COUNT; s++) {
            s32 y = list_y + s * ROW_H + 2;
            int open = slot_open(s);
            int palette = !open ? TEXT_DIM : s == cursor ? TEXT_GOOD : TEXT_NORMAL;
            int worn = worn_in(s);
            eo_ui_small(TEXT_X, y, palette, slot_names[s]);
            eo_ui_small(TEXT_X + 7 * SMALL_W, y, palette, !open ? "LOCKED" : worn >= 0 ? catalog[worn].name : "-");
        }
    } else {
        eo_ui_small(WIN_X + SMALL_PAD, WIN_Y + SMALL_PAD, TEXT_NORMAL, slot_names[slot_cursor]);
        for (int i = 0; i < choice_count; i++) {
            int palette = i == cursor ? TEXT_GOOD : TEXT_NORMAL;
            eo_ui_small(TEXT_X, list_y + i * ROW_H + 2, palette, choices[i] < 0 ? "NOTHING" : catalog[choices[i]].name);
        }
    }

    // Bottom line: a message, or what the highlighted piece does.
    if (message != 0) {
        eo_ui_small(WIN_X + SMALL_PAD, info_y, TEXT_WARN, message);
    } else {
        int piece = choosing ? choices[cursor] : worn_in(cursor);
        eo_ui_small(WIN_X + SMALL_PAD, info_y, TEXT_NORMAL, piece >= 0 ? catalog[piece].effect : "");
    }
    eo_icons_begin();
    for (int i = 0; i < rows; i++) {
        s32 y = list_y + i * ROW_H + 2 + ICON_TEXT_DY;
        if (!choosing) {
            eo_icon_draw(slot_open(i) ? ICON_STAFF + i : ICON_LOCK, ICON_X, y);
        } else if (choices[i] >= 0) {
            eo_icon_draw(ICON_STAFF + slot_cursor, ICON_X, y);
        }
    }
    eo_icon_draw(ICON_POINTER, WIN_X + SMALL_PAD - 1, list_y + cursor * ROW_H + 2 + ICON_TEXT_DY);
}

RECOMP_HOOK_RETURN("hud_draw_main") void eo_equipment_draw(void) {
    if (!window_open) {
        return;
    }
    handle_input();
    if (window_open) {
        draw_window();
    }
}

// ---- Charms that change MP costs: measured around spell_cast_init, which takes the MP ----

#define BRIAN_MP (((u16*)gPlayerMainData)[4])
s32 get_rand(s32 n);  // 0..n-1

static u16 mp_before_cast;

RECOMP_HOOK("spell_cast_init") void eo_equipment_before_cast(void) {
    mp_before_cast = BRIAN_MP;
}

RECOMP_HOOK_RETURN("spell_cast_init") void eo_equipment_after_cast(void) {
    u16 mp = BRIAN_MP;
    if (mp >= mp_before_cast) {
        return;
    }
    u16 cost = (u16)(mp_before_cast - mp);
    int charm = worn_in(SLOT_CHARM);
    if (charm == EQ_PILGRIMS_SOUL) {
        BRIAN_MP = (u16)(mp + cost / 2);            // spells cost half (rounded up)
    } else if (charm == EQ_GAMBLERS_COIN) {
        if (get_rand(2) == 0) {
            BRIAN_MP = mp_before_cast;              // free
        } else {
            BRIAN_MP = mp >= cost ? (u16)(mp - cost) : 0;  // double
        }
    }
}

// Pilgrim's Soul: MP comes back only from walking, so a staff hit's +1 MP is taken back.
static u16 mp_before_staff;

RECOMP_HOOK("player_state_staff_hit") void eo_equipment_before_staff(void) {
    mp_before_staff = BRIAN_MP;
}

RECOMP_HOOK_RETURN("player_state_staff_hit") void eo_equipment_after_staff(void) {
    if (worn_in(SLOT_CHARM) == EQ_PILGRIMS_SOUL && BRIAN_MP > mp_before_staff) {
        BRIAN_MP = mp_before_staff;
    }
}
