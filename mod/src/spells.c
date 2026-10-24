#include "eo.h"

// The 60 spells of the overhaul (Spell Trees doc, "How combos work" and the four school tables, 2026-10-22/23).
// Every vanilla slot keeps its button presses and its tier (from the level vanilla gave it), and becomes the
// design's spell for that combo. Each one borrows a vanilla record (how it flies, what it targets, whether it
// passes through foes) and a vanilla look, then gets its own type, power and shots; what makes it that spell
// happens here: Burn, Frost and Freeze, shields, splash onto nearby foes, guards and other effects on Brian.
//
// Numbers are the balance engine's (tools/balance/engine.py); spell power goes through progression.c's curve.
// Effects that need a hit run a frame after it (so the game's damage routine is never re-entered from its own
// hooks); effects of self-cast spells run as they are cast.

#define SPELL_STAT_TABLES   ((u8**)0x800C1B14)
#define SPELL_VISUAL_TABLES ((u8**)0x800C3520)
#define SPELL_STAT_SIZE 0x44
#define SPELL_VISUAL_SIZE 0x48
#define ENEMY_SCHOOL 4
#define VANILLA_COPY_BASE 0x12   // enemy_spells.c keeps an untouched copy of every player spell here
#define SCHOOLS 4
#define SPELLS 15

// Stat record fields (research/spells).
#define REC_RULE 0x02
#define RULE_OUTSIDE_BATTLE 0x0002
#define REC_FIRST_COPIED 0x0A    // range onwards; level, presses and count stay the slot's own
#define REC_BASE 0x0C
#define REC_TYPE 0x10
#define REC_SHOTS 0x20
#define REC_STATUS 0x32          // 12 status bytes
#define STATUS_BYTES 12

#define TYPE_DAMAGE 0
#define TYPE_SELF 5              // "no damage": buffs and statuses

#define MAX_ENEMIES 6
#define ENEMY_SLOTS 0x8007C998
#define ENEMY_SLOT_SIZE 0x128
#define ENEMY_BATTLE_DATA(i) ((u8*)(ENEMY_SLOTS + (i) * ENEMY_SLOT_SIZE + 0x24))
#define ENEMY_HP(i) (*(u16*)(ENEMY_SLOTS + (i) * ENEMY_SLOT_SIZE + 0x0A))
#define ENEMY_RECORD(i) (*(u8**)(ENEMY_BATTLE_DATA(i) + 0x64))
#define RECORD_MAX_HP 0x06
#define BRIAN_BATTLE_DATA ((u8*)0x8007BACC)
#define BRIAN_HP     (((u16*)gPlayerMainData)[2])
#define BRIAN_MAX_HP (((u16*)gPlayerMainData)[3])
#define BRIAN_MP     (((u16*)gPlayerMainData)[4])
#define BRIAN_MAX_MP (((u16*)gPlayerMainData)[5])
#define BRIAN_STATUS_BITS (*(u16*)0x8007BB38)
#define BRIAN_STATUS_COUNTDOWNS ((u8*)0x8007BB3A)
#define ELEMENT_LEVELS ((u8*)0x8007BAA4)

#define BATTLE_ACTIVE     0x1
#define BATTLE_ENEMY_TURN 0x2
#define BATTLE_BOSS       0x100

// Fighter stats (battle data +0x68): status bits, then a countdown per bit; +0x90 the move range multiplier.
#define STATS(bd) (*(u8**)((bd) + 0x68))
#define STATUS_MOVEMENT 0x010
#define MOVEMENT_COUNTDOWN 6     // 2 + the bit's index
#define STATS_RANGE 0x90

enum { FIRE, EARTH, WATER, WIND };

s32 get_rand(s32 n);
extern u16 gBattleState;

// ---- The spell table ----

enum {
    K_PLAIN,       // damage only
    K_KINDLE,      // p = Burn stacks
    K_BELLOWS, K_SCALD, K_FLARE_UP, K_PYRE, K_FIRESTORM, K_DETONATE, K_BANKED, K_METEOR, K_HEAT_HAZE, K_SEARING,
    K_HEAVY,       // p = extra shields a weakness hit cracks
    K_MOLTEN, K_STONESKIN, K_MUDSHOT, K_QUAGMIRE, K_FISSURE, K_LANDSLIDE, K_JUGGERNAUT, K_ERUPTION, K_ROCKSTORM,
    K_SPOUT, K_MEND, K_ICE_WALL, K_SOOTHE,
    K_CHILL,       // p = steps colder
    K_HAIL, K_WHITEOUT, K_FROST_NOVA, K_RENEW, K_SHATTER, K_UNDERTOW, K_ABSOLUTE_ZERO,
    K_FAN, K_WINDCHILL, K_GUST,
    K_TEMPEST,     // p = percent a hit cracks a shield of any weakness
    K_WHIRLWIND, K_CLEAR_SIGHT, K_CONFUSION, K_DOWNBURST,
    K_VANILLA,     // the borrowed record's own effect (Erode, Escape, Homeward, Swiftstep, Mirage)
};

// Where a hit also lands: only the target, foes near it, foes in a wider ring, or every foe.
enum { A_ONE, A_NEAR, A_WIDE, A_ALL };
#define NEAR_RADIUS 70.0f        // battle units; a pack stands about 20-60 apart
#define WIDE_RADIUS 110.0f
#define BOOST_RADIUS 25.0f       // more reach per charge (Burst, Blast, Detonate, Quagmire, Fissure)

typedef struct {
    const char* name;            // at most 15 characters, the game's name slots
    const char* text;            // one line in the HUD's small font
    u8 rec;                      // vanilla slot (school x 15 + index) whose record it borrows
    u8 look;                     // vanilla slot whose visuals and sounds it borrows
    u8 type;
    u8 shots;
    u16 base;
    u8 kind;
    u8 area;
    u8 p;
    u8 mp;
} SpellDef;

#define F(i) (i)
#define E(i) (15 + (i))
#define W(i) (30 + (i))
#define A(i) (45 + (i))

static const SpellDef spells[SCHOOLS][SPELLS] = {
    {   // Fire: kindle sets Burn, stoke heats it, spread throws it, clashes set it off.
        { "EMBER", "A FIREBALL. 1 BURN STACK.", F(0), F(0), TYPE_DAMAGE, 1, 200, K_KINDLE, A_ONE, 1, 2 },
        { "TWIN FLAME", "TWO FIREBALLS. 1 BURN STACK EACH.", F(1), F(1), TYPE_DAMAGE, 2, 115, K_KINDLE, A_ONE, 1, 3 },
        { "BURST", "EXPLODES: BURNS EVERY FOE NEAR IT.", F(0), F(0), TYPE_DAMAGE, 1, 180, K_KINDLE, A_NEAR, 1, 4 },
        { "BELLOWS", "FOE'S BURN DEALS 1.5X FOR 3 TURNS.", F(3), F(3), TYPE_DAMAGE, 1, 120, K_BELLOWS, A_ONE, 0, 4 },
        { "SCALD", "STEAM. BURNING FOES TICK AT ONCE.", F(14), F(14), TYPE_DAMAGE, 1, 150, K_SCALD, A_ONE, 0, 4 },
        { "BLAZE", "A BIG FIREBALL. 2 BURN STACKS.", F(5), F(5), TYPE_DAMAGE, 1, 320, K_KINDLE, A_ONE, 2, 7 },
        { "FLARE UP", "FOE'S BURN GROWS BY HALF. RESTARTS.", F(5), F(5), TYPE_DAMAGE, 1, 150, K_FLARE_UP, A_ONE, 0, 7 },
        { "BLAST", "WIDE BLAST. 2 BURN STACKS ON ALL HIT.", F(10), F(10), TYPE_DAMAGE, 1, 220, K_KINDLE, A_WIDE, 2, 7 },
        { "PYRE", "ALL THE FOE'S BURN LANDS AT ONCE.", F(5), F(5), TYPE_DAMAGE, 1, 40, K_PYRE, A_ONE, 0, 7 },
        { "FIRESTORM", "HITS ALL. EVERY BURN DEALS 1.5X.", F(5), F(5), TYPE_DAMAGE, 1, 100, K_FIRESTORM, A_ALL, 0, 10 },
        { "DETONATE", "FOE'S BURN BURSTS ONTO FOES NEAR IT.", F(10), F(10), TYPE_DAMAGE, 1, 100, K_DETONATE, A_ONE, 0, 10 },
        { "BANKED FIRE", "BURN WAITS 2 TURNS. THEN TICKS 2X.", F(0), F(0), TYPE_DAMAGE, 1, 80, K_BANKED, A_ONE, 0, 10 },
        { "METEOR", "NEXT TURN A METEOR HITS THE AREA.", F(10), F(12), TYPE_DAMAGE, 1, 60, K_METEOR, A_ONE, 0, 16 },
        { "HEAT HAZE", "3 TURNS: FOES THAT HIT YOU BURN.", F(2), F(2), TYPE_SELF, 1, 0, K_HEAT_HAZE, A_ONE, 0, 14 },
        { "SEARING STEAM", "3X THE FOE'S BURN AT ONCE. THAWS ALL.", F(14), F(14), TYPE_DAMAGE, 1, 30, K_SEARING, A_ONE, 0, 12 },
    },
    {   // Earth: heavy hits that Settle Brian (combat.c); guards; shields by force.
        { "ROCK", "A HEAVY STONE. SETTLES YOU.", E(0), E(0), TYPE_DAMAGE, 1, 495, K_HEAVY, A_ONE, 0, 2 },
        { "BOULDER", "BIG STONE. A WEAK HIT CRACKS 2.", E(1), E(1), TYPE_DAMAGE, 1, 600, K_HEAVY, A_ONE, 1, 3 },
        { "STONESKIN", "TAKE 30 PCT LESS DAMAGE: 3 TURNS.", E(2), E(2), TYPE_SELF, 1, 0, K_STONESKIN, A_ONE, 0, 4 },
        { "MOLTEN ROCK", "A HEAVY HIT THAT ALSO BURNS.", E(3), E(3), TYPE_DAMAGE, 1, 550, K_MOLTEN, A_ONE, 0, 4 },
        { "SANDBLAST", "4 GRIT HITS. DOESN'T SETTLE YOU.", E(0), E(0), TYPE_DAMAGE, 4, 110, K_PLAIN, A_ONE, 0, 4 },
        { "MEGALITH", "THE BIGGEST HIT. SETTLES YOU 2 DEEP.", E(5), E(5), TYPE_DAMAGE, 1, 1035, K_HEAVY, A_ONE, 0, 10 },
        { "MUDSHOT", "A HEAVY HIT THAT FROSTS THE FOE.", E(6), E(6), TYPE_DAMAGE, 1, 690, K_MUDSHOT, A_ONE, 0, 7 },
        { "QUAGMIRE", "MUD: FROSTS EVERY FOE NEAR THE FOE.", E(4), E(4), TYPE_DAMAGE, 1, 60, K_QUAGMIRE, A_WIDE, 0, 7 },
        { "AVALANCHE", "HEAVY ROCKS FALL ON EVERY FOE.", E(11), E(11), TYPE_DAMAGE, 1, 450, K_HEAVY, A_ONE, 0, 7 },
        { "FISSURE", "CRACKS A SHIELD ON ALL FOES NEAR IT.", E(0), E(0), TYPE_DAMAGE, 1, 40, K_FISSURE, A_NEAR, 0, 10 },
        { "ERODE", "LOWERS A FOE'S DEFENSE.", E(10), E(10), TYPE_SELF, 1, 1, K_VANILLA, A_ONE, 0, 10 },
        { "LANDSLIDE", "HITS ALL IN ITS LINE. PUSHES BACK.", E(13), E(13), TYPE_DAMAGE, 1, 715, K_LANDSLIDE, A_ONE, 0, 10 },
        { "JUGGERNAUT", "NEXT SWING DEALS 3X. CRACKS 2.", E(2), E(7), TYPE_SELF, 1, 0, K_JUGGERNAUT, A_ONE, 0, 14 },
        { "ERUPTION", "HUGE HIT NEAR THE FOE. BURNS ALL.", E(5), E(5), TYPE_DAMAGE, 1, 945, K_ERUPTION, A_NEAR, 0, 16 },
        { "ROCKSTORM", "3 TURNS: A BOULDER FALLS EACH TURN.", E(2), E(12), TYPE_SELF, 1, 0, K_ROCKSTORM, A_ONE, 0, 14 },
    },
    {   // Water: Frost into Freeze, guards, weak heals.
        { "SPOUT", "A PILLAR OF WATER. BOOST: FROST.", W(0), W(0), TYPE_DAMAGE, 1, 280, K_SPOUT, A_ONE, 0, 2 },
        { "MEND", "FREE. HEALS SOME HP AND MP.", W(2), W(2), TYPE_SELF, 1, 0, K_MEND, A_ONE, 0, 0 },
        { "ICE WALL", "ENEMY SPELLS CAN'T HIT YOU: 1 TURN.", W(2), W(5), TYPE_SELF, 1, 0, K_ICE_WALL, A_ONE, 0, 4 },
        { "SOOTHE", "A SMALL HEAL. CURES YOUR STATUS.", W(2), W(10), TYPE_SELF, 1, 0, K_SOOTHE, A_ONE, 0, 3 },
        { "ICICLE", "ICE SPEAR. FROSTS: FROST FREEZES.", W(6), W(6), TYPE_DAMAGE, 1, 420, K_CHILL, A_ONE, 1, 4 },
        { "GLACIER", "FROSTS EVERY FOE NEAR THE FOE.", W(5), W(5), TYPE_DAMAGE, 1, 120, K_CHILL, A_NEAR, 1, 4 },
        { "HAILSTORM", "ICE HITS NEAR THE FOE. MAY FREEZE.", W(6), W(6), TYPE_DAMAGE, 6, 70, K_HAIL, A_NEAR, 0, 7 },
        { "ICE SHARDS", "3 ICE DARTS. EACH ONE FROSTS.", W(6), W(6), TYPE_DAMAGE, 3, 140, K_CHILL, A_ONE, 1, 7 },
        { "WHITEOUT", "FOG: FOES MISS MORE FOR 2 TURNS.", W(2), W(14), TYPE_SELF, 1, 0, K_WHITEOUT, A_ONE, 0, 7 },
        { "FROST NOVA", "COLD BURST: FROSTS FOES NEAR YOU.", W(2), W(5), TYPE_SELF, 1, 90, K_FROST_NOVA, A_ONE, 0, 7 },
        { "RENEW", "HEALS AT THE START OF 3 TURNS.", W(10), W(10), TYPE_SELF, 1, 0, K_RENEW, A_ONE, 0, 8 },
        { "SHATTER", "1.8X ON A FROZEN FOE. ENDS FREEZE.", W(6), W(6), TYPE_DAMAGE, 1, 315, K_SHATTER, A_ONE, 0, 10 },
        { "UNDERTOW", "DRAGS THE FOE CLOSE. FROSTS IT.", W(12), W(12), TYPE_DAMAGE, 1, 260, K_UNDERTOW, A_ONE, 0, 10 },
        { "ABSOLUTE ZERO", "FREEZES THE FOE FOR 2 TURNS.", E(10), W(3), TYPE_SELF, 1, 1, K_ABSOLUTE_ZERO, A_ONE, 0, 12 },
        { "DELUGE", "A WAVE ON EVERY FOE. FROSTS ALL.", W(4), W(4), TYPE_DAMAGE, 1, 500, K_CHILL, A_ALL, 1, 16 },
    },
    {   // Wind: many hits; any Wind hit can crack a shield; tempo and defense.
        { "WIND BLADE", "TWO LIGHT BLADES OF WIND.", A(0), A(0), TYPE_DAMAGE, 2, 150, K_PLAIN, A_ONE, 0, 2 },
        { "SHEAR", "3 BLADES. FOES NEAR IT ARE CUT TOO.", A(1), A(1), TYPE_DAMAGE, 3, 140, K_PLAIN, A_NEAR, 0, 3 },
        { "FAN THE FLAMES", "LIGHT HITS. 1.6X ON BURNING FOES.", A(1), A(1), TYPE_DAMAGE, 3, 90, K_FAN, A_ONE, 0, 2 },
        { "WINDCHILL", "COLD HITS THAT MAY FROST THE FOE.", A(1), A(1), TYPE_DAMAGE, 3, 100, K_WINDCHILL, A_ONE, 0, 4 },
        { "GUST", "NO DAMAGE. BLOWS A FOE FAR AWAY.", A(4), A(4), TYPE_SELF, 1, 1, K_GUST, A_ONE, 0, 4 },
        { "TEMPEST", "6 HITS. EACH MAY CRACK ANY SHIELD.", A(5), A(5), TYPE_DAMAGE, 6, 110, K_TEMPEST, A_ONE, 10, 4 },
        { "ESCAPE", "FLEE FROM BATTLE.", W(8), W(8), 0xFF, 0, 0, K_VANILLA, A_ONE, 0, 2 },
        { "HOMEWARD", "RETURNS YOU TO THE LAST TOWN.", W(9), W(9), 0xFF, 0, 0, K_VANILLA, A_ONE, 0, 4 },
        { "SWIFTSTEP", "YOU MOVE FARTHER FOR A WHILE.", A(12), A(12), 0xFF, 0, 0, K_VANILLA, A_ONE, 0, 7 },
        { "MIRAGE", "AFTERIMAGES: FOES MISS MORE.", A(9), A(9), 0xFF, 0, 0, K_VANILLA, A_ONE, 0, 5 },
        { "WHIRLWIND", "HITS EVERY FOE. PULLS THEM TOGETHER.", A(10), A(10), TYPE_DAMAGE, 1, 340, K_WHIRLWIND, A_ALL, 0, 10 },
        { "CLEAR SIGHT", "SHOWS A FOE'S WEAKNESS.", W(3), W(3), 0xFF, 0, 0, K_CLEAR_SIGHT, A_ONE, 0, 4 },
        { "CONFUSION", "FOES HIT EACH OTHER. BOSSES WAIT.", E(4), E(9), TYPE_SELF, 1, 1, K_CONFUSION, A_ONE, 0, 14 },
        { "DOWNBURST", "ONE HEAVY HIT. CRACKS 2 SHIELDS.", A(6), A(6), TYPE_DAMAGE, 1, 800, K_DOWNBURST, A_ONE, 0, 14 },
        { "HURRICANE", "HITS ALL 6 TIMES. EVERY HIT CRACKS.", A(5), A(5), TYPE_DAMAGE, 6, 90, K_TEMPEST, A_ALL, 100, 16 },
    },
};

static const SpellDef* def_of(int school, int index) {
    return school >= 0 && school < SCHOOLS && index >= 0 && index < SPELLS ? &spells[school][index] : 0;
}

const char* eo_spells_name(int school, int index) {
    const SpellDef* d = def_of(school, index);
    return d != 0 ? d->name : 0;
}

const char* eo_spells_text(int school, int index) {
    const SpellDef* d = def_of(school, index);
    return d != 0 ? d->text : 0;
}

u32 eo_spells_mp_cost(int school, int index) {
    const SpellDef* d = def_of(school, index);
    return d != 0 ? (d->mp * (100 - (u32)eo_talents_mp_percent(school)) + 50) / 100 : 0;
}

// ---- MP: every spell its own cost ----
//
// The game checks and takes MP from a table by the number of presses (cost[n] = n; research/battle, "MP cost").
// Just before it checks a spell (spell_check_castable) or casts one (spell_cast_init), that entry is set to the
// spell's own cost, so "not enough MP" and the payment are the game's as always.

#define MP_COSTS ((u16*)0x800C1B26)   // u16 [presses]
#define REC_PRESSES 0x04
#define BRIAN_SPELL_PRESSES ((u8*)0x8007BBD1)
#define BRIAN_SPELL_COUNT (*(u16*)0x8007BBD4)
s32 spell_find_by_inputs(u16 count, u8* inputs);

#define REC_PRESS_LIST 0x06
#define MENU_PAD (*(u16*)0x80092876)    // the buttons spell_menu_input reads
#define MENU_C_BUTTONS 0x000F

static u32 cost_of(int school, int index) {
    return (spells[school][index].mp * (100 - (u32)eo_talents_mp_percent(school)) + 50) / 100;
}

// While Brian presses elements, each press is checked against the spell the presses so far make, and refused if
// he can't pay for it. Pressing should always open a school's spells (playtest, 2026-10-23), so a press costs
// nothing; casting checks the spell's own cost.
static int menu_open = 0;

RECOMP_HOOK("spell_menu_input") void eo_spells_menu_begin(void) {
    menu_open = 1;
}

RECOMP_HOOK_RETURN("spell_menu_input") void eo_spells_menu_end(void) {
    menu_open = 0;
}

static void set_mp_cost(s32 spell) {
    int school = (spell >> 8) & 0xF;
    int index = spell & 0xFF;
    const SpellDef* d = def_of(school, index);
    if (spell < 0 || d == 0) {
        return;
    }
    u16 presses = *(u16*)(SPELL_STAT_TABLES[school] + index * SPELL_STAT_SIZE + REC_PRESSES);
    if (presses >= 1 && presses <= 3) {
        int pressing = menu_open && (MENU_PAD & MENU_C_BUTTONS);
        MP_COSTS[presses] = (u16)(pressing ? 0 : cost_of(school, index));
    }
}

RECOMP_HOOK("spell_check_castable") void eo_spells_before_check(u32 spell) {
    set_mp_cost((s32)(spell & 0xFFFF));
}

RECOMP_HOOK("spell_cast_init") void eo_spells_before_cast(void) {
    if (BRIAN_SPELL_COUNT >= 1 && BRIAN_SPELL_COUNT <= 3) {
        set_mp_cost(spell_find_by_inputs(BRIAN_SPELL_COUNT, BRIAN_SPELL_PRESSES));
    }
}

// ---- Boot: rewrite the 60 records ----

static void copy_bytes(u8* dst, const u8* src, u32 n) {
    for (u32 i = 0; i < n; i++) {
        dst[i] = src[i];
    }
}

// Runs at boot after enemy_spells.c (which keeps vanilla copies) and spirits.c (which reads vanilla levels).
void eo_spells_init(void) {
    u8* vanilla_stats = SPELL_STAT_TABLES[ENEMY_SCHOOL] + VANILLA_COPY_BASE * SPELL_STAT_SIZE;
    u8* vanilla_looks = SPELL_VISUAL_TABLES[ENEMY_SCHOOL] + VANILLA_COPY_BASE * SPELL_VISUAL_SIZE;
    for (int s = 0; s < SCHOOLS; s++) {
        for (int i = 0; i < SPELLS; i++) {
            const SpellDef* d = &spells[s][i];
            u8* rec = SPELL_STAT_TABLES[s] + i * SPELL_STAT_SIZE;
            const u8* from = vanilla_stats + d->rec * SPELL_STAT_SIZE;
            *(u16*)(rec + REC_RULE) = *(const u16*)(from + REC_RULE);
            copy_bytes(rec + REC_FIRST_COPIED, from + REC_FIRST_COPIED, SPELL_STAT_SIZE - REC_FIRST_COPIED);
            copy_bytes(SPELL_VISUAL_TABLES[s] + i * SPELL_VISUAL_SIZE, vanilla_looks + d->look * SPELL_VISUAL_SIZE,
                       SPELL_VISUAL_SIZE);
            if (d->type == 0xFF) {
                continue;   // the borrowed spell as it is (Escape, Homeward, Swiftstep, Mirage, Clear Sight)
            }
            *(u16*)(rec + REC_TYPE) = d->type;
            *(u16*)(rec + REC_BASE) = d->base;
            *(u16*)(rec + REC_SHOTS) = d->shots;
            if (d->kind != K_VANILLA) {
                for (int k = 0; k < STATUS_BYTES; k++) {
                    rec[REC_STATUS + k] = 0;
                }
            }
            if (d->kind == K_MEND) {
                // Mend is for battle only: free in the field it would make every rest and inn pointless.
                *(u16*)(rec + REC_RULE) = (u16)(*(u16*)(rec + REC_RULE) & ~RULE_OUTSIDE_BATTLE);
            }
        }
    }
}

// ---- Boost, per spell (each school's default, and the spells that boost like another school) ----

// Percent more damage and percent more shots per charge (combat.c applies them to the record for the cast).
// Every damaging spell hits harder right away for its charge (playtest, 2026-10-23: Fire's Burn stacks and Water's
// Freeze alone didn't feel like anything); Earth, the heavy hitter, most. Many-shot spells get shots instead.
#define BOOST_DAMAGE_PERCENT 30
#define BOOST_EARTH_PERCENT 50
void eo_spells_boost(int school, int index, int* damage_percent, int* shots_percent) {
    *damage_percent = 0;
    *shots_percent = 0;
    const SpellDef* d = def_of(school, index);
    if (d == 0 || d->type != TYPE_DAMAGE) {
        return;
    }
    *damage_percent = BOOST_DAMAGE_PERCENT;
    if (school == EARTH) {
        if (index == 4) {
            *damage_percent = 0;
            *shots_percent = 40;      // Sandblast, the clash, boosts like Wind
        } else {
            *damage_percent = BOOST_EARTH_PERCENT;
        }
    } else if (school == WIND) {
        if (d->kind == K_DOWNBURST) {
            *damage_percent = BOOST_EARTH_PERCENT;   // the clash boosts like Earth
        } else {
            *damage_percent = 0;
            *shots_percent = 40;
        }
    } else if (school == WATER) {
        if (d->kind == K_HAIL || index == 7) {
            *damage_percent = 0;
            *shots_percent = 40;      // Hailstorm and Ice Shards, the Wind-pressed ones
        }
    } else if (school == FIRE && index == 1) {
        *damage_percent = 0;
        *shots_percent = 50;          // Twin Flame: one more fireball a charge
    }
}

// ---- Battle state ----

static int cast_school = -1;        // Brian's spell this turn
static int cast_index = -1;
static int cast_boost = 0;          // charges spent on it (a finisher: charges past 2)

static u8 frost_turns[MAX_ENEMIES];
static u8 confused[MAX_ENEMIES];
static u8 guard_turns, wall_turns, fog_turns, haze_turns, renew_turns, storm_turns;
static u8 guard_percent;
static u16 renew_heal;
static u8 juggernaut;               // the next swing: 0 none, else percent more / 100
static u8 meteor_armed;
static f32 meteor_x, meteor_z;
static u16 meteor_damage;
static u8 meteor_stacks;
static u16 rock_damage;
static u8 scalded;                  // Searing Steam scalded Brian this cast
static u8 phoenix_used;             // Phoenix: once a battle
static u8 cast_marks;               // once-a-cast arena effects already placed (MARK_*)
static u8 quarry_armed;             // Quarry: the pillar Brian stood by flies with this Earth spell
static u8 stones_armed;             // Mountain's Patience: stones hurled with this Earth spell
static u8 momentum;                 // Momentum: Earth hits in a row without moving
static f32 momentum_x, momentum_z;
static u8 was_frozen[MAX_ENEMIES];  // Glacier Heart: frozen last frame
static u8 burst_done[MAX_ENEMIES];
#define MARK_ZONE 0x01
#define MARK_PILLAR 0x02
#define MARK_QUARRY 0x04
#define MARK_STONES 0x08
#define OVERFLOW_PERCENT 25         // Overflow: Water healing can go this far past max HP

#define FOG_MISS_PERCENT 40
#define MEND_MP_PERCENT 25
#define MEND_HP_PERCENT 22          // Water heals got bigger with Overflow (take 3)
#define SOOTHE_HP_PERCENT 25
#define RENEW_HP_PERCENT 16
#define MEND_MIN_MP 3
#define FROST_TURNS 2
#define FREEZE_TURNS 2              // a monster skips 2 turns; bosses 1 (combat.c)
#define FROST_ON_FROST_PERCENT 50   // chilling a frosted foe Freezes it this often (Flash Freeze: always)
#define CONFUSED_HIT_PERCENT 12     // of the ally's max HP

static int in_battle(void) {
    return (gBattleState & BATTLE_ACTIVE) != 0;
}

static void reset_battle(void) {
    cast_school = cast_index = -1;
    cast_boost = 0;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        frost_turns[i] = 0;
        confused[i] = 0;
    }
    guard_turns = wall_turns = fog_turns = haze_turns = renew_turns = storm_turns = 0;
    juggernaut = 0;
    meteor_armed = 0;
    phoenix_used = 0;
    momentum = 0;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        was_frozen[i] = burst_done[i] = 0;
    }
}

static void heal_brian(u32 amount) {
    if (BRIAN_HP >= BRIAN_MAX_HP) {
        return;   // never trims Overflow
    }
    u32 hp = BRIAN_HP + amount;
    BRIAN_HP = (u16)(hp > BRIAN_MAX_HP ? BRIAN_MAX_HP : hp);
}

// Overflow (Water talent): Water healing goes past max HP, up to OVERFLOW_PERCENT more, shown as a blue end on the
// HP bar (eo_spells_overflow). The extra soaks hits first and is gone after the battle. Without the talent, healing
// stops at max HP.
static u32 overflow_cap(void) {
    if (!eo_talent(TALENT_DEEP_TIDE)) {
        return BRIAN_MAX_HP;
    }
    return (u32)BRIAN_MAX_HP + (u32)BRIAN_MAX_HP * OVERFLOW_PERCENT / 100;
}

static void heal_water(u32 amount) {
    u32 hp = BRIAN_HP + amount;
    u32 cap = in_battle() ? overflow_cap() : BRIAN_MAX_HP;
    if (hp > cap) {
        hp = cap > BRIAN_HP ? cap : BRIAN_HP;
    }
    BRIAN_HP = (u16)hp;
}

void eo_spells_heal(int percent) {
    heal_brian((u32)BRIAN_MAX_HP * (u32)percent / 100 + 1);
}

RECOMP_HOOK("battle_build_turn_order") void eo_spells_on_battle_start(void) {
    reset_battle();
}

static int alive(int i) {
    return i >= 0 && i < MAX_ENEMIES && ENEMY_HP(i) > 0;
}

static f32* pos(int i) {
    return (f32*)ENEMY_BATTLE_DATA(i);
}

static f32 dist2(f32* a, f32* b) {
    f32 dx = a[0] - b[0];
    f32 dz = a[2] - b[2];
    return dx * dx + dz * dz;
}

// A hit that ignores element and defense (status.c's stand-in attacker), as the game's own damage.
static void fixed_hit(int i, u32 amount) {
    if (alive(i) && amount > 0) {
        eo_status_fixed_damage(i, (u16)(amount > 0xFFFF ? 0xFFFF : amount));
    }
}

// Roughly what a spell of `base` deals now (power through the overhaul's curve, a typical defense taken off).
static u32 estimate(int school, u32 base) {
    return base * eo_progression_spell_power_permille(school) / 1000 * 8 / 10;
}

u32 eo_spells_estimate(int school, u32 base) {
    return estimate(school, base);
}

static void glacier_burst(int t);
static void shape_cast(int school, int index);
static u32 burn_amount(u32 hit);
#define BRIAN_POS ((f32*)BRIAN_BATTLE_DATA)

// ---- Frost and Freeze ----
//
// Frost: the game's Movement status at "slow" (move range x0.5, put back to 1.0 by the game when it runs out),
// for FROST_TURNS of the foe's turns; Fire thaws it. A frosted foe hit one step colder Freezes (combat.c's
// freeze window); a frozen one hit colder stays frozen a turn longer.

static int frosted(int i) {
    u8* st = STATS(ENEMY_BATTLE_DATA(i));
    return st != 0 && (*(u16*)st & STATUS_MOVEMENT) && frost_turns[i] > 0;
}

static void unfrost(int i) {
    u8* st = STATS(ENEMY_BATTLE_DATA(i));
    if (st != 0 && frost_turns[i] > 0) {
        *(u16*)st &= (u16)~STATUS_MOVEMENT;
        st[MOVEMENT_COUNTDOWN] = 0;
        *(f32*)(st + STATS_RANGE) = 1.0f;
    }
    frost_turns[i] = 0;
}

static void frost(int i, int turns) {
    u8* st = STATS(ENEMY_BATTLE_DATA(i));
    if (st == 0 || !alive(i)) {
        return;
    }
    u16 bits = *(u16*)st;
    if ((bits & ~STATUS_MOVEMENT & ~0x008) != 0 && frost_turns[i] == 0) {
        return;   // one status at a time (status.c): another of the game's statuses holds it
    }
    *(u16*)st = (u16)(bits | STATUS_MOVEMENT);
    st[MOVEMENT_COUNTDOWN] = (u8)(turns + 1);   // the game counts it down too; ours clears it first
    *(f32*)(st + STATS_RANGE) = 0.5f;
    if (turns > frost_turns[i]) {
        frost_turns[i] = (u8)turns;
    }
}

// Water puts Burn out; each step goes Frost, then Freeze, then a longer Freeze.
static void chill(int i, int steps) {
    if (!alive(i) || eo_combat_broken(i)) {
        return;
    }
    eo_status_burn_clear(i);
    for (int k = 0; k < steps; k++) {
        if (eo_combat_frozen(i)) {
            eo_combat_freeze_longer(i);
        } else if (frosted(i) && (eo_talent(TALENT_FLASH_FREEZE) || get_rand(100) < FROST_ON_FROST_PERCENT)) {
            unfrost(i);
            eo_combat_freeze(i, FREEZE_TURNS);
        } else {
            frost(i, FROST_TURNS);
        }
    }
}

void eo_spells_chill(int slot, int steps) {
    chill(slot, steps);
}

// Fire thaws: Frost and Freeze go before a Burn lands.
static void burn(int i, u32 amount) {
    if (!alive(i)) {
        return;
    }
    unfrost(i);
    eo_combat_thaw(i);
    eo_status_add_burn(i, (u16)(amount < 1 ? 1 : amount > 0xFFFF ? 0xFFFF : amount));
}

void eo_spells_burn(int slot, u32 amount) {
    burn(slot, amount);
}

// Tide Staff: a swing Frosts.
void eo_spells_frost_foe(int slot) {
    if (alive(slot) && !eo_combat_broken(slot) && !eo_combat_frozen(slot)) {
        eo_status_burn_clear(slot);
        frost(slot, FROST_TURNS);
    }
}

int eo_spells_frosted(int slot) {
    return slot >= 0 && slot < MAX_ENEMIES && frost_turns[slot] > 0;
}

int eo_spells_confused(int slot) {
    return slot >= 0 && slot < MAX_ENEMIES && confused[slot] > 0;
}

// ---- Shaping a cast: Heavy Hands and Momentum make Earth spells bigger; Sun Heart and Lock-On make spells home ----
//
// The cast's record and visuals are changed as it launches and put back when Brian's turn ends.

#define REC_SIZE 0x1C            // f32 hitbox size
#define REC_HOMING 0x28          // u16: 0 none, 1 slight, 3 good
#define REC_SPEED 0x2E           // s16 desired, s16 initial
#define LOOK_STAGES 4
#define LOOK_STAGE_SIZE 0x10
#define LOOK_SIZE_X 0x08         // f32 x, then y

static u8* shaped_rec = 0;
static u8* shaped_look = 0;
static f32 saved_size;
static u16 saved_homing;
static s16 saved_speed[2];
static f32 saved_look[LOOK_STAGES][2];

static void unshape_cast(void) {
    if (shaped_rec != 0) {
        *(f32*)(shaped_rec + REC_SIZE) = saved_size;
        *(u16*)(shaped_rec + REC_HOMING) = saved_homing;
        *(s16*)(shaped_rec + REC_SPEED) = saved_speed[0];
        *(s16*)(shaped_rec + REC_SPEED + 2) = saved_speed[1];
        for (int s = 0; s < LOOK_STAGES; s++) {
            *(f32*)(shaped_look + s * LOOK_STAGE_SIZE + LOOK_SIZE_X) = saved_look[s][0];
            *(f32*)(shaped_look + s * LOOK_STAGE_SIZE + LOOK_SIZE_X + 4) = saved_look[s][1];
        }
        shaped_rec = 0;
    }
}

static void shape_cast(int school, int index) {
    unshape_cast();
    f32 grow = 1.0f;
    if (school == EARTH) {
        // Momentum counts Earth attacks in a row from the same spot.
        f32 dx = BRIAN_POS[0] - momentum_x;
        f32 dz = BRIAN_POS[2] - momentum_z;
        int rank = eo_talent(TALENT_MOMENTUM);
        momentum = (u8)(rank > 0 && dx * dx + dz * dz < 9.0f ? (momentum + 1 > rank ? rank : momentum + 1) : 0);
        momentum_x = BRIAN_POS[0];
        momentum_z = BRIAN_POS[2];
        grow += 0.25f * (f32)eo_talent(TALENT_HEAVY_HANDS) + 0.15f * (f32)momentum;
    }
    int home = (school == FIRE && eo_talent(TALENT_SUN_HEART)) || (eo_talent(TALENT_LOCK_ON) && eo_lockon_target() >= 0);
    if (grow == 1.0f && !home) {
        return;
    }
    u8* rec = SPELL_STAT_TABLES[school] + index * SPELL_STAT_SIZE;
    u8* look = SPELL_VISUAL_TABLES[school] + index * SPELL_VISUAL_SIZE;
    shaped_rec = rec;
    shaped_look = look;
    saved_size = *(f32*)(rec + REC_SIZE);
    saved_homing = *(u16*)(rec + REC_HOMING);
    saved_speed[0] = *(s16*)(rec + REC_SPEED);
    saved_speed[1] = *(s16*)(rec + REC_SPEED + 2);
    for (int s = 0; s < LOOK_STAGES; s++) {
        saved_look[s][0] = *(f32*)(look + s * LOOK_STAGE_SIZE + LOOK_SIZE_X);
        saved_look[s][1] = *(f32*)(look + s * LOOK_STAGE_SIZE + LOOK_SIZE_X + 4);
    }
    if (grow != 1.0f) {
        *(f32*)(rec + REC_SIZE) = saved_size * grow;
        for (int s = 0; s < LOOK_STAGES; s++) {
            *(f32*)(look + s * LOOK_STAGE_SIZE + LOOK_SIZE_X) = saved_look[s][0] * grow;
            *(f32*)(look + s * LOOK_STAGE_SIZE + LOOK_SIZE_X + 4) = saved_look[s][1] * grow;
        }
    }
    if (home) {
        *(u16*)(rec + REC_HOMING) = 3;
        *(s16*)(rec + REC_SPEED) = (s16)(saved_speed[0] * 3 / 2 + 1);
        *(s16*)(rec + REC_SPEED + 2) = (s16)(saved_speed[1] * 3 / 2 + 1);
    }
}

// ---- Casting ----

// Brian casts (combat.c, once per cast, as the first shot launches); `boost` is the charges spent on it.
void eo_spells_on_cast(int school, int index, int boost) {
    cast_school = school;
    cast_index = index;
    cast_boost = boost;
    scalded = 0;
    cast_marks = 0;
    quarry_armed = 0;
    stones_armed = 0;
    const SpellDef* d = def_of(school, index);
    if (d == 0) {
        return;
    }
    if (d->type == TYPE_DAMAGE) {
        shape_cast(school, index);
    }
    if (school == EARTH && d->type == TYPE_DAMAGE) {
        quarry_armed = (u8)(eo_talent(TALENT_QUARRY) && eo_arena_take_pillar_near_brian());
        stones_armed = (u8)eo_arena_take_stones();
    }
    switch (d->kind) {
        case K_HEAT_HAZE:
            haze_turns = (u8)(3 + boost);
            break;
        case K_STONESKIN:
            guard_turns = (u8)(3 + boost);
            guard_percent = 30;
            break;
        case K_JUGGERNAUT:
            juggernaut = (u8)(200 + 50 * boost > 255 ? 255 : 200 + 50 * boost);   // 3x, half again more a charge
            break;
        case K_ROCKSTORM:
            storm_turns = (u8)(3 + boost);
            rock_damage = (u16)estimate(EARTH, 495);
            break;
        case K_ICE_WALL:
            wall_turns = (u8)(1 + boost);
            break;
        case K_WHITEOUT:
            fog_turns = (u8)(2 + boost);
            break;
        case K_MEND: {
            // Free: 18% of max HP at Water 12 or more, less below (progression.c); and 25% of max
            // MP at any Water level, never under MEND_MIN_MP, so one Mend always pays for a tier-1 spell (playtest,
            // 2026-10-23: "I don't want to have to cast it so many times to cast a spell").
            // Deep Well: 2% more of each a rank; Wellspring: 5% more MP a rank.
            u32 scale = eo_progression_mend_permille();
            u32 well = 2 * (u32)eo_talent(TALENT_DEEP_WELL);
            u32 hp = (u32)BRIAN_MAX_HP * (MEND_HP_PERCENT + well) * scale / 100000 * (100 + 10 * (u32)boost) / 100;
            u32 mp = (u32)BRIAN_MAX_MP * (MEND_MP_PERCENT + well + 5 * (u32)eo_talent(TALENT_WELLSPRING)) / 100;
            heal_water(hp < 1 ? 1 : hp);
            u32 nmp = BRIAN_MP + (mp < MEND_MIN_MP ? MEND_MIN_MP : mp);
            BRIAN_MP = (u16)(nmp > BRIAN_MAX_MP ? BRIAN_MAX_MP : nmp);
            break;
        }
        case K_SOOTHE: {
            heal_water((u32)BRIAN_MAX_HP * (SOOTHE_HP_PERCENT + (u32)boost * 2) / 100);
            eo_status_cure_poison();
            for (int k = 0; k < 12; k++) {
                if ((BRIAN_STATUS_BITS & (1 << k)) && BRIAN_STATUS_COUNTDOWNS[k] > 1) {
                    BRIAN_STATUS_COUNTDOWNS[k] = 1;   // runs out at the next tick, undone the game's way
                }
            }
            break;
        }
        case K_RENEW:
            renew_turns = 3;
            renew_heal = (u16)((u32)BRIAN_MAX_HP * RENEW_HP_PERCENT * (100 + 10 * (u32)boost) / 10000);
            break;
        case K_FROST_NOVA: {
            f32 reach = NEAR_RADIUS + BOOST_RADIUS * boost;
            u32 amount = estimate(WATER, 90);
            for (int i = 0; i < MAX_ENEMIES; i++) {
                if (alive(i) && dist2(pos(i), (f32*)BRIAN_BATTLE_DATA) <= reach * reach) {
                    fixed_hit(i, amount);
                    chill(i, 1);
                    eo_fx_shield_hit(i, 0);
                }
            }
            break;
        }
        case K_CONFUSION:
            for (int i = 0; i < MAX_ENEMIES; i++) {
                if (alive(i) && !eo_combat_broken(i) && !eo_combat_frozen(i)) {
                    confused[i] = (u8)(1 + (boost > 2 ? 2 : boost));
                    eo_combat_skip_turn(i, confused[i]);
                }
            }
            break;
    }
}

// Spells that act on their target without damage (combat.c can't see them): the game's "no damage" handler.
#define PROJECTILE_CASTER 0x24
RECOMP_HOOK("spell_type_status_only") void eo_spells_on_status_spell(u8* projectile, u8* target) {
    if (!in_battle() || *(u8**)(projectile + PROJECTILE_CASTER) != BRIAN_BATTLE_DATA) {
        return;
    }
    int slot = -1;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (target == ENEMY_BATTLE_DATA(i)) {
            slot = i;
        }
    }
    const SpellDef* d = def_of(cast_school, cast_index);
    if (slot < 0 || d == 0 || !alive(slot)) {
        return;
    }
    switch (d->kind) {
        case K_GUST: {
            // Blown straight away from Brian.
            f32* p = pos(slot);
            f32* b = (f32*)BRIAN_BATTLE_DATA;
            f32 dx = p[0] - b[0];
            f32 dz = p[2] - b[2];
            f32 len = __builtin_sqrtf(dx * dx + dz * dz);
            if (len > 0.1f) {
                f32 push = 80.0f + 30.0f * cast_boost;
                p[0] += dx / len * push;
                p[2] += dz / len * push;
            }
            eo_fx_shield_hit(slot, 0);
            break;
        }
        case K_ABSOLUTE_ZERO:
            unfrost(slot);
            eo_status_burn_clear(slot);
            eo_combat_freeze(slot, FREEZE_TURNS + cast_boost);
            break;
        case K_CLEAR_SIGHT:
            eo_combat_reveal(slot);
            break;
    }
}

// ---- Hits ----

// More damage for this hit, in percent (combat.c, while it scales the hit).
int eo_spells_hit_percent(int slot, int frozen, int burning) {
    const SpellDef* d = def_of(cast_school, cast_index);
    if (d == 0) {
        return 0;
    }
    if (d->kind == K_SHATTER && frozen) {
        // 315 base on a frozen foe lands as 900 x 1.8 (the engine's Shatter), 0.2x more a talent rank; elsewhere
        // it's a weak hit.
        int tenths = 18 + 2 * eo_talent(TALENT_GLACIAL_SHATTER);
        return 900 * tenths * 10 / 315 - 100;
    }
    int more = 0;
    if (d->kind == K_FAN && burning) {
        more += 60;
    }
    // Kiln rank 2: a Fire spell that flew through ground fire hits a quarter harder.
    if (cast_school == FIRE && eo_talent(TALENT_KILN) >= 2 && slot >= 0 && slot < MAX_ENEMIES &&
        eo_arena_fire_between(BRIAN_POS[0], BRIAN_POS[2], pos(slot)[0], pos(slot)[2])) {
        more += 25;
    }
    // Momentum: each Earth attack in a row without moving, 10% more.
    if (cast_school == EARTH) {
        more += 10 * momentum;
    }
    return more;
}

// Extra shields a weakness hit cracks (Boulder).
int eo_spells_weak_extra_cracks(void) {
    const SpellDef* d = def_of(cast_school, cast_index);
    return d != 0 && d->kind == K_HEAVY ? d->p : 0;
}

// Juggernaut: the next staff swing deals 3x and cracks 2 shields.
int eo_spells_staff_percent(int slot) {
    if (juggernaut == 0) {
        return 0;
    }
    int percent = juggernaut;
    juggernaut = 0;
    eo_combat_crack(slot, 2);
    return percent;
}

// Hit effects wait for the next frame, outside the game's damage routine.
typedef struct {
    u8 slot;
    u8 weak;
    u8 frozen;
    u8 school;
    u8 index;
    u8 boost;
    u16 amount;
} Hit;
#define HIT_QUEUE 32
static Hit queue[HIT_QUEUE];
static int queued = 0;

void eo_spells_on_hit(int slot, u32 amount, int weak, int frozen) {
    if (queued >= HIT_QUEUE || cast_school < 0) {
        return;
    }
    Hit* h = &queue[queued++];
    h->slot = (u8)slot;
    h->weak = (u8)weak;
    h->frozen = (u8)frozen;
    h->school = (u8)cast_school;
    h->index = (u8)cast_index;
    h->boost = (u8)cast_boost;
    h->amount = (u16)(amount > 0xFFFF ? 0xFFFF : amount);
}

static u32 burn_amount(u32 hit) {
    return hit * (u32)eo_status_burn_percent() / 100;
}

u32 eo_spells_ember_stack(void) {
    return burn_amount(estimate(FIRE, 200)) + 1;
}

// Glacier Heart: a frozen foe that shatters or dies bursts into shards that freeze everything near it.
static void glacier_burst(int t) {
    if (!eo_talent(TALENT_GLACIER_HEART) || burst_done[t]) {
        return;
    }
    burst_done[t] = 1;
    eo_fx_shield_hit(t, 1);
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (i != t && alive(i) && dist2(pos(i), pos(t)) <= NEAR_RADIUS * NEAR_RADIUS) {
            chill(i, 2);
        }
    }
}

// What a spell does to one foe it hit (the target, or a foe the hit splashed onto).
static void effect(const SpellDef* d, const Hit* h, int i, u32 amount, int main_target) {
    int b = h->boost;
    switch (d->kind) {
        case K_KINDLE: {
            // Ember and Blaze boost with stacks; Twin Flame with fireballs, Burst and Blast with reach.
            int stacks = d->p + (h->index == 0 || h->index == 5 ? b : 0) + (eo_equipment_worn(EQ_EMBER_HEART) ? 1 : 0);
            for (int k = 0; k < stacks; k++) {
                burn(i, burn_amount(amount));
            }
            break;
        }
        case K_BELLOWS:
            eo_status_burn_heat(i, 3, 50 + 25 * b);
            break;
        case K_SCALD:
            if (eo_status_burn_total(i) > 0) {
                for (int k = 0; k <= b && main_target; k++) {
                    eo_status_burn_tick_now(i);
                }
            } else {
                burn(i, burn_amount(amount));
            }
            unfrost(i);
            eo_combat_thaw(i);
            break;
        case K_FLARE_UP:
            eo_status_burn_grow(i, 50 + 25 * b);
            break;
        case K_PYRE:
            eo_status_burn_cash(i, 100 + 20 * (u32)b);
            break;
        case K_FIRESTORM:
            eo_status_burn_heat(i, 3 + b, 50);
            break;
        case K_DETONATE:
            eo_status_burn_detonate(i, NEAR_RADIUS + BOOST_RADIUS * b);
            break;
        case K_BANKED:
            eo_status_burn_bank(i, 2, 100 + 50 * b);
            break;
        case K_METEOR:
            // Marks the spot; the meteor lands as Brian's next turn begins.
            meteor_armed = 1;
            meteor_x = pos(i)[0];
            meteor_z = pos(i)[2];
            meteor_damage = (u16)(amount * 8 > 0xFFFF ? 0xFFFF : amount * 8);
            meteor_stacks = (u8)(2 + b);
            break;
        case K_SEARING: {
            u32 total = eo_status_burn_total(i) * (300 + 50 * (u32)b) / 100;
            eo_status_burn_clear(i);
            fixed_hit(i, total);
            for (int k = 0; k < MAX_ENEMIES; k++) {
                unfrost(k);
                eo_combat_thaw(k);
            }
            if (!scalded) {
                scalded = 1;   // the steam rings Brian; it scalds him once a cast
                u32 scald = (u32)BRIAN_MAX_HP * 5 / 100;
                BRIAN_HP = (u16)(BRIAN_HP > scald + 1 ? BRIAN_HP - scald : 1);
            }
            break;
        }
        case K_MOLTEN:
        case K_ERUPTION:
            burn(i, burn_amount(amount));
            break;
        case K_MUDSHOT:
            chill(i, 1);
            break;
        case K_QUAGMIRE:
            chill(i, 1);
            if (frost_turns[i] > 0 && frost_turns[i] < 2 + b) {
                frost_turns[i] = (u8)(2 + b);
            }
            break;
        case K_FISSURE:
            eo_combat_crack(i, b >= 3 ? 2 : 1);
            break;
        case K_LANDSLIDE: {
            f32* p = pos(i);
            f32* br = (f32*)BRIAN_BATTLE_DATA;
            f32 dx = p[0] - br[0];
            f32 dz = p[2] - br[2];
            f32 len = __builtin_sqrtf(dx * dx + dz * dz);
            if (len > 0.1f) {
                p[0] += dx / len * (30.0f + 10.0f * b);
                p[2] += dz / len * (30.0f + 10.0f * b);
            }
            break;
        }
        case K_SPOUT:
            if (b > 0) {
                chill(i, b);
            }
            break;
        case K_CHILL:
            chill(i, d->p + b);
            break;
        case K_HAIL:
        case K_WINDCHILL:
            // A frosted foe hit again may Freeze; otherwise the hit may Frost it.
            if (frosted(i) ? get_rand(100) < 25 : get_rand(100) < 40) {
                chill(i, 1);
            }
            break;
        case K_SHATTER:
            if (h->frozen && main_target) {
                eo_combat_thaw(i);
            }
            break;
        case K_UNDERTOW: {
            f32* p = pos(i);
            f32* br = (f32*)BRIAN_BATTLE_DATA;
            f32 keep = b >= 1 ? 0.3f : 0.5f;   // dragged most of the way to Brian
            p[0] = br[0] + (p[0] - br[0]) * keep;
            p[2] = br[2] + (p[2] - br[2]) * keep;
            chill(i, b >= 2 ? 2 : 1);
            break;
        }
        case K_FAN:
            if (alive(i) && eo_status_burn_total(i) == 0) {
                burn(i, burn_amount(amount));
            }
            break;
        case K_TEMPEST:
            if (!h->weak && get_rand(100) < d->p) {
                eo_combat_crack(i, 1);
            }
            break;
        case K_WHIRLWIND:
            break;
        case K_DOWNBURST:
            if (!h->weak) {
                eo_combat_crack(i, 2);
            } else {
                eo_combat_crack(i, 1);   // a weakness hit already cracked one
            }
            break;
    }
}

// The nearest live foes to `from` other than it, up to `n`, into `out` (Ricochet, Chain Shatter, bounces).
static int nearest_others(int from, int n, int* out) {
    int found = 0;
    u8 used[MAX_ENEMIES] = { 0 };
    used[from] = 1;
    while (found < n) {
        int best = -1;
        f32 bd = 1e9f;
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (!used[i] && alive(i) && dist2(pos(i), pos(from)) < bd) {
                bd = dist2(pos(i), pos(from));
                best = i;
            }
        }
        if (best < 0) {
            break;
        }
        used[best] = 1;
        out[found++] = best;
    }
    return found;
}

static int fireball(int index) {   // Fire spells that fly as fireballs (Firewall: they leave ground fire)
    return index == 0 || index == 1 || index == 5 || index == 6 || index == 8 || index == 9 || index == 11;
}

// What the talents add to a hit on its main target, once the spell's own effect is done.
static void talent_effects(const SpellDef* d, const Hit* h, int t) {
    f32* p = pos(t);
    u32 amount = h->amount;
    int targets[MAX_ENEMIES];
    if (h->school == FIRE) {
        // Kiln: a Fire spell that flew through ground fire (already there) adds a stack.
        if (eo_talent(TALENT_KILN) && alive(t) && !(cast_marks & MARK_ZONE) &&
            eo_arena_fire_between(BRIAN_POS[0], BRIAN_POS[2], p[0], p[2])) {
            burn(t, burn_amount(amount));
        }
        // Ground fire: Burst and Blast leave it; with Firewall, fireballs too.
        if (!(cast_marks & MARK_ZONE)) {
            if (h->index == 2 || h->index == 7) {
                eo_arena_ground_fire(p[0], p[1], p[2], h->index == 7, 1);
                cast_marks |= MARK_ZONE;
            } else if (eo_talent(TALENT_FIREWALL) && fireball(h->index)) {
                eo_arena_ground_fire(p[0], p[1], p[2], 0, 0);
                cast_marks |= MARK_ZONE;
            }
        }
        // Flashpoint: the hit bursts; every burning foe near it takes one tick at once.
        if (eo_talent(TALENT_FLASHPOINT)) {
            for (int i = 0; i < MAX_ENEMIES; i++) {
                if (alive(i) && eo_status_burn_total(i) > 0 && dist2(pos(i), p) <= NEAR_RADIUS * NEAR_RADIUS) {
                    eo_status_burn_tick_now(i);
                }
            }
        }
        // Rekindle: the Burn starts over, longer.
        if (eo_talent(TALENT_REKINDLE) && alive(t)) {
            eo_status_burn_rekindle(t, eo_talent(TALENT_REKINDLE));
        }
    } else if (h->school == EARTH && d->type == TYPE_DAMAGE) {
        // Monolith: Rock and Boulder leave a pillar where they land.
        if ((h->index == 0 || h->index == 1) && eo_talent(TALENT_MONOLITH) && !(cast_marks & MARK_PILLAR)) {
            f32* b = BRIAN_POS;
            f32 dx = b[0] - p[0];
            f32 dz = b[2] - p[2];
            f32 len = __builtin_sqrtf(dx * dx + dz * dz);
            f32 k = len > 0.1f ? -18.0f / len : 0.0f;   // just behind the target: Impact slams foes into it
            eo_arena_pillar(p[0] + dx * k, p[1], p[2] + dz * k);
            cast_marks |= MARK_PILLAR;
        }
        // Quarry and Mountain's Patience: the stones Brian hurls with it.
        if (quarry_armed && !(cast_marks & MARK_QUARRY)) {
            cast_marks |= MARK_QUARRY;
            fixed_hit(t, estimate(EARTH, 495));
            eo_fx_shield_hit(t, 1);
        }
        if (stones_armed > 0 && !(cast_marks & MARK_STONES)) {
            cast_marks |= MARK_STONES;
            for (int k = 0; k < stones_armed; k++) {
                fixed_hit(t, estimate(EARTH, 300));
            }
            eo_fx_shield_hit(t, 1);
        }
        // Gravity Well: foes near the impact are pulled in.
        int well = eo_talent(TALENT_GRAVITY_WELL);
        if (well > 0) {
            eo_arena_pull(p[0], p[2], 40.0f + 25.0f * well, 0.5f);
        }
        // Bouncing Stone: rolling stones bounce on to more foes.
        int bounce = eo_talent(TALENT_RICOCHET_BOULDER);
        if (bounce > 0 && (h->index == 3 || h->index == 11)) {
            int n = nearest_others(t, bounce, targets);
            for (int k = 0; k < n; k++) {
                fixed_hit(targets[k], amount * 7 / 10);
                eo_fx_shield_hit(targets[k], 0);
            }
        }
        // Impact: the hit knocks the foe back; slammed into something, it takes the hit again.
        int impact = eo_talent(TALENT_IMPACT);
        if (impact > 0 && alive(t)) {
            eo_arena_knock(t, BRIAN_POS[0], BRIAN_POS[2], 15.0f * impact, amount);
        }
    } else if (h->school == WATER && d->type == TYPE_DAMAGE) {
        if (!(cast_marks & MARK_ZONE)) {
            cast_marks |= MARK_ZONE;
            if (h->index == 6) {
                eo_arena_hailcloud(p[0], p[1], p[2]);   // Hailcloud
            } else {
                eo_arena_ice_floor(p[0], p[1], p[2]);   // Ice Floor
            }
        }
        // Chain Shatter: shards fly from the broken ice to the nearest foes.
        int chain = eo_talent(TALENT_CHAIN_SHATTER);
        if (d->kind == K_SHATTER && h->frozen && chain > 0) {
            int n = nearest_others(t, chain, targets);
            for (int k = 0; k < n; k++) {
                fixed_hit(targets[k], amount * 3 / 10);
                chill(targets[k], 1);
                eo_fx_shield_hit(targets[k], 0);
            }
        }
        if (d->kind == K_SHATTER && h->frozen) {
            glacier_burst(t);
        }
    } else if (h->school == WIND && d->type == TYPE_DAMAGE) {
        if (h->index == 10 && !(cast_marks & MARK_ZONE)) {
            cast_marks |= MARK_ZONE;
            eo_arena_vortex(p[0], p[1], p[2]);   // Vortex
        }
        // Ricochet: each hit bounces on to the nearest foes.
        int rico = eo_talent(TALENT_RICOCHET);
        if (rico > 0) {
            int n = nearest_others(t, rico, targets);
            for (int k = 0; k < n; k++) {
                fixed_hit(targets[k], amount * 7 / 10);
                eo_fx_shield_hit(targets[k], 0);
            }
        }
        // Crosswind: the wind carries flame and frost it passed through.
        int cross = eo_talent(TALENT_CROSSWIND);
        if (cross > 0 && alive(t)) {
            int fire = eo_arena_fire_between(BRIAN_POS[0], BRIAN_POS[2], p[0], p[2]);
            int ice = eo_arena_ice_between(BRIAN_POS[0], BRIAN_POS[2], p[0], p[2]);
            if (fire && (cross >= 2 || !ice)) {
                for (int k = 0; k < cross; k++) {
                    burn(t, eo_spells_ember_stack());
                }
            }
            if (ice && (cross >= 2 || !fire)) {
                chill(t, cross);
            }
        }
    }
}

static void run_hit(const Hit* h) {
    const SpellDef* d = def_of(h->school, h->index);
    int t = h->slot;
    if (d == 0 || t >= MAX_ENEMIES) {
        return;
    }
    // The splash: the same hit on every other foe in reach of the target.
    f32 reach = d->area == A_NEAR ? NEAR_RADIUS : d->area == A_WIDE ? WIDE_RADIUS : 0.0f;
    if (h->school == FIRE && (h->index == 2 || h->index == 7)) {
        reach += BOOST_RADIUS * h->boost;   // Burst and Blast: a wider explosion
    }
    if (d->kind == K_QUAGMIRE || d->kind == K_FISSURE) {
        reach += BOOST_RADIUS * h->boost;
    }
    f32 here[3] = { pos(t)[0], pos(t)[1], pos(t)[2] };
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (i == t || !alive(i) || d->area == A_ONE) {
            continue;
        }
        if (d->area != A_ALL && dist2(pos(i), here) > reach * reach) {
            continue;
        }
        fixed_hit(i, h->amount);
        effect(d, h, i, h->amount, 0);
    }
    if (alive(t) || d->kind == K_METEOR || d->kind == K_DETONATE) {
        effect(d, h, t, h->amount, 1);
    }
    talent_effects(d, h, t);
    // Whirlwind: everyone it hit drifts toward the target.
    if (d->kind == K_WHIRLWIND) {
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (i != t && alive(i)) {
                pos(i)[0] = here[0] + (pos(i)[0] - here[0]) * 0.8f;
                pos(i)[2] = here[2] + (pos(i)[2] - here[2]) * 0.8f;
            }
        }
    }
}

// ---- Brian taking hits: guards, the wall, the fog, Heat Haze ----

static int spell_on_brian = 0;
static u16 hp_before_hit;
static void* hit_attacker;

RECOMP_HOOK("spell_type_default") void eo_spells_before_spell_hit(u8* projectile, u8* target) {
    spell_on_brian = target == BRIAN_BATTLE_DATA;
}

RECOMP_HOOK_RETURN("spell_type_default") void eo_spells_after_spell_hit(void) {
    spell_on_brian = 0;
}

RECOMP_HOOK("player_take_damage") void eo_spells_before_brian_damage(u16 power, u16 flags, void* attacker) {
    hp_before_hit = BRIAN_HP;
    hit_attacker = attacker;
}

RECOMP_HOOK_RETURN("player_take_damage") void eo_spells_after_brian_damage(void) {
    if (!in_battle() || BRIAN_HP >= hp_before_hit) {
        return;
    }
    u32 lost = hp_before_hit - BRIAN_HP;
    // Boss hits land as hard as the balance engine fitted them (balance.c).
    u32 scale = eo_balance_enemy_hit_permille();
    if (scale != 1000 && hit_attacker != 0 && hit_attacker != BRIAN_BATTLE_DATA) {
        lost = (lost * scale + 500) / 1000;
        if (lost < 1) {
            lost = 1;
        }
        BRIAN_HP = (u16)(hp_before_hit > lost ? hp_before_hit - lost : 0);
    }
    u32 back = 0;
    int attacker = -1;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (hit_attacker == ENEMY_BATTLE_DATA(i)) {
            attacker = i;
        }
    }
    int evade = 3 * eo_talent(TALENT_EVASION) + eo_arena_miss_percent(hit_attacker);
    if (wall_turns > 0 && spell_on_brian) {
        back = lost;   // the wall stops the spell
        if (eo_talent(TALENT_MIRROR_ICE) && attacker >= 0) {
            fixed_hit(attacker, lost);   // Mirror Ice: and throws it back
        }
    } else if (spell_on_brian && eo_arena_blocks_shot(hit_attacker)) {
        back = lost;   // a pillar, a frozen statue or an orbiting stone took it
    } else if (fog_turns > 0 && get_rand(100) < FOG_MISS_PERCENT) {
        back = lost;   // lost in the fog
    } else if (get_rand(100) < evade) {
        back = lost;   // Evasion, smoke, blinding, Zephyr Step
    } else {
        u32 soak = guard_turns > 0 ? guard_percent : 0;
        if (spell_on_brian && eo_combat_settled()) {
            soak += 10 * (u32)eo_talent(TALENT_GRANITE_BODY);   // Granite Body
        }
        back = lost * (soak > 90 ? 90 : soak) / 100;
    }
    if (back > 0 && BRIAN_HP > 0) {
        u32 hp = BRIAN_HP + back;
        BRIAN_HP = (u16)(hp > hp_before_hit ? hp_before_hit : hp);
    }
    // Spirit Charge comes from being hurt (playtest, 2026-10-23): one a hit that got through.
    if (attacker >= 0 && lost > back) {
        eo_combat_gain_charge(1);
    }
    // Ember Skin: a foe that hit him up close catches fire.
    if (eo_talent(TALENT_EMBER_SKIN) && !spell_on_brian && attacker >= 0 && lost > back) {
        burn(attacker, eo_spells_ember_stack());
    }
    // Phoenix: once a battle, falling under a quarter of his HP heals 30%.
    if (eo_talent(TALENT_PHOENIX) && !phoenix_used && BRIAN_HP > 0 && (u32)BRIAN_HP * 4 < BRIAN_MAX_HP) {
        phoenix_used = 1;
        heal_brian((u32)BRIAN_MAX_HP * 30 / 100);
        // and bursts into flame: every foe near him catches 2 stacks.
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (alive(i) && dist2(pos(i), BRIAN_POS) <= NEAR_RADIUS * NEAR_RADIUS) {
                burn(i, eo_spells_ember_stack());
                burn(i, eo_spells_ember_stack());
            }
        }
    }
    // Heat Haze: whoever hit him catches a Burn stack.
    if (haze_turns > 0 && lost > back) {
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (hit_attacker == ENEMY_BATTLE_DATA(i)) {
                burn(i, burn_amount(estimate(FIRE, 200)));   // an Ember's stack
            }
        }
    }
}

// ---- The clock: Brian's turn begins ----

static void brian_turn_begins(void) {
    if (guard_turns > 0) guard_turns--;
    if (wall_turns > 0) wall_turns--;
    if (fog_turns > 0) fog_turns--;
    if (haze_turns > 0) haze_turns--;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (frost_turns[i] == 1) {
            unfrost(i);
        } else if (frost_turns[i] > 1) {
            frost_turns[i]--;
        }
        if (!alive(i)) {
            frost_turns[i] = 0;
            confused[i] = 0;
        }
    }
    // Overflow fades a quarter a turn (Deep Tide: it keeps).

    if (renew_turns > 0) {
        renew_turns--;
        heal_water(renew_heal);
    }
    if (eo_talent(TALENT_LIVING_WATER) && BRIAN_HP > 0) {
        heal_water((u32)BRIAN_MAX_HP * 3 / 100 + 1);
    }
    eo_arena_brian_turn();
    // Meteor lands: heavy damage and hot Burn on everything near the mark.
    if (meteor_armed) {
        meteor_armed = 0;
        f32 at[3] = { meteor_x, 0.0f, meteor_z };
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (alive(i) && dist2(pos(i), at) <= WIDE_RADIUS * WIDE_RADIUS) {
                eo_fx_flare(i);
                fixed_hit(i, meteor_damage);
                for (int k = 0; k < meteor_stacks; k++) {
                    burn(i, burn_amount(meteor_damage));
                }
                eo_status_burn_heat(i, 3, 50);
            }
        }
    }
    // Rockstorm: a boulder on a random foe.
    if (storm_turns > 0) {
        storm_turns--;
        int n = 0;
        int pick[MAX_ENEMIES];
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (alive(i)) {
                pick[n++] = i;
            }
        }
        if (n > 0) {
            int i = pick[get_rand(n)];
            eo_fx_shield_hit(i, 0);
            fixed_hit(i, rock_damage);
        }
    }
}

// A confused foe whose skipped turn has come and gone struck one of its allies (combat.c skipped its turn).
static void confused_turn_over(int i) {
    confused[i]--;
    int n = 0;
    int pick[MAX_ENEMIES];
    for (int k = 0; k < MAX_ENEMIES; k++) {
        if (k != i && alive(k)) {
            pick[n++] = k;
        }
    }
    if (n > 0 && !(gBattleState & BATTLE_BOSS)) {
        int k = pick[get_rand(n)];
        u8* rec = ENEMY_RECORD(k);
        u32 max_hp = rec != 0 ? *(u16*)(rec + RECORD_MAX_HP) : 50;
        fixed_hit(k, max_hp * CONFUSED_HIT_PERCENT / 100 + 1);
    }
}

// An enemy's turn ends (battle_end_turn gets the fighter whose turn it was): a confused one lashed out.
RECOMP_HOOK("battle_end_turn") void eo_spells_on_turn_end(u8* fighter) {
    if (!in_battle()) {
        return;
    }
    if (!(gBattleState & BATTLE_ENEMY_TURN)) {
        unshape_cast();
        return;
    }
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (fighter == ENEMY_BATTLE_DATA(i) && confused[i] > 0) {
            confused_turn_over(i);
        }
    }
}

RECOMP_HOOK_RETURN("frame_build") void eo_spells_frame(void) {
    static int was_enemy_turn = 0;
    static int was_in_battle = 0;
    if (!in_battle()) {
        queued = 0;
        was_enemy_turn = 0;
        if (was_in_battle) {
            was_in_battle = 0;
            unshape_cast();
            if (BRIAN_HP > BRIAN_MAX_HP) {
                BRIAN_HP = BRIAN_MAX_HP;   // Overflow is gone after the battle
            }
        }
        return;
    }
    was_in_battle = 1;
    // Glacier Heart: a frozen foe that dies bursts too.
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (was_frozen[i] && !alive(i)) {
            glacier_burst(i);
        }
        was_frozen[i] = (u8)(alive(i) && eo_combat_frozen(i));
    }
    for (int k = 0; k < queued; k++) {
        run_hit(&queue[k]);
    }
    queued = 0;
    int enemy_turn = (gBattleState & BATTLE_ENEMY_TURN) != 0;
    if (was_enemy_turn && !enemy_turn) {
        brian_turn_begins();
        cast_school = cast_index = -1;
        cast_boost = 0;
    }
    was_enemy_turn = enemy_turn;
}
