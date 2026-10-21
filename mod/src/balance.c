#include "eo.h"

// First-pass element rebalance of Brian's spells (game design document, "What changes in the numbers"
// and "A job for each element"). Spell power is linear in baseDamage, so damage per cast is base x shots.
//
//   Level-1 spell     vanilla        now
//   Rock              290            247       Earth: heavy hits that need Boost to lead (x0.85)
//   Water Pillar      365            285       Water: moderate, it's the school that heals (x0.78)
//   Wind Cutter       120 x 1        150 x 2   Wind: several light hits
//   Fire Ball         240            200       Fire: modest upfront (x0.83); Burn adds 20% per stack per turn
//
// Without Boost, Water and Wind lead upfront (about 285-300), Earth follows (247) and Fire is lowest (200) but
// leads over a few turns through Burn (20% per stack per turn); with Boost, Earth's +40% per charge makes it the biggest single hit.
// Enemies cast their own copies (enemy_spells.c), so none of this changes them. Tune in playtests.

#define SPELL_STAT_TABLES ((u8**)0x800C1B14)
#define SPELL_STAT_SIZE 0x44
#define SPELL_BASE_DAMAGE 0x0C   // u16
#define SPELL_SHOTS       0x20   // u16

enum { FIRE, EARTH, WATER, WIND };

typedef struct {
    u8 school;
    u8 index;
    u16 base;
    u16 shots;  // 0 = unchanged
} SpellBalance;

static const SpellBalance spell_balance[] = {
    // Fire x0.83
    { FIRE,   0, 200, 0 },  // Fire Ball Lv1      240
    { FIRE,   1, 158, 0 },  // Fire Ball Lv2      190 x 2
    { FIRE,   3,  42, 0 },  // Homing Arrow Lv1    50 x 3
    { FIRE,   4, 267, 0 },  // Hot Steam Lv1      320
    { FIRE,   5, 140, 0 },  // Fire Ball Lv3      168 x 3
    { FIRE,   8, 325, 0 },  // Fire Pillar        390
    { FIRE,   9,  42, 0 },  // Homing Arrow Lv2    50 x 5
    { FIRE,  10, 267, 0 },  // Fire Bomb          320
    { FIRE,  12, 150, 0 },  // Magma Ball         180 x 3
    { FIRE,  14, 275, 0 },  // Hot Steam Lv2      330
    // Earth x0.85: playtest (2026-10-20) found Earth still the default answer even off-weakness at x1.1.
    // Its single-target lead now comes from Boost (+40% per charge).
    { EARTH,  0, 247, 0 },  // Rock Lv1           290
    { EARTH,  1, 264, 0 },  // Rock Lv2           310
    { EARTH,  3, 238, 0 },  // Rolling Rock Lv1   280
    { EARTH,  5, 272, 0 },  // Rock Lv3           320
    { EARTH,  6, 204, 0 },  // Magnet Rock        240
    { EARTH,  8, 391, 0 },  // Avalanche          460 x 10
    { EARTH, 11, 128, 0 },  // Rock Shower        150
    { EARTH, 13, 238, 0 },  // Rolling Rock Lv2   280
    // Water x0.78 (healing unchanged)
    { WATER,  0, 285, 0 },  // Water Pillar Lv1   365
    { WATER,  1, 292, 0 },  // Water Pillar Lv2   374
    { WATER,  4, 298, 0 },  // Water Pillar Lv3   382
    { WATER,  5, 218, 0 },  // Ice Wall           280
    { WATER,  6, 156, 0 },  // Ice Knife          200
    { WATER, 12, 195, 0 },  // Walking Water      250
    // Wind: more, lighter hits
    { WIND,   0, 150, 2 },  // Wind Cutter Lv1    120 x 1 (now 2 shots, like Lv2's 3)
    { WIND,   8, 200, 0 },  // Wind Bomb          180
    { WIND,  10, 280, 0 },  // Cyclone            250
};

static void scale_bosses(void);

void eo_balance_apply(void) {
    scale_bosses();
    for (u32 i = 0; i < sizeof(spell_balance) / sizeof(spell_balance[0]); i++) {
        const SpellBalance* b = &spell_balance[i];
        u8* spell = SPELL_STAT_TABLES[b->school] + b->index * SPELL_STAT_SIZE;
        *(u16*)(spell + SPELL_BASE_DAMAGE) = b->base;
        if (b->shots != 0) {
            *(u16*)(spell + SPELL_SHOTS) = b->shots;
        }
    }
}

// ---- Enemies keep pace with the overhaul's power (balance pass, 2026-10-21) ----
// Gear, talents, Break, Boost and Burn raise what an optimised Brian deals per turn to about 1.15x vanilla in
// the first beat, 1.5x by Zelse, 2x by the desert and 2.2x at the end (research/balance/README.md). Enemy HP is
// scaled by monster set a little above that, so fights take about as many turns as before or slightly more;
// attack rises a little late, against the new robes and defensive talents. Zelse keeps his spike over Solvaring.
// All percentages.

#define MONSTER_SIZE 0x38
#define MONSTER_HP_CUR 0x04
#define MONSTER_HP_MAX 0x06
#define MONSTER_ATTACK 0x2A
#define CURRENT_SET_ROW (*(u32**)0x8007D0BC)
#define SET_TABLE 0x80054160
#define SET_ROW_SIZE 0x14
#define BOSS_DATA ((u8*)0x803A9F20)
#define BOSS_COUNT 8

typedef struct {
    u8 count;      // monsters in the set
    u16 hp;        // HP percent
    u16 attack;    // attack percent
} SetScale;

static const SetScale set_scale[] = {
    { 12, 120, 100 },   // 0 Holy Plains, Connor Forest (beat 1)
    { 15, 150, 105 },   // 1 Dondoran Flats and the long road (beat 2)
    { 11, 175, 110 },   // 2 Carmagh caves, Limelin (beats 2 and 4)
    { 13, 190, 115 },   // 3 the desert, Boil Hole, Brannoch (beats 4-7)
    { 12, 165, 110 },   // 4 Normoon, Isle of Skye (beats 2-3)
    { 4,  210, 120 },   // 5 Mammon's World
};

static const SetScale boss_scale[BOSS_COUNT] = {
    { 1, 120, 100 },   // Solvaring
    { 1, 155, 105 },   // Zelse: the spike stays
    { 1, 170, 110 },   // Nepty
    { 1, 180, 110 },   // Shilf
    { 1, 190, 115 },   // Fargo: the second spike stays
    { 1, 200, 115 },   // Guilty
    { 1, 205, 120 },   // Beigis
    { 1, 215, 120 },   // Mammon
};

static void scale_monster(u8* m, const SetScale* s) {
    u32 hp = (u32)*(u16*)(m + MONSTER_HP_MAX) * s->hp / 100;
    hp = hp > 0xFFFF ? 0xFFFF : hp;
    *(u16*)(m + MONSTER_HP_MAX) = (u16)hp;
    *(u16*)(m + MONSTER_HP_CUR) = (u16)hp;
    u32 atk = (u32)*(u16*)(m + MONSTER_ATTACK) * s->attack / 100;
    *(u16*)(m + MONSTER_ATTACK) = (u16)(atk > 0xFFFF ? 0xFFFF : atk);
}

// The set is copied fresh from ROM each time a map loads it, so scaling it once per load is exact.
RECOMP_HOOK_RETURN("enemy_set_load") void eo_balance_on_set_load(void) {
    u32* row = CURRENT_SET_ROW;
    if (row == 0) {
        return;
    }
    u32 set = ((u32)row - SET_TABLE) / SET_ROW_SIZE;
    if (set >= sizeof(set_scale) / sizeof(set_scale[0])) {
        return;
    }
    u8* monsters = (u8*)row[3];
    for (int i = 0; i < set_scale[set].count; i++) {
        scale_monster(monsters + i * MONSTER_SIZE, &set_scale[set]);
    }
}

// Boss data stays in RAM from boot: scaled once (from eo_balance_apply).
static void scale_bosses(void) {
    for (int i = 0; i < BOSS_COUNT; i++) {
        scale_monster(BOSS_DATA + i * MONSTER_SIZE, &boss_scale[i]);
    }
}
