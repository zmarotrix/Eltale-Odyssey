#include "eo.h"

// Talents (playtests, 2026-10-21: "a full page of talents for each magic", "you should NOT be able to even come close
// to affording all of them", ranks "like 1/3 so it gets stronger like in WoW"). Each element has a tree of nine
// talents on its own page of the Spirits window, deeper rows gated by that element's level, bought one rank at a
// time with Spirit Points. Every tree filled costs about 150 points on top of the 92 for full attunement, against the
// 98 the game's spirits give: a build picks a lane.
//
// Effects live where the rule they bend lives (status.c, combat.c, equipment.c); eo_talent() is how they ask.

#define ELEMENT_LEVELS ((u8*)0x8007BAA4)   // Fire, Earth, Water, Wind
#define RANK_COST 2
#define CAPSTONE_COST 5

// Effect lines are for the small HUD font (uppercase, digits and . : ' - ? only; at most 33 characters).
static const TalentInfo talents[TALENT_COUNT] = {
    // Fire: Burn, the Pyromancer's lane
    [TALENT_SMOULDER]       = { "Smoulder",       0, 1,  2, RANK_COST, "BURN LASTS 1 MORE TURN A RANK." },
    [TALENT_FANNED_FLAMES]  = { "Fanned Flames",  0, 3,  3, RANK_COST, "BURN STACKS HIT HARDER A RANK." },
    [TALENT_KINDLING]       = { "Kindling",       0, 6,  3, RANK_COST, "FIRE SPELLS 4 PCT MORE A RANK." },
    [TALENT_CONFLAGRATION]  = { "Conflagration",  0, 10, 2, RANK_COST, "IGNITING DEALS 25 PCT MORE A RANK." },
    [TALENT_FIREBRAND]      = { "Firebrand",      0, 12, 1, RANK_COST, "ANY STAFF HIT SETS FOES BURNING." },
    [TALENT_WILDFIRE]       = { "Wildfire",       0, 16, 2, RANK_COST, "A BURNING FOE'S DEATH SPREADS IT." },
    [TALENT_SEARING]        = { "Searing",        0, 20, 3, RANK_COST, "BURNING FOES TAKE 5 PCT MORE A RANK." },
    [TALENT_HEAT_HAZE]      = { "Heat Haze",      0, 25, 1, RANK_COST, "EACH BURN TICK GIVES YOU 1 MP." },
    [TALENT_INFERNO]        = { "Inferno",        0, 30, 1, CAPSTONE_COST, "BURN STACKS UP TO 4." },
    // Earth: heavy hits, guard and Break
    [TALENT_BEDROCK]        = { "Bedrock",        1, 1,  2, RANK_COST, "START WITH 1 MORE CHARGE A RANK." },
    [TALENT_WEIGHT]         = { "Weight",         1, 3,  3, RANK_COST, "EARTH SPELLS 4 PCT MORE A RANK." },
    [TALENT_STONESKIN]      = { "Stoneskin",      1, 6,  3, RANK_COST, "DEFENSE 2 MORE A RANK IN BATTLE." },
    [TALENT_SHATTER]        = { "Shatter",        1, 10, 2, RANK_COST, "BROKEN FOES TAKE 15 PCT MORE A RANK." },
    [TALENT_FAULT_LINE]     = { "Fault Line",     1, 12, 1, RANK_COST, "EARTH WEAK HITS TAKE 2 SHIELDS." },
    [TALENT_AFTERSHOCK]     = { "Aftershock",     1, 16, 2, RANK_COST, "EARTH BOOST HITS HARDER A RANK." },
    [TALENT_BULWARK]        = { "Bulwark",        1, 20, 1, RANK_COST, "FIRST HIT ON YOU EACH FIGHT HALVED." },
    [TALENT_LANDSLIDE]      = { "Landslide",      1, 25, 1, RANK_COST, "A BREAK CRACKS EVERY OTHER FOE." },
    [TALENT_EPICENTRE]      = { "Epicentre",      1, 30, 1, CAPSTONE_COST, "BROKEN FOES STAY BROKEN LONGER." },
    // Water: healing, Freeze and sustain
    [TALENT_DEEP_FREEZE]    = { "Deep Freeze",    2, 1,  2, RANK_COST, "FREEZE LASTS 1 MORE TURN A RANK." },
    [TALENT_WELLSPRING]     = { "Wellspring",     2, 3,  3, RANK_COST, "WATER HEALS 10 PCT MORE A RANK." },
    [TALENT_TIDAL_FORCE]    = { "Tidal Force",    2, 6,  3, RANK_COST, "WATER SPELLS 4 PCT MORE A RANK." },
    [TALENT_STILL_WATERS]   = { "Still Waters",   2, 10, 2, RANK_COST, "REGAIN MP AT THE START OF TURNS." },
    [TALENT_UNDERTOW]       = { "Undertow",       2, 12, 1, RANK_COST, "WATER HEALING ENDS YOUR STATUS." },
    [TALENT_FROSTBITE]      = { "Frostbite",      2, 16, 2, RANK_COST, "FROZEN FOES TAKE 15 PCT MORE A RANK." },
    [TALENT_RIPTIDE]        = { "Riptide",        2, 20, 1, RANK_COST, "FREEZING A FOE GIVES 1 CHARGE." },
    [TALENT_UNDERCURRENT]   = { "Undercurrent",   2, 25, 3, RANK_COST, "WATER SPELLS MAY COST NO MP." },
    [TALENT_GLACIER]        = { "Glacier",        2, 30, 1, CAPSTONE_COST, "FREEZING A FOE CRACKS A SHIELD." },
    // Wind: many hits, charge and statuses
    [TALENT_CROSSWIND]      = { "Crosswind",      3, 1,  2, RANK_COST, "WIND BOOST GIVES MORE SHOTS." },
    [TALENT_LOCK_ON]        = { "Lock-On",        3, 3,  1, RANK_COST, "HOLD Z TO LOCK ON. TAP TO SWITCH." },
    [TALENT_TAILWIND]       = { "Tailwind",       3, 6,  2, RANK_COST, "MORE CHARGE FROM WEAK HITS." },
    [TALENT_SHIFTING_WINDS] = { "Shifting Winds", 3, 10, 1, RANK_COST, "YOUR STATUSES IGNORE THE LIMIT." },
    [TALENT_EYE_OF_THE_STORM] = { "Eye of Storm", 3, 12, 2, RANK_COST, "YOUR STATUSES LAST 1 MORE A RANK." },
    [TALENT_CYCLONE]        = { "Cyclone",        3, 16, 3, RANK_COST, "WIND HITS MAY CRACK ANY SHIELD." },
    [TALENT_SECOND_WIND]    = { "Second Wind",    3, 20, 1, RANK_COST, "BOOSTING 3 GIVES 1 CHARGE BACK." },
    [TALENT_UPDRAFT]        = { "Updraft",        3, 25, 2, RANK_COST, "HOLD 1 MORE CHARGE A RANK." },
    [TALENT_MAELSTROM]      = { "Maelstrom",      3, 30, 1, CAPSTONE_COST, "GAIN 2 CHARGE EACH TURN." },
};

const TalentInfo* eo_talent_info(int talent) {
    return talent >= 0 && talent < TALENT_COUNT ? &talents[talent] : 0;
}

int eo_talent(int talent) {
    return talent >= 0 && talent < TALENT_COUNT ? gEo.talent_ranks[talent] : 0;
}

int eo_talent_learn(int talent) {
    const TalentInfo* t = eo_talent_info(talent);
    if (t == 0 || gEo.talent_ranks[talent] >= t->ranks || ELEMENT_LEVELS[t->school] < t->level ||
        gEo.spirit_points < t->cost) {
        return 0;
    }
    gEo.spirit_points = (u8)(gEo.spirit_points - t->cost);
    gEo.talent_ranks[talent]++;
    return 1;
}
