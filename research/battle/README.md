# How a Quest 64 battle works (USA 1.0)

Status: 2026-10-20. **Static analysis only**: read from the recompiled code's MIPS comments
(`Quest64-Recomp/RecompiledFuncs/*.c`) and ROM data. Nothing here has been tested in game yet.
All addresses are real RAM addresses (not the byte-swapped ones in the community RAM sheet).

The spell formula below matches the community "Damage Calculator" sheet exactly. Its Water example
(level 65, base 850) gives 677 power and 451 damage against defense 100, and so does this formula.
That's good evidence the reading is right.

## Plain-language summary

**Turns.** A battle alternates Brian with one enemy at a time:
Brian → enemy 1 → Brian → enemy 2 → Brian → enemy 3 → Brian → enemy 1 ...
Brian gets a turn between *every* enemy turn, so more enemies doesn't mean fewer Brian turns.
Agility only decides **who goes first**. If the first enemy's agility is higher than Brian's,
the enemy opens; on a tie Brian goes first. In boss fights Brian always goes first.
A turn ends when the fighter's attack, spell or item has finished, or when Brian presses A with
no enemy in reach, which passes.

**Escaping** isn't a menu. If Brian **ends his turn outside the battle circle** (radius 100 in
random battles), the battle ends as an escape. Boss fights keep Brian inside the circle.
The Escape spell (Water) does the same thing directly.

**Damage** has three steps, the same for spells, staff and enemy attacks:
1. **Power**: for spells, the spell's `baseDamage` grows with the caster's element level. For the
   staff, power comes from all four element levels.
2. **Attack vs defense**: `power × attack ÷ (attack + defense)`. Brian's "attack" is the **sum of
   his four element levels**. An enemy's is its attack stat.
3. **Randomness**: add a random 0..√(damage). The minimum is 1. **There are no critical hits** in
   the USA code (the community sheet marks crits as Japan-only).

Elements only matter when **Brian's spells hit an enemy**. The **same element does half damage**
(fire on fire). **Opposites do 1.25×** (Fire↔Water, Earth↔Wind). Other pairs are normal.
Enemy spells hit Brian without any element modifier, and the staff ignores elements.

**Hitting**: spells first roll their accuracy %. Then every attack, staff or spell, rolls
`100 × attacker agility ÷ (attacker agility + target agility/8)` %. Agility mostly decides dodging.

**Status effects** use one shared system for Brian and every enemy. Each fighter has 12 status
bits plus 12 one-byte turn counters. A status lands with a fixed chance: Brian 62.5% (31% for
Restriction and Silence), enemies on Brian only 25% (12.5%). Self-buffs always land.
A status lasts **2–6 of the target's own turns**, or 3–7 when you buff yourself.
Magic Barrier lasts only 2–3. The counters tick down **at the end of that fighter's own turn**.

**MP cost** = the number of element buttons pressed: 1, 2 or 3 MP. It isn't stored per spell.
A staff hit that connects gives 1 MP back.

**Winning** pays out each defeated enemy's element EXP and a second EXP value. There's a
**1-in-4 chance of an item**, taken from a random enemy in the fight. Bosses always drop theirs.
Stat growth (HP/MP/Agility/Defense) builds up during battle from what Brian does, but the
level-ups happen afterwards, out of battle.
**Escaping** gives nothing. **Losing** (HP reaches 0) gives nothing too. There's no game over:
Brian is sent back to his last rest spot with **1 HP and full MP**, and everything earned in that
fight is lost.

### What this means for the overhaul (hook points)

| Need | Best hook | Why |
|---|---|---|
| Spirit Charge gain "per turn" | `battle_turn_manager` `0x8001CFE8`, at the moment it clears the "turn pending" bit (`0x8001D200`). gBattleState bit `0x2` then says whose turn it is (0 = Brian) | Runs once at the start of every turn, including the first |
| Burn / Poison ticks | `battle_end_turn` `0x8001D358` (arg = the fighter whose turn just ended), next to its call to `status_tick_durations` | That's where the game already ticks status counters |
| "Lose a turn" (Break, Paralysis) | Enemies: `enemy_state_idle` `0x80009378` already skips the turn when status bit `0x8` (Restriction) is set. Brian has no such skip: Restriction only stops his movement (`0x80003CD0`). Add one in `brian_update` `0x80002F60` | Copy the Restriction pattern |
| Shield points / Break on damage | `battle_damage_enemy` `0x8000ACC0` (gets enemy index, damage, element, flags, attacker) | Every hit on an enemy goes through it, staff and spells alike |
| Empowered spells (spend Spirit Charge) | `spell_calc_power` `0x800175D4` (returns power) or the `spellType` table at `0x8004CD1C` | Power is computed in one place per hit |
| New statuses | Bit `0x800` (index 11) is unused, and its handler `0x800175B8` only sets the bit. The u16 has bits 12–15 free, but there are no counter bytes for them, so new statuses are better kept in the mod's own table keyed by fighter | The 12 counter bytes are all taken |

## 1. Turn flow

### The loop

`func_80001800` is the main game loop. Every frame, while `gBattleState & 1` (in battle), it calls
`battle_turn_manager` (`0x8001CFE8`), or `0x8001CACC` during the battle intro (state bit `0x200`).
Then it runs `brian_update` (`0x80002F60`), the enemy update and the spell update `0x800156D0`.

| Function | Proposed name | What it does |
|---|---|---|
| `0x8001C5F4` | `battle_try_random_encounter` | Adds walked distance to `0x8008C574`. Every 50 units it adds 50 to `0x8008C578`, and a battle starts if `rand(2000) < 0x8008C578`. Sets gBattleState = `0x205`, circle radius 100. **Turn order**: compares Brian's Agility (`0x8007BA8C`) with the base Agility of the first enemy in the turn list (`MonsterBaseData+0x0C`). If Brian's is lower, the enemy goes first |
| `0x8001C8C4` | `battle_start_boss` | gBattleState = `0x305` (bit `0x100` = boss), one enemy, Brian always first |
| `0x80003A1C` | `brian_battle_init` | Sets up Brian's battle block (below) and clears the battle EXP pools and statuses |
| `0x8001CFE8` | `battle_turn_manager` | Waits for the 20-frame delay `0x8008C594` and for the next fighter's "busy" flag (`battleData+0x60` bit 0) to clear. Then: no enemies left → victory (bit `0x8`). Brian outside the circle → escape (bit `0x10`). Otherwise the next turn starts: clears bit `0x4`, and on an enemy's turn calls `enemy_turn_begin` |
| `0x8000932C` | `enemy_turn_begin` | Sets `EnemyAction+0x08` bit 0 ("your turn") on the current enemy |
| `0x8001D358` | `battle_end_turn` | Called by whoever just acted. Ticks *that fighter's* statuses (`status_tick_durations`). If Brian just acted, picks the next living enemy (round robin through the list at `0x8007D0B0`). If none are left, sets bit `0x400` (all defeated). Then flips bit `0x2` (whose turn) and sets bit `0x4` (turn change pending) |

**Callers of `battle_end_turn`** (= every way a turn ends):
Brian: `0x80004448` staff hit done, `0x800045F0` spell finished, `0x80004AB8` item finished
(likely), `0x80002F60` at `0x800031E0` (A pressed with no target = pass).
Enemies: `0x80009378` (Restriction skip), `0x80009588` (move), `0x80009934`, `0x800099DC` (melee),
`0x80009D78` (spell), `ai_spell_init` `0x8000A284` (silenced enemy loses its spell turn).

### Battle state and globals

`gBattleState` (u16 `0x8008C592`; the RAM sheet's 8-bit "Battle State 0x8c590" is its low byte):

| Bit | Meaning |
|---|---|
| `0x0001` | In battle |
| `0x0002` | It's an enemy's turn (0 = Brian's) |
| `0x0004` | Turn change pending (turn manager will start the next turn) |
| `0x0008` | Victory (read by `brian_update` to pay rewards) |
| `0x0010` | Escape |
| `0x0100` | Boss battle (blocks Escape, Compression, Restriction, Silence; always drops item) |
| `0x0200` | Battle intro running |
| `0x0400` | All enemies defeated |
| `0x8000` | Kept across battles (meaning unknown) |

| RAM | Type | What |
|---|---|---|
| `0x8008C594` | u16 | Delay before the next turn (20 frames after Brian's turn, 0 after an enemy's) |
| `0x8008C598` | u16 | Index of the current enemy in the turn list |
| `0x8008C59A` | u16 | Number of enemies in the turn list |
| `0x8008C59C`, `0x8008C5A0` | f32 | Battle circle centre X, Z |
| `0x8008C434` | f32 | Battle circle radius (100.0 random battles; per-boss table for bosses) |
| `0x8008C574` / `0x8008C578` | f32 / u16 | Random-encounter distance accumulator / encounter chance counter |
| `0x8007C990` | s32 | Enemies still alive (0 → victory) |
| `0x8007C998` | `EnemyAction[]` | Enemies, `0x128` bytes each (decomp `monsters.h`) |
| `0x8007D0B0` | u16[] | Turn list (indices into the enemy array) |
| `0x8007BA68` / `0x8007BA6C` | u32 | This battle's element-EXP pool / second EXP pool |
| `0x8007BA70` | u32 | Item won (`0xFF` = none) |

### Fighter records

Brian and enemies use the same shape, so most battle code takes a `battleData` pointer for
either one. `battleData+0x5E` bit 0 = "is an enemy", `+0x64` = base stats pointer,
`+0x68` = **combat block** pointer, `+0x60` bit 0 = busy/animating.

| | Brian | Enemy *n* (`E = 0x8007C998 + n×0x128`) |
|---|---|---|
| Controller | `gPlayer` `0x8007BAB8` (+0 state, +2 staff target, +0x10 → stats, +0x11C press count, +0x11E spell ID) | `EnemyAction` at `E` (+0 action state, +0xA HP, +0xC likely MP, +0x20 → `MonsterBaseData`) |
| battleData | `0x8007BACC` | `E+0x24` |
| Base stats | `0x8007BA80` (HP +4, MaxHP +6, MP +8, MaxMP +0xA, Agi +0xC, Def +0xE, element levels +0x24..27) | `MonsterBaseData` (agi +0xC, def +0xE, EXP +0x10, EXP2 +0x14, element +0x26, attack/level +0x2A, item +0x34) |
| **Combat block** | `0x8007BB38` | `E+0x90` |

Combat block layout (decomp's `UnknownMonsterData2`, 0x98 bytes):

| Offset | Brian's address | What | Set at battle start to |
|---|---|---|---|
| `+0x00` | `0x8007BB38` | u16 status bits (table in §3) | 0 |
| `+0x02`..`+0x0D` | `0x8007BB3A`..`45` | u8 turns left for status 0..11 | – |
| `+0x84` | `0x8007BBBC` | **Attack**: Brian = Fire+Earth+Water+Wind levels; enemy = `MonsterBaseData+0x2A` | |
| `+0x86` | `0x8007BBBE` | In-battle Agility | base Agility |
| `+0x88` | `0x8007BBC0` | In-battle Defense | base Defense |
| `+0x8A` | `0x8007BBC2` | Staff/melee power | 16 |
| `+0x90` | `0x8007BBC8` | f32 movement range multiplier | 1.0 |

Brian's movement range per turn is `gPlayer+0x0C = Agility × 0.2857 + 17` (`brian_battle_init`),
times the multiplier.

## 2. Damage

### Spell power — `spell_calc_power` `0x800175D4`

```
L (Brian) = level[elem] + level[opposite] >> 4 + level[side1] >> 3 + level[side2] >> 3
            (levels at 0x8007BAA4: Fire, Earth, Water, Wind; neighbour table 0x8004CDD0)
L (enemy) = MonsterBaseData+0x2A
power     = trunc( baseDamage × 0.02 × (1 + 0.2 × Σ_{i=0}^{L-1} 1.03^i) )
          = baseDamage × 0.02 × (1 + 0.2 × (1.03^L − 1) / 0.03)
```
Constants are doubles at `0x800714D0` (0.02, 0.2, 1.03). If the **caster** has Compression (status
bit 0), power is halved (`spell_type_default` `0x80016520`).

### Staff power — `brian_staff_damage` `0x80006B1C`

```
T     = Fire + Earth + Water + Wind levels
power = ( T/2 + Σ min(level_e, T/4) ) × staffPower / 16      (staffPower = block+0x8A: 16, Power Staff 24/32)
```
A balanced build hits hardest. Halved if Brian has Compression.
Enemy melee (`enemy_melee_damage` `0x8000AC84`): `attack × power/16`, halved if Compressed.

### Applying damage to an enemy — `battle_damage_enemy` `0x8000ACC0`

Args: enemy index, power, element, flags, attacker battleData.
1. If `flags & 0x8000` is clear (spells), apply element: `mask = elemMask[attackElem] | elemMask[enemyElem]`
   (`0x8004C2A8`: Fire 1, Earth 2, Water 4, Wind 8, element 4 = none 0). The jump table at `0x80071198` gives
   **0.5×** when both are the same element, or when the enemy has element 4 ("none" = resists everything).
   It gives **1.25×** for Fire+Water and Earth+Wind, and **1×** otherwise. The two multipliers are the
   instructions the ROM sheet points at: `srl 1` at `0x8000AD88`, `x + x>>2` at `0x8000AD98`. The staff
   passes `0x8004`, so it never gets element modifiers.
2. `d = trunc( power × A / (A + enemyDefense) )`, with A = attacker's block `+0x84` and defense = `E+0x118`.
3. `d += rand(0 .. floor(√d))` (`get_rand` `0x80022FD0` is an LCG: `seed×0x41C64E6D+0x3039`, returns `(seed>>16) % n`). If d = 0, d = 1.
4. HP (`E+0xA`) −= d, floored at 0. If the enemy has status bit 9, its MP (`E+0xC`) += d.
5. Every damaging spell hit on an enemy also gives Brian +1 MP growth EXP (`stats+0x2A`).

### Applying damage to Brian — `battle_damage_brian` `0x80006BEC`

Same steps without the element step: `d = power × A/(A + Brian's in-battle Defense 0x8007BBC0)`,
plus `rand(0..√d)`, minimum 1. HP `0x8007BA84` −= d. Every hit gives +1 HP growth EXP (`+0x28`)
and +1 Defense growth EXP (`+0x2E`). If Brian has status bit 9, MP += d.
Brian is immune while his state is 6 (knocked down).

### Hit and miss

| Function | Proposed name | Rule |
|---|---|---|
| `0x800183C4` | `spell_hit_check` | `rand(100) < accuracy` (spell `+0x0E`), then the agility roll below. Self-cast always hits |
| `0x80006A74` | `battle_hit_check_vs_enemy` | `rand(100) < 100·a / (a + (d+7)>>3)` with a = attacker block `+0x86`, d = enemy's |
| `0x8000ABF4` | `battle_hit_check_vs_brian` | Same formula, enemy attacking Brian. A dodged melee gives Brian +1 Agility growth EXP |

Magic Barrier (status bit 8) makes the target ignore all non-self spells (`0x8001581C`).

## 3. Status effects

Spell byte `0x32 + i` sets status `i`. The spell update `0x800156D0` runs the `spellType` handler and
then, for each non-zero status byte: success roll (`spell_status_roll` `0x80018484`), duration
(`spell_status_set_duration` `0x80018524`), and the status handler from the table at `0x8004CD44`
(= entries 0x0A–0x14 of the spellType table `0x8004CD1C`).

- **Chance**: `rand(32) < chance[i]` when Brian casts; `rand(80) < chance[i]` when an enemy casts.
  Self-target spells and `proj+0xA & 4` always succeed. `chance` = bytes at `0x8004CDF0`:
  20 for all, 10 for Restriction and Silence.
- **Duration**: `rand(dur[i]) + 2` on others, `+ 3` on self. `dur` = u32 at `0x8004CDFC`: 5 for all, 2 for Barrier.
- **Countdown**: `status_tick_durations` `0x8001817C` (called only from `battle_end_turn`) lowers each
  active counter by 1 at the end of the fighter's own turn. At 0 it clears the bit and restores the stat
  listed below.
- Brian's statuses are cleared at battle start (`brian_battle_init`), at victory/escape, and on defeat.
  Enemy statuses are cleared at spawn (`0x80009218`).

| Bit | Spell byte | Status | Handler | Effect (from code) | On expiry |
|---|---|---|---|---|---|
| `0x001` | `0x32` | Compression | `0x8001700C` | Model scale ×0.5; halves that fighter's spell, staff and melee power. Not on bosses | scale restored |
| `0x002` | `0x33` | Vampire's Touch | `0x80017094` | Staff/melee hits heal (dmg+3)/4 + 1 HP | – |
| `0x004` | `0x34` | Power Staff (attack up) | `0x800170DC` | Staff power 16 → 24 (value 1) or 32 (value 2) | power = 16 |
| `0x008` | `0x35` | Restriction (freeze) | `0x8001715C` | Enemies **skip their turn** (`0x80009378`). Brian **can't move** but can still act (`0x80003CD0`). Not on bosses | busy flag cleared |
| `0x010` | `0x36` | Movement (slow/fast) | `0x800171E4` | Range multiplier 0.5 (1 = slow) or 2.0 (2 = Wind Walk) | 1.0 |
| `0x020` | `0x37` | Agility / Evade | `0x800172A0` | Agility ×2 (1), ×4 (2), ×0.5 (3) | base Agility |
| `0x040` | `0x38` | Silence | `0x8001737C` | Brian can't open the spell input (`0x80003578`). A silenced enemy that picks a spell **loses the turn** (`0x8000A2C0`). Not on bosses | – |
| `0x080` | `0x39` | Soul Searcher | `0x800173E0` | Bit only; effect not traced (probably the HP display) | – |
| `0x100` | `0x3A` | Magic Barrier | `0x80017428` | Blocks all non-self spells | – |
| `0x200` | `0x3B` | "Confusion" (spell `0x109`, internal name `CONFUSION`, Earth, self-cast) | `0x80017470` | **Damage taken is also added to MP** (`0x80006E3C`, `0x8000AF9C`). No confusion-style AI effect found | – |
| `0x400` | `0x3C` | Defense up/down | `0x800174B8` | Defense ×0.5 (1), ×0.75 (2), ×1.5 (3), ×2 (4) | base Defense |
| `0x800` | `0x3D` | (unused) | `0x800175B8` | Sets the bit; nothing reads it, and no spell uses it | – |

Brian's countdown bytes: `0x8007BB3A` Compression, `3B` Vampire, `3C` Power Staff, `3D` Restriction,
`3E` Movement, `3F` Evade, `40` Silence, `41` Soul Searcher, `42` Barrier, `43` "Confusion",
`44` Defense, `45` unused. These line up with the RAM sheet once its byte swap is undone.

## 4. MP cost

`spell_cast_init` `0x800072A8`: `MP −= cost[gPlayer+0x11C]`, where `+0x11C` is the number of element
buttons pressed. `cost[]` is u16 at `0x800C1B26 + 2n`, i.e. the words right after the spell-table
pointers: `[n=1] 1, [n=2] 2, [n=3] 3`. So **cost = number of presses**.
`spell_check_castable` `0x800149D0` returns flag 1 when MP < cost (using the spell's `inputCount`
`+0x04`), flag 2 when the spell isn't usable here (battle/field bits of `+0x02`), and flag 4 when the
element level is below `levelRequirement`.

Other MP sources: a connecting staff hit +1 MP (`0x800044D8`, the ROM sheet's "MP Recovered from Staff
hits"); walking outside battle +1 MP per distance step (`0x800037E4` skips this in battle); Drain Magic;
status bit 9.

## 5. Rewards, defeat, escape

**When an enemy dies** (`enemy_state_hurt` `0x80009818`, end of the hit reaction with HP 0), it is
removed: `monster_id = -1`, and the alive count `0x8007C990` goes down by 1. Then:
- `0x8007BA68 += MonsterBaseData+0x10` (element EXP; 6 in the measured fight)
- `0x8007BA6C += MonsterBaseData+0x14` (second value; the decomp calls it `stone_dropped`. It pays into
  `stats+0x14` = `0x8007BA94`, which rose 532 → 564 in the stats log. Nothing reads it in battle
  code, and its use is still unknown)

**Victory** (`battle_turn_manager` sets bit `0x8`; `brian_update` pays out at `0x8000364C`):
- `stats+0x10` (element EXP `0x8007BA90`) += pool 1; `stats+0x14` += pool 2.
- Item: `battle_roll_item_drop` `0x8001D828`. Normal battles: `rand(4) == 0`, then `MonsterBaseData+0x34`
  of a random enemy slot `rand(count)`. Bosses always drop their item. It's added to the inventory at
  `0x800036A8` (`0x800036B0` is the ROM sheet's "Item Drop Lock"); if the bag is full it's retried out of
  battle (`0x800079C8`).
- Brian's statuses are cleared and he plays the victory pose (state `0xF`).

**Level-ups after battle** happen in the field (`brian_process_growth` `0x800074A0`, run from Brian's
idle state):
- **Element**: while `element EXP ≥ chart[levelsGained]`, subtract it and grant a level the player
  places. The code reads the chart at `0x80053D3C` (ROM `0x5493C`), **one entry before** the community
  "EXP Per Element Level Chart" (`0x54940`). That first entry is **8**, which explains the 8-EXP level
  the stats session measured. Capped at 98 battle levels (`0x80007944`).
- **HP / MP / Agility / Defense**: growth EXP (`stats+0x28/2A/2C/2E`) against the stat chart `0x80053ECC`
  (×1 for HP and Agility, ×4 for MP, ×2 for Defense), indexed by levels gained (`+0x30..33`, cap 54).
  Max HP and MP cap at 500, Agility and Defense at 255.

**Escape** (bit `0x10`; ending the turn outside the circle sets `gPlayer+8` bit `0x20` in `0x80005748`, or
the Escape spell `spell_type_escape` `0x80016940`): the battle ends and the pools are never paid
(they're zeroed at the next battle start). Escape is impossible in boss fights. Escape cast **on an enemy**
removes that enemy without giving its EXP.

**Defeat** (`brian_state_hurt` `0x80004040`): when Brian's hit reaction ends with HP 0, he goes to state 6
(down, 230 frames), plays the defeat music `0x1E`, and sets game flags `0x880` (`0x8007B2E4`). He is warped
to the respawn point `0x8007BA60/62/64` (map/submap/entrance), which is updated when he rests at an inn
(`0x80001DB8`) or by `0x8000435C`. On arrival `brian_spawn` `0x80002918` sees flag `0x800` and sets
**HP = 1, MP = max**, clears statuses and drops the pending item. No EXP or item is kept.

## Still unknown

- Whether "Confusion" also changes behaviour anywhere else, e.g. enemy targeting. Only the MP-on-hit
  effect was found; worth checking in game.
- What Soul Searcher (bit `0x80`) actually does on screen.
- Enemy action states 3 (`0x80009934`) and the AI action choice inside `ai_spell_init` / the AI scripts.
- What `stats+0x14` (`0x8007BA94`, the second EXP pool) is used for.
- How the "busy" flag (`battleData+0x60` bit 0) set by Restriction gets cleared when no hit reaction
  follows. The turn manager waits on it.
- The number of enemy slots, and how the turn list is built (`0x8001D640`, not read).
- `0x80004AB8` is very likely the item-use state (it calls `0x800213D8`), but that wasn't confirmed.

## Proposed function names

**Merged into `Quest64Syms/names.toml` on 2026-10-20** (commit 778bbb4). Where the four write-ups proposed different names for the same address, names.toml wins. Changes from this table:

- Brian's functions use `player_*` instead of `brian_*` (`player_update`, `player_spawn`, `player_state_idle`, ...)
- `battle_damage_enemy` → `enemy_take_damage`, `battle_damage_brian` → `player_take_damage`
- `battle_hit_check_vs_enemy` / `_vs_brian` → `battle_hit_roll_vs_enemy` / `battle_hit_roll_vs_player`
- `enemy_melee_damage` → `ai_melee_damage`; enemy states use `ai_state_*` (`enemy_state_move` → `ai_state_approach`)
- `enemy_spawn_init` → `enemy_spawn`; `battle_turn_manager` → `battle_update` (it also runs the victory rewards)
- Data: `gPlayerStats` kept as the existing `gPlayerMainData`; `gEnemiesAlive` → `gEnemyCount`; `gElementMasks` → `gElementWeakMasks`; `gBrianStateHandlers` → `gPlayerStateHandlers`. `gPlayer`, `gPlayerCombat` and `gSpellMpCost` were not added (no data symbol at those addresses; the MP cost is a field of each spell record)

| Address | Name | Evidence |
|---|---|---|
| `0x80002F60` | `brian_update` | Per-frame Brian logic; calls `spell_menu_input`/`spell_cast_init`; pays victory rewards; dispatches state table `0x8004C230` |
| `0x80002918` | `brian_spawn` | Places Brian on map load; if game flag `0x800` (defeated): HP = 1, MP = max, statuses cleared |
| `0x80003A1C` | `brian_battle_init` | Writes attack = sum of element levels, power 16, agility, defense, move ×1.0; zeroes EXP pools and statuses |
| `0x80003B60` | `brian_state_idle` | States 0–2 of `0x8004C230`; stick input zeroed under Restriction (`0x80003CD0`); calls growth |
| `0x80004040` | `brian_state_hurt` | State 5; HP 0 → state 6, defeat music `0x1E`, warp to respawn point |
| `0x80004448` | `brian_state_staff_hit` | State 13; hit check, staff damage, +1 MP, Vampire heal, `battle_end_turn` |
| `0x800045F0` | `brian_state_casting` | State 14 (set by `spell_cast_init`); `projectile_init`, waits for spells, ends turn |
| `0x80005748` | `brian_battle_move` | Distance from circle centre vs radius `0x8008C434`; clamps in boss fights, otherwise sets the "outside circle" flag `gPlayer+8 \| 0x20` |
| `0x800074A0` | `brian_process_growth` | HP/MP/Agi/Def level-ups vs chart `0x80053ECC`; element levels vs `0x80053D3C` |
| `0x80006B1C` | `brian_staff_damage` | `(T/2 + Σ min(Lv, T/4)) × power/16`, halved by Compression |
| `0x800149D0` | `spell_check_castable` | Returns 1 = MP too low, 2 = not usable here, 4 = level too low |
| `0x800175D4` | `spell_calc_power` | `base × 0.02 × (1 + 0.2 Σ 1.03^i)`; enemy level = `MonsterBaseData+0x2A` |
| `0x800156D0` | `spell_update_all` | Community "Main Spell Motion Caller"; also runs collision → spellType handler → status loop |
| `0x800183C4` | `spell_hit_check` | Accuracy roll then agility roll; result via table `0x8004CDE4` |
| `0x80018484` | `spell_status_roll` | `rand(32)` (Brian) / `rand(80)` (enemy) `< chance[i]` from `0x8004CDF0` |
| `0x80018524` | `spell_status_set_duration` | `block[2+i] = rand(dur[i]) + 2` (others) / `+3` (self); `dur` at `0x8004CDFC` |
| `0x8001817C` | `status_tick_durations` | Loops 12 bits, decrements counters, restores stats on expiry |
| `0x8000ACC0` | `battle_damage_enemy` | Element ×0.5/×1.25, `A/(A+D)`, `+rand(√d)`, subtract HP, hit reaction |
| `0x80006BEC` | `battle_damage_brian` | Same without element; HP and Defense growth EXP +1 |
| `0x80006A74` | `battle_hit_check_vs_enemy` | `rand(100) < 100a/(a+(d+7)/8)` |
| `0x8000ABF4` | `battle_hit_check_vs_brian` | Same, enemy → Brian |
| `0x8000AC84` | `enemy_melee_damage` | `attack × power >> 4`, halved by Compression |
| `0x8001C5F4` | `battle_try_random_encounter` | Distance/`rand(2000)` encounter; gBattleState `0x205`; agility decides first turn |
| `0x8001C8C4` | `battle_start_boss` | gBattleState `0x305`; called from `brian_state_idle` path |
| `0x8001CFE8` | `battle_turn_manager` | Victory/escape check, starts the next turn, calls `enemy_turn_begin` |
| `0x8001D358` | `battle_end_turn` | Ticks statuses, next enemy (round robin), toggles whose turn |
| `0x8001D828` | `battle_roll_item_drop` | `rand(4)==0` (non-boss), random enemy's `MonsterBaseData+0x34` |
| `0x8000932C` | `enemy_turn_begin` | Sets `EnemyAction+8 \| 1` on the current enemy |
| `0x80008FE0` | `enemy_spawn_init` | Clears statuses; attack = base `+0x2A`, power 16, agility, defense, move 1.0 |
| `0x80009378` | `enemy_state_idle` | Action state 0 (table `0x8004C290`); Restriction → end turn; else move or attack |
| `0x80009588` | `enemy_state_move` | State 1; walks toward Brian, then attacks or ends turn |
| `0x80009818` | `enemy_state_hurt` | State 2; on HP 0: remove enemy, alive count −1, add EXP to pools |
| `0x800099DC` | `enemy_state_melee` | State 4; hit check, `enemy_melee_damage`, `battle_damage_brian` |
| `0x80009D78` | `enemy_state_cast` | State 5; calls `projectile_init` |
| `0x800169C8` | `spell_type_return` | spellType 4 entry of `0x8004CD1C` |
| `0x80016A84` | `spell_type_status_only` | spellType 5 (buff/debuff, calls damage with 0) |
| `0x80016B18` | `spell_type_exit` | spellType 6 |
| `0x80016BEC` | `spell_type_extinction` | spellType 7 |
| `0x80016D60` | `spell_type_dispel_damage` | spellType 9 = default + dispel |
| `0x8001700C` | `status_apply_compression` | Status handler 0 (`0x8004CD44`) |
| `0x80017094` | `status_apply_vampire` | Status handler 1 |
| `0x800170DC` | `status_apply_power_staff` | Status handler 2 |
| `0x8001715C` | `status_apply_restriction` | Status handler 3 |
| `0x800171E4` | `status_apply_movement` | Status handler 4 |
| `0x800172A0` | `status_apply_agility` | Status handler 5 |
| `0x8001737C` | `status_apply_silence` | Status handler 6 |
| `0x800173E0` | `status_apply_soul_searcher` | Status handler 7 |
| `0x80017428` | `status_apply_magic_barrier` | Status handler 8 |
| `0x80017470` | `status_apply_mp_absorb` | Status handler 9 (community "Confusion"; code effect = damage → MP) |
| `0x800174B8` | `status_apply_defense` | Status handler 10 |
| `0x800175B8` | `status_apply_unused` | Status handler 11 (bit `0x800`, never read) |

Data names: `gBattleState` `0x8008C592` (already in decomp), `gPlayer` `0x8007BAB8`,
`gPlayerStats` `0x8007BA80`, `gPlayerCombat` `0x8007BB38`, `gEnemies` `0x8007C998`,
`gEnemiesAlive` `0x8007C990`, `gBattleTurnList` `0x8007D0B0`, `gBattleTurnIndex` `0x8008C598`,
`gBattleTurnCount` `0x8008C59A`, `gBattleExpPool` `0x8007BA68`, `gBattleExpPool2` `0x8007BA6C`,
`gBattleItemDrop` `0x8007BA70`, `gBattleCircleRadius` `0x8008C434`, `gSpellTypeHandlers` `0x8004CD1C`,
`gElementMasks` `0x8004C2A8`, `gStatusChance` `0x8004CDF0`, `gStatusDuration` `0x8004CDFC`,
`gSpellMpCost` `0x800C1B26`, `gBrianStateHandlers` `0x8004C230`, `gEnemyStateHandlers` `0x8004C290`.
`get_rand` `0x80022FD0` is already named in the decomp's `symbol_addrs.txt`.
