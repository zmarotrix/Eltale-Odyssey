#include "eo.h"

// One-line descriptions shown while a spell is selected in battle, in the HUD's small font (uppercase,
// digits and : ' - . ? = only; no commas). First pass: written from the spell data and the community notes.

static const char* const descriptions[4][15] = {
    {   // Fire
        "ONE FIREBALL. SETS THE FOE BURNING.",
        "TWO FIREBALLS. EACH HIT ADDS BURN.",
        "STAFF HITS HARDER FOR A WHILE.",
        "THREE ARROWS THAT SEEK THE FOE.",
        "SCALDING STEAM BURSTS AROUND YOU.",
        "THREE FIREBALLS. STACKS BURN FAST.",
        "HALVES A FOE'S ATTACK FOR A WHILE.",
        "STAFF HITS MUCH HARDER FOR A WHILE.",
        "A PILLAR OF FIRE ON THE FOE.",
        "FIVE ARROWS THAT SEEK THE FOE.",
        "SETS OFF ALL BURN ON THE FOE AT ONCE.",
        "STAFF HITS DRAIN HP FOR A WHILE.",
        "THREE BALLS OF MAGMA.",
        "MAY DEFEAT A FOE OUTRIGHT.",
        "STRONGER SCALDING STEAM.",
    },
    {   // Earth
        "HURLS A HEAVY ROCK.",
        "HURLS A BIGGER ROCK.",
        "RAISES YOUR DEFENSE FOR A WHILE.",
        "A BOULDER ROLLS AT THE FOE.",
        "LOWERS A FOE'S DEFENSE.",
        "HURLS THE BIGGEST ROCK.",
        "A ROCK DRAWN TO THE FOE.",
        "GREATLY RAISES YOUR DEFENSE.",
        "A RAIN OF ROCKS ON EVERY FOE.",
        "CONFUSES A FOE.",
        "GREATLY LOWERS A FOE'S DEFENSE.",
        "ROCKS FALL ACROSS THE FIELD.",
        "BLOCKS SPELL DAMAGE FOR A WHILE.",
        "A BIGGER BOULDER ROLLS AT THE FOE.",
        "LOWERS EVERY FOE'S DEFENSE.",
    },
    {   // Water
        "A COLUMN OF WATER ON THE FOE.",
        "A STRONGER COLUMN OF WATER.",
        "RESTORES SOME HP. CURES POISON.",
        "SHOWS A FOE'S STRENGTH.",
        "THE STRONGEST COLUMN OF WATER.",
        "A WALL OF ICE STRIKES THE FOE.",
        "A BLADE OF ICE THAT NEVER MISSES.",
        "LEADS YOU OUT OF A DUNGEON.",
        "FLEE FROM BATTLE.",
        "RETURNS YOU TO THE LAST TOWN.",
        "RESTORES MUCH HP. CURES POISON.",
        "SHOWS EVERY FOE'S STRENGTH.",
        "A WAVE THAT WALKS TO THE FOE.",
        "STEALS MP FROM A FOE.",
        "CANCELS A FOE'S SPELLS.",
    },
    {   // Wind
        "TWO BLADES OF WIND.",
        "THREE BLADES OF WIND.",
        "HOLDS A FOE IN PLACE.",
        "HARDER TO HIT FOR A WHILE.",
        "STOPS A FOE FROM CASTING.",
        "SIX BLADES OF WIND.",
        "ONE GREAT BLADE OF WIND.",
        "HOLDS A FOE IN PLACE LONGER.",
        "A BURST OF WIND ON THE FOE.",
        "MUCH HARDER TO HIT FOR A WHILE.",
        "A WHIRLWIND AROUND THE FOE.",
        "SLOWS A FOE.",
        "YOU WALK FASTER.",
        "STOPS A FOE CASTING FOR LONGER.",
        "A WIND THAT NEVER MISSES.",
    },
};

// Spell names (playtest, 2026-10-21: no "Lv 2" names, "Maybe Rock and Boulder"): one name per spell, shown in the
// spell wheel and the "Learned" notice. Uppercase like the game's; at most 15 characters (the game's name slots).
// None repeats a talent's name.
static const char* const names[4][15] = {
    {   // Fire
        "EMBER", "TWIN FLAME", "FLAME STAFF", "SEEKER", "SCALD",
        "BLAZE", "WILT", "BLAZING STAFF", "PYRE", "FLAME VOLLEY",
        "DETONATE", "LEECHFIRE", "MAGMA", "EXTINCTION", "SEARING STEAM",
    },
    {   // Earth
        "ROCK", "BOULDER", "HARDEN", "ROLLING STONE", "CRUMBLE",
        "MEGALITH", "LODESTONE", "FORTRESS", "AVALANCHE", "TREMOR",
        "ERODE", "ROCKFALL", "SPELLWARD", "JUGGERNAUT", "QUAKE",
    },
    {   // Water
        "SPOUT", "GEYSER", "MEND", "SCRYING POOL", "TORRENT",
        "FROST WALL", "ICICLE", "RIVER PATH", "ESCAPE", "HOMEWARD",
        "RENEW", "CLEAR SIGHT", "SWELL", "DRAIN", "NULLIFY",
    },
    {   // Wind
        "WIND BLADE", "SHEAR", "BIND", "MIRAGE", "HUSH",
        "TEMPEST", "SKYCLEAVER", "SHACKLE", "DOWNBURST", "PHANTOM",
        "WHIRLWIND", "LULL", "SWIFTSTEP", "STILLNESS", "ZEPHYR",
    },
};

const char* eo_spell_name(int school, int index) {
    if (school < 0 || school >= 4 || index < 0 || index >= 15) {
        return 0;
    }
    return names[school][index];
}

const char* eo_spell_description(int school, int index) {
    if (school < 0 || school >= 4 || index < 0 || index >= 15) {
        return 0;
    }
    return descriptions[school][index];
}
