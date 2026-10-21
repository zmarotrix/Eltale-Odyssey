# Balance model (first pass, 2026-10-21)

Status: paper model plus in-game checks of the numbers in RAM. Nobody has played a full run with it yet, so
every number here is a starting point for playtests. The code is `mod/src/balance.c` (spells, enemy and boss
scaling), `combat.c` (Boost, gear and talent percentages), `status.c` (Burn) and `equipment.c` (gear).

## The goal

The user asked for the same difficulty curve as vanilla, or slightly harder to pay for the new strategy, and
for nothing to trivialise the game. So the yardstick is **turns to kill**: a fight should take about as many
of Brian's turns as it did in vanilla for a player using the best option they had, or a little more.

Vanilla's best option is simple: cast the strongest spell Brian has, every turn. Everything the overhaul adds
(gear, talents, Spirit Charge and Boost, Break, Burn, stoking) multiplies that. Enemy HP is scaled by about
that multiplier, a bit more, so the same fight takes the same number of turns or one more.

## How damage works (for the maths below)

Spell power is `base x 0.02 x (1 + 0.2 x sum of 1.03^i over Brian's element levels)`, then
`damage = power x A / (A + defense) + random(sqrt(damage))`. Opposite element x1.25, same element x0.5. So
damage is **linear in the spell's base and in every percentage the mod adds**, which makes the model easy:
multiply. All the mod's bonuses are added together as percentages first, then applied once (no stacking of
multipliers on multipliers), except Break (+100%, double), stoking (70% / 140%), Tidal Echo and Boost, which multiply.

## What an optimised Brian deals per turn, against vanilla's best

Rough figures for a player who commits to one style and owns that beat's gear. Averaged over a typical
fight, including the turns spent building charge or Burn and the turns where the enemy is Broken (double damage; was +50% until a playtest asked for a harder choice between healing and attacking in the Break window: about +10% on the averages below, which the HP margins absorb).

| Style | Beat 1 (Solvaring) | Beat 2 (Zelse) | Beat 4 (Shilf) | End (Mammon) | Where it comes from |
|---|---|---|---|---|---|
| Pyromancer | 1.1x | 1.4x | 1.9x | 2.1x | Fire spells x0.83, then Burn: up to 3 stacks (4 with Inferno) at 20% of the kindling hit per turn (+5% per Fanned Flames rank); Ember Heart and Phoenix Staff get stacks up faster |
| Stonewall | 1.15x | 1.5x | 2.0x | 2.2x | Earth x0.85, Boost +40% per charge (+10% Aftershock), Stone Memory up to +30%, Mountain Staff breaks two shields a hit, so Break comes about twice as often |
| Tidecaller | 1.0x | 1.2x | 1.5x | 1.7x | Water x0.78, Tidal Echo 2 x 65% = 1.3x, Leviathan 1.25x. Lower damage, paid back in healing |
| Tempest | 1.15x | 1.5x | 2.0x | 2.2x | Wind Cutter 150 x 2 shots; Boost adds 40% more shots per charge (+10% per Crosswind rank, +10% Skyreed); Gale Tether one more shot |
| Staff Brawler | 1.1x | 1.4x | 1.8x | 2.0x | Titan Staff +30 swing power and a shield crack on the first swing, Spellstrike a charge, Heavy Staff early |
| Elementalist | 1.1x | 1.4x | 1.8x | 2.0x | Always hitting the opposite element (x1.25), Prism Staff +10% all, Prism Sigil +20% on each change of element |
| Glass Charm (any) | +25% | +25% | +25% | +25% | ...and Brian takes 25% more. A wild card for players who want shorter, riskier fights |

Enemies were scaled a little above the top of each column.

## Enemy scaling

Monster sets are copied fresh from the ROM each time a map loads (`enemy_set_load`, `0x80008EF4`); the mod
scales the copy right after. Boss records stay in RAM from boot and are scaled once.

| Monster set (research/enemies) | Reached | HP | Attack |
|---|---|---|---|
| 0 Holy Plains, Connor Forest | beat 1 | x1.20 | x1.00 |
| 1 Dondoran Flats and the long road | beat 2 | x1.50 | x1.05 |
| 4 Normoon, Isle of Skye | beats 2-3 | x1.65 | x1.10 |
| 2 Carmagh caves, Limelin | beats 2 and 4 | x1.75 | x1.10 |
| 3 the desert, Boil Hole, Brannoch | beats 4-7 | x1.90 | x1.15 |
| 5 Mammon's World | beat 8 | x2.10 | x1.20 |

A set covers several beats, so its multiplier sits near the middle of the beats where it is fought most.
Early visits to a set are a little harder than the curve; late visits a little easier.

| Boss | Vanilla HP | Now | Attack |
|---|---|---|---|
| Solvaring | 200 | 240 | x1.00 |
| Zelse | 680 | 1054 | x1.05 |
| Nepty | 880 | 1496 | x1.10 |
| Shilf | 1000 | 1800 | x1.10 |
| Fargo | 1500 | 2850 | x1.15 |
| Guilty | 1800 | 3600 | x1.15 |
| Beigis | 1900 | 3895 | x1.20 |
| Mammon | 2300 | 4945 | x1.20 |

Checked in game: the boss records read back exactly these values after boot, and a monster set reloaded by
warping back to the same map keeps x1.2 (it does not stack).

Zelse keeps his vanilla spike over Solvaring (x4.4 HP now, x3.4 before): the user likes that spike because it
forces early specialisation. Attack rises late because the new robes (Knight's Mantle 16, Dragon Robe 24),
Bedrock and Stoneskin raise Brian's defense more than vanilla ever did.

Monster HP is the record's `+0x06` (max HP, which is also the start MP); `+0x04` is an unused copy and is
scaled too so the two stay equal. Attack (`+0x2A`) is also the power of the monster's spells.

## Things that were too strong, and what changed

| Problem | Why it mattered | Change |
|---|---|---|
| Wind Boost gave a flat +2 shots per charge | A 3-charge Wind Cutter Lv1 went from 2 shots to 8 (4x); with Crosswind 3 and Skyreed, 14 (7x). Nothing else came close | +40% of the spell's shots per charge, rounded, at least 1 (+10% per Crosswind rank, +10% Skyreed). 3 charges on 2 shots is now 4-5 |
| Burn 30% per stack | 3 stacks + Fanned Flames dealt about 1.3x the kindling hit every turn, forever. Fire became the best boss damage by a wide margin | 20% per stack (+5% per Fanned Flames rank) |
| Tidal Echo 2 x 75% | Cast twice at 75% is a flat 1.5x on every Water spell, heals included, from one sigil | 2 x 65% = 1.3x |
| Stone Memory up to +45% | Earth is already the burst school; +45% on top of Boost and Mountain Staff was the strongest single bonus | Capped at +30% (two Earth casts in a row) |

## What keeps the game from being trivialised

- **One cast a turn, one charge a turn.** Charge is the throttle on every burst; nothing grants more than one
  extra charge per turn (Spellstrike gives one on the first swing only).
- **Break costs setup.** Shields need the right element (or a staff swing) to crack; Broken lasts one turn.
- **Burn ramps.** It needs a turn per stack (Ember Heart: half that), and the one-status rule stops Poison and
  Burn stacking on the same foe (unless Brian buys Shifting Winds).
- **Sustain is limited.** Only Water heals; shops sell few recovery items a day; drops are 1 in 8.
- **Gear is found, not bought.** The top staves sit deep in late dungeons (Boil Hole, Brannoch, Leonardo's
  section), so the late multipliers arrive with the late enemies.

## What to watch in playtests

- Fights in each beat: count Brian's turns. Target is vanilla's count, or one more. If a style kills in fewer,
  lower that style's bonus, not enemy HP (that would punish the other styles).
- Zelse: an even-split Brian should struggle (intended); a focused one should win in 6-9 turns.
- Tidecaller damage may be too low late. If bosses run past 12 turns, raise Leviathan to 1.35x first.
- Glass Charm in the last set: enemy attack x1.2 plus 25% more taken may be lethal. That is the point, but
  check it is not a one-shot.
- Gale Tether early: +1 shot is +50% on a 2-shot spell at beat 2. Watch Zelse with it.
