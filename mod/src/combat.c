#include "eo.h"

// The combat system (game design document, "Combat resource"): Spirit Charge, plus enemy shields and Break.
//
// Spirit Charge: Brian starts a battle with 1 charge and gains 1 at the start of each of his turns (max 5),
// except the turn after he spent some. His first weakness hit in a turn gives +1. Before casting, D-pad up/down
// picks a Boost of 0-3 (no more than his charge); it's spent when the spell launches:
//   Fire  +1 Burn stack per charge (status.c); at 3, the hit first ignites the target's existing Burn
//   Earth +40% damage per charge
//   Water +30% healing per charge; a damaging Water spell Freezes its target for 1 turn per charge
//   Wind  +2 shots per charge
//
// Shields and Break: each enemy has shields (2-4 by toughness, bosses 6-10) and a weakness, the element
// opposite its own (the game's own x1.25 rule), hidden until hit. Each weakness hit from Brian removes a
// shield; at 0 the enemy is Broken: it loses its next turn (the game's own Restriction status) and takes 50%
// more damage until then. Then its shields come back; a boss's grow by one each Break.

#define MAX_ENEMIES 6
#define MAX_CHARGE 5
#define MAX_BOOST 3
#define START_CHARGE 1
#define BROKEN_BONUS_PERCENT 50
#define NO_WEAKNESS 0xFF

#define ENEMY_SLOTS 0x8007C998
#define ENEMY_SLOT_SIZE 0x128
#define ENEMY_SLOT(i) ((u8*)(ENEMY_SLOTS + (i) * ENEMY_SLOT_SIZE))
#define ENEMY_HP(i) (*(u16*)(ENEMY_SLOT(i) + 0x0A))
#define ENEMY_RECORD(i) (*(u8**)(ENEMY_SLOT(i) + 0x20))
#define ENEMY_BATTLE_DATA(i) (ENEMY_SLOT(i) + 0x24)
#define RECORD_MODEL_INDEX 0x02  // also the index into the name table
#define RECORD_MAX_HP 0x06
#define RECORD_ELEMENT 0x26

// Status block (battle data +0x68): u16 status bits, then one duration byte per status at +2.
#define STATUS_RESTRICTION_BIT 0x8
#define STATUS_RESTRICTION_DURATION 0x5

#define BATTLE_ACTIVE     0x1
#define BATTLE_ENEMY_TURN 0x2
#define BATTLE_BOSS       0x100
#define CURRENT_BOSS (*(s32*)0x8008C570)
#define NAME_TABLE (*(char***)0x8007D0AC)

#define SPELL_STAT_TABLES ((u8**)0x800C1B14)
#define SPELL_STAT_SIZE 0x44
#define SPELL_BASE   0x0C
#define SPELL_TYPE   0x10
#define SPELL_SHOTS  0x20
#define SPELL_TYPE_DAMAGE 0
#define SPELL_TYPE_HEAL 1

enum { FIRE, EARTH, WATER, WIND };

s32 enemy_take_damage(s32 slot, u16 power, u16 element, u16 flags, void* attacker);
void damage_popup_spawn(void* battle_data, s32 kind, s32 amount);
extern u16 gBattleState;
extern u16 gPadButtonsNew;

typedef struct {
    u8 shields;
    u8 max_shields;
    u8 weakness;
    u8 revealed;
    u8 broken;
} Guard;

static Guard guards[MAX_ENEMIES];
static u8 charge = 0;
static u8 boost = 0;           // chosen for the next cast
static u8 spent_last_turn = 0;
static u8 spent_this_turn = 0;
static u8 weakness_bonus_given = 0;

// The boost of the spell being cast right now (0 when none), and what was changed for it.
static u8 active_boost = 0;
static u8 active_school = 0;
static u8* active_spell = 0;
static u16 saved_base;
static u16 saved_shots;

static int in_battle(void) {
    return (gBattleState & BATTLE_ACTIVE) != 0;
}

static int brians_turn(void) {
    return in_battle() && !(gBattleState & BATTLE_ENEMY_TURN);
}

static int is_enemy_battle_data(void* p) {
    u32 a = (u32)p;
    return a >= ENEMY_SLOTS && a < ENEMY_SLOTS + MAX_ENEMIES * ENEMY_SLOT_SIZE;
}

int eo_combat_active_boost(void) {
    return active_boost;
}

int eo_combat_active_school(void) {
    return active_spell != 0 ? active_school : -1;
}

// ---- Battle start ----

static u8 shields_for(u8* record, int boss) {
    if (boss) {
        return (u8)(6 + (CURRENT_BOSS >= 0 && CURRENT_BOSS < 8 ? CURRENT_BOSS / 2 : 0));
    }
    u16 hp = *(u16*)(record + RECORD_MAX_HP);
    return hp < 60 ? 2 : hp < 200 ? 3 : 4;
}

RECOMP_HOOK("battle_build_turn_order") void eo_combat_on_battle_start(void) {
    int boss = (gBattleState & BATTLE_BOSS) != 0;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        Guard* g = &guards[i];
        *g = (Guard){ 0 };
        u8* record = ENEMY_RECORD(i);
        if (record == 0 || ENEMY_HP(i) == 0) {
            continue;
        }
        u16 element = *(u16*)(record + RECORD_ELEMENT);
        g->weakness = element == FIRE ? WATER : element == WATER ? FIRE : element == EARTH ? WIND :
                      element == WIND ? EARTH : NO_WEAKNESS;
        g->max_shields = g->shields = shields_for(record, boss);
    }
    charge = START_CHARGE;
    boost = 0;
    spent_last_turn = spent_this_turn = 0;
    weakness_bonus_given = 0;
    active_boost = 0;
    active_spell = 0;
}

// ---- Break ----

static void break_enemy(int i) {
    Guard* g = &guards[i];
    g->broken = 1;
    u8* status = *(u8**)(ENEMY_BATTLE_DATA(i) + 0x68);
    *(u16*)status |= STATUS_RESTRICTION_BIT;   // its next turn is skipped by the game itself
    status[STATUS_RESTRICTION_DURATION] = 1;
    sfx_play(SFX_CONFIRM, 0xFF, 0x40);
}

static void freeze_enemy(int i, int turns) {
    u8* status = *(u8**)(ENEMY_BATTLE_DATA(i) + 0x68);
    if (gBattleState & BATTLE_BOSS) {
        turns = 1;  // bosses resist control effects
    }
    *(u16*)status |= STATUS_RESTRICTION_BIT;
    if (status[STATUS_RESTRICTION_DURATION] < turns) {
        status[STATUS_RESTRICTION_DURATION] = (u8)turns;
    }
}

// Hits from Brian: remember the target's HP, count weakness hits.
static int hit_slot = -1;
static u16 hit_hp;
static u8 hit_was_broken;

RECOMP_HOOK("enemy_take_damage") void eo_combat_before_enemy_damage(s32 slot, u16 power, u16 element, u16 flags, void* attacker) {
    hit_slot = -1;
    if (!in_battle() || slot < 0 || slot >= MAX_ENEMIES || power == 0 || (flags & 0x8000) ||
        is_enemy_battle_data(attacker)) {
        return;  // not a spell from Brian (burn ticks and the staff carry flag 0x8000)
    }
    hit_slot = slot;
    hit_hp = ENEMY_HP(slot);
    Guard* g = &guards[slot];
    hit_was_broken = g->broken;
    if (g->weakness == NO_WEAKNESS || element != g->weakness || g->broken) {
        return;
    }
    g->revealed = 1;
    if (!weakness_bonus_given && charge < MAX_CHARGE) {
        charge++;
        weakness_bonus_given = 1;
    }
    if (g->shields > 0 && --g->shields == 0) {
        break_enemy(slot);
    }
}

RECOMP_HOOK_RETURN("enemy_take_damage") void eo_combat_after_enemy_damage(void) {
    int slot = hit_slot;
    hit_slot = -1;
    if (slot < 0) {
        return;
    }
    u16 hp = ENEMY_HP(slot);
    // A Broken foe takes 50% more from Brian's hits.
    if (hit_was_broken && hp > 0 && hp < hit_hp) {
        u16 extra = (u16)((hit_hp - hp) * BROKEN_BONUS_PERCENT / 100);
        if (extra > 0) {
            ENEMY_HP(slot) = extra >= hp ? 0 : (u16)(hp - extra);
            damage_popup_spawn(ENEMY_BATTLE_DATA(slot), 0, extra);
        }
    }
    // A boosted damaging Water spell freezes its target.
    if (active_boost > 0 && active_school == WATER && ENEMY_HP(slot) > 0) {
        freeze_enemy(slot, active_boost);
    }
}

// ---- Charge, boost and the turn clock ----

static void restore_active_spell(void) {
    if (active_spell != 0) {
        *(u16*)(active_spell + SPELL_BASE) = saved_base;
        *(u16*)(active_spell + SPELL_SHOTS) = saved_shots;
        active_spell = 0;
    }
    active_boost = 0;
}

RECOMP_HOOK("battle_end_turn") void eo_combat_on_turn_end(void) {
    if (!in_battle()) {
        return;
    }
    if (!(gBattleState & BATTLE_ENEMY_TURN)) {
        // Brian's turn is over: undo this turn's boost, remember whether he spent.
        restore_active_spell();
        spent_last_turn = spent_this_turn;
        spent_this_turn = 0;
        boost = 0;
        return;
    }
    // An enemy's turn is over, so Brian's begins: a Broken enemy that has now lost its turn recovers.
    for (int i = 0; i < MAX_ENEMIES; i++) {
        Guard* g = &guards[i];
        u8* status = *(u8**)(ENEMY_BATTLE_DATA(i) + 0x68);
        if (g->broken && !(*(u16*)status & STATUS_RESTRICTION_BIT)) {
            g->broken = 0;
            if (gBattleState & BATTLE_BOSS) {
                g->max_shields++;
            }
            g->shields = g->max_shields;
        }
    }
    if (!spent_last_turn && charge < MAX_CHARGE) {
        charge++;
    }
    spent_last_turn = 0;
    weakness_bonus_given = 0;
}

// Brian launches a spell: spend the chosen boost on it. projectile_init runs for every shot; the boost is
// applied on the first one of the turn and undone when his turn ends.
RECOMP_HOOK("projectile_init") void eo_combat_on_projectile(void* caster, u16 spell) {
    if (!brians_turn() || is_enemy_battle_data(caster) || boost == 0 || active_spell != 0) {
        return;
    }
    u16 school = (spell >> 8) & 0xF;
    u16 index = spell & 0xFF;
    if (school > WIND || index >= 15) {
        return;
    }
    u8* stats = SPELL_STAT_TABLES[school] + index * SPELL_STAT_SIZE;
    active_spell = stats;
    active_school = (u8)school;
    active_boost = boost;
    saved_base = *(u16*)(stats + SPELL_BASE);
    saved_shots = *(u16*)(stats + SPELL_SHOTS);
    u16 type = *(u16*)(stats + SPELL_TYPE);
    if (school == EARTH && type == SPELL_TYPE_DAMAGE) {
        *(u16*)(stats + SPELL_BASE) = (u16)(saved_base * (100 + 40 * boost) / 100);
    } else if (school == WATER && type == SPELL_TYPE_HEAL) {
        *(u16*)(stats + SPELL_BASE) = (u16)(saved_base * (100 + 30 * boost) / 100 + boost);
    } else if (school == WIND && type == SPELL_TYPE_DAMAGE) {
        *(u16*)(stats + SPELL_SHOTS) = (u16)(saved_shots + 2 * boost);
    }
    charge -= boost;
    spent_this_turn = 1;
    boost = 0;
}

// D-pad up/down picks the boost on Brian's turn.
#define BTN_DUP 0x0800
#define BTN_DDOWN 0x0400

RECOMP_HOOK_RETURN("pad_read") void eo_combat_read_input(void) {
    if (!brians_turn() || active_spell != 0) {
        return;
    }
    u8 most = charge < MAX_BOOST ? charge : MAX_BOOST;
    if ((gPadButtonsNew & BTN_DUP) && boost < most) {
        boost++;
        sfx_play(SFX_CONFIRM, 0xFF, 0x40);
    } else if ((gPadButtonsNew & BTN_DDOWN) && boost > 0) {
        boost--;
        sfx_play(SFX_CANCEL, 0xFF, 0x40);
    }
    if (boost > most) {
        boost = most;
    }
}

// ---- Battle panels ----

// Bottom right: clear of the HUD (HP/MP, the selected-spell bar, element gems) and of the spell wheel.
#define CHARGE_WIN_W (8 * 8 + 20)
#define CHARGE_WIN_H 36
#define CHARGE_WIN_X (320 - 16 - 3 - CHARGE_WIN_W)
#define CHARGE_WIN_Y (240 - 16 - 3 - CHARGE_WIN_H)

// One row per enemy: name, shields (or "Broken"), weakness ("?" until revealed), Burn stacks ("B3").
// 21 columns keep the panel clear of the HUD's HP box.
#define NAME_CHARS 9
#define SHIELD_COL 10
#define WEAKNESS_COL 13
#define BURN_COL 19
#define FOE_COLS 21
#define FOES_WIN_W (FOE_COLS * 8 + 20)
#define FOES_WIN_X (320 - 16 - 3 - FOES_WIN_W)
#define FOES_WIN_Y 16
#define FOE_ROW_H 14

static const char* const element_names[4] = { "Fire", "Earth", "Water", "Wind" };

static void draw_panels(void) {
    char line[32];
    eo_ui_window(CHARGE_WIN_X, CHARGE_WIN_Y, CHARGE_WIN_W, CHARGE_WIN_H);
    eo_format_amount(line, "Charge", charge, 8, "");
    eo_ui_text(CHARGE_WIN_X + 10, CHARGE_WIN_Y + 7, line);
    eo_format_amount(line, "Boost", boost, 8, "");
    eo_ui_text(CHARGE_WIN_X + 10, CHARGE_WIN_Y + 21, line);

    int rows = 0;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (ENEMY_RECORD(i) != 0 && ENEMY_HP(i) > 0) {
            rows++;
        }
    }
    if (rows == 0) {
        return;
    }
    eo_ui_window(FOES_WIN_X, FOES_WIN_Y, FOES_WIN_W, 12 + rows * FOE_ROW_H);
    s32 y = FOES_WIN_Y + 7;
    char** names = NAME_TABLE;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        u8* record = ENEMY_RECORD(i);
        if (record == 0 || ENEMY_HP(i) == 0) {
            continue;
        }
        Guard* g = &guards[i];
        char name[NAME_CHARS + 1];
        eo_title_case(name, names != 0 ? names[*(u16*)(record + RECORD_MODEL_INDEX)] : "Foe", NAME_CHARS);
        eo_ui_text(FOES_WIN_X + 10, y, name);
        if (g->broken) {
            eo_ui_text(FOES_WIN_X + 10 + SHIELD_COL * 8, y, "Broken");
        } else {
            eo_format_amount(line, "", g->shields, 2, "");
            eo_ui_text(FOES_WIN_X + 10 + SHIELD_COL * 8, y, line);
        }
        if (g->weakness != NO_WEAKNESS && !g->broken) {
            eo_ui_text(FOES_WIN_X + 10 + WEAKNESS_COL * 8, y, g->revealed ? element_names[g->weakness] : "?");
        }
        int stacks = eo_status_burn_stacks(i);
        if (stacks > 0) {
            eo_format_amount(line, "B", (u32)stacks, 2, "");
            eo_ui_text(FOES_WIN_X + 10 + BURN_COL * 8, y, line);
        }
        y += FOE_ROW_H;
    }
}

RECOMP_HOOK_RETURN("hud_draw_main") void eo_combat_draw(void) {
    if (in_battle()) {
        draw_panels();
    }
}
