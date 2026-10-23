#include "eo.h"

// Progression on the game's own leveling (Spell Trees doc, "Progression", 2026-10-22/23): spirits and battles raise
// element levels exactly as in the original, but an element can go to 99 instead of 50, and spell power grows 1.5%
// per level instead of 3%, with all of Brian's damage at 0.76x, so the numbers stay near the original game's while
// one school can be mastered (two never can: that would take 98 battle levels on top of every spirit).

#define ELEMENT_LEVELS ((u8*)0x8007BAA4)   // Fire, Earth, Water, Wind
#define GAME_FLAGS (*(u32*)0x8007B2E4)
#define FLAG_LEVEL_UP 0x8                  // the "raise an element" screen is waiting for a C button
#define GAME_LEVEL_CAP 50
#define LEVEL_CAP 99

extern u16 gPadButtonsNew;

// ---- Element levels up to 99 ----
//
// func_8001F3DC is the screen that raises an element after a spirit or a battle level: each C button raises its
// element if it is below 50, clears the level-up flag and closes the screen; at 50 it does nothing. Its buttons, in
// the order it checks them: C-up Fire, C-down Water, C-right Wind, C-left Earth. Here, before the game looks, an
// element from 50 to 98 is raised the same way and the button is eaten, so the game doesn't also act on it.

static const struct { u16 button; u8 element; } level_buttons[4] = {
    { 0x8, 0 }, { 0x4, 2 }, { 0x1, 3 }, { 0x2, 1 },
};

RECOMP_HOOK("func_8001F3DC") void eo_progression_level_up_screen(void) {
    if (!(GAME_FLAGS & FLAG_LEVEL_UP)) {
        return;
    }
    for (int i = 0; i < 4; i++) {
        if (!(gPadButtonsNew & level_buttons[i].button)) {
            continue;
        }
        u8* level = &ELEMENT_LEVELS[level_buttons[i].element];
        if (*level < GAME_LEVEL_CAP) {
            return;   // the game raises it
        }
        if (*level < LEVEL_CAP) {
            (*level)++;
            GAME_FLAGS &= ~FLAG_LEVEL_UP;
            gPadButtonsNew = 0;
        }
        return;
    }
}

// ---- Spell power ----
//
// The game's spell power is base x 0.02 x (1 + 0.2 x (1.03^L - 1) / 0.03), where L is the element's level plus a
// sixteenth of the opposite element's and an eighth of each side element's (research/battle). The overhaul wants
// 1.015 in place of 1.03 and 0.76x on top; scaling the spell's base by the ratio of the two curves does that without
// touching the game's formula. Returns the factor in thousandths.

static const u8 opposite[4] = { 2, 3, 0, 1 };   // Fire-Water, Earth-Wind

static u32 curve(u32 steps, u32 growth_thousandths) {
    // 1 + 0.2 x sum of growth^k for k < steps, in thousandths
    u32 term = 1000, sum = 0;
    for (u32 k = 0; k < steps; k++) {
        sum += term;
        term = term * growth_thousandths / 1000;
    }
    return 1000 + sum / 5;
}

u32 eo_progression_spell_scale(int school) {
    u32 own = ELEMENT_LEVELS[school];
    u32 opp = ELEMENT_LEVELS[opposite[school]];
    u32 sides = 0;
    for (int s = 0; s < 4; s++) {
        if (s != school && s != opposite[school]) {
            sides += ELEMENT_LEVELS[s] >> 3;
        }
    }
    u32 steps = own + (opp >> 4) + sides;
    return curve(steps, 1015) * DAMAGE_SCALE_THOUSANDTHS / curve(steps, 1030);
}

// Spell power per 1000 base right now: base x 0.02 x the overhaul's curve x 0.76 (what the game's formula gives
// once the base is scaled above). For effects that deal a spell's worth of damage themselves (spells.c).
u32 eo_progression_spell_power_permille(int school) {
    u32 own = ELEMENT_LEVELS[school];
    u32 opp = ELEMENT_LEVELS[opposite[school]];
    u32 sides = 0;
    for (int s = 0; s < 4; s++) {
        if (s != school && s != opposite[school]) {
            sides += ELEMENT_LEVELS[s] >> 3;
        }
    }
    return curve(own + (opp >> 4) + sides, 1015) * 2 * DAMAGE_SCALE_THOUSANDTHS / 100000;
}

// Mend heals by Water level (Spell Trees doc): full at level 12, less below, on the spell-power curve.
#define MEND_FULL_LEVEL 12

u32 eo_progression_mend_permille(void) {
    u32 water = ELEMENT_LEVELS[2];
    if (water >= MEND_FULL_LEVEL) {
        return 1000;
    }
    return curve(water, 1015) * 1000 / curve(MEND_FULL_LEVEL, 1015);
}
