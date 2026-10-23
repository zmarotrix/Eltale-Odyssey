#include "eo.h"

// Numbers the overhaul changes at boot: a new game's gold, and enemy HP and attack (below). Brian's spells are
// spells.c's.

static void scale_bosses(void);

// A new game's starting gold, from the game's new-game stats table (ROM 0x54908, copied into Brian's stats by
// new_game_init; +0x10 is the gold counter). Vanilla gives 500; 100 now that early chests hold some (playtest,
// 2026-10-22).
#define NEW_GAME_GOLD (*(u32*)0x80053D18)
#define STARTING_GOLD 100

void eo_balance_apply(void) {
    NEW_GAME_GOLD = STARTING_GOLD;
    scale_bosses();
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
