#include "eo.h"

// Talent trees (Spell Trees doc, "Talent trees, take 3", 2026-10-23): each school has five rows that open by its
// element level (1, 12, 30, 48, 60), each with two talents of up to 3 ranks and one single pick, and a capstone at
// 68. Talents change what happens in the arena (ground fire, smoke, pillars, knockback, ice, ricochets, extra
// actions) rather than adding percentages; each tree keeps one plain scaling talent. Deeper ranks cost more: 1
// point in rows 1 and 2, 2 in row 3, 3 in rows 4 and 5, 4 for a capstone. Only one capstone can be learned.
// Balance comes after play (playtest, 2026-10-23: "interesting first").
//
// Effects live where the rule they bend lives (arena.c, spells.c, combat.c, status.c); eo_talent() is how they
// ask. Ranks are stored 2 bits each in the save's talent bytes.

#define ELEMENT_LEVELS ((u8*)0x8007BAA4)   // Fire, Earth, Water, Wind
#define CAPSTONE_LEVEL 68
#define CAPSTONE_COST 4

// Effect lines are for the small HUD font (uppercase, digits and . : ' - ? only; at most 38 characters).
static const TalentInfo talents[TALENT_COUNT] = {
    [TALENT_KINDLING] = { "Kindling", 0, 1, 3, 1, "BURN TICKS DEAL 10 PCT MORE A RANK." },
    [TALENT_SMOKE] = { "Smoke", 0, 1, 3, 1, "SMOKE SPREADS WIDER. HIDES YOU IN IT." },
    [TALENT_FLASHPOINT] = { "Flashpoint", 0, 1, 1, 1, "FIRE HITS TICK BURNING FOES NEAR IT." },
    [TALENT_FIREWALL] = { "Firewall", 0, 12, 3, 1, "FIREBALLS LEAVE FIRE. IT LASTS LONGER." },
    [TALENT_REKINDLE] = { "Rekindle", 0, 12, 3, 1, "FIRE HITS RESTART BURN. 1 TURN MORE." },
    [TALENT_STOKED_MIND] = { "Stoked Mind", 0, 12, 1, 1, "BURN TICKS GIVE BACK TWICE THE MP." },
    [TALENT_WILDFIRE] = { "Wildfire", 0, 30, 1, 2, "A BURNING FOE THAT DIES SPREADS BURN." },
    [TALENT_KILN] = { "Kiln", 0, 30, 2, 2, "SPELLS PAST YOUR FIRE: 1 MORE STACK." },
    [TALENT_EMBER_SKIN] = { "Ember Skin", 0, 30, 1, 2, "FOES THAT HIT YOU UP CLOSE CATCH FIRE." },
    [TALENT_LIVING_FLAME] = { "Living Flame", 0, 48, 2, 3, "FOES ON 3 STACKS DROP FIRE AS THEY GO." },
    [TALENT_BONFIRE] = { "Bonfire", 0, 48, 2, 3, "START A TURN IN YOUR FIRE: 1 CHARGE." },
    [TALENT_PHOENIX] = { "Phoenix", 0, 48, 1, 3, "ONCE A FIGHT: HEAL 30 PCT UNDER 25." },
    [TALENT_OVERLOAD] = { "Overload", 0, 60, 2, 3, "A FOE HOLDS 1 MORE BURN STACK A RANK." },
    [TALENT_SMOLDERING_SHIELDS] = { "Smolder Shields", 0, 60, 3, 3, "BURN TICKS: 10 PCT A RANK TO CRACK." },
    [TALENT_ASH_CLOUD] = { "Ash Cloud", 0, 60, 1, 3, "BURN THAT ENDS BLINDS THE FOE A TURN." },
    [TALENT_SUN_HEART] = { "Sun Heart", 0, CAPSTONE_LEVEL, 1, CAPSTONE_COST, "YOU BURN FOES NEAR. FIRE NEVER ENDS." },
    [TALENT_BEDROCK] = { "Bedrock", 1, 1, 3, 1, "EARTH SPELLS DEAL 7 PCT MORE A RANK." },
    [TALENT_HEAVY_HANDS] = { "Heavy Hands", 1, 1, 3, 1, "EARTH SPELLS ARE BIGGER EACH RANK." },
    [TALENT_MONOLITH] = { "Monolith", 1, 1, 1, 1, "ROCKS LEAVE PILLARS THAT BLOCK SHOTS." },
    [TALENT_STEADY_FOOTING] = { "Steady Footing", 1, 12, 3, 1, "SETTLED HITS LOSE 7 PCT LESS A RANK." },
    [TALENT_MOMENTUM] = { "Momentum", 1, 12, 3, 1, "EARTH HITS WITHOUT MOVING GROW BIGGER." },
    [TALENT_BRACE] = { "Brace", 1, 12, 1, 1, "PASSING WHILE SETTLED HALVES HITS." },
    [TALENT_FAULT_LINE] = { "Fault Line", 1, 30, 3, 2, "EARTH WEAK HITS MAY CRACK 1 MORE." },
    [TALENT_IMPACT] = { "Impact", 1, 30, 3, 2, "KNOCKBACK. SLAMMED FOES TAKE IT AGAIN." },
    [TALENT_QUARRY] = { "Quarry", 1, 30, 1, 2, "CAST BY YOUR PILLAR TO HURL IT TOO." },
    [TALENT_TREMOR] = { "Tremor", 1, 48, 2, 3, "PASSING WHILE SETTLED SLOWS FOES NEAR." },
    [TALENT_GRAVITY_WELL] = { "Gravity Well", 1, 48, 2, 3, "EARTH SPELLS PULL FOES TO THE IMPACT." },
    [TALENT_PLANTED_FEET] = { "Planted Feet", 1, 48, 1, 3, "PASSING WHILE SETTLED: 1 MORE CHARGE." },
    [TALENT_RICOCHET_BOULDER] = { "Bouncing Stone", 1, 60, 2, 3, "ROLLING STONES BOUNCE TO MORE FOES." },
    [TALENT_GRANITE_BODY] = { "Granite Body", 1, 60, 3, 3, "SETTLED: SPELLS ON YOU CRUMBLE." },
    [TALENT_SEISMIC] = { "Seismic", 1, 60, 1, 3, "HEAVY EARTH HITS ALWAYS CRACK 1." },
    [TALENT_MOUNTAINS_PATIENCE] = { "Patient Mountain", 1, CAPSTONE_LEVEL, 1, CAPSTONE_COST, "PASSES STORE STONES. EARTH HURLS THEM." },
    [TALENT_DEEP_WELL] = { "Deep Well", 2, 1, 3, 1, "MEND: 2 PCT MORE HP AND MP A RANK." },
    [TALENT_COLD_SNAP] = { "Cold Snap", 2, 1, 3, 1, "WATER SPELLS DEAL 6 PCT MORE A RANK." },
    [TALENT_FLASH_FREEZE] = { "Flash Freeze", 2, 1, 1, 1, "CHILLING A FROSTED FOE ALWAYS FREEZES." },
    [TALENT_CHAIN_SHATTER] = { "Chain Shatter", 2, 12, 2, 1, "SHATTER THROWS SHARDS AT MORE FOES." },
    [TALENT_CLEAR_WATERS] = { "Clear Waters", 2, 12, 3, 1, "WATER SPELLS 9 PCT LESS MP A RANK." },
    [TALENT_WINTERS_GRIP] = { "Winter's Grip", 2, 12, 1, 1, "BOSSES RESIST FREEZE 1 TURN LESS." },
    [TALENT_GLACIAL_SHATTER] = { "Glacial Shatter", 2, 30, 3, 2, "SHATTER 0.2X MORE A RANK." },
    [TALENT_ICE_FLOOR] = { "Ice Floor", 2, 30, 3, 2, "WATER LEAVES ICE. FOES SLIDE ON IT." },
    [TALENT_MIRROR_ICE] = { "Mirror Ice", 2, 30, 1, 2, "ICE WALL THROWS SPELLS BACK." },
    [TALENT_HAILCLOUD] = { "Hailcloud", 2, 48, 2, 3, "HAILSTORM LINGERS AS A CLOUD." },
    [TALENT_PERMAFROST] = { "Permafrost", 2, 48, 3, 3, "AT RANK 3 FREEZE LASTS 1 MORE TURN." },
    [TALENT_LIVING_WATER] = { "Living Water", 2, 48, 1, 3, "HEAL 3 PCT OF MAX HP EACH TURN." },
    [TALENT_FROZEN_STATUE] = { "Frozen Statue", 2, 60, 3, 3, "FROZEN FOES BLOCK FOES AND SHOTS." },
    [TALENT_WELLSPRING] = { "Wellspring", 2, 60, 3, 3, "MEND RESTORES 5 PCT MORE MP A RANK." },
    [TALENT_DEEP_TIDE] = { "Overflow", 2, 60, 1, 3, "HEALS GO PAST MAX HP. SHOWN BLUE." },
    [TALENT_GLACIER_HEART] = { "Glacier Heart", 2, CAPSTONE_LEVEL, 1, CAPSTONE_COST, "SHATTERED FOES BURST AND FREEZE ALL." },
    [TALENT_GALE_FORCE] = { "Gale Force", 3, 1, 3, 1, "WIND SPELLS DEAL 6 PCT MORE A RANK." },
    [TALENT_RICOCHET] = { "Ricochet", 3, 1, 3, 1, "WIND HITS BOUNCE TO 1 MORE FOE A RANK." },
    [TALENT_HIT_AND_RUN] = { "Hit and Run", 3, 1, 1, 1, "MOVE AFTER CASTING. END WITH A." },
    [TALENT_FLEET] = { "Fleet", 3, 12, 3, 1, "A BIGGER MOVE CIRCLE EACH RANK." },
    [TALENT_EVASION] = { "Evasion", 3, 12, 3, 1, "FOES MISS YOU 3 PCT MORE A RANK." },
    [TALENT_TAILWIND] = { "Tailwind", 3, 12, 1, 1, "WIND SPELLS COST 25 PCT LESS MP." },
    [TALENT_CYCLONE] = { "Cyclone", 3, 30, 3, 2, "WIND HITS: 5 PCT A RANK TO CRACK ANY." },
    [TALENT_CROSSWIND] = { "Crosswind", 3, 30, 2, 2, "WIND CARRIES FIRE AND FROST IT PASSES." },
    [TALENT_LOCK_ON] = { "Lock-On", 3, 30, 1, 2, "HOLD Z TO LOCK ON. SPELLS HOME FAST." },
    [TALENT_VORTEX] = { "Vortex", 3, 48, 2, 3, "WHIRLWIND DRIFTS ON FOR MORE TURNS." },
    [TALENT_SQUALL] = { "Squall", 3, 48, 3, 3, "MULTI-HIT SPELLS: 8 PCT MORE A RANK." },
    [TALENT_SECOND_BREATH] = { "Second Breath", 3, 48, 1, 3, "START EACH BATTLE WITH 1 CHARGE." },
    [TALENT_JET_STREAM] = { "Jet Stream", 3, 60, 2, 3, "MOVE FARTHER BEFORE YOU CAST." },
    [TALENT_ZEPHYR_STEP] = { "Zephyr Step", 3, 60, 2, 3, "MOVE FAR: HARD TO HIT TILL NEXT TURN." },
    [TALENT_STORM_CALLER] = { "Storm Caller", 3, 60, 1, 3, "WIND FINISHERS NEED 1 LESS CHARGE." },
    [TALENT_SECOND_WIND] = { "Second Wind", 3, CAPSTONE_LEVEL, 1, CAPSTONE_COST, "YOU ACT TWICE EVERY TURN." },
};

const TalentInfo* eo_talent_info(int talent) {
    return talent >= 0 && talent < TALENT_COUNT ? &talents[talent] : 0;
}

int eo_talent(int talent) {
    if (talent < 0 || talent >= TALENT_COUNT) {
        return 0;
    }
    return (gEo.talent_ranks[talent / 4] >> ((talent % 4) * 2)) & 3;
}

static void set_rank(int talent, int rank) {
    u8* b = &gEo.talent_ranks[talent / 4];
    int shift = (talent % 4) * 2;
    *b = (u8)((*b & ~(3 << shift)) | (rank << shift));
}

static int is_capstone(int talent) {
    return talent % TALENTS_PER_SCHOOL == TALENTS_PER_SCHOOL - 1;
}

// Only one capstone, in any tree.
static int capstone_taken(void) {
    for (int s = 0; s < 4; s++) {
        if (eo_talent(s * TALENTS_PER_SCHOOL + TALENTS_PER_SCHOOL - 1)) {
            return 1;
        }
    }
    return 0;
}

int eo_talent_blocked(int talent) {
    return is_capstone(talent) && eo_talent(talent) == 0 && capstone_taken();
}

int eo_talent_learn(int talent) {
    const TalentInfo* t = eo_talent_info(talent);
    int rank = eo_talent(talent);
    if (t == 0 || rank >= t->ranks || ELEMENT_LEVELS[t->school] < t->level || gEo.spirit_points < t->cost ||
        eo_talent_blocked(talent)) {
        return 0;
    }
    gEo.spirit_points = (u8)(gEo.spirit_points - t->cost);
    set_rank(talent, rank + 1);
    return 1;
}

// A save from the nine-talent trees (one byte a rank, 2 points a rank, capstones 5): its points come back once and
// the old ranks go (spirits.c, as a save is first seen).
#define OLD_TALENTS 36
#define OLD_RANK_COST 2
#define OLD_CAPSTONE_COST 5

void eo_talents_refund_old(void) {
    u32 refund = 0;
    for (int i = 0; i < OLD_TALENTS; i++) {
        refund += (u32)gEo.talent_ranks[i] * (i % 9 == 8 ? OLD_CAPSTONE_COST : OLD_RANK_COST);
    }
    for (u32 i = 0; i < sizeof(gEo.talent_ranks); i++) {
        gEo.talent_ranks[i] = 0;
    }
    u32 points = gEo.spirit_points + refund;
    gEo.spirit_points = (u8)(points > 255 ? 255 : points);
}

// ---- Effects other files ask about ----

// Brian's spells of `element`: each school's one plain scaling talent (Fire's is Kindling, on Burn ticks).
int eo_talents_spell_percent(int element) {
    switch (element) {
        case 1: return 7 * eo_talent(TALENT_BEDROCK);
        case 2: return 6 * eo_talent(TALENT_COLD_SNAP);
        case 3: return 6 * eo_talent(TALENT_GALE_FORCE);
    }
    return 0;
}

// Percent off a spell's MP cost.
int eo_talents_mp_percent(int school) {
    switch (school) {
        case 2: return 9 * eo_talent(TALENT_CLEAR_WATERS);
        case 3: return eo_talent(TALENT_TAILWIND) ? 25 : 0;
    }
    return 0;
}

// ---- Fleet: how far Brian moves ----

#define BRIAN_MOVE_MULTIPLIER (*(f32*)0x8007BBC8)   // set by brian_battle_init; gear scales it (equipment.c)

RECOMP_HOOK_RETURN("player_battle_init") void eo_talents_on_battle_init(void) {
    BRIAN_MOVE_MULTIPLIER *= 1.0f + 0.15f * (f32)eo_talent(TALENT_FLEET);
}
