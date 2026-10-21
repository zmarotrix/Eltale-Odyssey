#include "eo.h"

// Talents (playtest, 2026-10-21: "I want talents for you to spend your points on"): per-element passives bought with
// Spirit Points in the Spirits window's second page, each gated by that element's level. They compete with tier
// attunement for the same points, so a build can't have everything. First pass: two per element.

#define ELEMENT_LEVELS ((u8*)0x8007BAA4)   // Fire, Earth, Water, Wind

// Effect lines are for the small HUD font: uppercase letters, digits and . : ' - ? only.
static const TalentInfo talents[TALENT_COUNT] = {
    [TALENT_SMOULDER]       = { "Smoulder",       0, 5,  3, "BURN LASTS ONE MORE TURN." },
    [TALENT_FANNED_FLAMES]  = { "Fanned Flames",  0, 12, 5, "BURN HITS HARDER." },
    [TALENT_BEDROCK]        = { "Bedrock",        1, 5,  3, "START BATTLES WITH 2 CHARGE." },
    [TALENT_SHATTER]        = { "Shatter",        1, 12, 5, "BROKEN FOES TAKE EVEN MORE." },
    [TALENT_DEEP_FREEZE]    = { "Deep Freeze",    2, 5,  3, "FREEZE LASTS ONE MORE TURN." },
    [TALENT_UNDERTOW]       = { "Undertow",       2, 12, 5, "WATER HEALING ENDS YOUR STATUS." },
    [TALENT_SHIFTING_WINDS] = { "Shifting Winds", 3, 5,  3, "YOUR STATUSES IGNORE THE LIMIT." },
    [TALENT_CROSSWIND]      = { "Crosswind",      3, 12, 5, "WIND BOOST GIVES MORE SHOTS." },
};

const TalentInfo* eo_talent_info(int talent) {
    return talent >= 0 && talent < TALENT_COUNT ? &talents[talent] : 0;
}

int eo_talent(int talent) {
    return talent >= 0 && talent < TALENT_COUNT && (gEo.talents & (1u << talent)) != 0;
}

int eo_talent_learn(int talent) {
    const TalentInfo* t = eo_talent_info(talent);
    if (t == 0 || eo_talent(talent) || ELEMENT_LEVELS[t->school] < t->level || gEo.spirit_points < t->cost) {
        return 0;
    }
    gEo.spirit_points = (u8)(gEo.spirit_points - t->cost);
    gEo.talents |= 1u << talent;
    return 1;
}
