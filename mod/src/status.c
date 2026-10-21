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
// One status at a time (playtest, 2026-10-21): a fighter with a status can't get another until it wears off,
// whether it's one of the game's statuses (from a spell roll), Burn, Freeze or Poison. Burn on a burning foe is the
// same status, so it still stacks. Break isn't a status (its turn skip uses the Restriction bit, which a Broken foe's
// check ignores). Wind's Shifting Winds talent lets Brian's statuses through.
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
#define ENEMY_BATTLE_DATA(slot) ((void*)(ENEMY_SLOTS + (slot) * ENEMY_SLOT_SIZE + 0x24))
#define BRIAN_BATTLE_DATA ((void*)0x8007BACC)
#define BRIAN_STATUS_BITS (*(u16*)0x8007BB38)
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
    b->turns_left = (u8)(BURN_TURNS + (eo_talent(TALENT_SMOULDER) ? 1 : 0));
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
            eo_fx_burn_hit(slot);
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
        eo_fx_burn_hit(slot);
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
    // Spirit Charge on a Fire spell (combat.c): +1 stack per charge, and at 3 the hit ignites first.
    int boost = eo_combat_active_school() == ELEMENT_FIRE ? eo_combat_active_boost() : 0;
    if ((brian_spell == SPELL_FIRE_BOMB || boost >= 3) && burns[slot].turns_left > 0) {
        burn_ignite(slot);
    }
    if (hit > 0 && ENEMY_HP(slot) > 0 && !eo_status_blocks(ENEMY_BATTLE_DATA(slot), 1, 1)) {
        u32 amount = (u32)hit * (u32)eo_status_burn_percent() / 100;
        for (int i = 0; i <= boost; i++) {
            burn_add_stack(slot, (u16)(amount < 1 ? 1 : amount));
        }
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
    if (poison_pending && BRIAN_HP > 0 && BRIAN_HP < poison_hp_before && !eo_equip_poison_immune() &&
        !eo_status_blocks(BRIAN_BATTLE_DATA, 0, 0) && get_rand(100) < POISON_CHANCE) {
        poison_turns = POISON_TURNS;
    }
    poison_pending = 0;
}

void eo_status_add_burn(int slot, u16 amount) {
    if (slot >= 0 && slot < MAX_ENEMIES && ENEMY_HP(slot) > 0 && !eo_status_blocks(ENEMY_BATTLE_DATA(slot), 1, 1)) {
        burn_add_stack(slot, amount);
    }
}

// Ember Staff flare (playtest, 2026-10-21): a staff hit on a burning foe throws its Burn onto every enemy within
// FLARE_RADIUS of it, as a stack as strong as the foe's strongest.
#define FLARE_RADIUS 70.0f   // battle units; a pack's members stand about 20-60 apart

void eo_status_flare(int slot) {
    if (slot < 0 || slot >= MAX_ENEMIES || burns[slot].turns_left == 0) {
        return;
    }
    u16 strongest = 0;
    for (int i = 0; i < BURN_STACKS; i++) {
        if (burns[slot].stacks[i] > strongest) {
            strongest = burns[slot].stacks[i];
        }
    }
    f32* from = (f32*)(ENEMY_SLOTS + slot * ENEMY_SLOT_SIZE + 0x24);   // battle data: x, y, z
    eo_fx_flare(slot);
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (i == slot || ENEMY_HP(i) == 0) {
            continue;
        }
        f32* to = (f32*)(ENEMY_SLOTS + i * ENEMY_SLOT_SIZE + 0x24);
        f32 dx = to[0] - from[0];
        f32 dz = to[2] - from[2];
        if (dx * dx + dz * dz <= FLARE_RADIUS * FLARE_RADIUS && !eo_status_blocks(to, 1, 1)) {
            burn_add_stack(i, strongest);
            eo_fx_burn_hit(i);
        }
    }
}

// Damage that ignores element and defense (the stand-in attacker), through the game's own damage routine.
void eo_status_fixed_damage(int slot, u16 amount) {
    if (slot >= 0 && slot < MAX_ENEMIES && ENEMY_HP(slot) > 0 && amount > 0) {
        enemy_take_damage(slot, amount, ELEMENT_FIRE, DAMAGE_NO_ELEMENT, burn_attacker);
    }
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

int eo_status_burn_stacks(int slot) {
    if (slot < 0 || slot >= MAX_ENEMIES || burns[slot].turns_left == 0) {
        return 0;
    }
    int n = 0;
    for (int i = 0; i < BURN_STACKS; i++) {
        n += burns[slot].stacks[i] != 0;
    }
    return n;
}

void eo_status_cure_poison(void) {
    poison_turns = 0;
}

// Water's healing spells cure poison when Brian casts them (not on an enemy's turn). With Undertow they also end
// his other status: its countdown is set to run out at the next tick, which undoes its effect the game's way.
#define BRIAN_STATUS_COUNTDOWNS ((u8*)0x8007BB3A)   // one per status bit

RECOMP_HOOK("spell_type_heal") void eo_status_on_heal(void) {
    if (!(gBattleState & BATTLE_ACTIVE) || !(gBattleState & BATTLE_ENEMY_TURN)) {
        eo_status_cure_poison();
        if (eo_talent(TALENT_UNDERTOW)) {
            for (int i = 0; i < 12; i++) {
                if ((BRIAN_STATUS_BITS & (1 << i)) && BRIAN_STATUS_COUNTDOWNS[i] > 1) {
                    BRIAN_STATUS_COUNTDOWNS[i] = 1;
                }
            }
        }
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

// ---- One status at a time ----

#define STATUS_RESTRICTION 0x008
#define BATTLE_DATA_STATS 0x68     // pointer to the fighter's stats; +0x00 is its u16 status bits
#define STATUS_CHANCE ((u8*)0x8004CDF0)   // spell_status_roll's chance per status bit
#define PROJECTILE_FLAGS 0x0A      // u16; 0x4 = always lands (self-cast)
#define PROJECTILE_SPELL 0x2C      // the spell's stat record
#define SPELL_STATUS_MODE 0x18     // u16; 2 = always lands
#define ALWAYS_LANDS_FLAG 0x4
#define ALWAYS_LANDS_MODE 2

int eo_status_burn_percent(void) {
    return eo_talent(TALENT_FANNED_FLAMES) ? BURN_PERCENT * 3 / 2 : BURN_PERCENT;
}

int eo_status_blocks(void* fighter, int by_brian, int except_burn) {
    if (fighter == 0 || (by_brian && eo_talent(TALENT_SHIFTING_WINDS))) {
        return 0;
    }
    if (fighter == BRIAN_BATTLE_DATA) {
        return BRIAN_STATUS_BITS != 0 || poison_turns > 0;
    }
    if (!is_enemy_battle_data(fighter)) {
        return 0;
    }
    int slot = (int)(((u32)fighter - ENEMY_SLOTS) / ENEMY_SLOT_SIZE);
    u8* stats = *(u8**)((u8*)fighter + BATTLE_DATA_STATS);
    u16 bits = stats != 0 ? *(u16*)stats : 0;
    if (eo_combat_broken(slot)) {
        bits &= (u16)~STATUS_RESTRICTION;
    }
    return bits != 0 || (!except_burn && burns[slot].turns_left > 0);
}

// The game's statuses come from spell_update_all: after the spell type's handler (projectile, target) runs, each
// status the spell carries is rolled (spell_status_roll(projectile, caster, status)) and applied on success. The
// roll doesn't see the target, so the handlers note it; when the target already has a status, the roll is made to
// fail by turning off its three ways to succeed (self-cast flag, always-lands mode, chance) until it returns.
static void* roll_target = 0;
static int roll_blocked = 0;
static u8* roll_projectile;
static u8* roll_spell;
static s32 roll_status;
static u16 saved_flags;
static u16 saved_mode;
static u8 saved_chance;

#define NOTE_TARGET(handler) \
    RECOMP_HOOK(#handler) void eo_status_target_##handler(void* projectile, void* target) { roll_target = target; }
NOTE_TARGET(spell_type_default)
NOTE_TARGET(spell_type_heal)
NOTE_TARGET(spell_type_drain_magic)
NOTE_TARGET(spell_type_status_only)
NOTE_TARGET(spell_type_extinction)
NOTE_TARGET(spell_type_dispel)
NOTE_TARGET(spell_type_dispel_damage)

RECOMP_HOOK("spell_status_roll") void eo_status_before_roll(u8* projectile, void* caster, s32 status) {
    roll_blocked = status >= 0 && status < 12 && (gBattleState & BATTLE_ACTIVE) &&
                   eo_status_blocks(roll_target, caster == BRIAN_BATTLE_DATA, 0);
    if (!roll_blocked) {
        return;
    }
    roll_projectile = projectile;
    roll_spell = *(u8**)(projectile + PROJECTILE_SPELL);
    roll_status = status;
    saved_flags = *(u16*)(projectile + PROJECTILE_FLAGS);
    saved_mode = *(u16*)(roll_spell + SPELL_STATUS_MODE);
    saved_chance = STATUS_CHANCE[status];
    *(u16*)(projectile + PROJECTILE_FLAGS) = (u16)(saved_flags & ~ALWAYS_LANDS_FLAG);
    if (saved_mode == ALWAYS_LANDS_MODE) {
        *(u16*)(roll_spell + SPELL_STATUS_MODE) = 0;
    }
    STATUS_CHANCE[status] = 0;
}

RECOMP_HOOK_RETURN("spell_status_roll") void eo_status_after_roll(void) {
    if (!roll_blocked) {
        return;
    }
    *(u16*)(roll_projectile + PROJECTILE_FLAGS) = saved_flags;
    *(u16*)(roll_spell + SPELL_STATUS_MODE) = saved_mode;
    STATUS_CHANCE[roll_status] = saved_chance;
    roll_blocked = 0;
}

// ---- Display ----

// Brian's poison shows on Brian himself (fx.c), with no icon or turn count.
int eo_status_poisoned(void) {
    return poison_turns > 0;
}

// Set up the stand-in burn attacker once (from main.c).
void eo_status_init(void) {
    *(u8**)(burn_attacker + 0x68) = burn_attacker_stats;
    *(u16*)(burn_attacker_stats + 0x84) = 0xFFFF;
}
