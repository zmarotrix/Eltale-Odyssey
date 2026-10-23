#include "eo.h"

// Talent trees (Spell Trees doc, "Talent trees", 2026-10-23; tools/balance/talents.py): each school has five rows
// that open by its element level (1, 12, 30, 48, 60), each with two talents of up to 3 ranks and one single pick,
// and a capstone at 68. Deeper ranks cost more: 1 point in rows 1 and 2, 2 in row 3, 3 in rows 4 and 5, 4 for a
// capstone, so a whole tree is 74 points: everything a player who finds 75% of the spirits earns. Points can go
// all into one tree or be spread over two; only one capstone can be learned.
//
// Effects live where the rule they bend lives (spells.c, combat.c, status.c); eo_talent() is how they ask. Ranks are
// stored 2 bits each in the save's talent bytes.

#define ELEMENT_LEVELS ((u8*)0x8007BAA4)   // Fire, Earth, Water, Wind
#define CAPSTONE_LEVEL 68
#define CAPSTONE_COST 4

// Effect lines are for the small HUD font (uppercase, digits and . : ' - ? only; at most 38 characters).
static const TalentInfo talents[TALENT_COUNT] = {
    // Fire: Burn
    [TALENT_KINDLING]           = { "Kindling",          0, 1,  3, 1, "BURN TICKS DEAL 10 PCT MORE A RANK." },
    [TALENT_EMBER_HEART]        = { "Ember Heart",       0, 1,  3, 1, "FIRE SPELLS DEAL 6 PCT MORE A RANK." },
    [TALENT_SEARING_STAFF]      = { "Searing Staff",     0, 1,  1, 1, "SWINGS ADD BURN. TAKE 6 PCT LESS." },
    [TALENT_FAN_THE_COALS]      = { "Fan the Coals",     0, 12, 3, 1, "STOKING HEATS 10 PCT MORE A RANK." },
    [TALENT_LINGERING_FLAME]    = { "Lingering Flame",   0, 12, 3, 1, "AT RANK 3 BURN LASTS 1 MORE TURN." },
    [TALENT_HOT_HANDS]          = { "Hot Hands",         0, 12, 1, 1, "FIRE SPELLS COST 25 PCT LESS MP." },
    [TALENT_WILDFIRE]           = { "Wildfire",          0, 30, 3, 2, "AT RANK 3 A FOE HOLDS 1 MORE STACK." },
    [TALENT_PYRE]               = { "Pyre",              0, 30, 3, 2, "SEARING STEAM 15 PCT MORE A RANK." },
    [TALENT_ASH_VEIL]           = { "Ash Veil",          0, 30, 1, 2, "TAKE 7 PCT LESS DAMAGE." },
    [TALENT_CONFLAGRATION]      = { "Conflagration",     0, 48, 3, 3, "BURNING FOES TAKE 5 PCT MORE A RANK." },
    [TALENT_BLAZE]              = { "Blaze",             0, 48, 3, 3, "FIRE SPELLS DEAL 5 PCT MORE A RANK." },
    [TALENT_PHOENIX]            = { "Phoenix",           0, 48, 1, 3, "ONCE A FIGHT: HEAL 30 PCT UNDER 25." },
    [TALENT_INFERNO]            = { "Inferno",           0, 60, 3, 3, "BURN TICKS DEAL 10 PCT MORE A RANK." },
    [TALENT_SMOLDERING_SHIELDS] = { "Smolder Shields",   0, 60, 3, 3, "BURN TICKS: 10 PCT A RANK TO CRACK." },
    [TALENT_CINDER_SKIN]        = { "Cinder Skin",       0, 60, 1, 3, "TAKE 7 PCT LESS DAMAGE." },
    [TALENT_ETERNAL_FLAME]      = { "Eternal Flame",     0, CAPSTONE_LEVEL, 1, CAPSTONE_COST, "BURN NEVER ENDS. A FOE HOLDS 5." },
    // Earth: weight, Settle and shields
    [TALENT_BEDROCK]            = { "Bedrock",           1, 1,  3, 1, "EARTH SPELLS DEAL 7 PCT MORE A RANK." },
    [TALENT_THICK_SKIN]         = { "Thick Skin",        1, 1,  3, 1, "TAKE 2 PCT LESS DAMAGE A RANK." },
    [TALENT_HEAVY_STAFF]        = { "Heavy Staff",       1, 1,  1, 1, "SWINGS 30 PCT MORE. TAKE 6 PCT LESS." },
    [TALENT_STEADY_FOOTING]     = { "Steady Footing",    1, 12, 3, 1, "SETTLED HITS LOSE 7 PCT LESS A RANK." },
    [TALENT_MOMENTUM]           = { "Momentum",          1, 12, 3, 1, "EARTH BOOST 5 PCT MORE A RANK." },
    [TALENT_BRACE]              = { "Brace",             1, 12, 1, 1, "PASSING WHILE SETTLED HALVES HITS." },
    [TALENT_FAULT_LINE]         = { "Fault Line",        1, 30, 3, 2, "EARTH WEAK HITS MAY CRACK 1 MORE." },
    [TALENT_STONESKIN_MASTERY]  = { "Stone Mastery",     1, 30, 3, 2, "GUARDS BLOCK 5 PCT MORE A RANK." },
    [TALENT_QUARRY]             = { "Quarry",            1, 30, 1, 2, "EARTH SPELLS COST 25 PCT LESS MP." },
    [TALENT_TREMOR]             = { "Tremor",            1, 48, 3, 3, "EARTH 10 PCT MORE ON BROKEN A RANK." },
    [TALENT_GRANITE_WILL]       = { "Granite Will",      1, 48, 3, 3, "TAKE 3 PCT LESS DAMAGE A RANK." },
    [TALENT_PLANTED_FEET]       = { "Planted Feet",      1, 48, 1, 3, "PASSING WHILE SETTLED: 1 MORE CHARGE." },
    [TALENT_COLOSSUS]           = { "Colossus",          1, 60, 3, 3, "EARTH SPELLS DEAL 7 PCT MORE A RANK." },
    [TALENT_UNBREAKABLE]        = { "Unbreakable",       1, 60, 3, 3, "HITS ON YOU SOAK 4 PCT A RANK." },
    [TALENT_SEISMIC]            = { "Seismic",           1, 60, 1, 3, "HEAVY EARTH HITS ALWAYS CRACK 1." },
    [TALENT_AVALANCHE]          = { "Avalanche",         1, CAPSTONE_LEVEL, 1, CAPSTONE_COST, "EVERY THIRD EARTH CAST HITS 2X." },
    // Water: Mend, Freeze and Shatter
    [TALENT_DEEP_WELL]          = { "Deep Well",         2, 1,  3, 1, "MEND: 2 PCT MORE HP AND MP A RANK." },
    [TALENT_COLD_SNAP]          = { "Cold Snap",         2, 1,  3, 1, "WATER SPELLS DEAL 6 PCT MORE A RANK." },
    [TALENT_TIDE_STAFF]         = { "Tide Staff",        2, 1,  1, 1, "SWINGS FROST. TAKE 6 PCT LESS." },
    [TALENT_FROSTBITE]          = { "Frostbite",         2, 12, 3, 1, "FROZEN FOES TAKE 8 PCT MORE A RANK." },
    [TALENT_CLEAR_WATERS]       = { "Clear Waters",      2, 12, 3, 1, "WATER SPELLS 9 PCT LESS MP A RANK." },
    [TALENT_WINTERS_GRIP]       = { "Winter's Grip",     2, 12, 1, 1, "BOSSES RESIST FREEZE 1 TURN LESS." },
    [TALENT_GLACIAL_SHATTER]    = { "Glacial Shatter",   2, 30, 3, 2, "SHATTER 0.2X MORE A RANK." },
    [TALENT_HYPOTHERMIA]        = { "Hypothermia",       2, 30, 3, 2, "CHILL: 10 PCT A RANK TO GO COLDER." },
    [TALENT_ICE_ARMOR]          = { "Ice Armor",         2, 30, 1, 2, "TAKE 7 PCT LESS DAMAGE." },
    [TALENT_RIME]               = { "Rime",              2, 48, 3, 3, "WATER SPELLS DEAL 5 PCT MORE A RANK." },
    [TALENT_PERMAFROST]         = { "Permafrost",        2, 48, 3, 3, "AT RANK 3 FREEZE LASTS 1 MORE TURN." },
    [TALENT_LIVING_WATER]       = { "Living Water",      2, 48, 1, 3, "HEAL 3 PCT OF MAX HP EACH TURN." },
    [TALENT_DEEP_FREEZE]        = { "Deep Freeze",       2, 60, 3, 3, "SHATTER 0.2X MORE A RANK." },
    [TALENT_WELLSPRING]         = { "Wellspring",        2, 60, 3, 3, "MEND RESTORES 5 PCT MORE MP A RANK." },
    [TALENT_TIDAL_HEART]        = { "Tidal Heart",       2, 60, 1, 3, "YOUR HEALING IS 25 PCT STRONGER." },
    [TALENT_GLACIER_HEART]      = { "Glacier Heart",     2, CAPSTONE_LEVEL, 1, CAPSTONE_COST, "BOSSES REFREEZE RIGHT AFTER THAW." },
    // Wind: hits, evasion and tempo
    [TALENT_GALE_FORCE]         = { "Gale Force",        3, 1,  3, 1, "WIND SPELLS DEAL 6 PCT MORE A RANK." },
    [TALENT_FLEET]              = { "Fleet",             3, 1,  3, 1, "5 PCT MORE AGILITY A RANK." },
    [TALENT_WHISTLING_STAFF]    = { "Whistling Staff",   3, 1,  1, 1, "SWINGS 30 PCT MORE. TAKE 6 PCT LESS." },
    [TALENT_SQUALL]             = { "Squall",            3, 12, 3, 1, "MULTI-HIT SPELLS: 8 PCT MORE A RANK." },
    [TALENT_EVASION]            = { "Evasion",           3, 12, 3, 1, "FOES MISS YOU 3 PCT MORE A RANK." },
    [TALENT_TAILWIND]           = { "Tailwind",          3, 12, 1, 1, "WIND SPELLS COST 25 PCT LESS MP." },
    [TALENT_CYCLONE]            = { "Cyclone",           3, 30, 3, 2, "WIND HITS: 5 PCT A RANK TO CRACK ANY." },
    [TALENT_CROSSWIND]          = { "Crosswind",         3, 30, 3, 2, "WIND 10 PCT MORE ON BROKEN A RANK." },
    [TALENT_UPDRAFT]            = { "Updraft",           3, 30, 1, 2, "MIRAGE AND WHITEOUT LAST 1 MORE TURN." },
    [TALENT_TEMPEST_FURY]       = { "Tempest Fury",      3, 48, 3, 3, "WIND SPELLS DEAL 5 PCT MORE A RANK." },
    [TALENT_EYE_OF_THE_STORM]   = { "Eye of the Storm",  3, 48, 3, 3, "TAKE 3 PCT LESS DAMAGE A RANK." },
    [TALENT_SECOND_BREATH]      = { "Second Breath",     3, 48, 1, 3, "START EACH BATTLE WITH 1 CHARGE." },
    [TALENT_SHEAR_FORCE]        = { "Shear Force",       3, 60, 3, 3, "MULTI-HIT SPELLS: 8 PCT MORE A RANK." },
    [TALENT_ZEPHYR_STEP]        = { "Zephyr Step",       3, 60, 3, 3, "FOES MISS YOU 3 PCT MORE A RANK." },
    [TALENT_STORM_CALLER]       = { "Storm Caller",      3, 60, 1, 3, "WIND FINISHERS NEED 1 LESS CHARGE." },
    [TALENT_SECOND_WIND]        = { "Second Wind",       3, CAPSTONE_LEVEL, 1, CAPSTONE_COST, "EVERY THIRD ROUND YOU ACT TWICE." },
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

// Brian's spells of `element` against a foe: the school's damage talents, and the ones that pay off on a foe's state.
int eo_talents_spell_percent(int element, int broken, int frozen, int burning) {
    int p = 0;
    switch (element) {
        case 0: p += 6 * eo_talent(TALENT_EMBER_HEART) + 5 * eo_talent(TALENT_BLAZE); break;
        case 1: p += 7 * eo_talent(TALENT_BEDROCK) + 7 * eo_talent(TALENT_COLOSSUS) +
                     (broken ? 10 * eo_talent(TALENT_TREMOR) : 0); break;
        case 2: p += 6 * eo_talent(TALENT_COLD_SNAP) + 5 * eo_talent(TALENT_RIME); break;
        case 3: p += 6 * eo_talent(TALENT_GALE_FORCE) + 5 * eo_talent(TALENT_TEMPEST_FURY) +
                     (broken ? 10 * eo_talent(TALENT_CROSSWIND) : 0); break;
    }
    if (burning) {
        p += 5 * eo_talent(TALENT_CONFLAGRATION);
    }
    if (frozen) {
        p += 8 * eo_talent(TALENT_FROSTBITE);
    }
    return p;
}

// Staff swings.
int eo_talents_staff_percent(int frozen) {
    return 30 * (eo_talent(TALENT_HEAVY_STAFF) + eo_talent(TALENT_WHISTLING_STAFF)) +
           (frozen ? 8 * eo_talent(TALENT_FROSTBITE) : 0);
}

// Percent less damage Brian takes.
int eo_talents_taken_percent(void) {
    return 6 * (eo_talent(TALENT_SEARING_STAFF) + eo_talent(TALENT_HEAVY_STAFF) + eo_talent(TALENT_TIDE_STAFF) +
                eo_talent(TALENT_WHISTLING_STAFF)) +
           7 * (eo_talent(TALENT_ASH_VEIL) + eo_talent(TALENT_CINDER_SKIN) + eo_talent(TALENT_ICE_ARMOR)) +
           2 * eo_talent(TALENT_THICK_SKIN) + 3 * (eo_talent(TALENT_GRANITE_WILL) + eo_talent(TALENT_EYE_OF_THE_STORM)) +
           4 * eo_talent(TALENT_UNBREAKABLE);
}

// Percent off a spell's MP cost.
int eo_talents_mp_percent(int school) {
    switch (school) {
        case 0: return eo_talent(TALENT_HOT_HANDS) ? 25 : 0;
        case 1: return eo_talent(TALENT_QUARRY) ? 25 : 0;
        case 2: return 9 * eo_talent(TALENT_CLEAR_WATERS);
        case 3: return eo_talent(TALENT_TAILWIND) ? 25 : 0;
    }
    return 0;
}

// ---- Fleet: agility in battle ----

#define BRIAN_BATTLE_AGILITY (*(u16*)0x8007BBBE)

RECOMP_HOOK_RETURN("player_battle_init") void eo_talents_on_battle_init(void) {
    int fleet = eo_talent(TALENT_FLEET);
    if (fleet > 0) {
        BRIAN_BATTLE_AGILITY = (u16)(BRIAN_BATTLE_AGILITY * (100 + 5 * fleet) / 100);
    }
}
