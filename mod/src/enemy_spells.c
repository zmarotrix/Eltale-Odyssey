#include "eo.h"

// Gives enemies their own copies of the spells they cast, so Brian's spells and enemy spells can be
// balanced separately (research/enemies, "Giving enemies their own spells").
//
// The game keeps one stat table and one visuals table per school; school 4 holds the 17 boss spells, and
// monsters cast Brian's spells straight from schools 0-3 (action ID 0x8000 | school << 8 | index).
// This builds a bigger school-4 table in mod memory:
//   0x00-0x10  the 17 boss spells, unchanged
//   0x11       Zelse's Large Cutter (the only player spell a boss casts; see BOSS_COPY below)
//   0x12+      a copy of every player spell: 0x12 + school * 15 + index
// then points the game's school-4 pointers at it and rewrites monster actions to 0x84nn.

#define SPELL_STAT_TABLES   ((u8**)0x800C1B14)  // 5 pointers: Fire, Earth, Water, Wind, enemy
#define SPELL_VISUAL_TABLES ((u8**)0x800C3520)  // same, for the visuals
#define SPELL_STAT_SIZE   0x44
#define SPELL_VISUAL_SIZE 0x48
#define PLAYER_SCHOOLS 4
#define SPELLS_PER_SCHOOL 15
#define ENEMY_SCHOOL 4
#define BOSS_SPELLS 17

// boss_spell_anims_queue looks up a 20-entry table by the spell index alone, for every action in a boss
// fight. Entries 0x11-0x13 are null, so a player-spell copy a boss casts must sit there, not at 0x12+.
#define BOSS_COPY_INDEX 0x11
#define BOSS_COPY_SPELL 0x0306   // Large Cutter (Wind, index 6), cast by Zelse
#define PLAYER_COPY_BASE 0x12
#define ENEMY_SPELL_COUNT (PLAYER_COPY_BASE + PLAYER_SCHOOLS * SPELLS_PER_SCHOOL)

#define ACTION_SIZE 0x18
#define ACTION_CAST 0x8000
#define MONSTER_SIZE 0x38
#define MONSTER_ACTION_COUNT 0x2C   // u16
#define MONSTER_ACTION_PTR   0x30   // u8*

// Bosses: 8 monster records at a fixed address (loaded at boot).
#define BOSS_MONSTERS 0x803A9F20
#define BOSS_COUNT 8

// The loaded monster set: row pointer into the set table (romStart, romEnd, models, monsterData, names).
#define CURRENT_SET_ROW (*(u32**)0x8007D0BC)
#define SET_TABLE 0x80054160
#define SET_ROW_SIZE 0x14
static const u8 monsters_per_set[] = { 12, 15, 11, 13, 12, 4 };

static u8 enemy_stats[ENEMY_SPELL_COUNT][SPELL_STAT_SIZE] __attribute__((aligned(8)));
static u8 enemy_visuals[ENEMY_SPELL_COUNT][SPELL_VISUAL_SIZE] __attribute__((aligned(8)));
static int tables_ready = 0;

static void copy_bytes(u8* dst, const u8* src, u32 size) {
    for (u32 i = 0; i < size; i++) {
        dst[i] = src[i];
    }
}

static void copy_spell(int to, int school, int index) {
    copy_bytes(enemy_stats[to], SPELL_STAT_TABLES[school] + index * SPELL_STAT_SIZE, SPELL_STAT_SIZE);
    copy_bytes(enemy_visuals[to], SPELL_VISUAL_TABLES[school] + index * SPELL_VISUAL_SIZE, SPELL_VISUAL_SIZE);
}

// The enemy-table index that replaces a player spell (school 0-3) in a monster's or boss's action.
static int copy_index_for(u16 spell) {
    if (spell == BOSS_COPY_SPELL) {
        return BOSS_COPY_INDEX;
    }
    return PLAYER_COPY_BASE + (spell >> 8) * SPELLS_PER_SCHOOL + (spell & 0xFF);
}

// Rewrites a monster's cast actions from player spells to their enemy copies. Safe to run twice:
// rewritten actions are school 4 and are left alone.
static void rewrite_actions(u8* monster) {
    u16 count = *(u16*)(monster + MONSTER_ACTION_COUNT);
    u8* actions = *(u8**)(monster + MONSTER_ACTION_PTR);
    if (actions == 0 || count > 3) {
        return;
    }
    for (u16 i = 0; i < count; i++) {
        u16* id = (u16*)(actions + i * ACTION_SIZE);
        if ((*id & ACTION_CAST) == 0) {
            continue;  // physical attack
        }
        u16 spell = *id & 0x7FFF;
        u16 school = spell >> 8;
        if (school < PLAYER_SCHOOLS && (spell & 0xFF) < SPELLS_PER_SCHOOL) {
            *id = ACTION_CAST | (ENEMY_SCHOOL << 8) | copy_index_for(spell);
        }
    }
}

// Runs once the game has copied its spell tables into RAM (from main.c), before Brian's spells are rebalanced.
void eo_enemy_spells_init(void) {
    for (int i = 0; i < BOSS_SPELLS; i++) {
        copy_spell(i, ENEMY_SCHOOL, i);
    }
    copy_spell(BOSS_COPY_INDEX, BOSS_COPY_SPELL >> 8, BOSS_COPY_SPELL & 0xFF);
    for (int school = 0; school < PLAYER_SCHOOLS; school++) {
        for (int index = 0; index < SPELLS_PER_SCHOOL; index++) {
            copy_spell(PLAYER_COPY_BASE + school * SPELLS_PER_SCHOOL + index, school, index);
        }
    }
    SPELL_STAT_TABLES[ENEMY_SCHOOL] = enemy_stats[0];
    SPELL_VISUAL_TABLES[ENEMY_SCHOOL] = enemy_visuals[0];

    for (int boss = 0; boss < BOSS_COUNT; boss++) {
        rewrite_actions((u8*)(BOSS_MONSTERS + boss * MONSTER_SIZE));
    }
    tables_ready = 1;
}

// A monster set was just copied in (each map load): point its spell actions at the enemy copies.
RECOMP_HOOK_RETURN("enemy_set_load") void eo_enemy_spells_on_set_load(void) {
    u32* row = CURRENT_SET_ROW;
    if (!tables_ready || row == 0) {
        return;
    }
    u32 set = ((u32)row - SET_TABLE) / SET_ROW_SIZE;
    if (set >= sizeof(monsters_per_set)) {
        return;
    }
    u8* monsters = (u8*)row[3];
    for (int i = 0; i < monsters_per_set[set]; i++) {
        rewrite_actions(monsters + i * MONSTER_SIZE);
    }
}

