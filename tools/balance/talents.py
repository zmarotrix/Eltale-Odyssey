"""Talent trees (2026-10-22 proposal) and how the balance engine spends Spirit Points on them.

Rows open by the school's element level (1, 12, 30, 48, 60; capstone 68). Each row: two talents of up to 3 ranks and
one single pick. Ranks in rows 1-2 cost 1 Spirit Point, row 3 costs 2, rows 4-5 cost 3, a capstone 4: a full tree is
74 points, all that a player who
finds 75% of the spirits earns (2026-10-23: "invest all points into a single tree to get EVERYTHING, OR spend some in
another tree"). Effects are keys the engine reads (Battle.tal), summed over ranks.
"""

FIRE, EARTH, WATER, WIND = 0, 1, 2, 3
ROWS = [1, 12, 30, 48, 60]
CAPSTONE_LEVEL = 68
RANK_COST = 2                               # flat cost (row_cost="flat")
ROW_COST = {1: 1, 12: 1, 30: 2, 48: 3, 60: 3}   # deeper rows cost more (row_cost="deep")
CAPSTONE_COST = 4

# (name, row level, max ranks, effect per rank, what it does)
TREES = {
    FIRE: [
        ("Kindling", 1, 3, {"burn_tick": 0.10}, "Burn ticks deal 10% more per rank"),
        ("Ember Heart", 1, 3, {"dmg0": 0.06}, "Fire spells deal 6% more per rank"),
        ("Searing Staff", 1, 1, {"staff_burn": 1, "taken": 0.06}, "Staff hits add a Burn stack; take 6% less damage"),
        ("Fan the Coals", 12, 3, {"flare": 0.10}, "Stoking (Flare Up, Firestorm) heats Burn 10% more per rank"),
        ("Lingering Flame", 12, 3, {"burn_turns": 1 / 3}, "Burn lasts 1 more turn at rank 3"),
        ("Hot Hands", 12, 1, {"mp0": 0.25}, "Fire spells cost 25% less MP"),
        ("Wildfire", 30, 3, {"max_stacks": 1 / 3}, "1 more Burn stack fits on a foe at rank 3"),
        ("Pyre", 30, 3, {"cash": 0.15}, "Searing Steam cashes Burn in for 15% more per rank"),
        ("Ash Veil", 30, 1, {"taken": 0.07}, "Take 7% less damage"),
        ("Conflagration", 48, 3, {"burning_vuln": 0.05}, "Burning foes take 5% more from Brian's spells per rank"),
        ("Blaze", 48, 3, {"dmg0": 0.05}, "Fire spells deal 5% more per rank"),
        ("Phoenix", 48, 1, {"phoenix": 1}, "Once a battle, falling under 25% HP heals 30%"),
        ("Inferno", 60, 3, {"burn_tick": 0.10}, "Burn ticks deal 10% more per rank"),
        ("Smoldering Shields", 60, 3, {"burn_crack": 0.10}, "Each Burn tick has a 10% chance per rank to crack a shield"),
        ("Cinder Skin", 60, 1, {"taken": 0.07}, "Take 7% less damage"),
    ],
    EARTH: [
        ("Bedrock", 1, 3, {"dmg1": 0.07}, "Earth spells deal 7% more per rank"),
        ("Thick Skin", 1, 3, {"taken": 0.02}, "Take 2% less damage per rank"),
        ("Heavy Staff", 1, 1, {"staff_mult": 0.30, "taken": 0.06}, "Staff hits deal 30% more; take 6% less damage"),
        ("Steady Footing", 12, 3, {"settle_add": 0.07}, "Settled Earth attacks lose 7% less per rank (60% to 39%)"),
        ("Momentum", 12, 3, {"earth_boost": 0.05}, "Earth Boost adds 5% more per Charge per rank"),
        ("Brace", 12, 1, {"brace": 1}, "On a turn ended with B while Settled, take half damage"),
        ("Fault Line", 30, 3, {"fault": 1 / 3}, "Earth weakness hits crack 1 more shield (a third of the time per rank)"),
        ("Stoneskin Mastery", 30, 3, {"guard_add": 0.05}, "Guards block 5% more per rank"),
        ("Quarry", 30, 1, {"mp1": 0.25}, "Earth spells cost 25% less MP"),
        ("Tremor", 48, 3, {"broken1": 0.10}, "Earth spells deal 10% more to Broken foes per rank"),
        ("Granite Will", 48, 3, {"taken": 0.03}, "Take 3% less damage per rank"),
        ("Planted Feet", 48, 1, {"rest_charge": 1}, "A turn ended with B while Settled gives 1 more Charge"),
        ("Colossus", 60, 3, {"dmg1": 0.07}, "Earth spells deal 7% more per rank"),
        ("Unbreakable", 60, 3, {"max_hp": 0.04}, "4% more max HP per rank"),
        ("Seismic", 60, 1, {"seismic": 1}, "Heavy Earth hits crack a shield even off-weakness"),
    ],
    WATER: [
        ("Deep Well", 1, 3, {"mend_add": 0.02}, "Mend heals 2% more HP and MP per rank"),
        ("Cold Snap", 1, 3, {"dmg2": 0.06}, "Water spells deal 6% more per rank"),
        ("Tide Staff", 1, 1, {"staff_frost": 1, "taken": 0.06}, "Staff hits Frost the foe; take 6% less damage"),
        ("Frostbite", 12, 3, {"frozen_vuln": 0.08}, "Frozen foes take 8% more from Brian per rank"),
        ("Clear Waters", 12, 3, {"mp2": 0.09}, "Water spells cost 9% less MP per rank"),
        ("Winter's Grip", 12, 1, {"immune_less": 1}, "Bosses resist freezing 1 turn less after thawing"),
        ("Glacial Shatter", 30, 3, {"shatter_add": 0.2}, "Shatter deals 0.2x more per rank"),
        ("Hypothermia", 30, 3, {"chill_extra": 0.10}, "Chilling has a 10% chance per rank to go a step colder"),
        ("Ice Armor", 30, 1, {"taken": 0.07}, "Take 7% less damage"),
        ("Rime", 48, 3, {"dmg2": 0.05}, "Water spells deal 5% more per rank"),
        ("Permafrost", 48, 3, {"freeze_extra": 1 / 3}, "Monsters stay frozen 1 more turn at rank 3"),
        ("Living Water", 48, 1, {"regen": 0.03}, "Heal 3% of max HP each turn"),
        ("Deep Freeze", 60, 3, {"shatter_add": 0.2}, "Shatter deals 0.2x more per rank"),
        ("Wellspring", 60, 3, {"max_mp": 0.05}, "5% more max MP per rank"),
        ("Tidal Heart", 60, 1, {"heal_mult": 0.25}, "All of Brian's healing 25% stronger"),
    ],
    WIND: [
        ("Gale Force", 1, 3, {"dmg3": 0.06}, "Wind spells deal 6% more per rank"),
        ("Fleet", 1, 3, {"agi": 0.05}, "5% more agility per rank"),
        ("Whistling Staff", 1, 1, {"staff_mult": 0.30, "taken": 0.06}, "Staff hits deal 30% more; take 6% less damage"),
        ("Squall", 12, 3, {"hits": 0.08}, "Many-hit spells hit 8% more often per rank"),
        ("Evasion", 12, 3, {"evade": 0.03}, "Attacks miss Brian 3% more often per rank"),
        ("Tailwind", 12, 1, {"mp3": 0.25}, "Wind spells cost 25% less MP"),
        ("Cyclone", 30, 3, {"any_crack": 0.05}, "Each Wind hit has a 5% chance per rank to crack any shield"),
        ("Crosswind", 30, 3, {"broken3": 0.10}, "Wind spells deal 10% more to Broken foes per rank"),
        ("Updraft", 30, 1, {"veil": 1}, "Mirage and Whiteout last 1 more turn"),
        ("Tempest Fury", 48, 3, {"dmg3": 0.05}, "Wind spells deal 5% more per rank"),
        ("Eye of the Storm", 48, 3, {"taken": 0.03}, "Take 3% less damage per rank"),
        ("Second Breath", 48, 1, {"start_charge": 1}, "Start each battle with 1 Charge"),
        ("Shear Force", 60, 3, {"hits": 0.08}, "Many-hit spells hit 8% more often per rank"),
        ("Zephyr Step", 60, 3, {"evade": 0.03}, "Attacks miss Brian 3% more often per rank"),
        ("Storm Caller", 60, 1, {"finisher_less": 1}, "Tier-5 Wind spells need 1 less Charge"),
    ],
}
CAPSTONES = {
    FIRE: ("Eternal Flame", {"eternal": 1}, "Burn never runs out, and a foe holds up to 5 stacks"),
    EARTH: ("Avalanche", {"avalanche": 1}, "Every third Earth hit in a battle deals double"),
    WATER: ("Glacier Heart", {"glacier": 1}, "Bosses can be frozen again right after they thaw"),
    WIND: ("Second Wind", {"second_wind": 1}, "Every third round, Brian acts twice"),
}


def allocate(build, levels, points, policy="focus", row_cost="deep", overrides=None):
    """Spend Spirit Points the way a player of this build would. "focus": the main school's tree row by row (as its
    level allows) and its capstone, then the second school. "split": each purchase goes to the school furthest below
    its share of the build's weights (capstones only once a tree is otherwise full). Returns the summed effects."""
    schools = sorted([s for s in range(4) if build[s] > 0], key=lambda s: -build[s]) or [0]
    queues = {}
    for s in schools:
        q = [(effects, RANK_COST if row_cost == "flat" else ROW_COST[row])
             for name, row, ranks, effects, _ in [(n, r, k, (overrides or {}).get(n, e), d) for n, r, k, e, d in TREES[s]]
             if levels[s] >= row for _ in range(ranks)]
        if levels[s] >= CAPSTONE_LEVEL:
            q.append((CAPSTONES[s][1], CAPSTONE_COST))
        queues[s] = q
    tal, spent, by_school = {}, 0, {s: 0 for s in schools}
    capstone_taken = False
    while True:
        if policy == "focus":
            options = [s for s in schools if queues[s]][:1]
        else:
            weight = sum(build[s] for s in schools)
            options = sorted([s for s in schools if queues[s]], key=lambda s: by_school[s] / build[s] - weight * 0)
        options = [s for s in options if not (capstone_taken and queues[s][0][1] == CAPSTONE_COST)]
        if not options:
            return tal
        s = options[0]
        effects, cost = queues[s][0]
        if spent + cost > points:
            return tal
        queues[s].pop(0)
        for k, v in effects.items():
            tal[k] = tal.get(k, 0) + v
        spent += cost
        by_school[s] += cost
        capstone_taken = capstone_taken or cost == CAPSTONE_COST
