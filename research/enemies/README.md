# Enemies, bosses and enemy AI in Quest 64 (USA 1.0)

Status: 2026-10-20. Static analysis only: the ROM, plus the original MIPS instructions kept as comments in
`Quest64-Recomp/RecompiledFuncs/*.c`. Nothing was run in game. The community "Quest 64 Modding Notes"
(`../modding-notes-text/`, credit: the Eltale Monsters Discord community) were checked against the code and
the ROM. They were right about almost everything; corrections are marked **(correction)**.
Every table below was dumped straight from `Quest 64 (USA).z64` by a script, not copied from the notes.

## Plain-language summary

- **Monsters come in 6 "sets".** Each set is one block of ROM holding up to 15 monsters, their AI and their
  names. When Brian enters a map, the game copies that map's set into RAM at `0x8020E6F0` (same address for
  every set). Bosses have their own fixed table that stays loaded the whole game.
- **Each monster is a 0x38-byte record**: HP, agility, defense, attack, element, EXP, a second reward number,
  item drop, size/hitbox, and a pointer to a short list of 1–3 **actions**.
- **There are no resistances or weaknesses per monster.** The only defensive stat is the monster's single
  element. The rule is global: a spell of the **same** element does **half** damage; **Fire↔Water** and
  **Earth↔Wind** do **×1.25** (both directions); everything else is normal. A monster with element "None"
  (Guilty, Mammon) takes half damage from every element. Brian has no element at all: enemy spells hurt
  him the same whatever their element.
- **There is no gold.** Field `0x14` (which the notes call "gold?") is added to a counter at `0x8007BA94` after
  a win, but no code we found ever reads it back. Drops: after a normal win there is a **1 in 4** chance of an
  item, taken from a random monster in the group; bosses always drop.
- **An action is 0x18 bytes**: what to do (`0x8000 | spellID` = cast that spell, `0x0000` = physical attack),
  timings, animations, a maximum range and (for physical attacks) a reach. On its turn a monster sorts its
  actions by range, picks the one that fits the distance to Brian (random between two when both fit), walks
  toward Brian for up to 4 steps if needed, then attacks. Mammon picks completely at random.
- **Enemies cast through the same function and tables Brian uses.** The cast code takes the spell ID, looks up
  the element table `(ID >> 8)` and entry `(ID & 0xFF)`, and launches it. So 103 of the 128 enemy actions
  point straight at Brian's own spells; changing Fire Ball changes it for 3 monsters too.
- **Bosses** use the same AI and spell code, plus: a field trigger (walk close to the boss on its map), a
  fixed arena, no escaping, slower-growing movement, full-speed animations, extra boss-only visual effects,
  an always-dropped item, a permanent "defeated" flag and a Max HP reward.
- **Cleanest way to give enemies their own spells:** build a new, bigger copy of the enemy spell table (and
  its visuals table) in mod memory, point the game's two "enemy table" pointers at it, put a copy of every
  player spell that enemies use after the 17 boss spells, and rewrite the action IDs from `0x8xyy` to
  `0x84nn`. One small code fix is needed for boss fights (see "Giving enemies their own spells").

## Address conversion (and two corrections)

| Block | ROM | RAM | Notes |
|---|---|---|---|
| Main code | `0x1000`… | `0x80000400`… | RAM = ROM − 0x1000 + 0x80000400 |
| Spell/item tables | `0xD4B3C0`–`0xD77380` | `0x800C0000` | copied once at boot by `boot_load_static_data` |
| Boss/item/text block | `0xD77380`–**`0xD884F0`** | `0x80399AB0` | **(correction)** the copy ends at `0xD884F0`, not `0xD88470` (`0x8000104C`), so the boss name pointers at ROM `0xD88488` are loaded too |
| Monster set (per map) | see table below | `0x8020E6F0` | copied on map load by `func_80008EF4` |

**(correction to `../spells/README.md`)** The 5th spell-visuals pointer (`0x800C3530`) is not null: it points
to `0x800C3058`, a 17-entry visuals table for the enemy/boss spells (ending exactly at `0x800C3520`).

## 1. Monster data

### Where each map's monsters come from

`func_80008EF4` reads the current map header (`*0x80084F1C`, field `+0x12` = set number), picks the set's row
in the table at RAM `0x80054160` (ROM `0x54D60`, 6 rows × 0x14 bytes) and DMAs the whole set to `0x8020E6F0`.
Row layout: `romStart, romEnd, modelPtrs, monsterData, namePtrs` (the last three are RAM addresses inside the
copied block). It stores the row pointer at `0x8007D0BC`, the model pointer table at `0x8007D0A8` and the name
pointer table at `0x8007D0AC`.

| Set | Notes' name | ROM block | Monsters | Monster data (ROM) | AI actions (ROM) | Name ptrs (ROM) |
|---|---|---|---|---|---|---|
| 0 | Holy Plains | `0xA7A190`–`0xADC420` | 12 | `0xADC090` | `0xADBEE0` | `0xADC3E4` |
| 1 | Dondoran Flats | `0xADC420`–`0xB637E0` | 15 | `0xB63378` | `0xB63120` | `0xB637A0` |
| 2 | Carmagh caves / Limelin | `0xB637E0`–`0xBBE1E0` | 11 | `0xBBDE98` | `0xBBDD00` | `0xBBE1A8` |
| 3 | Desert / Brannoch | `0xBBE1E0`–`0xC31BD0` | 13 | `0xC317E8` | `0xC31590` | `0xC31B94` |
| 4 | Normoon / Isle of Skye | `0xC31BD0`–`0xC9C1D0` | 12 | `0xC9BE50` | `0xC9BCD0` | `0xC9C1A0` |
| 5 | Mammon's World | `0xC9C1D0`–`0xCC4570` | 4 | `0xCC4438` | `0xCC4360` | `0xCC4558` |

To find a field in RAM: RAM = ROM − romStart + `0x8020E6F0`. Example: Dondoran's actions at ROM `0xB63120`
are at RAM `0x802953F0`, which is exactly the pointer stored in Goblin's record. The set is re-copied
whenever a map with a different (or the same) set loads, so **a mod that edits monster data must re-apply
its edits after `func_80008EF4` runs** (hook its return).

The monster ID used everywhere in battle (packs, model table, name table) is the index **within the set**.

### Random encounters (packs)

`func_8001D640` spawns a battle from an encounter-zone record: `u16 count, u16 packIndex[count]`; it picks one
pack at random. The map's encounter data pointer is `0x8008C564` (`+0` → formation position list,
`+4` → pack table). Pack entry (8 bytes): `u32 groupsPtr, u16 groupCount, u16 formation` (the notes'
"IDs and Lock" word is groupCount and formation). Group (12 bytes): `u32 monsterIndex, u32 baseCount,
u32 extra`; the game spawns `baseCount + random(0..extra)` of that monster. Spawn positions come from the
formation's position list (12 bytes per monster), rotated to Brian's facing. The notes' "Enemy Packs" and
"Enemy Spawn Regions" sheets list every pack and zone; their layout matches the code.

### `MonsterBaseData` (0x38 bytes) — confirmed against code

Compared with `Quest64-Decomp/include/monsters.h`. "Used by" = where the code reads it.

| Off | Type | Decomp name | What it really is | Used by |
|---|---|---|---|---|
| `0x00` | u16 | monsterType | 0 ground, 1 flying, 2 Mammon (no shadow) | spawn, field |
| `0x02` | s16 | monsterID | Model/sprite index in the set's model table | spawn (`0x80009024` → battleData `+0x50`) |
| `0x04` | u16 | currHP | Unused copy of HP (always equals `0x06`) | — |
| `0x06` | u16 | maxHP | **HP**: start HP and heal cap; also the enemy's start **MP** | spawn `0x80009088`, `0x80009BD0` |
| `0x08` | u16 | unk8 | Always 0 (notes: "MP?") | — |
| `0x0A` | u16 | unkA | Always 0; used as the cap when an enemy with status `0x200` gains MP on hit (`0x8000AFC4`) | `func_8000ACC0` |
| `0x0C` | u16 | agi | Agility: hit chance, turn order, movement range | spawn, `func_8000ABF4`, `func_80009260` |
| `0x0E` | u16 | def | Defense | spawn → EnemyAction `+0x118` |
| `0x10` | s32 | expValue | EXP, added to Brian's element EXP `0x8007BA90` after a win | `0x800098D4` |
| `0x14` | s32 | stone_dropped | Second reward, added to `0x8007BA94` after a win; **no reader found** (not gold: the game has none) | `0x800098F4` |
| `0x18` | f32 | hitboxHeight | Really the **width/radius** (copied to battleData `+0x28`, the collision radius) | spawn |
| `0x1C` | f32 | hitboxWidth | Really the **height** | spell code |
| `0x20` | f32 | modelScale | Model scale (battleData `+0x24`; also scales action ranges) | spawn, AI |
| `0x24` | u16 | unk24 | **AI flags**: bit 2 (`4`) = pick a random action, ignoring ranges (only Mammon). Bit 0 is set on about half the monsters; no reader found | `0x8000A238` |
| `0x26` | u16 | element | 0 Fire, 1 Earth, 2 Water, 3 Wind, 4 None | `func_8000ACC0`, info window |
| `0x28` | u16 | unk28 | **Hurt animation** index (3 most, 2 bosses/Wyvern/Rose Knights, 6 Beigis) | `0x8000B04C` |
| `0x2A` | u16 | atk | Attack: physical damage **and** the power level of the monster's spells | spawn → `+0x114`, `func_800175D4` |
| `0x2C` | u16 | aiScriptNumber | **Number of actions** (1–3) | `func_8000A008` |
| `0x2E` | u16 | unk2E | Always 0 | — |
| `0x30` | ptr | AIScript | RAM pointer to the first action (0x18 each) | `func_8000A008` |
| `0x34` | u8 | itemDrop | Item ID, `0xFF` = none | `func_8001D828` |
| `0x35`–`0x37` | | | Always 0 | — |

### Runtime enemy slot (`EnemyAction`, 0x128 bytes × 6 at `0x8007C998`)

Active count at `0x8007C990`. The decomp header is right about the size and the main pointers; what the code
shows about the rest:

| Off | What | Evidence |
|---|---|---|
| `0x00` | AI state 0–5 (index into the handler table at `0x8004C290`) | `func_80008C20` |
| `0x02` | Frame timer for the current state | all states |
| `0x04` | Approach steps left (set to 4) | `0x80009454`, `0x80009640` |
| `0x06` | Sub-phase inside the melee/cast state | `func_80009D78` |
| `0x08` | Flags: bit 0 = it's my turn, bit 1 = must act now, bit 2 = Mammon, bit 3 = hit/cast done, bit 4 = must approach first | states 0/1/4/5 |
| `0x0A` / `0x0C` | HP / MP (MP starts equal to HP; Drain Magic takes 11/64 of it; casting doesn't spend it) | spawn, `spell_type_drain_magic` |
| `0x10` | Movement bound: normal `agi × 0.2857 + 17`; boss `√agi × 3.846 + 17.838` | `0x800090C0`, `0x8000B8B4` |
| `0x1C` | Chosen action (pointer) | `func_8000A008` |
| `0x20` | → `MonsterBaseData` | spawn |
| `0x24` | `MonsterBattleData` (position `0x00`–`0x08`, facing `0x10`, scale `+0x24`, radius `+0x28`, model `+0x50` (−1 = free slot), `+0x5E` bit 0 = "is an enemy", `+0x64` → base data, `+0x68` → stats block) | spawn |
| `0x90` | Status flags (stats block, shared layout with Brian): bit 0 = attack halved, bit 1 = heals ¼ of melee damage dealt, bit 3 = skips its turn, bit 6 (`0x40`) = silenced (can't cast), bit 9 (`0x200`) = gains MP when hit | `func_8000AC84`, `0x80009BA4`, `0x800093F8`, `ai_spell_init`, `0x8000AF9C` |
| `0x114` / `0x116` / `0x118` | Atk / Agi / Def copied from the base record (buffs change these) | spawn |
| `0x11A` | Attack multiplier in 16ths (starts 16) | `func_8000AC84` |
| `0x120` | Movement-bound multiplier (starts 1.0) | `func_8000A008` |

### Damage rules (all from code)

- **Brian hits a monster** (`func_8000ACC0`, called by `spell_type_default` and the staff code):
  1. Element: `mask = bit[spellElement] | bit[monsterElement]`, bits at `0x8004C2A8` = Fire 1, Earth 2, Water 4,
     Wind 8, None 0. Jump table `0x80071198`: mask 1/2/4/8 (same element, or attacker vs a "None" monster) →
     **×0.5** (`0x8000AD88`); mask 5 (Fire+Water) or 10 (Earth+Wind) → **×1.25** (`0x8000AD94`); else ×1.
     Skipped when the attack's flag word has bit `0x8000`.
  2. `damage × atk / (atk + monsterDef)`, plus a random `0..√damage`, minimum 1.
  3. Monster goes to the hurt state; at 0 HP it dies, and its EXP (`0x10`) and `0x14` value are added to the
     battle totals (`0x8007BA68`/`0x8007BA6C`).
- **Monster hits Brian** (`func_80006BEC`): `damage × monsterAtk / (monsterAtk + BrianDef)` + random `0..√damage`.
  No element step at all.
- **Monster physical attack** (`func_8000AC84`): `atk × mult / 16`, halved if status bit 0.
  Hit chance (`func_8000ABF4`): `100 × agi / (agi + (BrianAgi + 7) / 8)` %; a miss counts toward Brian's
  agility growth.
- **Spell damage** (`func_800175D4`): base `= baseDamage × 0.02`, then `+ baseDamage × 0.004 × 1.03^k` for each
  of `P` steps. For Brian `P` comes from his element levels; **for a monster `P` is its `atk`**. So a monster's
  Atk powers both its punches and its spells.

### All regular monsters and bosses

"Set" = monster set above, "#" = index in the set (the ID used by packs). Element is the only defense stat.
EXP goes to element EXP; "0x14 reward" goes to the unused counter. Drop happens 1 time in 4 (bosses: always).
Spell names are the ROM's own (`0xD4B81C`).

| Set | # | Name | Element | HP | Agi | Def | Atk | EXP (0x10) | 0x14 reward | Drop (0x34) | Fly | Flags 0x24 | Actions (spell ID / melee) |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 0 | 0 | Were Hare | Wind | 9 | 5 | 1 | 3 | 2 | 7 | Fresh Bread |  | 1 | `300` Wind Cutter Lv1 |
| 0 | 1 | Hell Hound | Fire | 10 | 8 | 2 | 3 | 3 | 16 | Dew Drop |  | 0 | `000` Fire Ball Lv1 |
| 0 | 2 | Man Eater | Earth | 24 | 6 | 4 | 5 | 4 | 13 | none |  | 0 | `100` Rock Lv1 |
| 0 | 3 | Big Mouth | Water | 28 | 4 | 2 | 2 | 4 | 14 | Fresh Bread |  | 0 | `201` Water Pillar Lv2 |
| 0 | 4 | Bumbershoot | Fire | 9 | 6 | 2 | 3 | 3 | 10 | Fresh Bread |  | 1 | `000` Fire Ball Lv1, `004` Hot Steam Lv1 |
| 0 | 5 | Parassault | Water | 13 | 6 | 2 | 2 | 3 | 12 | Dew Drop |  | 1 | `206` Ice Knife, `202` Healing Lv1 |
| 0 | 6 | Ork Jr | Earth | 63 | 16 | 14 | 12 | 22 | 40 | Fresh Bread |  | 1 | `101` Rock Lv2, `102` Spirit Armor Lv1 |
| 0 | 7 | Gremlin | Wind | 83 | 46 | 15 | 13 | 24 | 55 | Silent Flute |  | 1 | `205` Ice Wall, `301` Wind Cutter Lv2 |
| 0 | 8 | Skeleton | Fire | 80 | 40 | 20 | 12 | 32 | 80 | Fresh Bread |  | 0 | `306` Large Cutter, `301` Wind Cutter Lv2 |
| 0 | 9 | Ghosthound | Wind | 99 | 43 | 16 | 12 | 25 | 75 | none |  | 1 | `001` Fire Ball Lv2 |
| 0 | 10 | Merrow | Water | 77 | 36 | 14 | 12 | 24 | 50 | Replica |  | 1 | `206` Ice Knife, `104` Weakness Lv1 |
| 0 | 11 | Wolf Goat | Earth | 95 | 39 | 17 | 14 | 36 | 80 | Honey Bread |  | 1 | `108` Avalanche |
| 1 | 0 | Goblin | Fire | 83 | 19 | 12 | 20 | 9 | 25 | Fresh Bread |  | 1 | `003` Homing Arrow Lv1 |
| 1 | 1 | Frog King | Water | 108 | 18 | 11 | 9 | 12 | 40 | Silver Amulet |  | 0 | `306` Large Cutter |
| 1 | 2 | Apophis | Wind | 128 | 21 | 12 | 14 | 15 | 50 | Spirit Light |  | 1 | `301` Wind Cutter Lv2 |
| 1 | 3 | Mad Doll | Earth | 70 | 17 | 11 | 10 | 8 | 35 | Dew Drop |  | 1 | `101` Rock Lv2, melee |
| 1 | 4 | Death Hugger | Wind | 50 | 20 | 9 | 10 | 6 | 20 | none |  | 0 | melee, `206` Ice Knife |
| 1 | 5 | Kobold | Wind | 40 | 16 | 6 | 7 | 5 | 20 | Fresh Bread |  | 0 | `000` Fire Ball Lv1, `30A` Cyclone |
| 1 | 6 | Man Trap | Earth | 65 | 8 | 4 | 11 | 8 | 33 | none |  | 1 | `20A` Healing Lv2, melee |
| 1 | 7 | Bat | Wind | 24 | 11 | 3 | 5 | 4 | 10 | Celine's Bell |  | 1 | `300` Wind Cutter Lv1, `200` Water Pillar Lv1 |
| 1 | 8 | Frog Knight | Water | 38 | 10 | 8 | 5 | 6 | 20 | Silver Amulet |  | 0 | `301` Wind Cutter Lv2, `30A` Cyclone |
| 1 | 9 | Marionasty | Earth | 28 | 10 | 6 | 6 | 4 | 30 | Fresh Bread |  | 1 | `301` Wind Cutter Lv2, melee |
| 1 | 10 | Dark Goblin | Wind | 80 | 35 | 14 | 12 | 20 | 30 | Replica |  | 1 | `301` Wind Cutter Lv2, `303` Evade Lv1 |
| 1 | 11 | Hot Lips | Fire | 125 | 30 | 6 | 14 | 24 | 50 | Celine's Bell |  | 1 | `008` Fire Pillar |
| 1 | 12 | Ghost Stalker | Earth | 75 | 28 | 18 | 11 | 20 | 38 | none |  | 0 | `101` Rock Lv2, melee |
| 1 | 13 | Treant | Water | 160 | 24 | 7 | 14 | 28 | 60 | Dew Drop |  | 0 | `301` Wind Cutter Lv2 |
| 1 | 14 | Cockatrice | Earth | 140 | 16 | 10 | 19 | 30 | 80 | none | yes | 0 | `10B` Rock Shower, `108` Avalanche |
| 2 | 0 | Multi Optics | Water | 175 | 56 | 23 | 28 | 72 | 105 | Dew Drop |  | 0 | `003` Homing Arrow Lv1, `20A` Healing Lv2 |
| 2 | 1 | Mimic | Water | 143 | 45 | 26 | 26 | 65 | 200 | none |  | 0 | `20C` Walking Water, `206` Ice Knife |
| 2 | 2 | Crawler | Water | 150 | 53 | 20 | 18 | 70 | 115 | none |  | 1 | `201` Water Pillar Lv2, `206` Ice Knife |
| 2 | 3 | Scorpion | Water | 175 | 62 | 19 | 28 | 80 | 110 | Dew Drop |  | 1 | `20C` Walking Water, `009` Homing Arrow Lv2 |
| 2 | 4 | Scare Crow | Wind | 120 | 38 | 10 | 19 | 42 | 80 | Honey Bread |  | 1 | `301` Wind Cutter Lv2 |
| 2 | 5 | Wyvern | Wind | 310 | 54 | 15 | 20 | 62 | 160 | Healing Potion | yes | 1 | `001` Fire Ball Lv2, `008` Fire Pillar |
| 2 | 6 | Skelebat | Wind | 79 | 46 | 9 | 15 | 30 | 60 | Honey Bread | yes | 0 | `301` Wind Cutter Lv2 |
| 2 | 7 | Cryshell | Water | 275 | 68 | 32 | 29 | 155 | 70 | none |  | 1 | `205` Ice Wall |
| 2 | 8 | Blood Jell | Earth | 97 | 32 | 11 | 20 | 40 | 80 | none |  | 0 | `009` Homing Arrow Lv2 |
| 2 | 9 | Caterpillar | Fire | 230 | 65 | 28 | 27 | 140 | 60 | Dew Drop |  | 1 | `008` Fire Pillar, `009` Homing Arrow Lv2 |
| 2 | 10 | Fish Man | Water | 275 | 27 | 27 | 35 | 175 | 70 | Honey Bread |  | 1 | `20C` Walking Water |
| 3 | 0 | Sandman | Earth | 340 | 80 | 70 | 40 | 420 | 200 | Honey Bread |  | 0 | melee, `106` Magnet Rock |
| 3 | 1 | Were Cat | Fire | 230 | 95 | 50 | 32 | 340 | 110 | Dragon's Potion |  | 1 | `00A` Fire Bomb, `001` Fire Ball Lv2 |
| 3 | 2 | Nightmare | Fire | 245 | 92 | 56 | 32 | 290 | 95 | Dew Drop |  | 1 | `001` Fire Ball Lv2, `304` Silence Lv1 |
| 3 | 3 | Blue Man | Water | 264 | 80 | 32 | 31 | 380 | 130 | Mint Leaves |  | 1 | `001` Fire Ball Lv2, `001` Fire Ball Lv2 |
| 3 | 4 | Winged Sunfish | Wind | 320 | 95 | 70 | 34 | 420 | 120 | Honey Bread | yes | 1 | `20C` Walking Water |
| 3 | 5 | Gloom Wing | Fire | 320 | 100 | 68 | 34 | 440 | 130 | Dew Drop |  | 1 | `008` Fire Pillar, `10A` Weakness Lv2 |
| 3 | 6 | Ogre | Water | 440 | 85 | 72 | 37 | 560 | 170 | Giant's Shoes |  | 1 | `005` Fire Ball Lv3, `00A` Fire Bomb |
| 3 | 7 | Rocky | Earth | 350 | 90 | 80 | 35 | 450 | 130 | Honey Bread |  | 0 | `105` Rock Lv3, `10B` Rock Shower |
| 3 | 8 | Red Wyvern | Fire | 550 | 120 | 85 | 40 | 650 | 220 | Healing Potion | yes | 0 | `009` Homing Arrow Lv2, `005` Fire Ball Lv3 |
| 3 | 9 | Flamed Mane | Fire | 360 | 80 | 60 | 36 | 450 | 160 | none |  | 0 | `00E` Hot Steam Lv2, `00A` Fire Bomb |
| 3 | 10 | Magma Fish | Fire | 242 | 90 | 80 | 34 | 320 | 110 | Mint Leaves | yes | 1 | `00C` Magma Ball, `107` Spirit Armor Lv2 |
| 3 | 11 | Red Rose Knight | Fire | 260 | 95 | 70 | 38 | 600 | 200 | Mint Leaves |  | 0 | `306` Large Cutter, `309` Evade Lv2 |
| 3 | 12 | White Rose Knight | Fire | 299 | 90 | 76 | 54 | 600 | 200 | Honey Bread |  | 0 | `003` Homing Arrow Lv1, `107` Spirit Armor Lv2 |
| 4 | 0 | Ork | Earth | 198 | 33 | 14 | 25 | 65 | 156 | Honey Bread |  | 1 | `00E` Hot Steam Lv2 |
| 4 | 1 | Ghost | Fire | 260 | 84 | 42 | 37 | 250 | 160 | Heroes Drink |  | 1 | `009` Homing Arrow Lv2, `10A` Weakness Lv2 |
| 4 | 2 | Will-O'-Wisp | Fire | 160 | 80 | 33 | 33 | 190 | 165 | none | yes | 1 | `001` Fire Ball Lv2 |
| 4 | 3 | Sprite | Wind | 242 | 78 | 38 | 30 | 220 | 150 | Spirit Light | yes | 1 | `00E` Hot Steam Lv2 |
| 4 | 4 | Jack-O'-Lantern | Earth | 160 | 30 | 30 | 24 | 60 | 85 | Silver Amulet |  | 0 | `009` Homing Arrow Lv2, `104` Weakness Lv1 |
| 4 | 5 | Arachnoid | Fire | 148 | 36 | 17 | 22 | 60 | 70 | Celine's Bell |  | 0 | `00C` Magma Ball |
| 4 | 6 | Lamia | Wind | 220 | 34 | 17 | 24 | 80 | 110 | Silver Amulet |  | 0 | `305` Wind Cutter Lv3 |
| 4 | 7 | Temptress | Water | 132 | 28 | 16 | 19 | 58 | 80 | Fresh Bread | yes | 1 | `00E` Hot Steam Lv2, `003` Homing Arrow Lv1 |
| 4 | 8 | Pixie | Wind | 231 | 70 | 26 | 28 | 90 | 40 | Mint Leaves | yes | 1 | `20C` Walking Water |
| 4 | 9 | Grangach | Water | 230 | 75 | 30 | 30 | 110 | 70 | Dew Drop |  | 1 | `20C` Walking Water |
| 4 | 10 | Thunder Jell | Earth | 198 | 25 | 15 | 26 | 80 | 50 | Honey Bread |  | 0 | `009` Homing Arrow Lv2, `202` Healing Lv1 |
| 4 | 11 | Termant | Water | 165 | 30 | 12 | 20 | 60 | 45 | Honey Bread |  | 1 | `301` Wind Cutter Lv2 |
| 5 | 0 | Judgment | Earth | 322 | 100 | 78 | 44 | 800 | 350 | Golden Amulet |  | 0 | `10B` Rock Shower, `105` Rock Lv3, `008` Fire Pillar |
| 5 | 1 | Pale Rider | Wind | 480 | 120 | 63 | 45 | 850 | 250 | Healing Potion |  | 0 | `30A` Cyclone, `306` Large Cutter |
| 5 | 2 | Pin Head | Fire | 580 | 130 | 70 | 46 | 900 | 300 | Celine's Bell |  | 0 | melee, `005` Fire Ball Lv3 |
| 5 | 3 | Spriggan | Water | 462 | 110 | 62 | 46 | 820 | 280 | Heroes Drink |  | 0 | melee, `304` Silence Lv1 |
| Boss | 0 | Solvaring | Earth | 200 | 17 | 15 | 6 | 100 | 200 | Earth Orb |  | 0 | `400` Sol_Panch, `401` Sol_Toge |
| Boss | 1 | Zelse | Wind | 680 | 60 | 30 | 22 | 1200 | 0 | Wind Jade |  | 0 | `402` Zel_Cat, `403` Zel_Beam, `306` Large Cutter |
| Boss | 2 | Nepty | Water | 880 | 80 | 65 | 24 | 2000 | 0 | Water Jewel |  | 0 | `404` Npt_Ball, `405` Npt_Turn |
| Boss | 3 | Shilf | Wind | 1000 | 90 | 77 | 27 | 2800 | 0 | none |  | 0 | `408` Sil_Laser, `409` Sil_Cat |
| Boss | 4 | Fargo | Fire | 1500 | 100 | 88 | 32 | 3600 | 0 | Fire Ruby |  | 0 | `406` Fag_Ball, `407` Fag_Bom |
| Boss | 5 | Guilty | None | 1800 | 120 | 88 | 34 | 5000 | 0 | none |  | 0 | `40A` Gil_Punch, `40B` Gil_Crow |
| Boss | 6 | Beigis | Fire | 1900 | 140 | 100 | 36 | 7000 | 0 | none |  | 0 | `40C` Ges_Cat, `40D` Ges_Beam |
| Boss | 7 | Mammon | None | 2300 | 150 | 100 | 41 | 0 | 0 | none | Mammon | 4 | `40E` On_Light, `40F` On_Wave, `410` On_Needle |


"Flags 0x24" bit 2 = random action choice. "Fly" = `monsterType`. The full per-action table is at the end.

## 2. Bosses

### Boss tables (fixed RAM, ROM `0xD873A0`…; RAM = ROM − `0xD77380` + `0x80399AB0`)

| What | RAM | ROM | Layout |
|---|---|---|---|
| Model bank ROM ranges | `0x803A9A90` | `0xD87360` | 8 × `{romStart, romEnd}`; DMA'd to `0x802A7BC0` when the boss's map loads |
| Field location | `0x803A9AD0` | `0xD873A0` | 8 × **0x18** **(correction: not 0x28)**: `u16 map, u16 submap, u16 modelBank, u16 pad, ptr model, f32 x, f32 z, f32 facing` |
| Trigger radius | `0x803A9B8C` + 4×boss# | `0xD8745C` | f32; boss# is 1-based: 75, 130, 65, 140, 120, 80, 140, 160 |
| Boss AI actions | `0x803A9D70` | `0xD87640` | same 0x18 action format |
| Boss data | `0x803A9F20` | `0xD877F0` | 8 × `MonsterBaseData` (0x38), same format as normal monsters |
| Battle model pointers | `0x803AA164` | `0xD87A34` | swapped in for the set's model table during a boss battle |
| Battle name pointers | `0x803AABB8` | `0xD88488` | swapped in for the set's name table |
| Arena radius | `0x8004D28C` + 4×boss# | `0x4DE8C` | f32: 85, 140, 100, 150, 150, 100, 100, 190 |
| Max HP reward | `0x8004C2C0` + 2×boss | `0x4CEC0` | u16: 10, 10, 15, 20, 20, 25, 25 (Mammon: ending instead) |
| Defeated flags | `0x8007D19C` (bits, mask table `0x8004D740`) | — | save data |
| Boss extra-effect lists | `0x80054640` | `0x55240` | 20 pointers (by `spellID & 0xFF`), each to a list of 0x18-byte steps at `0x800541D8`; effect defs at `0x80054690` (0x10 each) |

Order (boss# 1–8): Solvaring, Zelse, Nepty, Shilf, Fargo, Guilty, Beigis, Mammon. The ROM's internal spell
names show the developers' short names: SOL, ZEL, NPT, SIL, FAG, GIL, GES (Beigis), ON (Mammon).

### How a boss fight differs in code

1. **Spawn in the field** (`func_8000B530`, on map load): for each of the 8 locations matching the current
   map/submap whose defeated bit is clear (`func_8000B9D8`), DMA its model bank and place the boss
   (`func_8000B7DC`) into the field actor at `0x8007D1A8`; `0x8007D1A0` = boss# (1-based).
2. **Trigger** (`func_8000B740`): every frame, distance from Brian to the boss < trigger radius → start.
3. **Start** (`func_8001C8C4`): battle flags `0x8008C592 = 0x305` (bit `0x100` = boss battle). Saves and
   replaces the model/name tables with the boss ones, spawns one enemy from the boss table, sets the arena
   centre (Beigis: 0,0) and radius, plays music `0x29` for Mammon.
4. **During the fight** (all checks of bit `0x100`):
   - Animations play at full length (normal enemies use length/10 at speed 10) — `0x80009458`, `0x8000B030`.
   - While approaching, a boss moves to a point rotated around itself instead of straight at Brian (`0x800095D8`).
   - Movement bound grows with √agi instead of agi (`0x8000B8A8`).
   - After all enemies act, `boss_spell_anims_update` runs the boss extra-effect list queued by
     `ai_spell_init → boss_spell_anims_queue` (`0x8000A3D4`).
   - Escape (`spell_type_escape`, `0x80016988`) fails.
   - Mammon's record (`0x803AA0A8`) gets flag bit 2 and is lowered into place (`0x8000B94C`).
5. **End** (`func_8001CFE8`): the item drop roll is skipped (always drops, `func_8001D828`), sets the defeated
   bit (`func_8000BA0C`), gives the Max HP reward capped at 500 or runs the Mammon ending (`func_8000BB68`),
   then restores the set's model/name tables.

There is **no phase logic** anywhere: a boss is a normal monster with 2–3 actions. Phases, telegraphs and
adaptive behaviour would be new code (the natural hook is `func_8000A008`, the action chooser).

## 3. AI scripts

### Action entry (0x18 bytes)

| Off | Type | Meaning | Evidence |
|---|---|---|---|
| `0x00` | u16 | `0x8000 \| spellID` = cast; `0x0000` = physical attack (any value without bit 15) | `ai_spell_init` `0x8000A2A8` |
| `0x02` | u16 | Movement flags: bit 1 = keep moving during wind-up; bit 1 or 3 = keep moving after release. Bit 0 has no reader found | `0x80009DCC`, `0x80009ED4` |
| `0x04` | u16 | Wind-up frames before the spell launches / the attack becomes active | `0x8000A31C` |
| `0x06` | u16 | Frames the physical attack can hit; for spells, how long to hold the 2nd animation | `0x80009A8C`, `0x80009E98` |
| `0x08`, `0x0A`, `0x0C` | s16 | Animations: wind-up, after release (−1 = none), finish | `0x8000A378`, `0x80009E54` |
| `0x0E` | u16 | Always 0 | — |
| `0x10` | f32 | Maximum range (× model scale) | `func_8000A008` |
| `0x14` | f32 | **Physical attacks only**: reach of the hit (× scale). Ignored for spells (the notes call it "min range") | `0x80009B4C` |

### How a monster picks and performs an action

Six AI states, called each frame through the table at `0x8004C290` by `func_80008C20`:

| State | Function | What happens |
|---|---|---|
| 0 idle | `func_80009378` | Wanders. On its turn: if status bit 3, skip turn; else choose an action (`func_8000A008`); in range → `ai_spell_init`; otherwise state 1 with 4 steps |
| 1 approach | `func_80009588` | Walks toward Brian. Acts as soon as the gap < range; acts **anyway** when the 4 steps run out; re-chooses if it had none |
| 2 hurt | `func_80009818` | Plays hurt anim; at 0 HP: death effect, slot freed, EXP and `0x14` reward added |
| 3 | `func_80009934` | Turns toward Brian, waits for the timer, ends the turn (who sets state 3 is not traced) |
| 4 physical attack | `func_800099DC` | Wind-up → active window (hit if within reach and the hit roll passes) → finish |
| 5 cast | `func_80009D78` | Wind-up → `projectile_init(caster, spellID & 0x7FFF)` → after-release / finish anims |

**Choosing** (`func_8000A008`): gap = distance to Brian − Brian's radius. The actions are sorted by max range,
shortest first. Walking the list: the first action whose range covers the gap is picked; if the gap is only
within range + movement bound, the monster may pick it (random 50/50 against the next one) and approach first.
If none fits, it returns nothing and the monster walks. If base flag `0x24` bit 2 is set, a random action is
taken instead (Mammon). Turn order is by agility (`func_80009260`). Casting costs enemies nothing: they don't
use MP, level requirements or the "usable in battle" flag. Silence (status `0x40`) makes them skip.

`0x8000|spellID`: `ai_spell_init` only tests bit 15. The cast state removes it and calls
`projectile_init` (`0x80014A98`), which reads `D_800C1B14[(ID >> 8) & 0xF][ID & 0xFF]` (stats, 0x44 each) and
`D_800C3520[(ID >> 8) & 0xF][ID & 0xFF]` (visuals, 0x48 each). No bounds check. The projectile keeps
pointers to both records (`+0x2C`, `+0x30`) for its whole flight. A hit on Brian goes through the same spell
type handler as Brian's spells (`spell_type_default` → `func_80006BEC`).

## 4. Enemy spells

### Every spell enemies and bosses cast

52 distinct spells: **35 player spells** (103 of the 128 action entries) and the 17 boss-only spells
(`0x400`–`0x410`, the "enemy" table). Zelse also uses the player's Large Cutter. 8 entries are physical
attacks (Mad Doll, Death Hugger, Man Trap, Marionasty, Ghost Stalker, Sandman, Pin Head, Spriggan).

| ID | Spell | Action entries | Monsters |
|---|---|---|---|
| `000` | Fire Ball Lv1 | 3 | Hell Hound, Bumbershoot, Kobold |
| `001` | Fire Ball Lv2 | 7 | Ghosthound, Wyvern, Were Cat, Nightmare, Blue Man, Blue Man, Will-O'-Wisp |
| `003` | Homing Arrow Lv1 | 4 | Goblin, Multi Optics, White Rose Knight, Temptress |
| `004` | Hot Steam Lv1 | 1 | Bumbershoot |
| `005` | Fire Ball Lv3 | 3 | Ogre, Red Wyvern, Pin Head |
| `008` | Fire Pillar | 5 | Hot Lips, Wyvern, Caterpillar, Gloom Wing, Judgment |
| `009` | Homing Arrow Lv2 | 7 | Scorpion, Blood Jell, Caterpillar, Red Wyvern, Ghost, Jack-O'-Lantern, Thunder Jell |
| `00A` | Fire Bomb | 3 | Were Cat, Ogre, Flamed Mane |
| `00C` | Magma Ball | 2 | Magma Fish, Arachnoid |
| `00E` | Hot Steam Lv2 | 4 | Flamed Mane, Ork, Sprite, Temptress |
| `100` | Rock Lv1 | 1 | Man Eater |
| `101` | Rock Lv2 | 3 | Ork Jr, Mad Doll, Ghost Stalker |
| `102` | Spirit Armor Lv1 | 1 | Ork Jr |
| `104` | Weakness Lv1 | 2 | Merrow, Jack-O'-Lantern |
| `105` | Rock Lv3 | 2 | Rocky, Judgment |
| `106` | Magnet Rock | 1 | Sandman |
| `107` | Spirit Armor Lv2 | 2 | Magma Fish, White Rose Knight |
| `108` | Avalanche | 2 | Wolf Goat, Cockatrice |
| `10A` | Weakness Lv2 | 2 | Gloom Wing, Ghost |
| `10B` | Rock Shower | 3 | Cockatrice, Rocky, Judgment |
| `200` | Water Pillar Lv1 | 1 | Bat |
| `201` | Water Pillar Lv2 | 2 | Big Mouth, Crawler |
| `202` | Healing Lv1 | 2 | Parassault, Thunder Jell |
| `205` | Ice Wall | 2 | Gremlin, Cryshell |
| `206` | Ice Knife | 5 | Parassault, Merrow, Death Hugger, Mimic, Crawler |
| `20A` | Healing Lv2 | 2 | Man Trap, Multi Optics |
| `20C` | Walking Water | 6 | Mimic, Scorpion, Fish Man, Winged Sunfish, Pixie, Grangach |
| `300` | Wind Cutter Lv1 | 2 | Were Hare, Bat |
| `301` | Wind Cutter Lv2 | 10 | Gremlin, Skeleton, Apophis, Frog Knight, Marionasty, Dark Goblin, Treant, Scare Crow, Skelebat, Termant |
| `303` | Evade Lv1 | 1 | Dark Goblin |
| `304` | Silence Lv1 | 2 | Nightmare, Spriggan |
| `305` | Wind Cutter Lv3 | 1 | Lamia |
| `306` | Large Cutter | 5 | Skeleton, Frog King, Red Rose Knight, Pale Rider, Zelse |
| `309` | Evade Lv2 | 1 | Red Rose Knight |
| `30A` | Cyclone | 3 | Kobold, Frog Knight, Pale Rider |
| `400` | Sol_Panch | 1 | Solvaring |
| `401` | Sol_Toge | 1 | Solvaring |
| `402` | Zel_Cat | 1 | Zelse |
| `403` | Zel_Beam | 1 | Zelse |
| `404` | Npt_Ball | 1 | Nepty |
| `405` | Npt_Turn | 1 | Nepty |
| `406` | Fag_Ball | 1 | Fargo |
| `407` | Fag_Bom | 1 | Fargo |
| `408` | Sil_Laser | 1 | Shilf |
| `409` | Sil_Cat | 1 | Shilf |
| `40A` | Gil_Punch | 1 | Guilty |
| `40B` | Gil_Crow | 1 | Guilty |
| `40C` | Ges_Cat | 1 | Beigis |
| `40D` | Ges_Beam | 1 | Beigis |
| `40E` | On_Light | 1 | Mammon |
| `40F` | On_Wave | 1 | Mammon |
| `410` | On_Needle | 1 | Mammon |

### Giving enemies their own spells

**What assumes the shared tables:**

| Code | Reads | Matters for enemy copies? |
|---|---|---|
| `projectile_init` `0x80014B58`/`0x80014B5C` | `D_800C1B14[school]`, `D_800C3520[school]` | Yes: this is the one lookup that turns an action ID into a spell |
| Multi-shot queue (`0x80087170`, filled at `0x80014BCC`) | stores the spell **ID**, looked up again for later shots | Fine if the pointers are repointed (same lookup) |
| `boss_spell_anims_update` `0x8000B45C` | `D_800C1B14[school][idx].size` of the boss's current action | Fine (same lookup) |
| `boss_spell_anims_queue` `0x8000B2E0` | `0x80054640[ID & 0xFF]`, **ignores the school**, 20 entries, no bounds check; runs for every action in a boss battle | **Needs a fix** (below) |
| `spell_find_by_inputs`, `spell_cast_init`, `func_800149D0` | schools 0–3, 15 per element | Brian only; untouched |
| Spell name table `0xD4B81C` (77 names, flat) | — | Copies have no names; enemies never show spell names as far as we found |

Space: the enemy stat table (`0x800C1690`, 17 × 0x44) ends exactly where the pointer array `D_800C1B14` starts,
and right after that array sit the MP costs (`0x800C1B28`: 1, 2, 3). The enemy visuals table (`0x800C3058`)
ends exactly at `D_800C3520`, followed by the "trait" pointers at `0x800C3534`. So neither table nor pointer
array can grow in place, and a 6th school would collide with other data.

**Recommended plan (data-driven, one small patch):**

1. At boot, after `boot_load_static_data`, allocate a new stat table and a new visuals table in mod memory
   with room for 17 + 35 = 52 entries (or more for new enemy-only spells; the index is a byte, so up to 256).
2. Copy the 17 boss spells to entries `0x00`–`0x10` unchanged (so every boss keeps working), then copy each of
   the 35 player spells enemies use (stats from `D_800C1B14[s][i]`, visuals from `D_800C3520[s][i]`) to
   entries `0x11`…`0x33`.
3. Write the new addresses into `D_800C1B14[4]` (`0x800C1B24`) and `D_800C3520[4]` (`0x800C3530`).
4. Rewrite each action's ID from `0x8000 | (s << 8) | i` to `0x8400 | newIndex`:
   - boss actions once at boot (fixed RAM `0x803A9D70`; only Zelse's Large Cutter `0x8306` changes),
   - regular monsters **every time a set loads** (hook the end of `func_80008EF4`; the actions are at
     `0x8020E6F0 + (actionROM − setROMStart)`, see the action table below).
5. Fix `boss_spell_anims_queue`: only look up `0x80054640` when the index is < 0x11 and the school is 4;
   otherwise clear the queue. Today Zelse's Large Cutter reads entry 6 (Fargo's, which happens to be null),
   and an index ≥ 0x14 would read past the end of the table.

After this, player spells and enemy spells can be balanced separately, enemies can get new spells (status
payloads, elements, multi-shot) without touching Brian's, and the caster check in
`RuntimeProjectileTransform.x24_caster` is no longer needed for balance (still useful for mods).

Simpler but less clean alternative: leave the data alone and hook `projectile_init` — when the caster is an
enemy (`battleData +0x5E` bit 0), swap in a pointer to an enemy copy. It needs no ID rewriting but keeps the
"which spell is the enemy version" mapping in code instead of data.

Things to keep in mind when changing enemy spells:
- An enemy spell's **element doesn't matter against Brian** (no element step in `func_80006BEC`). It only
  matters if a spell hits a monster. Giving Brian resistances would be new code.
- Enemy spell damage scales with the monster's **Atk** (`func_800175D4`), then by Brian's Def.
- Enemies' heals and buffs work through the same handlers as Brian's (target type "self").

## Overhaul notes (what exists vs. what must be built)

| Overhaul feature | In the vanilla engine? |
|---|---|
| Per-monster weaknesses / resistances | No: one element per monster + a global rule (`0x8004C2A8`, jump table `0x80071198`). Needs a per-monster table and a patch in `func_8000ACC0` |
| Shields / Break | No. Natural place: `func_8000ACC0` (all hits on monsters pass through it) |
| Status resistances | No per-monster data. Statuses are bits in `EnemyAction +0x90` set by the spell handlers |
| Gold | No currency. Field `0x14` and counter `0x8007BA94` are a free, already-summed "reward" channel |
| Loot | One item per monster, 25% per battle, one item max (`func_8001D828`) |
| Boss phases / telegraphs / adaptive AI | No. Bosses are 2–3-action monsters; hook `func_8000A008` (choose) and `ai_spell_init` (wind-up length and animations already exist per action: `0x04`, `0x08`) |

## Proposed function names

**Merged into `Quest64Syms/names.toml` on 2026-10-20** (commit 778bbb4). Where the four write-ups proposed different names for the same address, names.toml wins. Changes from this table:

- `brian_take_damage` → `player_take_damage`
- `enemy_give_turn` → `enemy_turn_begin`
- `ai_melee_hit_roll` → `battle_hit_roll_vs_player` (spell_hit_check calls it too, not only melee)
- `battle_start_random` → `battle_try_random_encounter`; `battle_end` → `battle_update` (per-frame flow, not only the end)
- Data: `enemy_slots` → `gEnemies`, `enemy_count` → `gEnemyCount`, `enemy_set_table` → `gEnemySetTable`, `ai_state_handlers` → `gEnemyStateHandlers`, `element_weak_bits` → `gElementWeakMasks`, `boss_bonus_hp` → `gBossBonusHp`, `boss_spell_effect_lists` → `gBossSpellEffectLists`, `boss_defeated_flags` → `gBossDefeatedFlags`. The boss tables at 0x803A9xxx were not added (no data symbols there)

For the main session to merge into `Quest64Syms/names.toml`. CONFIRMED = behaviour read from the code;
PLAUSIBLE = consistent with the code but not fully traced.

| Address | Name | Evidence | |
|---|---|---|---|
| `0x80006BEC` | `brian_take_damage` | `dmg × atk/(atk+BrianDef 0x8007BBC0)` + random; called by `spell_type_default` and enemy melee | CONFIRMED |
| `0x80008BB0` | `enemies_reset` | Clears count `0x8007C990` and the 6 slots, then loads the set if map flag | CONFIRMED |
| `0x80008C20` | `enemies_update` | Per slot, calls `0x8004C290[state]`; then `boss_spell_anims_update` in boss battles | CONFIRMED |
| `0x80008EF4` | `enemy_set_load` | Picks row `0x80054160[map+0x12]`, DMAs it to `0x8020E6F0` | CONFIRMED |
| `0x80008F6C` | `enemies_free_all` | Frees every slot's model, count = 0 | CONFIRMED |
| `0x80008FE0` | `enemy_spawn` | Copies base record `idx×0x38` into a slot (HP, Atk/Agi/Def, bounds) | CONFIRMED |
| `0x80009260` | `battle_build_turn_order` | Sorts slot indices by agility into `0x8007D0B0` | CONFIRMED |
| `0x8000932C` | `enemy_give_turn` | Sets flag bit 0 on the enemy at turn index `0x8008C598` | CONFIRMED |
| `0x80009378` | `ai_state_idle` | State 0 handler | CONFIRMED |
| `0x80009588` | `ai_state_approach` | State 1 handler | CONFIRMED |
| `0x80009818` | `ai_state_hurt` | State 2: hurt, death, EXP/reward totals | CONFIRMED |
| `0x80009934` | `ai_state_face_and_wait` | State 3 | PLAUSIBLE |
| `0x800099DC` | `ai_state_melee` | State 4: physical attack | CONFIRMED |
| `0x80009D78` | `ai_state_cast` | State 5: calls `projectile_init` | CONFIRMED |
| `0x8000A008` | `ai_choose_action` | Range-sorted action choice | CONFIRMED |
| `0x8000A3F4` | `ai_turn_toward` | atan2 to a point, clamped turn | PLAUSIBLE |
| `0x8000A508` | `ai_move_step` | Called by every state to move | PLAUSIBLE |
| `0x8000ABF4` | `ai_melee_hit_roll` | `rand(100) < 100·agi/(agi+(BrianAgi+7)/8)` | CONFIRMED |
| `0x8000AC84` | `ai_melee_damage` | `atk × 0x11A / 16`, halved by status bit 0 | CONFIRMED |
| `0x8000ACC0` | `enemy_take_damage` | Element rule, def formula, HP, hurt state | CONFIRMED |
| `0x8000B140` | `enemy_index_from_battle_data` | Finds the slot of a battleData pointer | CONFIRMED |
| `0x8000B170` | `enemy_death_effect` | Called on death; uses effect table `0x80054690` | PLAUSIBLE |
| `0x8000B2B8` | `boss_spell_anims_clear` | Zeroes `0x8007D0C4` | CONFIRMED |
| `0x8000B530` | `boss_field_spawn` | Matches map to boss locations, loads model, spawns | CONFIRMED |
| `0x8000B740` | `boss_field_check_trigger` | Distance < radius → boss# | CONFIRMED |
| `0x8000B7DC` | `boss_spawn` | Like `enemy_spawn` but from `0x803A9F20` | CONFIRMED |
| `0x8000B9D8` | `boss_is_defeated` | Tests bit in `0x8007D19C` | CONFIRMED |
| `0x8000BA0C` | `boss_set_defeated` | Sets bit in `0x8007D19C` | CONFIRMED |
| `0x8000BB68` | `boss_give_reward` | Max HP bonus `0x8004C2C0` (cap 500); Mammon → ending | CONFIRMED |
| `0x8001C5F4` | `battle_start_random` | Calls `battle_spawn_pack` | PLAUSIBLE |
| `0x8001C8C4` | `battle_start_boss` | Flags `0x305`, swaps tables, `boss_spawn` | CONFIRMED |
| `0x8001CFE8` | `battle_end` | Defeated flag, reward, drop roll, restore tables | PLAUSIBLE (other parts belong to battle flow) |
| `0x8001D358` | `battle_end_turn` | Advances turn index / switches side | PLAUSIBLE |
| `0x8001D640` | `battle_spawn_pack` | Random pack from zone; spawns groups | CONFIRMED |
| `0x8001D828` | `battle_roll_item_drop` | 1/4 (bosses always), random enemy's `0x34` | CONFIRMED |
| `0x800175D4` | `spell_calc_power` | Base damage × level curve; enemy level = Atk | CONFIRMED |

Data (for `data_dump.toml` if wanted): `0x80054160` `enemy_set_table`, `0x8007C998` `enemy_slots`,
`0x8007C990` `enemy_count`, `0x8004C290` `ai_state_handlers`, `0x8004C2A8` `element_weak_bits`,
`0x8004C2C0` `boss_bonus_hp`, `0x803A9AD0` `boss_locations`, `0x803A9D70` `boss_actions`,
`0x803A9F20` `boss_data`, `0x80054640` `boss_spell_effect_lists`, `0x8007D19C` `boss_defeated_flags`.

## Still unknown

- Field `0x14`'s purpose: summed into `0x8007BA94` after a win; no reader found by scanning loads through
  Brian's stats pointer. It may be read under another base register, or be leftover. Watching `0x8007BA94` in
  the save code (saves session) would settle it.
- `MonsterBaseData +0x24` bit 0 and action flag `0x02` bit 0: set in the data, no reader found.
- Status bit `0x200` on a monster (gains MP when hit, capped at base `0x0A` = 0): which spell sets it.
- Who puts a monster in AI state 3.
- Which maps use which monster set: it's the map header byte `+0x12`; the notes' area labels are kept above
  but not checked map by map.

## Every action entry

ROM address of each action (add `0x8020E6F0 − setROMStart` for RAM; bosses: RAM = ROM − `0xD77380` +
`0x80399AB0`). Ranges are before model scale.

| Monster | ROM | Action | Flags 0x02 | Delay 0x04 | Hold 0x06 | Anims 0x08/0A/0C | Max range 0x10 | Melee reach 0x14 |
|---|---|---|---|---|---|---|---|---|
| Were Hare | `ADBEE0` | `8300` Wind Cutter Lv1 | 3 | 24 | 1 | 5/-1/-1 | 750 | 0 |
| Hell Hound | `ADBEF8` | `8000` Fire Ball Lv1 | A | 32 | 1 | 5/-1/-1 | 600 | 0 |
| Man Eater | `ADBF10` | `8100` Rock Lv1 | 7 | 23 | 1 | 5/-1/-1 | 750 | 0 |
| Big Mouth | `ADBF28` | `8201` Water Pillar Lv2 | 2 | 23 | 1 | 6/-1/-1 | 200 | 0 |
| Bumbershoot | `ADBF40` | `8000` Fire Ball Lv1 | 3 | 52 | 1 | 5/-1/-1 | 1000 | 0 |
| Bumbershoot | `ADBF58` | `8004` Hot Steam Lv1 | 2 | 20 | 1 | 6/-1/-1 | 200 | 0 |
| Parassault | `ADBF70` | `8206` Ice Knife | 3 | 24 | 1 | 5/-1/-1 | 800 | 0 |
| Parassault | `ADBF88` | `8202` Healing Lv1 | 2 | 20 | 1 | 6/-1/-1 | 1000 | 0 |
| Ork Jr | `ADBFA0` | `8101` Rock Lv2 | 3 | 34 | 1 | 5/-1/-1 | 750 | 0 |
| Ork Jr | `ADBFB8` | `8102` Spirit Armor Lv1 | 3 | 34 | 1 | 5/-1/-1 | 1000 | 0 |
| Gremlin | `ADBFD0` | `8205` Ice Wall | 3 | 29 | 1 | 5/-1/-1 | 250 | 0 |
| Gremlin | `ADBFE8` | `8301` Wind Cutter Lv2 | 2 | 20 | 1 | 6/-1/-1 | 1000 | 0 |
| Skeleton | `ADC000` | `8306` Large Cutter | 2 | 30 | 1 | 5/-1/-1 | 500 | 0 |
| Skeleton | `ADC018` | `8301` Wind Cutter Lv2 | 3 | 34 | 55 | 6/-1/-1 | 1000 | 100 |
| Ghosthound | `ADC030` | `8001` Fire Ball Lv2 | 2 | 35 | 1 | 5/-1/-1 | 750 | 0 |
| Merrow | `ADC048` | `8206` Ice Knife | 3 | 23 | 1 | 5/-1/-1 | 750 | 0 |
| Merrow | `ADC060` | `8104` Weakness Lv1 | 3 | 23 | 1 | 5/-1/-1 | 1000 | 0 |
| Wolf Goat | `ADC078` | `8108` Avalanche | 3 | 32 | 1 | 6/-1/-1 | 750 | 0 |
| Goblin | `B63120` | `8003` Homing Arrow Lv1 | 3 | 14 | 1 | 5/-1/-1 | 750 | 0 |
| Frog King | `B63138` | `8306` Large Cutter | 2 | 33 | 1 | 5/-1/-1 | 500 | 0 |
| Apophis | `B63150` | `8301` Wind Cutter Lv2 | 3 | 28 | 1 | 0/-1/-1 | 800 | 300 |
| Mad Doll | `B63168` | `8101` Rock Lv2 | 3 | 30 | 1 | 5/-1/-1 | 750 | 0 |
| Mad Doll | `B63180` | `0000` melee | 3 | 25 | 39 | 6/-1/-1 | 250 | 100 |
| Death Hugger | `B63198` | `0000` melee | 2 | 21 | 34 | 5/-1/-1 | 190 | 190 |
| Death Hugger | `B631B0` | `8206` Ice Knife | 3 | 37 | 1 | 6/-1/-1 | 1000 | 0 |
| Kobold | `B631C8` | `8000` Fire Ball Lv1 | 3 | 30 | 1 | 5/-1/-1 | 500 | 0 |
| Kobold | `B631E0` | `830A` Cyclone | 3 | 16 | 1 | 6/-1/-1 | 300 | 0 |
| Man Trap | `B631F8` | `820A` Healing Lv2 | 3 | 45 | 1 | 5/-1/-1 | 1000 | 0 |
| Man Trap | `B63210` | `0000` melee | 3 | 46 | 49 | 6/-1/-1 | 500 | 500 |
| Bat | `B63228` | `8300` Wind Cutter Lv1 | 2 | 15 | 30 | 5/-1/-1 | 1000 | 50 |
| Bat | `B63240` | `8200` Water Pillar Lv1 | 3 | 0 | 1 | 6/-1/-1 | 150 | 0 |
| Frog Knight | `B63258` | `8301` Wind Cutter Lv2 | 2 | 28 | 1 | 5/-1/-1 | 1000 | 0 |
| Frog Knight | `B63270` | `830A` Cyclone | 3 | 30 | 1 | 6/-1/-1 | 500 | 0 |
| Marionasty | `B63288` | `8301` Wind Cutter Lv2 | 3 | 24 | 1 | 5/-1/-1 | 750 | 0 |
| Marionasty | `B632A0` | `0000` melee | 3 | 25 | 39 | 6/-1/-1 | 250 | 100 |
| Dark Goblin | `B632B8` | `8301` Wind Cutter Lv2 | 3 | 30 | 1 | 5/-1/-1 | 650 | 0 |
| Dark Goblin | `B632D0` | `8303` Evade Lv1 | 3 | 40 | 0 | 6/-1/-1 | 1000 | 0 |
| Hot Lips | `B632E8` | `8008` Fire Pillar | 3 | 45 | 1 | 5/-1/-1 | 500 | 700 |
| Ghost Stalker | `B63300` | `8101` Rock Lv2 | 3 | 37 | 1 | 5/-1/-1 | 800 | 0 |
| Ghost Stalker | `B63318` | `0000` melee | 3 | 25 | 39 | 6/-1/-1 | 250 | 100 |
| Treant | `B63330` | `8301` Wind Cutter Lv2 | 3 | 13 | 1 | 5/-1/-1 | 1000 | 0 |
| Cockatrice | `B63348` | `810B` Rock Shower | 3 | 5 | 1 | 5/-1/-1 | 600 | 0 |
| Cockatrice | `B63360` | `8108` Avalanche | 3 | 5 | 1 | 5/-1/-1 | 1000 | 0 |
| Multi Optics | `BBDD00` | `8003` Homing Arrow Lv1 | 1 | 10 | 1 | 5/-1/-1 | 750 | 0 |
| Multi Optics | `BBDD18` | `820A` Healing Lv2 | 3 | 35 | 0 | 6/-1/-1 | 1000 | 0 |
| Mimic | `BBDD30` | `820C` Walking Water | 3 | 22 | 1 | 9/5/8 | 750 | 0 |
| Mimic | `BBDD48` | `8206` Ice Knife | 3 | 22 | 1 | 9/6/8 | 1000 | 0 |
| Crawler | `BBDD60` | `8201` Water Pillar Lv2 | 0 | 45 | 1 | 5/-1/-1 | 250 | 0 |
| Crawler | `BBDD78` | `8206` Ice Knife | 3 | 30 | 1 | 6/-1/-1 | 1000 | 0 |
| Scorpion | `BBDD90` | `820C` Walking Water | 0 | 15 | 34 | 5/-1/-1 | 250 | 100 |
| Scorpion | `BBDDA8` | `8009` Homing Arrow Lv2 | 3 | 35 | 1 | 6/-1/-1 | 750 | 15 |
| Scare Crow | `BBDDC0` | `8301` Wind Cutter Lv2 | 1 | 25 | 1 | 5/-1/-1 | 500 | 0 |
| Wyvern | `BBDDD8` | `8001` Fire Ball Lv2 | 3 | 29 | 1 | 3/-1/-1 | 600 | 0 |
| Wyvern | `BBDDF0` | `8008` Fire Pillar | 3 | 29 | 1 | 3/-1/-1 | 350 | 0 |
| Skelebat | `BBDE08` | `8301` Wind Cutter Lv2 | 3 | 1 | 1 | 5/-1/-1 | 750 | 0 |
| Cryshell | `BBDE20` | `8205` Ice Wall | D | 45 | 25 | 9/6/8 | 180 | 0 |
| Blood Jell | `BBDE38` | `8009` Homing Arrow Lv2 | 3 | 45 | 1 | 5/-1/-1 | 1000 | 0 |
| Caterpillar | `BBDE50` | `8008` Fire Pillar | 0 | 22 | 1 | 5/-1/-1 | 250 | 0 |
| Caterpillar | `BBDE68` | `8009` Homing Arrow Lv2 | 3 | 30 | 1 | 6/-1/-1 | 1000 | 0 |
| Fish Man | `BBDE80` | `820C` Walking Water | 1 | 25 | 1 | 5/-1/-1 | 500 | 0 |
| Sandman | `C31590` | `0000` melee | 3 | 30 | 32 | 5/-1/-1 | 300 | 300 |
| Sandman | `C315A8` | `8106` Magnet Rock | 0 | 60 | 1 | 6/-1/-1 | 1000 | 0 |
| Were Cat | `C315C0` | `800A` Fire Bomb | 0 | 60 | 1 | 5/-1/-1 | 1000 | 0 |
| Were Cat | `C315D8` | `8001` Fire Ball Lv2 | 2 | 60 | 1 | 5/-1/-1 | 500 | 0 |
| Nightmare | `C315F0` | `8001` Fire Ball Lv2 | 3 | 6 | 1 | 0/-1/-1 | 500 | 0 |
| Nightmare | `C31608` | `8304` Silence Lv1 | 3 | 6 | 1 | 0/-1/-1 | 1000 | 0 |
| Blue Man | `C31620` | `8001` Fire Ball Lv2 | 0 | 35 | 1 | 0/-1/-1 | 500 | 0 |
| Blue Man | `C31638` | `8001` Fire Ball Lv2 | 0 | 25 | 1 | 0/-1/-1 | 1000 | 0 |
| Winged Sunfish | `C31650` | `820C` Walking Water | 1 | 0 | 1 | 6/-1/-1 | 1000 | 0 |
| Gloom Wing | `C31668` | `8008` Fire Pillar | 3 | 6 | 1 | 0/-1/-1 | 300 | 0 |
| Gloom Wing | `C31680` | `810A` Weakness Lv2 | 3 | 6 | 1 | 0/-1/-1 | 800 | 0 |
| Ogre | `C31698` | `8005` Fire Ball Lv3 | 3 | 34 | 1 | 5/-1/-1 | 400 | 0 |
| Ogre | `C316B0` | `800A` Fire Bomb | 3 | 39 | 1 | 6/-1/-1 | 750 | 0 |
| Rocky | `C316C8` | `8105` Rock Lv3 | 3 | 29 | 32 | 5/-1/-1 | 750 | 0 |
| Rocky | `C316E0` | `810B` Rock Shower | 3 | 52 | 1 | 6/-1/-1 | 500 | 0 |
| Red Wyvern | `C316F8` | `8009` Homing Arrow Lv2 | 3 | 29 | 1 | 3/-1/-1 | 300 | 0 |
| Red Wyvern | `C31710` | `8005` Fire Ball Lv3 | 3 | 29 | 1 | 3/-1/-1 | 500 | 0 |
| Flamed Mane | `C31728` | `800E` Hot Steam Lv2 | 0 | 38 | 40 | 5/-1/-1 | 4000 | 0 |
| Flamed Mane | `C31740` | `800A` Fire Bomb | 3 | 34 | 1 | 6/-1/-1 | 10000 | 0 |
| Magma Fish | `C31758` | `800C` Magma Ball | 3 | 23 | 40 | 0/-1/-1 | 500 | 50 |
| Magma Fish | `C31770` | `8107` Spirit Armor Lv2 | 1 | 0 | 1 | 6/-1/-1 | 500 | 0 |
| Red Rose Knight | `C31788` | `8306` Large Cutter | 3 | 23 | 1 | 4/-1/-1 | 500 | 0 |
| Red Rose Knight | `C317A0` | `8309` Evade Lv2 | 3 | 23 | 1 | 4/-1/-1 | 1000 | 0 |
| White Rose Knight | `C317B8` | `8003` Homing Arrow Lv1 | 3 | 23 | 1 | 4/-1/-1 | 750 | 0 |
| White Rose Knight | `C317D0` | `8107` Spirit Armor Lv2 | 3 | 23 | 1 | 4/-1/-1 | 1000 | 0 |
| Ork | `C9BCD0` | `800E` Hot Steam Lv2 | 3 | 19 | 1 | 5/-1/-1 | 300 | 0 |
| Ghost | `C9BCE8` | `8009` Homing Arrow Lv2 | 2 | 37 | 1 | 5/-1/-1 | 700 | 0 |
| Ghost | `C9BD00` | `810A` Weakness Lv2 | 2 | 37 | 1 | 0/-1/-1 | 1000 | 0 |
| Will-O'-Wisp | `C9BD18` | `8001` Fire Ball Lv2 | 3 | 20 | 1 | 5/-1/-1 | 750 | 0 |
| Sprite | `C9BD30` | `800E` Hot Steam Lv2 | 3 | 11 | 1 | 5/-1/-1 | 250 | 0 |
| Jack-O'-Lantern | `C9BD48` | `8009` Homing Arrow Lv2 | 2 | 28 | 1 | 5/-1/-1 | 750 | 0 |
| Jack-O'-Lantern | `C9BD60` | `8104` Weakness Lv1 | 3 | 32 | 1 | 6/-1/-1 | 1000 | 0 |
| Arachnoid | `C9BD78` | `800C` Magma Ball | 0 | 19 | 1 | 5/-1/-1 | 750 | 0 |
| Lamia | `C9BD90` | `8305` Wind Cutter Lv3 | 3 | 34 | 1 | 5/-1/-1 | 500 | 0 |
| Temptress | `C9BDA8` | `800E` Hot Steam Lv2 | 3 | 11 | 1 | 5/-1/-1 | 250 | 0 |
| Temptress | `C9BDC0` | `8003` Homing Arrow Lv1 | 3 | 11 | 1 | 5/-1/-1 | 800 | 0 |
| Pixie | `C9BDD8` | `820C` Walking Water | 3 | 15 | 30 | 5/-1/-1 | 800 | 25 |
| Grangach | `C9BDF0` | `820C` Walking Water | 3 | 17 | 1 | 5/-1/-1 | 500 | 0 |
| Thunder Jell | `C9BE08` | `8009` Homing Arrow Lv2 | 1 | 1 | 0 | 0/-1/-1 | 750 | 0 |
| Thunder Jell | `C9BE20` | `8202` Healing Lv1 | 1 | 46 | 1 | 0/-1/-1 | 1000 | 0 |
| Termant | `C9BE38` | `8301` Wind Cutter Lv2 | 0 | 0 | 1 | 5/-1/-1 | 750 | 0 |
| Judgment | `CC4360` | `810B` Rock Shower | 0 | 0 | 1 | 0/-1/-1 | 1000 | 0 |
| Judgment | `CC4378` | `8105` Rock Lv3 | 1 | 0 | 1 | 0/-1/-1 | 750 | 0 |
| Judgment | `CC4390` | `8008` Fire Pillar | 1 | 0 | 1 | 0/-1/-1 | 300 | 0 |
| Pale Rider | `CC43A8` | `830A` Cyclone | 0 | 37 | 1 | 5/-1/-1 | 750 | 0 |
| Pale Rider | `CC43C0` | `8306` Large Cutter | 3 | 40 | 1 | 6/-1/-1 | 750 | 0 |
| Pin Head | `CC43D8` | `0000` melee | 3 | 30 | 60 | 5/-1/-1 | 350 | 300 |
| Pin Head | `CC43F0` | `8005` Fire Ball Lv3 | 3 | 30 | 1 | 6/-1/-1 | 600 | 0 |
| Spriggan | `CC4408` | `0000` melee | 3 | 32 | 1 | 5/-1/-1 | 350 | 300 |
| Spriggan | `CC4420` | `8304` Silence Lv1 | 2 | 25 | 1 | 6/-1/-1 | 750 | 0 |
| Solvaring | `D87640` | `8400` Sol_Panch | 2 | 21 | 1 | 3/-1/-1 | 1000 | 0 |
| Solvaring | `D87658` | `8401` Sol_Toge | 3 | 0 | 1 | 4/-1/-1 | 250 | 0 |
| Zelse | `D87670` | `8402` Zel_Cat | 2 | 28 | 34 | 3/-1/-1 | 1000 | 0 |
| Zelse | `D87688` | `8403` Zel_Beam | 3 | 12 | 1 | 4/-1/-1 | 500 | 0 |
| Zelse | `D876A0` | `8306` Large Cutter | 3 | 28 | 1 | 3/-1/-1 | 250 | 0 |
| Nepty | `D876B8` | `8404` Npt_Ball | 2 | 16 | 1 | 3/-1/-1 | 800 | 0 |
| Nepty | `D876D0` | `8405` Npt_Turn | 2 | 5 | 1 | 4/-1/-1 | 250 | 0 |
| Shilf | `D876E8` | `8408` Sil_Laser | 2 | 30 | 40 | 3/-1/-1 | 700 | 0 |
| Shilf | `D87700` | `8409` Sil_Cat | 3 | 23 | 1 | 4/-1/-1 | 5000 | 0 |
| Fargo | `D87718` | `8406` Fag_Ball | 2 | 26 | 1 | 3/-1/-1 | 800 | 0 |
| Fargo | `D87730` | `8407` Fag_Bom | 2 | 23 | 1 | 4/-1/-1 | 400 | 0 |
| Guilty | `D87748` | `840A` Gil_Punch | 3 | 33 | 1 | 3/-1/-1 | 400 | 0 |
| Guilty | `D87760` | `840B` Gil_Crow | 2 | 60 | 1 | 4/-1/-1 | 800 | 0 |
| Beigis | `D87778` | `840C` Ges_Cat | 3 | 35 | 1 | 2/-1/-1 | 300 | 0 |
| Beigis | `D87790` | `840D` Ges_Beam | 3 | 34 | 30 | 3/4/5 | 1000 | 0 |
| Mammon | `D877A8` | `840E` On_Light | F | 60 | 40 | 3/4/5 | 1500 | 0 |
| Mammon | `D877C0` | `840F` On_Wave | 2 | 14 | 1 | 6/-1/-1 | 1500 | 0 |
| Mammon | `D877D8` | `8410` On_Needle | 0 | 39 | 1 | 7/-1/-1 | 1000 | 0 |

