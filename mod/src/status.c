#include "eo.h"

// New status effects (game design document, "Status effects"):
//
// Burn, Fire's main damage: each damaging hit of one of Brian's Fire spells adds a stack worth BURN_PERCENT of
// that hit's damage (up to 3; a 4th replaces the weakest). Stacks tick at the end of each of Brian's turns,
// from the turn after they were applied, through the game's own enemy damage routine (damage number, hurt
// animation, death) and ignore defense. They fade after BURN_TURNS ticks without a new Fire hit.
// Fire Bomb ignites a burning foe: all the Burn damage still to come (stacks x turns left) lands at once.
//
// Poison: poison-themed monsters may poison Brian when they hurt him. He loses 5% of max HP at the end of each
// of his turns for POISON_TURNS turns, never going below 1 HP; outside battle it ticks every few seconds.
// Water's healing spells and a night at an inn cure it.
//
// Brian's turn is the clock because he gets a turn between every enemy turn: "3 turns" means the same thing
// with one enemy or three. Research: research/battle/README.md (turns, damage) and research/enemies.

#define MAX_ENEMIES 6
#define BURN_STACKS 3
#define BURN_PERCENT 30
#define BURN_TURNS 3
#define POISON_TURNS 4
#define POISON_PERCENT 5
#define POISON_CHANCE 35
#define POISON_FIELD_FRAMES 150

#define ELEMENT_FIRE 0
#define DAMAGE_NO_ELEMENT 0x8000   // enemy_take_damage flag: skip the element rule (the staff uses it too)

#define BATTLE_ACTIVE     0x1      // gBattleState
#define BATTLE_ENEMY_TURN 0x2

// Enemy slots: 6 x 0x128 bytes; +0x0A HP, +0x20 monster record, +0x24 battle data.
#define ENEMY_SLOTS 0x8007C998
#define ENEMY_SLOT_SIZE 0x128
#define ENEMY_HP(slot) (*(u16*)(ENEMY_SLOTS + (slot) * ENEMY_SLOT_SIZE + 0x0A))
#define BRIAN_BATTLE_DATA ((void*)0x8007BACC)
#define BRIAN_HP     (((u16*)gPlayerMainData)[2])
#define BRIAN_MAX_HP (((u16*)gPlayerMainData)[3])

#define CURRENT_SET_ROW (*(u32**)0x8007D0BC)
#define SET_TABLE 0x80054160
#define SET_ROW_SIZE 0x14
#define MONSTER_SIZE 0x38

// Monsters (index within their set) whose hits can poison Brian.
static const u16 poisoners_by_set[] = {
    (1 << 2) | (1 << 4),               // 0 Holy Plains: Man Eater, Bumbershoot
    (1 << 2) | (1 << 4) | (1 << 6),    // 1 Dondoran Flats: Apophis, Death Hugger, Man Trap
    (1 << 2) | (1 << 3) | (1 << 8),    // 2 Carmagh / Limelin: Crawler, Scorpion, Blood Jell
    0,                                 // 3 Desert / Brannoch
    (1 << 5) | (1 << 6) | (1 << 11),   // 4 Normoon / Isle of Skye: Arachnoid, Lamia, Termant
    0,                                 // 5 Mammon's World
};

s32 enemy_take_damage(s32 slot, u16 power, u16 element, u16 flags, void* attacker);
void damage_popup_spawn(void* battle_data, s32 unk, s32 amount);  // shows a damage number over a fighter
s32 get_rand(s32 n);  // 0..n-1
extern u16 gBattleState;

typedef struct {
    u16 stacks[BURN_STACKS];  // damage per tick of each stack, 0 = empty
    u8 turns_left;
    u8 fresh;                 // applied this turn: starts ticking next turn
} Burn;

static Burn burns[MAX_ENEMIES];
static u8 poison_turns = 0;
static int poison_field_timer = 0;

// A stand-in attacker for burn ticks: enemy_take_damage scales damage by atk / (atk + defense), so a huge
// attack makes a tick land at (almost exactly) its own amount.
static u8 burn_attacker[0x70] __attribute__((aligned(8)));
static u8 burn_attacker_stats[0x88] __attribute__((aligned(8)));

static int is_enemy_battle_data(void* p) {
    u32 a = (u32)p;
    return a >= ENEMY_SLOTS && a < ENEMY_SLOTS + MAX_ENEMIES * ENEMY_SLOT_SIZE;
}

// ---- Burn ----

static void burn_add_stack(int slot, u16 amount) {
    Burn* b = &burns[slot];
    int weakest = 0;
    for (int i = 0; i < BURN_STACKS; i++) {
        if (b->stacks[i] < b->stacks[weakest]) {
            weakest = i;
        }
    }
    if (amount > b->stacks[weakest]) {
        b->stacks[weakest] = amount;
    }
    b->turns_left = BURN_TURNS;
    b->fresh = 1;
}

static void burn_tick_all(void) {
    for (int slot = 0; slot < MAX_ENEMIES; slot++) {
        Burn* b = &burns[slot];
        if (b->turns_left == 0) {
            continue;
        }
        if (ENEMY_HP(slot) == 0) {
            *b = (Burn){ 0 };
            continue;
        }
        if (b->fresh) {
            b->fresh = 0;
            continue;
        }
        u32 total = 0;
        for (int i = 0; i < BURN_STACKS; i++) {
            total += b->stacks[i];
        }
        if (total > 0) {
            enemy_take_damage(slot, (u16)(total > 0xFFFF ? 0xFFFF : total), ELEMENT_FIRE, DAMAGE_NO_ELEMENT, burn_attacker);
        }
        if (--b->turns_left == 0) {
            *b = (Burn){ 0 };
        }
    }
}

#define SPELL_FIRE_BOMB 0x000A   // Fire school, index 10

// The spell Brian launched last (projectile_init runs for every shot, Brian's and enemies').
static u16 brian_spell = 0xFFFF;

RECOMP_HOOK("projectile_init") void eo_status_on_projectile(void* caster, u16 spell) {
    if (!is_enemy_battle_data(caster)) {
        brian_spell = spell & 0x0FFF;
    }
}

// Deals all of a burning foe's remaining Burn damage at once and clears it.
static void burn_ignite(int slot) {
    Burn* b = &burns[slot];
    u32 total = 0;
    for (int i = 0; i < BURN_STACKS; i++) {
        total += b->stacks[i];
    }
    total *= b->turns_left;
    *b = (Burn){ 0 };
    if (total > 0 && ENEMY_HP(slot) > 0) {
        enemy_take_damage(slot, (u16)(total > 0xFFFF ? 0xFFFF : total), ELEMENT_FIRE, DAMAGE_NO_ELEMENT, burn_attacker);
    }
}

// Remember who a Fire spell from Brian is about to hit; the stack is added once the damage is known.
static int pending_slot = -1;
static u16 pending_hp;

RECOMP_HOOK("enemy_take_damage") void eo_status_before_enemy_damage(s32 slot, u16 power, u16 element, u16 flags, void* attacker) {
    pending_slot = -1;
    if (power == 0 || element != ELEMENT_FIRE || (flags & DAMAGE_NO_ELEMENT) || attacker == burn_attacker ||
        is_enemy_battle_data(attacker) || slot < 0 || slot >= MAX_ENEMIES) {
        return;
    }
    pending_slot = slot;
    pending_hp = ENEMY_HP(slot);
}

RECOMP_HOOK_RETURN("enemy_take_damage") void eo_status_after_enemy_damage(void) {
    if (pending_slot < 0) {
        return;
    }
    int slot = pending_slot;
    pending_slot = -1;
    u16 hp = ENEMY_HP(slot);
    u16 hit = hp < pending_hp ? (u16)(pending_hp - hp) : 0;  // this hit only, before any ignition
    if (brian_spell == SPELL_FIRE_BOMB && burns[slot].turns_left > 0) {
        burn_ignite(slot);
    }
    if (hit > 0 && ENEMY_HP(slot) > 0) {
        u32 amount = (u32)hit * BURN_PERCENT / 100;
        burn_add_stack(slot, (u16)(amount < 1 ? 1 : amount));
    }
}

// A new battle: nothing burns yet.
RECOMP_HOOK("battle_build_turn_order") void eo_status_on_battle_start(void) {
    for (int slot = 0; slot < MAX_ENEMIES; slot++) {
        burns[slot] = (Burn){ 0 };
    }
}

// ---- Poison ----

static int monster_poisons(void* attacker) {
    u8* slot = (u8*)attacker - 0x24;
    u8* record = *(u8**)(slot + 0x20);
    u32* row = CURRENT_SET_ROW;
    if (record == 0 || row == 0) {
        return 0;
    }
    u32 set = ((u32)row - SET_TABLE) / SET_ROW_SIZE;
    u32 monsters = row[3];
    if (set >= sizeof(poisoners_by_set) / sizeof(poisoners_by_set[0]) || (u32)record < monsters) {
        return 0;  // e.g. a boss, whose record isn't in the set
    }
    u32 index = ((u32)record - monsters) / MONSTER_SIZE;
    return index < 16 && (poisoners_by_set[set] & (1 << index));
}

static int poison_pending = 0;
static u16 poison_hp_before;

RECOMP_HOOK("player_take_damage") void eo_status_before_brian_damage(u16 power, u16 flags, void* attacker) {
    poison_pending = power > 0 && is_enemy_battle_data(attacker) && monster_poisons(attacker);
    poison_hp_before = BRIAN_HP;
}

RECOMP_HOOK_RETURN("player_take_damage") void eo_status_after_brian_damage(void) {
    if (poison_pending && BRIAN_HP > 0 && BRIAN_HP < poison_hp_before && get_rand(100) < POISON_CHANCE) {
        poison_turns = POISON_TURNS;
    }
    poison_pending = 0;
}

static void poison_tick(int in_battle) {
    if (poison_turns == 0) {
        return;
    }
    poison_turns--;
    u16 hp = BRIAN_HP;
    if (hp <= 1) {
        return;
    }
    u32 amount = (u32)BRIAN_MAX_HP * POISON_PERCENT / 100;
    if (amount < 1) {
        amount = 1;
    }
    if (amount > (u32)(hp - 1)) {
        amount = hp - 1;
    }
    BRIAN_HP = (u16)(hp - amount);
    if (in_battle) {
        damage_popup_spawn(BRIAN_BATTLE_DATA, 0, (s32)amount);
    }
}

void eo_status_cure_poison(void) {
    poison_turns = 0;
}

// Water's healing spells cure poison when Brian casts them (not on an enemy's turn).
RECOMP_HOOK("spell_type_heal") void eo_status_on_heal(void) {
    if (!(gBattleState & BATTLE_ACTIVE) || !(gBattleState & BATTLE_ENEMY_TURN)) {
        eo_status_cure_poison();
    }
}

// ---- The clock ----

// End of a turn: when Brian's turn ends, burns and poison tick.
RECOMP_HOOK("battle_end_turn") void eo_status_on_turn_end(void) {
    if (!(gBattleState & BATTLE_ACTIVE) || (gBattleState & BATTLE_ENEMY_TURN)) {
        return;
    }
    burn_tick_all();
    poison_tick(1);
}

// Every frame: poison keeps ticking in the field, and a defeat clears it.
RECOMP_HOOK_RETURN("frame_build") void eo_status_frame(void) {
    if (BRIAN_HP == 0) {
        poison_turns = 0;
    }
    if (poison_turns == 0 || gGameMode != 1 || (gBattleState & BATTLE_ACTIVE) || (gGameState & 0x3)) {
        poison_field_timer = 0;
        return;
    }
    if (++poison_field_timer >= POISON_FIELD_FRAMES) {
        poison_field_timer = 0;
        poison_tick(0);
    }
}

// ---- Display ----

#define STATUS_WIN_X 16
#define STATUS_WIN_Y 16
#define STATUS_WIN_W (8 * 8 + 20)
#define STATUS_WIN_H 22

RECOMP_HOOK_RETURN("hud_draw_main") void eo_status_draw(void) {
    if (poison_turns == 0) {
        return;
    }
    eo_ui_window(STATUS_WIN_X, STATUS_WIN_Y, STATUS_WIN_W, STATUS_WIN_H);
    eo_ui_text(STATUS_WIN_X + 10, STATUS_WIN_Y + 7, "Poisoned");
}

// Set up the stand-in burn attacker once (from main.c).
void eo_status_init(void) {
    *(u8**)(burn_attacker + 0x68) = burn_attacker_stats;
    *(u16*)(burn_attacker_stats + 0x84) = 0xFFFF;
}
