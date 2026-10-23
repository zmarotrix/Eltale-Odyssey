"""Talent trees (2026-10-22 proposal) and how the balance engine spends Spirit Points on them.

Rows open by the school's element level (1, 12, 30, 48, 60; capstone 68). Each row: two talents of up to 3 ranks and
one single pick. A rank costs 1 Spirit Point, a capstone 3, and only one capstone can be learned. Effects are keys
the engine reads (Battle.tal), summed over ranks.
"""

FIRE, EARTH, WATER, WIND = 0, 1, 2, 3
ROWS = [1, 12, 30, 48, 60]
CAPSTONE_LEVEL = 68

# (name, row level, max ranks, effect per rank, what it does)
TREES = {
    FIRE: [
        ("Kindling", 1, 3, {"burn_tick": 0.08}, "Burn ticks deal 8% more per rank"),
        ("Ember Heart", 1, 3, {"dmg0": 0.03}, "Fire spells deal 3% more per rank"),
        ("Searing Staff", 1, 1, {"staff_burn": 1}, "Staff hits add a Burn stack"),
        ("Fan the Coals", 12, 3, {"flare": 0.08}, "Stoking (Flare Up, Firestorm) heats Burn 8% more per rank"),
        ("Lingering Flame", 12, 3, {"burn_turns": 1 / 3}, "Burn lasts 1 more turn at rank 3"),
        ("Hot Hands", 12, 1, {"mp0": 0.20}, "Fire spells cost 20% less MP"),
        ("Wildfire", 30, 3, {"max_stacks": 1 / 3}, "1 more Burn stack fits on a foe at rank 3"),
        ("Pyre", 30, 3, {"cash": 0.12}, "Searing Steam cashes Burn in for 12% more per rank"),
        ("Ash Veil", 30, 1, {"taken": 0.05}, "Take 5% less damage"),
        ("Conflagration", 48, 3, {"burning_vuln": 0.04}, "Burning foes take 4% more from Brian's spells per rank"),
        ("Blaze", 48, 3, {"dmg0": 0.04}, "Fire spells deal 4% more per rank"),
        ("Phoenix", 48, 1, {"phoenix": 1}, "Once a battle, falling under 25% HP heals 30%"),
        ("Inferno", 60, 3, {"burn_tick": 0.08}, "Burn ticks deal 8% more per rank"),
        ("Smoldering Shields", 60, 3, {"burn_crack": 0.08}, "Each Burn tick has an 8% chance per rank to crack a shield"),
        ("Cinder Skin", 60, 1, {"taken": 0.05}, "Take 5% less damage"),
    ],
    EARTH: [
        ("Bedrock", 1, 3, {"dmg1": 0.04}, "Earth spells deal 4% more per rank"),
        ("Thick Skin", 1, 3, {"taken": 0.015}, "Take 1.5% less damage per rank"),
        ("Heavy Staff", 1, 1, {"staff_mult": 0.25}, "Staff hits deal 25% more"),
        ("Steady Footing", 12, 3, {"settle_add": 0.05}, "Settled Earth attacks lose 5% less per rank (60% to 45%)"),
        ("Momentum", 12, 3, {"earth_boost": 0.04}, "Earth Boost adds 4% more per Charge per rank"),
        ("Brace", 12, 1, {"brace": 1}, "On a turn ended with B while Settled, take half damage"),
        ("Fault Line", 30, 3, {"fault": 1 / 3}, "Earth weakness hits crack 1 more shield (a third of the time per rank)"),
        ("Stoneskin Mastery", 30, 3, {"guard_add": 0.04}, "Guards block 4% more per rank"),
        ("Quarry", 30, 1, {"mp1": 0.20}, "Earth spells cost 20% less MP"),
        ("Tremor", 48, 3, {"broken1": 0.08}, "Earth spells deal 8% more to Broken foes per rank"),
        ("Granite Will", 48, 3, {"taken": 0.02}, "Take 2% less damage per rank"),
        ("Planted Feet", 48, 1, {"rest_charge": 1}, "A turn ended with B while Settled gives 1 more Charge"),
        ("Colossus", 60, 3, {"dmg1": 0.05}, "Earth spells deal 5% more per rank"),
        ("Unbreakable", 60, 3, {"max_hp": 0.03}, "3% more max HP per rank"),
        ("Seismic", 60, 1, {"seismic": 1}, "Heavy Earth hits crack a shield even off-weakness"),
    ],
    WATER: [
        ("Deep Well", 1, 3, {"mend_add": 0.015}, "Mend heals 1.5% more HP and MP per rank"),
        ("Cold Snap", 1, 3, {"dmg2": 0.03}, "Water spells deal 3% more per rank"),
        ("Tide Staff", 1, 1, {"staff_frost": 1}, "Staff hits Frost the foe"),
        ("Frostbite", 12, 3, {"frozen_vuln": 0.06}, "Frozen foes take 6% more from Brian per rank"),
        ("Clear Waters", 12, 3, {"mp2": 0.07}, "Water spells cost 7% less MP per rank"),
        ("Winter's Grip", 12, 1, {"immune_less": 1}, "Bosses resist freezing 1 turn less after thawing"),
        ("Glacial Shatter", 30, 3, {"shatter_add": 0.15}, "Shatter deals 0.15x more per rank"),
        ("Hypothermia", 30, 3, {"chill_extra": 0.08}, "Chilling has an 8% chance per rank to go a step colder"),
        ("Ice Armor", 30, 1, {"taken": 0.05}, "Take 5% less damage"),
        ("Rime", 48, 3, {"dmg2": 0.04}, "Water spells deal 4% more per rank"),
        ("Permafrost", 48, 3, {"freeze_extra": 1 / 3}, "Monsters stay frozen 1 more turn at rank 3"),
        ("Living Water", 48, 1, {"regen": 0.02}, "Heal 2% of max HP each turn"),
        ("Deep Freeze", 60, 3, {"shatter_add": 0.15}, "Shatter deals 0.15x more per rank"),
        ("Wellspring", 60, 3, {"max_mp": 0.04}, "4% more max MP per rank"),
        ("Tidal Heart", 60, 1, {"heal_mult": 0.20}, "All of Brian's healing 20% stronger"),
    ],
    WIND: [
        ("Gale Force", 1, 3, {"dmg3": 0.03}, "Wind spells deal 3% more per rank"),
        ("Fleet", 1, 3, {"agi": 0.04}, "4% more agility per rank"),
        ("Whistling Staff", 1, 1, {"staff_mult": 0.25}, "Staff hits deal 25% more"),
        ("Squall", 12, 3, {"hits": 0.06}, "Many-hit spells hit 6% more often per rank"),
        ("Evasion", 12, 3, {"evade": 0.025}, "Attacks miss Brian 2.5% more often per rank"),
        ("Tailwind", 12, 1, {"mp3": 0.20}, "Wind spells cost 20% less MP"),
        ("Cyclone", 30, 3, {"any_crack": 0.04}, "Each Wind hit has a 4% chance per rank to crack any shield"),
        ("Crosswind", 30, 3, {"broken3": 0.08}, "Wind spells deal 8% more to Broken foes per rank"),
        ("Updraft", 30, 1, {"veil": 1}, "Mirage and Whiteout last 1 more turn"),
        ("Tempest Fury", 48, 3, {"dmg3": 0.04}, "Wind spells deal 4% more per rank"),
        ("Eye of the Storm", 48, 3, {"taken": 0.02}, "Take 2% less damage per rank"),
        ("Second Breath", 48, 1, {"start_charge": 1}, "Start each battle with 1 Charge"),
        ("Shear Force", 60, 3, {"hits": 0.06}, "Many-hit spells hit 6% more often per rank"),
        ("Zephyr Step", 60, 3, {"evade": 0.025}, "Attacks miss Brian 2.5% more often per rank"),
        ("Storm Caller", 60, 1, {"finisher_less": 1}, "Tier-5 Wind spells need 1 less Charge"),
    ],
}
CAPSTONES = {
    FIRE: ("Eternal Flame", {"eternal": 1}, "Burn never runs out, and a foe holds up to 5 stacks"),
    EARTH: ("Avalanche", {"avalanche": 1}, "Every third Earth hit in a battle deals double"),
    WATER: ("Glacier Heart", {"glacier": 1}, "Bosses can be frozen again right after they thaw"),
    WIND: ("Second Wind", {"second_wind": 1}, "Every third round, Brian acts twice"),
}


def allocate(build, levels, points):
    """Spend Spirit Points the way a player of this build would: the main school's tree row by row (as its level
    allows), its capstone when reached, then the second school, then anything left. Returns the summed effects."""
    order = sorted([s for s in range(4) if build[s] > 0], key=lambda s: -build[s]) or [0]
    tal = {}
    spent = 0

    def take(effects, n):
        for k, v in effects.items():
            tal[k] = tal.get(k, 0) + v * n

    capstone_taken = False
    for i, school in enumerate(order):
        for name, row, ranks, effects, _ in TREES[school]:
            if levels[school] < row:
                continue
            n = min(ranks, points - spent)
            if n <= 0:
                return tal
            take(effects, n)
            spent += n
        if i == 0 and not capstone_taken and levels[school] >= CAPSTONE_LEVEL and points - spent >= 3:
            take(CAPSTONES[school][1], 1)
            spent += 3
            capstone_taken = True
    return tal
