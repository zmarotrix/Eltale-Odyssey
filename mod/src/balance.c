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

// ---- Enemies, from the balance engine (tools/balance/engine.py, 2026-10-23) ----
// Fitted for a player who finds 75% of the spirits, with the talent trees and the new spells, to play close to the
// original game, a little harder (Spell Progression Model doc, "Matching the original game's difficulty").
//
// Bosses: HP set outright (vanilla 200 to 2,300; now 270 to 6,220); attack as before; and every boss hit on Brian
// scaled by the engine's calibration, so an average boss action takes about 10-17% of the HP Brian should have
// by then (eo_balance_enemy_hit_permille, used by spells.c where hits on Brian are adjusted).
// Monsters: HP by set (the engine's pack HP, x1.3 per monster); attack by set as before. The engine flattens
// every monster's hit to a share of Brian's HP, which would make the first areas' monsters hit several times
// harder than the original's, so their attack keeps the game's own spread.

#define MONSTER_SIZE 0x38
#define MONSTER_HP_CUR 0x04
#define MONSTER_HP_MAX 0x06
#define MONSTER_ATTACK 0x2A
#define CURRENT_SET_ROW (*(u32**)0x8007D0BC)
#define SET_TABLE 0x80054160
#define SET_ROW_SIZE 0x14
#define BOSS_DATA ((u8*)0x803A9F20)
#define BOSS_COUNT 8
#define BOSSES_DEFEATED (*(u32*)0x8007D19C)   // one bit a boss (research/enemies)
#define CURRENT_BOSS (*(s32*)0x8008C570)       // 1 Solvaring .. 8 Mammon in a boss fight
#define BATTLE_BOSS 0x100

typedef struct {
    u8 count;      // monsters in the set
    u16 hp;        // HP percent
    u16 attack;    // attack percent
} SetScale;

static const SetScale set_scale[] = {
    { 12, 140, 115 },   // 0 Holy Plains, Connor Forest (before Solvaring): 1.2x fell in 2 hits, 2x took too long
    { 15, 142, 105 },   // 1 Dondoran Flats and the long road (before Zelse)
    { 11, 208, 110 },   // 2 Carmagh caves, Limelin (before Shilf)
    { 13, 199, 115 },   // 3 the desert, Boil Hole, Brannoch (Fargo to Beigis: see desert_hp)
    { 12, 125, 110 },   // 4 Normoon, Isle of Skye (before Nepty)
    { 4,  207, 120 },   // 5 Mammon's World
};
// The desert's set is met on the way to three bosses: its HP follows how many are down.
#define DESERT_SET 3
static const u16 desert_hp[3] = { 199, 234, 217 };   // before Fargo, Guilty, Beigis

static const u16 boss_hp[BOSS_COUNT] = { 270, 900, 1190, 2560, 3570, 3660, 5930, 6220 };
static const u16 boss_attack[BOSS_COUNT] = { 100, 105, 110, 110, 115, 115, 120, 120 };
// Each boss hit on Brian, in thousandths: Solvaring, Zelse, Nepty, Shilf, Fargo, Guilty, Beigis, Mammon.
static const u16 boss_hit[BOSS_COUNT] = { 579, 706, 637, 845, 1061, 980, 816, 552 };

static void scale_monster(u8* m, u32 hp_percent, u32 attack_percent) {
    u32 hp = (u32)*(u16*)(m + MONSTER_HP_MAX) * hp_percent / 100;
    hp = hp > 0xFFFF ? 0xFFFF : hp;
    *(u16*)(m + MONSTER_HP_MAX) = (u16)hp;
    *(u16*)(m + MONSTER_HP_CUR) = (u16)hp;
    u32 atk = (u32)*(u16*)(m + MONSTER_ATTACK) * attack_percent / 100;
    *(u16*)(m + MONSTER_ATTACK) = (u16)(atk > 0xFFFF ? 0xFFFF : atk);
}

static int bosses_down(void) {
    int n = 0;
    for (u32 bits = BOSSES_DEFEATED; bits != 0; bits &= bits - 1) {
        n++;
    }
    return n;
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
    u32 hp = set_scale[set].hp;
    if (set == DESERT_SET) {
        int down = bosses_down();   // Fargo is the fifth boss: 4 down on the way to him
        hp = desert_hp[down <= 4 ? 0 : down == 5 ? 1 : 2];
    }
    u8* monsters = (u8*)row[3];
    for (int i = 0; i < set_scale[set].count; i++) {
        scale_monster(monsters + i * MONSTER_SIZE, hp, set_scale[set].attack);
    }
}

// Boss data stays in RAM from boot: set once (from eo_balance_apply).
static void scale_bosses(void) {
    for (int i = 0; i < BOSS_COUNT; i++) {
        u8* m = BOSS_DATA + i * MONSTER_SIZE;
        *(u16*)(m + MONSTER_HP_MAX) = boss_hp[i];
        *(u16*)(m + MONSTER_HP_CUR) = boss_hp[i];
        scale_monster(m, 100, boss_attack[i]);
    }
}

// How hard an enemy's hit on Brian lands, in thousandths (spells.c).
u32 eo_balance_enemy_hit_permille(void) {
    extern u16 gBattleState;
    if ((gBattleState & BATTLE_BOSS) && CURRENT_BOSS >= 1 && CURRENT_BOSS <= BOSS_COUNT) {
        return boss_hit[CURRENT_BOSS - 1];
    }
    return 1000;
}
