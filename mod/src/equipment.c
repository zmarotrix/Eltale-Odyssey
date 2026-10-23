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
    [EQ_EMBER_STAFF]    = { "Ember Staff",       SLOT_STAFF, 900,  "Fire 1.1x. Swings spread Burn." },
    [EQ_HEAVY_STAFF]    = { "Heavy Staff",       SLOT_STAFF, 700,  "Swings deal 12 more damage." },
    [EQ_STONE_STAFF]    = { "Stone Staff",       SLOT_STAFF, 800,  "Earth 1.1x. Swings: +3 defense." },
    [EQ_TIDE_STAFF]     = { "Tide Staff",        SLOT_STAFF, 800,  "Water 1.1x. Swings heal you." },
    [EQ_GALE_STAFF]     = { "Gale Staff",        SLOT_STAFF, 800,  "Wind 1.1x. Move 30% farther." },
    [EQ_TRAVELERS_ROBE] = { "Traveler's Robe",   SLOT_ROBE,  300,  "Defense 4 more." },
    [EQ_TIDECLOTH_ROBE] = { "Tidecloth Robe",    SLOT_ROBE,  1200, "Defense 8 more, no Poison." },
    [EQ_PILGRIMS_SOUL]  = { "Pilgrim's Soul",    SLOT_CHARM, 2500, "Half-cost spells, no staff MP." },
    [EQ_GAMBLERS_COIN]  = { "Gambler's Coin",    SLOT_CHARM, 0,    "Casts cost no MP or double." },
    [EQ_MIRROR_SHARD]   = { "Mirror Shard",      SLOT_CHARM, 0,    "First hit each fight reflects." },
    [EQ_TIDAL_ECHO]     = { "Tidal Echo",        SLOT_SIGIL, 0,    "Water spells cast twice, 0.65x." },
    [EQ_LEECH_PILLAR]   = { "Leech Pillar",      SLOT_SIGIL, 0,    "Water Pillar drains a quarter." },
    [EQ_WOOL_CLOAK]     = { "Wool Cloak",        SLOT_ROBE,  0,    "Defense 2 more, Wind 1.1x." },
    // Build pieces found in the world (chests.c) or given by bosses (gold.c). By style:
    //   Pyromancer   Ember Staff > Ember Heart > Phoenix Staff     (talents: Smoulder, Inferno, Wildfire)
    //   Stonewall    Lead Mantle > Stone Memory > Mountain Staff  (Bedrock, Fault Line, Shatter)
    //   Tidecaller   Leech Pillar > Tidal Echo > Leviathan Staff (Wellspring, Undertow, Glacier)
    //   Tempest      Gale Tether > Pilgrim's Soul > Skyreed Staff (Crosswind, Cyclone, Maelstrom)
    //   Staff Brawler Heavy Staff > Spellstrike > Titan Staff    (Shatter, Firebrand, Tailwind)
    //   Elementalist Wool Cloak, Prism Staff > Prism Sigil         (any spread of talents)
    //   Wild cards   Gambler's Coin, Hunter's Charm, Glass Charm, Mirror Shard
    [EQ_LEAD_MANTLE]    = { "Lead Mantle",       SLOT_ROBE,  0,    "Defense 10 more, Earth 1.1x." },
    [EQ_GALE_TETHER]    = { "Gale Tether",       SLOT_SIGIL, 0,    "Wind spells fire one more shot." },
    [EQ_EMBER_HEART]    = { "Ember Heart",       SLOT_SIGIL, 0,    "Kindling adds one more Burn." },
    [EQ_STONE_MEMORY]   = { "Stone Memory",      SLOT_SIGIL, 0,    "Earth in a row: 15% more, to 30%." },
    [EQ_SPELLSTRIKE]    = { "Spellstrike",       SLOT_CHARM, 0,    "First swing a turn: 1 charge." },
    [EQ_TITAN_STAFF]    = { "Titan Staff",       SLOT_STAFF, 0,    "Swings 30 more, crack a shield." },
    [EQ_PHOENIX_STAFF]  = { "Phoenix Staff",     SLOT_STAFF, 0,    "Fire 1.25x. Swings burn." },
    [EQ_LEVIATHAN_STAFF]= { "Leviathan Staff",   SLOT_STAFF, 0,    "Water 1.25x, heals 25% more." },
    [EQ_SKYREED_STAFF]  = { "Skyreed Staff",     SLOT_STAFF, 0,    "Wind 1.25x, stronger Wind Boost." },
    [EQ_MOUNTAIN_STAFF] = { "Mountain Staff",    SLOT_STAFF, 0,    "Earth 1.25x, breaks 2 shields." },
    [EQ_PRISM_STAFF]    = { "Prism Staff",       SLOT_STAFF, 0,    "Swings 6 more, all magic 1.1x." },
    [EQ_KNIGHTS_MANTLE] = { "Knight's Mantle",   SLOT_ROBE,  0,    "Defense 16 more." },
    [EQ_DRAGON_ROBE]    = { "Dragon Robe",       SLOT_ROBE,  0,    "Defense 24 more, no Poison." },
    [EQ_HUNTERS_CHARM]  = { "Hunter's Charm",    SLOT_CHARM, 0,    "Battles pay half again in gold." },
    [EQ_GLASS_CHARM]    = { "Glass Charm",       SLOT_CHARM, 0,    "Deal and take 25% more damage." },
    [EQ_PRISM_SIGIL]    = { "Prism Sigil",       SLOT_SIGIL, 0,    "New element each cast: 20% more." },
    // Odd finds (chests.c): each a small trade-off or a hook for one style of play.
    [EQ_SHEPHERDS_CROOK]    = { "Shepherd's Crook",    SLOT_STAFF, 0, "Swings 4 more, still give MP." },
    [EQ_IRON_PLATE]         = { "Iron Plate",          SLOT_ROBE,  0, "Defense 14 more, move 30% less." },
    [EQ_HOURGLASS_CHARM]    = { "Hourglass Charm",     SLOT_CHARM, 0, "Battles start with 2 Charge." },
    [EQ_BLOODSTONE]         = { "Bloodstone",          SLOT_CHARM, 0, "Under half HP: spells 1.2x." },
    [EQ_EXECUTIONERS_CHARM] = { "Executioner's Charm", SLOT_CHARM, 0, "Foes under 25% HP take 1.3x." },
    [EQ_SMITHS_HAMMER]      = { "Smith's Hammer",      SLOT_STAFF, 0, "Swing 20 more; spells 1 MP more" },
    [EQ_SCARRED_LEATHER]    = { "Scarred Leather",     SLOT_ROBE,  0, "Defense 6. Low HP: hits 0.75x." },
    [EQ_STORM_GLASS]        = { "Storm Glass",         SLOT_SIGIL, 0, "Wind on frozen foes: 1.3x." },
    [EQ_EMBER_LANTERN]      = { "Ember Lantern",       SLOT_SIGIL, 0, "Spells on burning foes: 1.2x." },
    [EQ_MISERS_PURSE]       = { "Miser's Purse",       SLOT_CHARM, 0, "Gold 2x, take 10% more damage." },
    [EQ_GLACIAL_PIN]        = { "Glacial Pin",         SLOT_SIGIL, 0, "Spells on frozen foes: 1.25x." },
    [EQ_ECHO_STONE]         = { "Echo Stone",          SLOT_CHARM, 0, "First spell each battle: 1.5x." },
};

const EquipmentInfo* eo_equipment_info(int piece) {
    return piece >= 0 && piece < EQUIPMENT_COUNT ? &catalog[piece] : 0;
}

int eo_equipment_owned(int piece) {
    if (piece >= 32) {
        return (gEo.owned_more[(piece - 32) / 8] >> ((piece - 32) % 8)) & 1;
    }
    return (gEo.owned_equipment >> piece) & 1;
}

int eo_equipment_worn(int piece) {
    const EquipmentInfo* e = eo_equipment_info(piece);
    return e != 0 && gEo.equipped[e->slot] == piece + 1;
}

static int worn_in(int slot) {
    return gEo.equipped[slot] - 1;  // -1 = none
}

int eo_equipment_in_slot(int slot) {
    return slot >= 0 && slot < SLOT_COUNT ? worn_in(slot) : -1;
}

void eo_equipment_give(int piece) {
    if (piece >= 32) {
        gEo.owned_more[(piece - 32) / 8] |= (u8)(1u << ((piece - 32) % 8));
        return;
    }
    gEo.owned_equipment |= 1u << piece;
}

// ---- Effects the rest of the mod asks about ----

int eo_equip_staff_bonus(void) {
    switch (worn_in(SLOT_STAFF)) {
        case EQ_EMBER_STAFF: return 6;
        case EQ_HEAVY_STAFF: return 12;
        case EQ_STONE_STAFF:
        case EQ_TIDE_STAFF:
        case EQ_GALE_STAFF: return 6;   // the four element staves alike (playtest, 2026-10-21)
        case EQ_TITAN_STAFF: return 30;
        case EQ_PHOENIX_STAFF:
        case EQ_LEVIATHAN_STAFF:
        case EQ_SKYREED_STAFF:
        case EQ_MOUNTAIN_STAFF: return 8;
        case EQ_PRISM_STAFF: return 6;
        case EQ_SHEPHERDS_CROOK: return 4;
        case EQ_SMITHS_HAMMER: return 20;
        default: return 0;
    }
}

int eo_equip_element_percent(int element) {
    int staff = worn_in(SLOT_STAFF);
    int favoured = staff == EQ_EMBER_STAFF ? 0 : staff == EQ_STONE_STAFF ? 1 : staff == EQ_TIDE_STAFF ? 2 :
                   staff == EQ_GALE_STAFF ? 3 : -1;
    int percent = favoured == element ? 10 : 0;
    // The top staves: 1.25x for their element.
    int top = staff == EQ_PHOENIX_STAFF ? 0 : staff == EQ_MOUNTAIN_STAFF ? 1 : staff == EQ_LEVIATHAN_STAFF ? 2 :
              staff == EQ_SKYREED_STAFF ? 3 : -1;
    if (top == element) {
        percent += 25;
    }
    if (staff == EQ_PRISM_STAFF) {
        percent += 10;
    }
    if (element == 3 && worn_in(SLOT_ROBE) == EQ_WOOL_CLOAK) {
        percent += 10;   // Wool Cloak: Wind 1.1x
    }
    if (element == 1 && worn_in(SLOT_ROBE) == EQ_LEAD_MANTLE) {
        percent += 10;   // Lead Mantle: Earth 1.1x
    }
    return percent;
}

int eo_equip_defense(void) {
    switch (worn_in(SLOT_ROBE)) {
        case EQ_TRAVELERS_ROBE: return 4;
        case EQ_TIDECLOTH_ROBE: return 8;
        case EQ_WOOL_CLOAK: return 2;
        case EQ_LEAD_MANTLE: return 10;
        case EQ_KNIGHTS_MANTLE: return 16;
        case EQ_DRAGON_ROBE: return 24;
        case EQ_IRON_PLATE: return 14;
        case EQ_SCARRED_LEATHER: return 6;
        default: return 0;
    }
}

int eo_equip_poison_immune(void) {
    return worn_in(SLOT_ROBE) == EQ_TIDECLOTH_ROBE || worn_in(SLOT_ROBE) == EQ_DRAGON_ROBE;
}

// ---- Robe defense: added to Brian's battle copy of Defense when a battle starts ----

#define BRIAN_BATTLE_DEFENSE (*(u16*)0x8007BBC0)
#define BRIAN_MOVE_MULTIPLIER (*(f32*)0x8007BBC8)   // 1.0 from brian_battle_init

RECOMP_HOOK_RETURN("player_battle_init") void eo_equipment_on_battle_init(void) {
    // Robes, and Earth's Stoneskin talent (2 a rank).
    BRIAN_BATTLE_DEFENSE = (u16)(BRIAN_BATTLE_DEFENSE + eo_equip_defense() + 2 * eo_talent(TALENT_STONESKIN));
    // Gale Staff: Brian moves 30% farther each turn.
    if (worn_in(SLOT_STAFF) == EQ_GALE_STAFF) {
        BRIAN_MOVE_MULTIPLIER *= 1.3f;
    }
    // Iron Plate: heavy, so Brian moves 30% less each turn.
    if (worn_in(SLOT_ROBE) == EQ_IRON_PLATE) {
        BRIAN_MOVE_MULTIPLIER *= 0.7f;
    }
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

// An empty staff or robe slot is Brian's own (playtest, 2026-10-21): his starting staff is the only one that gives
// MP back on hits, so the window says so, and swapping it out shows what's lost. Charms and sigils are just empty.
static const char* const own_names[SLOT_COUNT] = { "Brian's Staff", "Brian's Robe", "-", "-" };
static const char* const own_effects[SLOT_COUNT] = { "Staff hits restore MP.", "His old robe. No bonuses.", "", "" };

static const char* choice_name(int slot, int piece) {
    return piece >= 0 ? catalog[piece].name : slot <= SLOT_ROBE ? own_names[slot] : "NOTHING";
}

static const char* piece_effect(int slot, int piece) {
    return piece >= 0 ? catalog[piece].effect : own_effects[slot];
}

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

static void list_choices(int slot) {
    choice_count = 0;
    choices[choice_count++] = -1;  // Brian's own staff or robe, or nothing for a charm or sigil
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
        eo_sfx(SFX_CANCEL);
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
                eo_sfx(SFX_CANCEL);
                message = "This slot isn't open yet.";
            } else if (!eo_inn_here()) {
                eo_sfx(SFX_CANCEL);
                message = "Change equipment at an inn.";
            } else {
                slot_cursor = cursor;
                list_choices(cursor);
                choosing = 1;
                cursor = 0;
                eo_sfx(SFX_CONFIRM);
            }
        } else {
            gEo.equipped[slot_cursor] = (u8)(choices[cursor] + 1);
            choosing = 0;
            cursor = slot_cursor;
            eo_sfx(SFX_CONFIRM);
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
            eo_ui_small(TEXT_X + 7 * SMALL_W, y, palette, !open ? "LOCKED" : worn >= 0 ? catalog[worn].name : own_names[s]);
        }
    } else {
        eo_ui_small(WIN_X + SMALL_PAD, WIN_Y + SMALL_PAD, TEXT_NORMAL, slot_names[slot_cursor]);
        for (int i = 0; i < choice_count; i++) {
            int palette = i == cursor ? TEXT_GOOD : TEXT_NORMAL;
            eo_ui_small(TEXT_X, list_y + i * ROW_H + 2, palette, choice_name(slot_cursor, choices[i]));
        }
    }

    // Bottom line: a message, or what the highlighted piece does.
    if (message != 0) {
        eo_ui_small(WIN_X + SMALL_PAD, info_y, TEXT_WARN, message);
    } else {
        int slot = choosing ? slot_cursor : cursor;
        int piece = choosing ? choices[cursor] : worn_in(cursor);
        eo_ui_small(WIN_X + SMALL_PAD, info_y, TEXT_NORMAL, slot_open(slot) ? piece_effect(slot, piece) : "");
    }
    eo_icons_begin();
    for (int i = 0; i < rows; i++) {
        s32 y = list_y + i * ROW_H + 2 + ICON_TEXT_DY;
        if (!choosing) {
            eo_icon_draw(slot_open(i) ? ICON_STAFF + i : ICON_LOCK, ICON_X, y);
        } else if (choices[i] >= 0 || slot_cursor <= SLOT_ROBE) {
            eo_icon_draw(ICON_STAFF + slot_cursor, ICON_X, y);
        }
    }
    eo_icon_draw(ICON_POINTER, WIN_X + SMALL_PAD - 1, list_y + cursor * ROW_H + 2 + ICON_TEXT_DY);
}

static void eo_equipment_draw_body(void);

// Drawn into the mod's own display list (ui.c).
RECOMP_HOOK_RETURN("hud_draw_main") void eo_equipment_draw(void) {
    eo_ui_begin();
    eo_equipment_draw_body();
    eo_ui_end();
}

static void eo_equipment_draw_body(void) {
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

#define BRIAN_SPELL_PRESSES ((u8*)0x8007BBD1)   // element of each press
#define BRIAN_SPELL_COUNT (*(u16*)0x8007BBD4)   // presses
s32 spell_find_by_inputs(u16 count, u8* inputs);
static int cast_school = -1;   // the spell being cast's element

RECOMP_HOOK("spell_cast_init") void eo_equipment_before_cast(void) {
    mp_before_cast = BRIAN_MP;
    s32 spell = BRIAN_SPELL_COUNT > 0 && BRIAN_SPELL_COUNT <= 3 ? spell_find_by_inputs(BRIAN_SPELL_COUNT, BRIAN_SPELL_PRESSES) : -1;
    cast_school = spell >= 0 ? (spell >> 8) & 0xF : -1;
}

RECOMP_HOOK_RETURN("spell_cast_init") void eo_equipment_after_cast(void) {
    u16 mp = BRIAN_MP;
    if (mp >= mp_before_cast) {
        return;
    }
    u16 cost = (u16)(mp_before_cast - mp);
    // Water's Undercurrent talent: 10% a rank that a Water spell costs nothing.
    if (cast_school == 2 && get_rand(100) < 10 * eo_talent(TALENT_UNDERCURRENT)) {
        BRIAN_MP = mp_before_cast;
        return;
    }
    int charm = worn_in(SLOT_CHARM);
    if (charm == EQ_PILGRIMS_SOUL) {
        BRIAN_MP = (u16)(mp + cost / 2);            // spells cost half (rounded up)
    } else if (worn_in(SLOT_STAFF) == EQ_SMITHS_HAMMER) {
        BRIAN_MP = mp > 0 ? (u16)(mp - 1) : 0;     // Smith's Hammer: spells cost 1 more MP
    }
    if (charm == EQ_GAMBLERS_COIN) {
        if (get_rand(2) == 0) {
            BRIAN_MP = mp_before_cast;              // free
        } else {
            BRIAN_MP = mp >= cost ? (u16)(mp - cost) : 0;  // double
        }
    }
}

// A staff hit's +1 MP comes only from Brian's starting staff (playtest, 2026-10-21): any other staff, or
// Pilgrim's Soul (MP comes back only from walking), takes it back.
static u16 mp_before_staff;

RECOMP_HOOK("player_state_staff_hit") void eo_equipment_before_staff(void) {
    mp_before_staff = BRIAN_MP;
}

RECOMP_HOOK_RETURN("player_state_staff_hit") void eo_equipment_after_staff(void) {
    // The Shepherd's Crook is a plain staff too: its swings still give MP.
    int staff = worn_in(SLOT_STAFF);
    int no_mp = (staff >= 0 && staff != EQ_SHEPHERDS_CROOK) || worn_in(SLOT_CHARM) == EQ_PILGRIMS_SOUL;
    if (no_mp && BRIAN_MP > mp_before_staff) {
        BRIAN_MP = mp_before_staff;
    }
}
