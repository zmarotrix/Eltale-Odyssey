# How spells work in Quest 64 (USA 1.0)

Status: 2026-10-20. Sources:
- The ROM and disassembly from the recompiled code (our own analysis).
- The [Merrow randomizer](https://github.com/hangedmandesign/merrow) (MIT), whose working patches confirm several fields.
- **The Quest 64 decomp community on decomp.me** (preset "Quest 64"): vbhayden, Rainchus, LLONSIT-glitch,
  Drahsid, inspectredc and others are actively decompiling the spell code (most of it in October 2026).
  Their struct and function names are used below. Their shared header is also in
  [vbhayden/Quest-64-Lua-Resources `decomp/context.c`](https://github.com/vbhayden/Quest-64-Lua-Resources/blob/HEAD/decomp/context.c).
  That repo has **no license**, so don't copy it into the GPL fork without asking. None of this spell
  work has been merged into [Rainchus/Quest64-Decomp](https://github.com/Rainchus/Quest64-Decomp) yet.
- **The "Quest 64 Modding Notes" spreadsheet** (exported HTML in `../Quest 64 Modding Notes/`, searchable
  text copies in `../modding-notes-text/`). This is the original source of the community's per-byte spell
  notes and also covers items, bosses, enemies, text, sounds and RAM. Credit: **the Eltale Monsters Discord community**.

Our independent findings matched theirs on the table locations, record sizes, spell ID format, status
bytes, multi-shot queue and active-spell slots. Their field names correct two of our guesses (`0x02` and `0x12`).

## Where the data lives

At boot, `func_80000FE8` copies ROM `0xD4B3C0`–`0xD77380` (0x2BFC0 bytes) to RAM `0x800C0000`.
It stays loaded, so every address below is fixed. ROM offset = RAM − `0x800C0000` + `0xD4B3C0`.

| RAM | Community name | What |
|---|---|---|
| `0x800C1B14` | `SpellDefinition* D_800C1B14[5]` | Pointers to the stat tables: Fire, Earth, Water, Wind, Enemy |
| `0x800C06A0`… | `SpellDefinition` | `0x44` bytes each: 15 per element (60) + 17 enemy/boss spells |
| `0x800C3520` | `GlobalSpellAnimTable` | Pointers to the visual/audio tables (player spells only; 5th is null) |
| `0x800C1F78`… | `SpellAnimDefinition` | `0x48` bytes each, 15 per element |
| `0x80086F18` | `RuntimeProjectileTransform D_80086F18[9]` | Active spells in flight, `0x3C` bytes each |
| `0x80087170` | `UnkMultiProjContext[3]` | Queue for multi-shot spells, `0x18` bytes each |

**Spell ID** = `(school << 8) | index`, e.g. Fire Ball 1 = `0x000`, Avalanche = `0x108`.
Enemy and boss AI actions use `0x8000 | spellID` to mean "cast this spell".

## The spell pipeline (function map)

| Function | Name (community) | Role |
|---|---|---|
| `func_800070C0` | Spell Menu Input | Reads Brian's element button presses |
| `func_80014890` | (ours) | Finds the spell matching the presses; **hardcodes 15 per element** (`0x80014958`) |
| `func_800072A8` | Spell Cast Init / Transaction | Starts a cast (MP etc.) |
| `func_80014A98` | Projectile / Spell Init | Creates the runtime spell: queues extra shots, fills a `RuntimeProjectileTransform` |
| `func_800156D0` | Main Spell Motion Caller | Per-frame update of active spells |
| `func_80015B8C` | Projectile Motion | Movement, homing, speed |
| `func_80017998` | Projectile Anim Init | Sets up the visual layers |
| `func_80017FA0` | Projectile Ground Impacts | |
| `func_800163E8` | Auto-Hit Spell Collision | For `autoHit` spells |
| `func_80016520` | Spell Type 0x0 – Default | Handler chosen by `spellType` (`0x10`): damage + status bytes |
| `func_800165E4` | Spell Type 0x1 – Healing | |
| `func_8001679C` | Spell Type 0x2 – Drain Magic | Drains 11/64 of the target's MP to Brian |
| `func_80016940` | Spell Type 0x3 – Escape | |
| `func_80016CB0` | Spell Type 0x8 – Dispel (Invalidity) | |
| `func_800140EC` | Spell Camera Snap | |
| `func_8000A284` | AI Spell Init + Special Anims | Enemy casting |
| `func_8000B300`, `func_8000B2C8`, `func_80008C20` | Boss Spell Anims (update / queue / call) | Boss-only visuals (`BossSpellAnimStage`) |

The key design point: **each spell type is a separate handler function**, picked by `spellType`.
This is the natural place for a modding API to add new spell types.

## Stat record: `SpellDefinition` (`0x44` bytes)

| Offset | Type | Field | Meaning |
|---|---|---|---|
| `0x00` | u16 | levelRequirement | Required element level (confirmed against published lists) |
| `0x02` | u16 | onHitBehavior ("spell rule") | Flags: bit 0 = usable in battle, bit 1 = usable outside battle, bit 2 = passes through and hits multiple targets (Rock, Avalanche). 0 = can't be cast |
| `0x04` | u16 | inputCount | Number of element presses (1–3) |
| `0x06`–`0x08` | u8×3 | spell_element, inputDirs | Press sequence: 0 Fire, 1 Earth, 2 Water, 3 Wind. The first one is the spell's element |
| `0x0A` | u16 | range | Range / area (only when not a free projectile) |
| `0x0C` | u16 | baseDamage | Damage (heal amount for Healing) |
| `0x0E` | u16 | accuracy | Accuracy % |
| `0x10` | u16 | spellType | Which handler runs (table below) |
| `0x12` | u16 | projectileType | 0 free, 1 vertical only, 2 Magnet Rock |
| `0x14` | u16 | heightSnapping | Travel height |
| `0x16` | u16 | autoHit | 1 = always hits |
| `0x18` | u16 | targetType | 0/1 enemy, 2 self |
| `0x1C` | f32 | size | Hitbox/effect size (Fire Ball 6.0, Rock Shower 1000.0) |
| `0x20` | u16 | launch_count | Number of shots (Fire Ball 1/2/3, Wind Cutter 1/3/6, Avalanche 10) |
| `0x22` | u16 | launch_delay | Frames/2 between shots |
| `0x24` | u16 | startupDelay | Frames/2 before the spell activates |
| `0x26` | u16 | startPoint | Where it spawns (table below) |
| `0x28` | u16 | homing | 0 none, 1 slight, 3 good |
| `0x2A` | u16 | angle | Vertical influence |
| `0x2C` | u16 | projectileDir | Initial direction |
| `0x2E`, `0x30` | s16 | speedDesired, speedInitial | Speed |
| `0x32`–`0x3D` | u8×12 | x32_effects | Status payload, one byte per status (below) |
| `0x3E`–`0x42` | u16×3 | unk | `0x42` is used by Invalidity |

Status bytes: `0x32` Compression · `0x33` Vampire's Touch (crashes on damage spells) · `0x34` attack up ·
`0x35` Restriction · `0x36` movement (1 slow, 2 fast) · `0x37` agility/evade · `0x38` silence ·
`0x39` Soul Searcher · `0x3A` magic barrier · `0x3B` confusion · `0x3C` defense (1–2 down, 3–4 up).

**spellType values** (from the modding notes; per the notes, values above 0x14 do nothing):

| | | | |
|---|---|---|---|
| 0 Damage | 1 Heal | 2 MP Drain | 3 Escape |
| 4 Return | 5 No damage (buffs) | 6 Exit | 7 Extinction |
| 8 Invalidity | 9 Invalidity + damage | A Compression | B Vampire's Touch |
| C Power Staff | D Restriction | E Lower Movement | F Evade |
| 10 Silence | 11 Soul Searcher | 12 Magic Barrier | 13 Confusion |
| 14 Def Up/Down | | | |

**startPoint values**: 0 below caster's feet · 1 behind caster · 2/4 above caster's head · 3 in front (standard) ·
5 far above head · 6 below target's feet · 7 random above head (Avalanche) · 8 in front, off to one side
(Rolling Rock) · 9 Mammon's Needle only. Boss-only combo values: `0x1E` self Vampire's Touch,
`0x1F` self Power Staff + damage target, `0x22` self Evade + damage, `0x25` self Magic Barrier + damage,
`0x26` self Confusion + damage. **These combo values show the engine can already pair a self-buff with an attack.**

## Visual/audio record: `SpellAnimDefinition` (`0x48` bytes)

```c
typedef struct { s16 model; u16 color; u16 duration; u16 unk6; f32 size_x; f32 size_y; } SpellAnimStage; // 0x10
typedef struct { u8 sound; u8 delay; } SpellAudioStage;
typedef struct { SpellAnimStage anims[4]; SpellAudioStage sounds[4]; } SpellAnimDefinition;      // 0x48
```

Each spell has up to 4 visual stages (`model` −1 = unused) and 4 sounds. Changing `color` live
works (tested in-game on Fire Ball, 2026-10-20). Note: on recomp builds without the billboard fix
(Rainchus/Quest64-Recomp#16), Fire Ball is invisible regardless of color; this was briefly
mistaken for a color problem. Per the modding notes, stage 2
is usually the projectile and stage 4 the on-contact impact, and the `duration` word acts as a "disappear"
setting. Fire Ball uses only stage 2 (model `0x19`, color 8, sound `0x0F`). Merrow recolors spells by
writing `color`. Player spell models are roughly `0x00`–`0x41`. **Boss attack models are `0x46`–`0x62`**
(Solvaring's Fire Blast, Zelse's Wind Razors, Nepty's Bubble, Beigis's Spirit Sword…), so a mod could
reuse them for new player spells if they're loaded outside boss fights (untested).

## Names and text

- Spell names: pointer table at ROM `0xD4B81C` (77 entries), element names at `0xD4BA50`. Both are in the
  same RAM block, so names can be changed at runtime too.
- Text uses a custom encoding: `0x00`–`0x19` = a–z (in some contexts `0x81`/`0x82` switch to upper/lower
  case), `0x7F` space, `0xE0` line break, `0xF0` page break, `0xFF` end, `0xA0`–`0xA3` text colors.
  See `../modding-notes-text/Text Information.tsv`.

## Brian's stats (RAM)

From the notes' RAM sheet: max/current MP `0x8007BA88`/`0x8007BA8A`, element levels `0x8007BAA4`–`0x8007BAA7`
(Wind, Water, Earth, Fire), element EXP `0x8007BA90`, status durations and icons around `0x8007BB38`–`0x8007BB53`.

## Enemies share player spells

Found in play (2026-10-20): with the Blue Fire mod on, enemies that cast Fire Ball also cast BLUE FIRE.
Enemy AI actions name spells by ID (`0x8000 | spellID`, see `boss_spell_anims_update`), and those IDs point
into the same per-element tables Brian uses, not only the enemy table. Any change to a player spell
therefore changes it for every enemy that casts it.

For the overhaul: give enemies their own spell entries (copies in the enemy table or a new table) and point
their AI actions at the copies, so player and enemy spells can be balanced separately. A modding API should
also expose the caster (`RuntimeProjectileTransform.x24_caster`) so mods can tell Brian's casts apart.

## What this means for modding

- **Changing existing spells: easy.** Every field above is plain data at a fixed address.
- **New spells built from existing models, sounds and spell types: realistic.** A new spell is a
  `SpellDefinition` plus a `SpellAnimDefinition`. The blocker is the **15-per-element limit**
  (hardcoded in the cast lookup and probably the menu and learning code). A recomp mod can move the
  tables to bigger arrays and patch every place that assumes 15.
- **New spell behaviors: realistic in a recomp.** Behavior comes from `spellType` handlers, which are
  ordinary functions. A recomp mod is native C, so it can add new handlers (e.g. a spell type that
  poisons, or chains between enemies) by hooking the dispatcher. This is much easier here than in a
  ROM hack like Merrow, which has to inject MIPS code into free ROM space.
- **New 3D effects/models: hard.** The model library behind `model` IDs isn't mapped yet.
- Merrow found spells that **crash** in some contexts (e.g. Homing Arrow, Magma Ball and the cutters
  for enemies; Vampire's Touch on damage spells). A modding API should validate or prevent those combinations.

## Open questions

1. **MP cost**: there's no MP field in the spell record, so cost must be computed or hardcoded. Find it.
2. Where else is the 15-per-element limit assumed (menu, learning, save data)?
3. How does the `spellType` dispatcher pick a handler?
4. Are boss models (`0x46`–`0x62`) loaded outside their boss fights?

## Tools

- [dump_spells.py](dump_spells.py): `python dump_spells.py "Quest 64 (USA).z64"` writes every
  stat record to `spells_raw.csv`.
