"""Eltale Odyssey balance engine: plays builds through boss fights and normal-enemy packs, many times each, and
gathers statistics (win rate, Brian's turns, enemy actions, HP lost, lowest HP, MP used, Breaks, items, spells cast).

Mechanics follow the Spell Trees doc (2026-10-22) and the game's formulas (research/battle, research/enemies):
spell power, element multipliers, defense, accuracy and agility rolls, Spirit Charge and Boost, Settling, Burn
(kindle / stoke / spread), Frost and Freeze, Break and shields, Wind's any-press rule, boss Freeze immunity.
Talents and gear are not modeled one by one; the KIT rows stand in for what they should add by each boss. CONFIG
holds the levers a scenario can change, and BALANCE_PASS the changes the balance pass proposed (2026-10-22).

Estimates (no measured source; change them here): Brian's HP, MP, defense and agility at each boss, heal amounts,
and how area spells spread over a pack. Spell bases for the new spells are proposals in vanilla's range.

Usage: python engine.py [runs] > results_balance.json   (every scenario in SCENARIOS, 3 healing items)
"""
import json
import math
import os
import random
import re
import sys

FIRE, EARTH, WATER, WIND = range(4)
NAMES = ["Fire", "Earth", "Water", "Wind"]
OPP = {FIRE: WATER, WATER: FIRE, EARTH: WIND, WIND: EARTH}
SIDES = {FIRE: (EARTH, WIND), WATER: (EARTH, WIND), EARTH: (FIRE, WATER), WIND: (FIRE, WATER)}
NONE = 4
RESEARCH = os.path.join(os.path.dirname(__file__), "..", "..", "research")

# ---- Progression (Spell Trees doc, "Progression: element level is attunement") ----
LEVELS_GAINED = [13, 32, 38, 53, 59, 69, 76, 84]   # element levels gained by each boss (EXP curve fit)
TIER_LEVEL = [1, 8, 20, 32, 44]
LEVEL_CAP = 50
BOSS_ORDER = ["Solvaring", "Zelse", "Nepty", "Shilf", "Fargo", "Guilty", "Beigis", "Mammon"]
BOSS_HP = [140, 480, 530, 850, 1110, 1800, 2060, 2550]   # re-derived for the curve (progression doc)
VANILLA_BOSS_HP = [200, 680, 880, 1000, 1500, 1800, 1900, 2300]
BOSS_ATTACK_SCALE = [1.00, 1.05, 1.10, 1.10, 1.15, 1.15, 1.20, 1.20]   # balance.c
# Brian's own stats when he reaches each boss (estimates; Zelse matches the 2026-10-21 playtest setup).
BRIAN_HP = [80, 160, 200, 260, 310, 370, 430, 490]
BRIAN_MP = [20, 50, 60, 75, 85, 100, 110, 120]
BRIAN_DEF = [8, 18, 24, 31, 37, 44, 50, 56]
BRIAN_AGI = [14, 35, 40, 46, 51, 56, 61, 66]
# Enemy damage is set from a design target, not from the guessed stats above: an average boss action takes
# BOSS_HIT of Brian's HP, a normal monster's action MONSTER_HIT. Boss attack values were sized for the old level
# curve and have to be re-derived for this one, like boss HP; these targets are what to derive them from.
BOSS_HIT = 0.12
MONSTER_HIT = 0.045
MONSTER_ACT_CHANCE = 0.75   # a monster spends some turns walking into range

# ---- Scenarios: levers for the balance pass (run() takes a dict that overrides these) ----
# KIT models what the final pass (combat items, gear, talents) is expected to add by each boss: damage dealt,
# damage taken, healing items carried, MP pool. Targets, not measurements: the balance pass assumed gear and talents
# roughly double Brian's damage by the end.
KIT_DMG = [1.00, 1.10, 1.20, 1.35, 1.50, 1.65, 1.80, 2.00]
KIT_TAKEN = [1.00, 0.95, 0.90, 0.85, 0.80, 0.76, 0.73, 0.70]
KIT_ITEMS = [2, 3, 3, 4, 4, 5, 5, 6]
KIT_MP = [1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7]
CONFIG = dict(
    kit=False,            # apply the KIT rows above
    none_mult=0.5,        # damage Brian's spells deal to no-element foes (game rule: 0.5)
    burn_full=False,      # Burn ticks ignore the target's element (a Fire boss still burns at full)
    shatter_neutral=False,  # Shatter breaks ice, so it ignores element
    beigis_shift=True,    # Beigis takes a random element after each Break (the mod's mechanic)
    boss_hp=[1.0] * 8,    # per-boss HP multipliers on BOSS_HP
    wellspring=0.0,       # Wellspring (Water tier 1, 0 MP): a turn that restores this share of max MP; 0 = not in the game
    wellspring_all=False,  # every build can draw on Wellspring, not only those with Water
    boss_shields=None,    # per-boss shield counts; None = 6 + boss number / 2
    pack_hp=1.0, pack_hit=[1.0] * 8,   # monster pack HP, and each monster action's share of Brian's HP, per beat
    boss_hit=[1.0] * 8,   # each boss action's share of Brian's HP, per boss
    spells={},            # per-spell overrides, {"Hurricane": {"base": 90}}
    finisher=0,           # tier-5 spells need this much Charge and spend it all (no Boost on top); 0 = off
)
DEFAULT_CONFIG = dict(CONFIG)

# The balance pass (2026-10-22) proposals, as one scenario. Not yet in the spec; the progression doc has the results.
BALANCE_PASS = dict(
    wellspring=0.2,       # Water tier 1: a turn for 20% of max MP (+10% a Charge)
    finisher=2,           # tier-5 spells need 2 Charge and spend all of it
    none_mult=0.75,       # no-element foes resist a quarter, not half
    spells={"Hurricane": {"base": 90}, "Shatter": {"base": 900}, "Icicle": {"base": 420}, "Undertow": {"base": 260}},
    pack_hit=[1, 1, 1, 1, 1.25, 1.5, 1.75, 2.0],   # late packs keep up with gear
    boss_hit=[1, 1, 1, 1, 1.1, 1.15, 1.2, 1.25],
)
SCENARIOS = {
    "before": dict(),
    "before+kit": dict(kit=True),
    "pass, no kit": dict(BALANCE_PASS),
    "pass+kit": dict(BALANCE_PASS, kit=True),
}

# Which monster set Brian fights while heading to each boss (research/enemies: set by region and beat).
PACK_SET = [0, 1, 4, 2, 3, 3, 3, 5]

# Boss attacks (ROM spell table, research/spells dump): base power, accuracy %, shots.
BOSS_SPELLS = {
    "Solvaring": [(550, 100, 1), (850, 100, 1)],
    "Zelse": [(50, 90, 5), (500, 100, 1), (420, 100, 1)],
    "Nepty": [(200, 90, 3), (500, 100, 1)],
    "Shilf": [(500, 100, 1), (100, 90, 5)],
    "Fargo": [(280, 100, 1), (380, 100, 1)],
    "Guilty": [(600, 100, 1), (300, 85, 1)],
    "Beigis": [(500, 90, 1), (600, 100, 1)],
    "Mammon": [(320, 85, 1), (480, 100, 3), (50, 100, 8)],
}


def parse_monsters():
    """Monster and boss rows from research/enemies/README.md: set, name, element, HP, agility, defense, attack."""
    path = os.path.join(RESEARCH, "enemies", "README.md")
    elements = {"Fire": FIRE, "Earth": EARTH, "Water": WATER, "Wind": WIND, "None": NONE}
    monsters, bosses = {}, {}
    row = re.compile(r"^\| (\d|Boss) \| (\d+) \| ([^|]+?) \| (\w+) \| (\d+) \| (\d+) \| (\d+) \| (\d+) \|")
    for line in open(path, encoding="utf-8"):
        m = row.match(line)
        if not m:
            continue
        s, _, name, elem, hp, agi, df, atk = m.groups()
        rec = dict(name=name.strip(), elem=elements.get(elem, NONE), hp=int(hp), agi=int(agi), df=int(df),
                   atk=int(atk))
        if s == "Boss":
            bosses[rec["name"]] = rec
        else:
            monsters.setdefault(int(s), []).append(rec)
    return monsters, bosses


MONSTERS, BOSSES = parse_monsters()

# ---- Spells ----
# (school, tier, name, base, hits, kind, area, extra)
# area: single, cone (up to 3 in front), near (target + others nearby), all, self
S = []


# Every spell has its own MP cost (playtest decision, 2026-10-22: not vanilla's 1 MP a press). Defaults by tier;
# spells below override them (cheap utility, expensive finishers). Proposal, to tune.
MP_BY_TIER = {1: 2, 2: 4, 3: 7, 4: 10, 5: 14}


def spell(school, t, name, base, hits, kind, area="single", mp=None, **extra):
    S.append(dict(school=school, tier=t, name=name, base=base, hits=hits, kind=kind, area=area,
                  mp=MP_BY_TIER[t] if mp is None else mp, **extra))


# Fire: kindle sets Burn, stoke makes it hotter, spread throws it, clashes set it off.
spell(FIRE, 1, "Ember", 200, 1, "kindle", stacks=1)
spell(FIRE, 1, "Twin Flame", 115, 2, "kindle", stacks=1, mp=3)
spell(FIRE, 2, "Burst", 180, 1, "kindle", "near", stacks=1)
spell(FIRE, 2, "Bellows", 120, 1, "stoke")
spell(FIRE, 2, "Scald", 150, 1, "scald", "cone")
spell(FIRE, 3, "Blaze", 320, 1, "kindle", stacks=2)
spell(FIRE, 3, "Flare Up", 150, 1, "flare")
spell(FIRE, 3, "Blast", 220, 1, "kindle", "near", stacks=2, wide=True)
spell(FIRE, 3, "Pyre", 0, 1, "cash", mult=1.0)
spell(FIRE, 4, "Firestorm", 100, 1, "stoke", "all")
spell(FIRE, 4, "Detonate", 100, 1, "detonate", "near")
spell(FIRE, 5, "Meteor", 480, 1, "kindle", "near", stacks=2, wide=True, hot=True, mp=16)
spell(FIRE, 5, "Searing Steam", 0, 1, "cash", mult=3.0, mp=12)
# Earth: heavy hits that Settle Brian; guards; shields by force. Bases about 1.5x the other schools'.
spell(EARTH, 1, "Rock", 430, 1, "heavy", crack=1)
spell(EARTH, 1, "Boulder", 520, 1, "heavy", crack=2, mp=3)
spell(EARTH, 2, "Stoneskin", 0, 1, "guard", "self", reduce=0.3, turns=3)
spell(EARTH, 2, "Molten Rock", 480, 1, "heavy", crack=1, burn=1)
spell(EARTH, 2, "Sandblast", 110, 4, "multi", settle=False)
spell(EARTH, 3, "Megalith", 900, 1, "heavy", crack=1, settle_turns=2, mp=10)
spell(EARTH, 3, "Mudshot", 600, 1, "heavy", crack=1, frost=True)
spell(EARTH, 3, "Quagmire", 60, 1, "chill", "near", wide=True, steps=1)
spell(EARTH, 3, "Avalanche", 560, 1, "heavy", "cone", crack=1)
spell(EARTH, 4, "Fissure", 40, 1, "fissure", "near")
spell(EARTH, 4, "Landslide", 620, 1, "heavy", "cone", crack=1)
spell(EARTH, 5, "Eruption", 820, 1, "heavy", "near", crack=1, burn=1, mp=16)
# Water: Frost into Freeze, guards, weak heals.
spell(WATER, 1, "Spout", 280, 1, "spout")
spell(WATER, 1, "Mend", 0, 1, "heal", "self", heal=0.22, mp=3)
# Wellspring (balance pass, 2026-10-22, playtest: "taking a turn to use water should be considered"): spend the turn
# drawing water to refill MP. Boost adds 10% of max MP a charge. Only in the game when CONFIG["wellspring"] > 0.
spell(WATER, 1, "Wellspring", 0, 1, "well", "self", mp=0)
spell(WATER, 2, "Ice Wall", 0, 1, "wall", "self")
spell(WATER, 2, "Soothe", 0, 1, "heal", "self", heal=0.15, mp=3)
spell(WATER, 2, "Icicle", 360, 1, "chill", steps=1)
spell(WATER, 2, "Glacier", 120, 1, "chill", "near", steps=1)
spell(WATER, 3, "Hailstorm", 70, 6, "chill", "near", steps=1, multi=True)
spell(WATER, 3, "Ice Shards", 140, 3, "chill", "spread3", steps=1)
spell(WATER, 3, "Whiteout", 0, 1, "fog", "all")
spell(WATER, 3, "Frost Nova", 90, 1, "chill", "all", steps=1)
spell(WATER, 4, "Renew", 0, 1, "regen", "self", heal=0.12, mp=8)
spell(WATER, 4, "Shatter", 720, 1, "shatter")
spell(WATER, 4, "Undertow", 200, 1, "chill", steps=2)
spell(WATER, 5, "Absolute Zero", 0, 1, "freeze", mp=12)
spell(WATER, 5, "Deluge", 500, 1, "chill", "all", steps=1, mp=16)
# Wind: many hits; any press of a Wind spell counts for shields; tempo and defense.
spell(WIND, 1, "Wind Blade", 150, 2, "multi")
spell(WIND, 1, "Shear", 140, 3, "multi", "cone", mp=3)
spell(WIND, 1, "Fan the Flames", 90, 3, "fan", presses_elements=(WIND, FIRE))
spell(WIND, 2, "Windchill", 100, 3, "multi", chill=True, presses_elements=(WIND, WATER))
spell(WIND, 2, "Tempest", 110, 6, "multi", any_chance=0.1)
spell(WIND, 3, "Mirage", 0, 1, "mirage", "self", mp=5)
spell(WIND, 4, "Whirlwind", 100, 4, "multi", "all")
spell(WIND, 5, "Confusion", 0, 1, "confuse", "all", presses_elements=(WIND, EARTH, WATER))
spell(WIND, 5, "Downburst", 800, 1, "downburst", presses_elements=(WIND, EARTH, EARTH), mp=14)
spell(WIND, 5, "Hurricane", 120, 6, "multi", "all", any_chance=1.0, mp=16)


def tier(level):
    return sum(1 for t in TIER_LEVEL if level >= t)


def power(base, elem, lv):
    L = lv[elem] + (lv[OPP[elem]] >> 4) + (lv[SIDES[elem][0]] >> 3) + (lv[SIDES[elem][1]] >> 3)
    return base * 0.02 * (1 + 0.2 * (1.03 ** L - 1) / 0.03)


def element_mult(atk_elem, def_elem):
    if def_elem == NONE:
        return CONFIG["none_mult"]
    if atk_elem == def_elem:
        return 0.5
    if OPP[atk_elem] == def_elem:
        return 1.25
    return 1.0


def weakness(elem):
    return OPP[elem] if elem != NONE else None


class Foe:
    def __init__(self, rec, hp, atk, boss=False, spells=None, boss_no=0):
        self.name, self.elem, self.df, self.agi = rec["name"], rec["elem"], rec["df"], rec["agi"]
        self.hp = self.max_hp = hp
        self.atk = atk
        self.boss, self.spells, self.boss_no = boss, spells, boss_no
        self.weak = weakness(self.elem)
        shields = CONFIG["boss_shields"][boss_no - 1] if boss and CONFIG["boss_shields"] else 6 + boss_no // 2
        self.shields = self.max_shields = shields if boss else (2 if hp < 60 else 3 if hp < 200 else 4)
        self.broken = 0
        self.stacks, self.burn_left, self.hot = [], 0, 0
        self.frost, self.freeze, self.freeze_immune = 0, 0, 0
        self.def_down = 0
        self.confused = 0
        self.skip = False

    def alive(self):
        return self.hp > 0


class Battle:
    def __init__(self, build, beat, foes, rng, items):
        self.rng = rng
        self.beat = beat
        self.lv = build_levels(build, beat)
        self.usable = [w > 0 for w in build]
        self.foes = foes
        self.hp = self.max_hp = BRIAN_HP[beat]
        self.mp = self.max_mp = BRIAN_MP[beat]
        self.df, self.agi = BRIAN_DEF[beat], BRIAN_AGI[beat]
        self.wind_school = build[WIND] > 0
        self.charge, self.spent_last = 0, False
        self.settled = 0
        self.guard, self.guard_turns = 0.0, 0
        self.wall = 0
        self.fog = 0
        self.mirage = 0
        self.regen, self.regen_turns = 0, 0
        self.items = items
        self.dmg_mult, self.taken_mult = 1.0, 1.0
        if CONFIG["kit"]:
            self.items = KIT_ITEMS[beat] if items else 0
            self.mp = self.max_mp = round(self.max_mp * KIT_MP[beat])
            self.dmg_mult, self.taken_mult = KIT_DMG[beat], KIT_TAKEN[beat]
        self.calibrate()
        self.turns = self.enemy_actions = self.breaks = 0
        self.hp_lost = self.mp_used = 0
        self.low_hp = self.max_hp   # the lowest Brian's HP got (danger)
        self.items_used = 0
        known = [dict(s, **CONFIG["spells"].get(s["name"], {})) for s in S]
        self.spells = [s for s in known if self.usable[s["school"]] and tier(self.lv[s["school"]]) >= s["tier"]
                       and s["kind"] != "well"]
        if CONFIG["wellspring"] and (self.usable[WATER] or CONFIG["wellspring_all"]):
            self.spells.append(next(s for s in known if s["kind"] == "well"))
        self.wells = self.staff_turns = 0
        self.casts = {}

    def calibrate(self):
        """Scale each enemy's damage so its average action costs the design share of Brian's HP."""
        for f in self.foes:
            if f.boss:
                L = f.atk
                avg = sum(base * 0.02 * (1 + 0.2 * (1.03 ** L - 1) / 0.03) * f.atk / (f.atk + self.df) * shots * acc / 100
                          for base, acc, shots in f.spells) / len(f.spells)
                f.dmg_scale = BOSS_HIT * CONFIG["boss_hit"][self.beat] * self.max_hp / avg
            else:
                avg = f.atk * f.atk / (f.atk + self.df)
                f.dmg_scale = MONSTER_HIT * CONFIG["pack_hit"][self.beat] * self.max_hp / max(avg, 0.1)

    # -- helpers --
    def alive_foes(self):
        return [f for f in self.foes if f.alive()]

    def hit_roll(self, atk_agi, def_agi):
        return self.rng.random() < atk_agi / (atk_agi + def_agi / 8)

    def dmg_to(self, foe, base, school):
        A = sum(self.lv)
        d = power(base, school, self.lv) * A / (A + foe.df * (1.4 if foe.freeze else 1.0) * (0.7 if foe.def_down else 1.0))
        d *= element_mult(school, foe.elem)
        if foe.broken:
            d *= 2
        if school == EARTH and self.settled:
            d *= 0.6
        d *= self.dmg_mult
        return d + self.rng.random() * math.sqrt(max(d, 0))

    def burn_from(self, d, school, foe):
        """A Burn stack from a hit: 20% of it, or of what it would have dealt ignoring element (burn_full)."""
        if CONFIG["burn_full"]:
            d = d / element_mult(school, foe.elem)
        return d * 0.2

    def crack(self, foe, n):
        if foe.broken or n <= 0 or not foe.alive():
            return
        foe.shields -= n
        if foe.shields <= 0:
            foe.broken, foe.skip = 2, True
            self.breaks += 1

    def targets(self, spell, main):
        alive = self.alive_foes()
        if not alive:
            return []
        others = [f for f in alive if f is not main]
        area = spell["area"]
        if area == "single":
            return [main]
        if area == "cone":
            return [main] + [f for f in others if self.rng.random() < 0.6]
        if area == "near":
            p = 0.75 if spell.get("wide") else 0.5
            return [main] + [f for f in others if self.rng.random() < p]
        if area == "spread3":
            return alive[:3]
        if area == "all":
            return alive
        return []

    def weak_hit(self, spell, foe):
        """A weakness hit: the spell's element, or for Wind spells any of its presses."""
        if foe.weak is None:
            return False
        if spell["school"] == foe.weak:
            return True
        return spell["school"] == WIND and foe.weak in spell.get("presses_elements", (WIND,))

    # -- one Brian turn --
    def brian_turn(self):
        self.turns += 1
        if not self.spent_last:
            self.charge = min(5, self.charge + 1)
        self.spent_last = False
        if self.regen_turns:
            self.heal(self.regen)
            self.regen_turns -= 1
        action = self.choose()
        self.act(action)
        if self.settled:
            self.settled -= 1

    def heal(self, amount):
        self.hp = min(self.max_hp, self.hp + amount)

    def boost(self):
        # With finishers in the game Brian banks Charge for his tier-5 spell instead of Boosting everything.
        if CONFIG["finisher"] and any(s["tier"] == 5 for s in self.spells):
            return 0
        return min(3, self.charge)

    def choose(self):
        # Low HP: heal (Water spells first, then an item if the scenario allows).
        if self.hp < self.max_hp * 0.35:
            heals = [s for s in self.spells if s["kind"] in ("heal", "regen") and self.mp >= s["mp"]]
            if heals:
                return ("spell", max(heals, key=lambda s: s.get("heal", 0)))
            if self.items > 0:
                return ("item", None)
        best, best_v = ("staff", None), self.staff_value()
        for s in self.spells:
            if self.mp < s["mp"] or not self.can_finish(s):
                continue
            v = self.value(s)
            if v > best_v:
                best, best_v = ("spell", s), v
        return best

    def can_finish(self, s):
        return s["tier"] < 5 or self.charge >= CONFIG["finisher"]

    def staff_damage(self, foe):
        T = sum(self.lv)
        p = (T / 2 + sum(min(l, T / 4) for l in self.lv))
        A = T
        d = p * A / (A + foe.df) * (2 if foe.broken else 1)
        return d

    def staff_value(self):
        alive = self.alive_foes()
        return self.staff_damage(alive[0]) * 0.8 if alive else 0

    def main_target(self):
        alive = self.alive_foes()
        # focus the weakest-to-us / lowest HP foe
        return min(alive, key=lambda f: f.hp)

    def threat(self):
        """What one enemy action is worth avoiding, in damage terms (a stand-in for safety)."""
        alive = self.alive_foes()
        return sum(f.max_hp for f in alive) * 0.05

    def value(self, s):
        main = self.main_target()
        b = self.boost() if s["kind"] not in ("heal", "regen") else 0
        kind = s["kind"]
        school = s["school"]
        n_targets = {"single": 1, "cone": 1 + 0.6 * (len(self.alive_foes()) - 1),
                     "near": 1 + 0.6 * (len(self.alive_foes()) - 1), "all": len(self.alive_foes()),
                     "spread3": min(3, len(self.alive_foes())), "self": 0}[s["area"]]
        per = self.dmg_to(main, s["base"], school) if s["base"] else 0
        v = 0.0
        if kind == "heavy":
            v = per * n_targets * (1 + 0.25 * b)
            if s.get("settle_turns") and self.settled:
                v *= 0.5
        elif kind == "kindle":
            stacks = s["stacks"] + b
            room = max(0, 3 - len(main.stacks))
            v = per * n_targets + min(room, stacks) * per * 0.2 * 3 * n_targets
        elif kind in ("stoke", "flare"):
            burning = [f for f in self.alive_foes() if f.stacks] if s["area"] == "all" else ([main] if main.stacks else [])
            if not burning:
                return 0
            v = per + sum(sum(f.stacks) for f in burning) * (0.5 + 0.25 * b) * 3
        elif kind == "cash":
            if not main.stacks:
                return 0
            v = sum(main.stacks) * main.burn_left * (s["mult"] * (1 + 0.2 * b) - 1)
        elif kind == "detonate":
            if not main.stacks or len(self.alive_foes()) < 2:
                return 0
            v = sum(main.stacks) * 3 * (len(self.alive_foes()) - 1) * 0.6
        elif kind == "scald":
            v = per * n_targets + sum(sum(f.stacks) for f in self.alive_foes() if f.stacks)
        elif kind == "spout":
            v = per * (1 + 0.2 * b) + (0.3 * self.threat() if b else 0)
        elif kind == "chill":
            v = per * n_targets * (6 if s.get("multi") else 1) / (6 if s.get("multi") else 1) * s["hits"]
            v += 0.5 * self.threat() * n_targets if not main.freeze_immune and not main.freeze else 0
        elif kind == "shatter":
            if not main.freeze:
                return 0
            v = per / 0.7 * (1 + 0.25 * b)
        elif kind == "freeze":
            if main.freeze or main.freeze_immune:
                return 0
            v = 2.0 * self.threat()
        elif kind == "multi" or kind == "fan":
            n = s["hits"] * (1 + 0.4 * b)
            v = per * n * n_targets * (1.6 if kind == "fan" and main.stacks else 1)
        elif kind == "downburst":
            v = per * (1 + 0.25 * b)
        elif kind == "fissure":
            v = per * n_targets + 0.5 * self.threat() * n_targets
        elif kind in ("guard", "wall", "fog", "mirage"):
            if (kind == "guard" and self.guard_turns) or (kind == "fog" and self.fog) or (kind == "mirage" and self.mirage) or (kind == "wall" and self.wall):
                return 0
            v = self.threat() * (1.2 if self.hp < self.max_hp * 0.6 else 0.4)
        elif kind == "confuse":
            v = self.threat() * len(self.alive_foes()) * (1 + b) * 0.8
        elif kind in ("heal", "regen"):
            v = 0
        elif kind == "well":
            return self.well_value()
        # shield value
        if kind not in ("heal", "regen", "guard", "wall", "fog", "mirage", "confuse"):
            if self.weak_hit(s, main) or s.get("any_chance") or kind in ("downburst", "fissure"):
                v += (1.0 / max(main.shields, 1)) * (self.threat() + per)
        return v

    def well_value(self):
        """A Wellspring turn is worth the damage the MP it brings back buys later, at the best rate Brian has."""
        if self.mp > self.max_mp * 0.4:
            return 0
        gain = min(self.max_mp - self.mp, self.max_mp * (CONFIG["wellspring"] + 0.1 * self.boost()))
        rates = [self.value(s) / s["mp"] for s in self.spells if s["mp"] and s["kind"] not in ("heal", "regen")]
        rate = max(rates, default=0)
        return gain * rate * 0.6

    def act(self, choice):
        kind, s = choice
        alive = self.alive_foes()
        if not alive:
            return
        if kind == "staff":
            self.staff_turns += 1
            f = alive[0]
            if self.hit_roll(self.agi, f.agi):
                f.hp -= self.staff_damage(f)
            self.mp = min(self.max_mp, self.mp + 2)   # Brian's own staff restores MP on a hit
            return
        if kind == "item":
            self.items -= 1
            self.items_used += 1
            self.heal(self.max_hp * 0.4)
            return
        self.casts[s["name"]] = self.casts.get(s["name"], 0) + 1
        self.mp -= s["mp"]
        self.mp_used += s["mp"]
        b = self.boost() if s["kind"] not in ("heal", "regen", "freeze") else 0
        if CONFIG["finisher"] and s["tier"] == 5:
            self.charge, b = 0, 0
            self.spent_last = True
        if b:
            self.charge -= b
            self.spent_last = True
        main = self.main_target()
        school = s["school"]
        k = s["kind"]
        if k == "well":
            self.wells += 1
            self.mp = min(self.max_mp, self.mp + self.max_mp * (CONFIG["wellspring"] + 0.1 * b))
            return
        if k == "heal":
            self.heal(self.max_hp * s["heal"] * (1 + 0.1 * b))
            return
        if k == "regen":
            self.regen, self.regen_turns = self.max_hp * s["heal"], 3
            return
        if k == "guard":
            self.guard, self.guard_turns = s["reduce"], s["turns"] + b
            return
        if k == "wall":
            self.wall = 1 + b
            return
        if k == "fog":
            self.fog = 2 + b
            return
        if k == "mirage":
            self.mirage = 2 + b
            return
        if k == "confuse":
            for f in self.alive_foes():
                if f.boss:
                    f.skip = True
                else:
                    f.confused = 1 + b
            return
        if k == "freeze":
            if not main.freeze_immune:
                main.freeze = 1 if main.boss else 2 + b
            return
        targets = self.targets(s, main)
        for f in targets:
            if not self.hit_roll(self.agi, f.agi):
                continue
            if k == "heavy":
                f.hp -= self.dmg_to(f, s["base"], school) * (1 + 0.25 * b)
                if self.weak_hit(s, f):
                    self.crack(f, s["crack"])
                if s.get("burn"):
                    self.add_burn(f, self.burn_from(self.dmg_to(f, s["base"], school), school, f))
                if s.get("frost"):
                    self.chill(f, 1)
            elif k == "kindle":
                d = self.dmg_to(f, s["base"], school)
                f.hp -= d * s["hits"]
                for _ in range(s["stacks"] + b):
                    self.add_burn(f, self.burn_from(d, school, f))
                if s.get("hot"):
                    f.hot = 3
                if self.weak_hit(s, f):
                    self.crack(f, 1)
            elif k == "stoke":
                if f.stacks:
                    f.hp -= self.dmg_to(f, s["base"], school)
                    f.hot = 3 + b
            elif k == "flare":
                f.hp -= self.dmg_to(f, s["base"], school)
                f.stacks = [x * (1.5 + 0.25 * b) for x in f.stacks]
                f.burn_left = 3
            elif k == "cash":
                f.hp -= sum(f.stacks) * f.burn_left * s["mult"] * (1 + 0.2 * b)
                f.stacks, f.burn_left = [], 0
            elif k == "detonate":
                for o in self.alive_foes():
                    if o is not f and self.rng.random() < 0.6:
                        for x in f.stacks:
                            self.add_burn(o, x)
            elif k == "scald":
                if f.stacks:
                    f.hp -= sum(f.stacks)
                else:
                    self.add_burn(f, self.burn_from(self.dmg_to(f, s["base"], school), school, f))
                f.hp -= self.dmg_to(f, s["base"], school)
                f.freeze, f.frost = 0, 0
            elif k == "spout":
                f.hp -= self.dmg_to(f, s["base"], school) * (1 + 0.2 * b)
                if b:
                    self.chill(f, 1 + (b - 1))
                if self.weak_hit(s, f):
                    self.crack(f, 1)
            elif k == "chill":
                hits = s["hits"]
                f.hp -= self.dmg_to(f, s["base"], school) * hits
                self.chill(f, s["steps"] + b)
                if self.weak_hit(s, f):
                    self.crack(f, 1)
            elif k == "shatter":
                if f.freeze:
                    d = self.dmg_to(f, s["base"], school)
                    if CONFIG["shatter_neutral"]:
                        d /= element_mult(school, f.elem)
                    f.hp -= d / 0.7 * (1 + 0.25 * b)
                    f.freeze, f.frost = 0, 0
                    f.freeze_immune = 2 if f.boss else 0
            elif k in ("multi", "fan"):
                n = int(round(s["hits"] * (1 + 0.4 * b)))
                for _ in range(n):
                    d = self.dmg_to(f, s["base"], school)
                    if k == "fan" and f.stacks:
                        d *= 1.6
                    f.hp -= d
                    if k == "fan":
                        self.add_burn(f, self.burn_from(d, FIRE, f), turns=1)
                    if s.get("chill"):
                        self.chill(f, 1) if self.rng.random() < 0.4 else None
                    if self.weak_hit(s, f):
                        self.crack(f, 1)
                    elif s.get("any_chance") and self.rng.random() < s["any_chance"]:
                        self.crack(f, 1)
            elif k == "downburst":
                f.hp -= self.dmg_to(f, s["base"], school) * (1 + 0.25 * b)
                self.crack(f, 2)
            elif k == "fissure":
                f.hp -= self.dmg_to(f, s["base"], school)
                self.crack(f, 2 if b >= 3 else 1)
        if school == EARTH and s.get("settle", True) and k == "heavy":
            self.settled = s.get("settle_turns", 1) + 1   # covers the next turn (decremented at turn end)

    def add_burn(self, f, amount, turns=3):
        if f.frost or f.freeze:
            f.frost = f.freeze = 0   # Fire thaws
        if len(f.stacks) < 3:
            f.stacks.append(amount)
        f.burn_left = max(f.burn_left, turns)

    def chill(self, f, steps):
        if f.stacks:
            f.stacks, f.burn_left = [], 0   # Water puts Burn out
        level = f.frost + (1 if f.freeze else 0) + steps
        if level >= 2 and not f.freeze_immune:
            f.freeze = 1 if f.boss else 2
            f.frost = 0
        else:
            f.frost = 1

    # -- the enemies' turn --
    def enemies_turn(self):
        for f in self.alive_foes():
            # Burn ticks
            if f.stacks and f.burn_left:
                f.hp -= sum(f.stacks) * (1.5 if f.hot else 1.0)
                f.burn_left -= 1
                if f.burn_left == 0:
                    f.stacks = []
            if f.hot:
                f.hot -= 1
        for f in self.alive_foes():
            if f.skip:
                f.skip = False
            elif f.freeze:
                f.freeze -= 1
                if f.freeze == 0 and f.boss:
                    f.freeze_immune = 2
            elif f.confused:
                f.confused -= 1
                others = [o for o in self.alive_foes() if o is not f]
                if others:
                    others[0].hp -= MONSTER_HIT * self.max_hp * 2   # a confused foe hits an ally
            else:
                if f.frost and not f.boss and self.rng.random() < 0.3:
                    pass    # too slow to reach Brian
                else:
                    self.enemy_attack(f)
                    if f.freeze_immune:
                        f.freeze_immune -= 1
            if f.broken:
                f.broken -= 1
                if f.broken == 0:
                    if f.boss:
                        f.max_shields += 1
                        if f.name == "Beigis" and CONFIG["beigis_shift"]:
                            f.elem = self.rng.randrange(4)   # any element, 25% each
                            f.weak = weakness(f.elem)
                    f.shields = f.max_shields
            if f.frost and self.rng.random() < 0.3:
                f.frost = 0
            if f.def_down:
                f.def_down -= 1
        for t in ("guard_turns", "wall", "fog", "mirage"):
            if getattr(self, t):
                setattr(self, t, getattr(self, t) - 1)
        if not self.guard_turns:
            self.guard = 0

    def enemy_attack(self, f):
        self.enemy_actions += 1
        acc_mult = (0.6 if self.fog else 1.0) * (0.5 if self.mirage else 1.0) * (0.95 if self.wind_school else 1.0)
        if f.boss:
            base, acc, shots = self.rng.choice(f.spells)
            if self.wall:
                return      # Ice Wall stops a projectile
            L = f.atk
            p = base * 0.02 * (1 + 0.2 * (1.03 ** L - 1) / 0.03)
            for _ in range(shots):
                if self.rng.random() * 100 < acc * acc_mult and self.hit_roll(f.agi, self.agi):
                    d = p * f.atk / (f.atk + self.df) * f.dmg_scale * self.taken_mult
                    d *= 0.75 + 0.5 * self.rng.random()
                    d *= (1 - self.guard)
                    self.hp -= d
                    self.hp_lost += d
                    self.low_hp = min(self.low_hp, self.hp)
        else:
            if self.rng.random() < MONSTER_ACT_CHANCE and self.rng.random() < acc_mult and self.hit_roll(f.agi, self.agi):
                d = f.atk * f.atk / (f.atk + self.df) * f.dmg_scale * self.taken_mult
                d *= 0.75 + 0.5 * self.rng.random()
                d *= (1 - self.guard)
                self.hp -= d
                self.hp_lost += d
                self.low_hp = min(self.low_hp, self.hp)


def build_levels(build, beat):
    """Brian's four element levels: 1 each, plus the beat's gained levels spread by the build's weights."""
    total = LEVELS_GAINED[beat]
    weights = [w if w > 0 else 0.15 for w in build]
    lv = [1 + round(total * w / sum(weights)) for w in weights]
    for i in range(4):
        if lv[i] > LEVEL_CAP:
            spill, lv[i] = lv[i] - LEVEL_CAP, LEVEL_CAP
            for k in sorted(range(4), key=lambda k: -build[k]):
                if k != i and lv[k] < LEVEL_CAP:
                    lv[k] = min(LEVEL_CAP, lv[k] + spill)
                    break
    return lv


def boss_fight(build, beat, rng, items):
    name = BOSS_ORDER[beat]
    rec = BOSSES[name]
    atk = round(rec["atk"] * BOSS_ATTACK_SCALE[beat])
    foe = Foe(rec, round(BOSS_HP[beat] * CONFIG["boss_hp"][beat]), atk, boss=True, spells=BOSS_SPELLS[name], boss_no=beat + 1)
    return Battle(build, beat, [foe], rng, items)


def pack_fight(build, beat, rng, items):
    pool = MONSTERS[PACK_SET[beat]]
    scale = BOSS_HP[beat] / VANILLA_BOSS_HP[beat]   # same rescale as the bosses get
    n = rng.choice([2, 3, 3, 4])
    foes = []
    for _ in range(n):
        rec = rng.choice(pool)
        foes.append(Foe(rec, max(1, round(rec["hp"] * scale * 1.3 * CONFIG["pack_hp"])), rec["atk"]))
    return Battle(build, beat, foes, rng, items)


def play(battle, max_turns=80):
    while battle.alive_foes() and battle.hp > 0 and battle.turns < max_turns:
        battle.brian_turn()
        if not battle.alive_foes():
            break
        battle.enemies_turn()
    won = not battle.alive_foes() and battle.hp > 0
    return dict(won=won, turns=battle.turns, enemy_actions=battle.enemy_actions,
                hp_lost=min(1.0, battle.hp_lost / battle.max_hp), mp_used=battle.mp_used / battle.max_mp,
                breaks=battle.breaks, wells=battle.wells, staff=battle.staff_turns,
                low_hp=max(0.0, battle.low_hp / battle.max_hp), items_used=battle.items_used, casts=battle.casts)


BUILDS = {
    "Fire": [8, 1, 1, 1], "Earth": [1, 8, 1, 1], "Water": [1, 1, 8, 1], "Wind": [1, 1, 1, 8],
    "Fire/Earth": [6, 4, 0, 0], "Fire/Water": [6, 0, 4, 0], "Fire/Wind": [6, 0, 0, 4],
    "Earth/Fire": [4, 6, 0, 0], "Earth/Water": [0, 6, 4, 0], "Earth/Wind": [0, 6, 0, 4],
    "Water/Fire": [4, 0, 6, 0], "Water/Earth": [0, 4, 6, 0], "Water/Wind": [0, 0, 6, 4],
    "Wind/Fire": [4, 0, 0, 6], "Wind/Earth": [0, 4, 0, 6], "Wind/Water": [0, 0, 4, 6],
}


def summarize(results):
    def pct(values, q):
        values = sorted(values)
        return values[min(len(values) - 1, int(q * len(values)))]
    won = [r for r in results if r["won"]]
    out = dict(win_rate=len(won) / len(results))
    casts = {}
    for r in results:
        for name, n in r["casts"].items():
            casts[name] = casts.get(name, 0) + n
    total = sum(casts.values()) or 1
    out["casts"] = {k: round(v / total, 3) for k, v in sorted(casts.items(), key=lambda kv: -kv[1])[:6]}
    for key in ("turns", "enemy_actions", "hp_lost", "mp_used", "breaks", "wells", "staff", "low_hp", "items_used"):
        vals = [r[key] for r in results]
        out[key] = dict(median=pct(vals, 0.5), p10=pct(vals, 0.1), p90=pct(vals, 0.9),
                        mean=sum(vals) / len(vals))
    return out


def run(runs=300, items=0, seed=1, config=None, builds=None):
    CONFIG.clear()
    CONFIG.update(DEFAULT_CONFIG, **(config or {}))
    rng = random.Random(seed)
    data = []
    for label, build in BUILDS.items():
        if builds and label not in builds:
            continue
        for beat in range(8):
            for kind, maker in (("boss", boss_fight), ("pack", pack_fight)):
                results = [play(maker(build, beat, rng, items)) for _ in range(runs)]
                data.append(dict(build=label, beat=beat, boss=BOSS_ORDER[beat], fight=kind, items=items,
                                 levels=build_levels(build, beat), **summarize(results)))
    return data


if __name__ == "__main__":
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 300
    print(json.dumps({name: run(runs, items=3, seed=7, config=cfg) for name, cfg in SCENARIOS.items()}))
