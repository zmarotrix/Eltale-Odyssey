# Saving and loading (Quest 64 USA 1.0)

Found 2026-10-20 by static analysis of the recompiled code (`Quest64-Recomp/RecompiledFuncs`), the recomp runtime's
Controller Pak code (`lib/N64ModernRuntime/librecomp/src/pak.cpp`), the ROM, and the save file already in the repo
(`Quest64-Recomp/controllerPak_file_0.sav`, a save made at the Melrode Inn). The game was not run.
Data names are from `Quest64Syms/data_dump.toml`.

## In plain words

- **Each save slot is one Controller Pak "note" of 512 bytes**, named `QUEST64` with a letter (A, B, C...). The game
  supports **up to 16 slots** (one per note; a Controller Pak can hold at most 16 notes). In the PC version every note
  is its own file: `controllerPak_file_0.sav`, `controllerPak_file_1.sav`, ... plus `controllerPak_header.sav`
  (a 32-byte directory entry per note), all in the game's working folder.
- **Only 300 of the 512 bytes hold real data.** A 100-byte "summary" (Brian's stats, day, play time, where you saved,
  settings) and a 200-byte "world" part (opened chests, spirits found, inventory, bosses beaten).
  52 more bytes get written but hold leftover junk, and **the last 160 bytes are never touched**.
- **There is no checksum.** The game copies bytes in and out and trusts them. Editing a save file by hand works.
- **You save at inns and a few houses, not anywhere.** There are 17 save places (table below). The save records the
  room you are in; loading puts Brian back at a fixed spot in that room. The time of day is not saved (the day number is).
- **Not saved today:** anything the overhaul adds. Gold, unlocked spells, charms, shop stock, Spirit Points all need
  new room. **Recommendation:** put an "overhaul block" in the unused last 160 bytes of each save note (same file, saved
  and loaded together with the vanilla data), with a version number and checksum. 160 bytes is enough for the planned
  features if stored compactly (budget below). If the overhaul later needs more, add a PC-side extra file keyed by an
  ID kept in that block. Details in "Where can overhaul data go?".

## 1. What gets saved (the save block)

Saving is two copies. `func_8002E234(slot)` copies game state into two RAM buffers; `func_8002E404(slot)` copies them
back. Then the pak code writes them to the note.

- **Summary block**, 100 bytes (0x64) per slot, at `0x8008FD58 + slot * 100` (`D_8008FD58`, 16 entries end exactly at
  `0x80090398`). Written to note offset `0x00`.
- **World block**, 200 bytes (0xC8), one shared staging buffer at `0x80090398` (`D_80090398`). Written to note offset `0x80`.

The summary blocks of all slots are read when the save menu opens (the list shows them); the world block is read only
when you pick a slot to load.

### Summary block (note offset 0x00-0x63)

| Offset | Size | From RAM | Meaning | Value in the repo's save |
|---|---|---|---|---|
| `0x00` | 0x38 bytes | `0x8007BA80` `gPlayerMainData` +0x00..+0x37 | Brian's stat block, copied whole (rows below) | |
| `0x00` | u16 | `0x8007BA80` | unk0 | 0 |
| `0x02` | s16 | `0x8007BA82` | unk2 | 0 |
| `0x04` | u16 | `0x8007BA84` | HP | 53 |
| `0x06` | u16 | `0x8007BA86` | Max HP | 53 |
| `0x08` | u16 | `0x8007BA88` | MP | 15 |
| `0x0A` | u16 | `0x8007BA8A` | Max MP | 15 |
| `0x0C` | u16 | `0x8007BA8C` | Agility | 5 |
| `0x0E` | u16 | `0x8007BA8E` | Defense | 5 |
| `0x10` | u32 | `0x8007BA90` | Element EXP | 6 |
| `0x14` | u32 | `0x8007BA94` | Decomp calls it `stone`, "unused currency"; stats research saw it rise 532 -> 564 after a won battle, so it may be total EXP | 532 |
| `0x18` | f32 | `0x8007BA98` | Collision size | 50.0 |
| `0x1C` | f32 | `0x8007BA9C` | Height (decomp guess) | 160.0 |
| `0x20` | f32 | `0x8007BAA0` | Collision size, other axis (decomp guess) | 0.07 |
| `0x24` | u8 x4 | `0x8007BAA4` | Fire, Earth, Water, Wind levels | 1, 1, 1, 1 |
| `0x28` | u16 x4 | `0x8007BAA8` | HP, MP, Agility, Defense growth EXP | 1, 17, 5, 13 |
| `0x30` | u8 x4 | `0x8007BAB0` | HP, MP, Agility, Defense levels gained | 3, 0, 0, 1 |
| `0x34` | u8 | `0x8007BAB4` | Element levels gained through battle | 0 |
| `0x35` | u8 x3 | `0x8007BAB5` | No direct reads/writes found (may be reached through a pointer) | 0 |
| `0x38` | u16 | `0x800859E2` `D_800859E2` | Play time (likely minutes, see `research/time`) | 5 |
| `0x3A` | u16 | `0x800859E0` `gCurrentDay` | Day number | 2 |
| `0x3C` | s32 | `0x80084EE4` `gNextMap` | Map of the room you saved in (picks the location name and the spawn point) | 15 |
| `0x40` | s32 | `0x80084EE8` `gNextSubmap` | Submap of that room | 5 (= Melrode Inn) |
| `0x44` | s32 | `0x80084EF8` | **Return** spell destination: map | 2 |
| `0x48` | s32 | `0x80084EFC` | Return destination: submap | 0 |
| `0x4C` | s32 | `0x80084F00` | Return destination: entrance number | 0 |
| `0x50` | s32 | `0x80084F04` | **Exit** spell destination: map (-1 = none) | -1 |
| `0x54` | s32 | `0x80084F08` | Exit destination: submap | 0 |
| `0x58` | s32 | `0x80084F0C` | Exit destination: entrance number | 0 |
| `0x5C` | u16 | `0x8007BA60` | Map of the last room entered | 15 |
| `0x5E` | u16 | `0x8007BA62` | Submap of the last room entered | 5 |
| `0x60` | u16 | `0x8007BA64` | Entrance used to enter it (`D_80085370`; 0xFFFF = none) | 0xFFFF |
| `0x62` | u8 | `0x8005FA00` | Message speed setting | 2 |
| `0x63` | u8 | `0x8005F010` | Music volume setting | 2 |

Evidence: field copies at `0x8002E264`-`0x8002E35C`. Return/Exit: the Return spell handler `func_800169C8` (spell
type 4) warps to `0x80084EF8/EFC/F00`, the Exit handler `func_80016B18` (type 6) warps to `0x80084F04/F08/F0C`, and
`func_8000BD10` updates the Return destination on room changes. `0x8007BA60/62/64` are set from the current
map/submap/entrance on every room change (`0x80001DB8`) and set to 13/17/0 at a new game (`func_80000EEC`).

### World block (note offset 0x80-0x147)

| Note offset | Block offset | Size | From RAM | Meaning |
|---|---|---|---|---|
| `0x80` | `0x00` | 32 bytes | `0x800869D8` | Treasure chest opened flags (bits). Community notes say `0x869D8-0x869E3` are in use, so likely only 12 of the 32 bytes |
| `0xA0` | `0x20` | 16 bytes | `0x80086AE8` | Spirit collected flags (128 bits; the game has 98 spirits) |
| `0xB0` | `0x30` | 151 bytes (0x97) | `0x8008CF78` `gInventory` | Inventory. The repo's save has item 0x05 then 0xFF (empty) in every other byte |
| `0x147` | `0xC7` | 1 byte | `0x8007D19C` | Boss defeated flags (bits) |

Evidence: copy loops at `0x8002E360` (0x20), `0x8002E38C` (0x10), `0x8002E3B8` (0x97), `0x8002E3E4` (1).

**Not saved:** time of day (`gCurrentTime`), Brian's exact position (he is put at the save point instead), current
status effects, and anything outside the tables above (for example `D_8008C634`, the unused week value).

## 2. The Controller Pak note

### Note identity

| Field | Value | Evidence |
|---|---|---|
| Company code | `0x3738` ("78") | `func_80031890` / `func_80031574` |
| Game code | `0x4E455445` ("NETE", same as the ROM header) | same |
| Note name | "QUEST64" in N64 pak characters (`2A 2E 1E 2C 2D 16 14`) at `0x8005FB40` | ROM `0x60740` |
| Extension | One letter per slot, A, B, C... (`0x1A` + n) | `func_80031800`, `func_8002D8BC` |
| Size | 512 bytes (`0x200`) = 2 pak pages | `func_80031890` passes 0x200 to `osPfsAllocateFile` |

### Byte use in the 512-byte note

| Note offset | Size | What is there |
|---|---|---|
| `0x000-0x063` | 100 | Summary block (real data) |
| `0x064-0x07F` | 28 | Junk: the game writes 0x80 bytes from a 100-byte-per-slot buffer, so these are the first 28 bytes of the *next* slot's summary. Ignored on load |
| `0x080-0x147` | 200 | World block (real data) |
| `0x148-0x15F` | 24 | Junk: the write is 0xE0 bytes but the block is 0xC8; these come from `D_80090460`, the menu's read buffer |
| `0x160-0x1FF` | **160** | **Never written** (zero in the repo's save) |

Writes: `func_8002D8BC` calls `func_80031A44(pak, file, offset 0, 0x80, summary)` at `0x8002DA14` and
`func_80031A44(pak, file, offset 0x80, 0xE0, 0x80090398)` at `0x8002DAEC`. Reads: `func_8002DD18` reads 0x80 bytes at
offset 0 of every note into `0x80090460` and keeps 0x64 of them (`0x8002DE9C`); `func_8002D748` reads 0xE0 bytes at
offset 0x80 into `0x800904E0` and keeps 0xC8 (`0x8002D780`). The repo's save file matches this byte for byte, including
the junk at `0x148` (a copy of a stats block).

### Slots

- Up to **16 slots**: the slot list at `0x8008FD30` has 16 two-byte entries (pak file number, extension letter), and
  the summary buffer has room for exactly 16. `func_8002DD18` fills the list from the notes found by
  `func_80031574`, which checks all 16 pak directory entries for the right company code, game code and name.
- A "new save" entry is added to the list only if the pak has at least 2 free pages and fewer than 16 notes are used.
  Saving to it creates a new note (`func_80031890`) with the first unused letter.
- Deleting a slot deletes the note (`func_8002D614` -> `func_8003195C` -> `osPfsDeleteFile`).
- The selected slot is `0x8008FD2A` (s8). (The community notes list it as `0x08fd29`, the byte-swapped address.)

### Checksum

**None.** No code between the copy routines and the pak calls computes or checks a sum; the bytes go straight to
`osPfsReadWriteFile`. (The "Things that change the checksum" sheet is about the ROM, not saves.)

### How the PC version stores it

`pak.cpp` replaces the N64 pak functions. Each note becomes `controllerPak_file_<n>.sav` (n = directory index 0-15) and
`controllerPak_header.sav` holds a 32-byte directory entry per note (size, game code, company code, extension, name).
Free space is reported as 123 pages minus the pages used. Data in the `.sav` files is in N64 byte order (the same
bytes a real pak would hold), so offsets in this document can be read straight from the file with a hex viewer.
Note: the runtime's read/write does not check the note size, and the files use relative paths (current folder).

## 3. Where and how the player saves

- **Trigger:** when bit `0x400` of `gGameState` (`0x8007B2E4`) is set, the field loop (`func_80001800`, `0x80002070`)
  calls `func_8002B510(4)`, which sets bit `0x4000` of the menu flags (`0x8008FD0C`) and resets the save menu.
  `func_80026A7C` then runs the save menu (`func_8002B57C`) every frame while that bit is set.
  The code that sets bit `0x400` was not found by searching for direct writes; it is most likely the innkeeper's
  dialogue/event script, which writes through a pointer. (Checking this needs the NPC/event research or a live test.)
- **From the title screen** (`func_800011DC`, `0x80001680`/`0x80001690`) the same menu opens with mode 0 or 8 to load.
- **Where loading resumes:** the save stores the room (`gNextMap`/`gNextSubmap`, offsets 0x3C/0x40). On load,
  `func_80002EA0` looks that room up in `gSaveSpawnLocationTbl` (`0x80054048`, ROM `0x54C48`, 17 entries of
  map u16, submap u16, x, z, facing as floats) and puts Brian at that spot.
- **The name shown in the save list** comes from the same room: `func_8002C818` switches on the slot's map/submap
  (`0x8002CA80`) and prints a name from `gSaveLocationNameTable` (`0x8006AC00`, ROM `0x6B800`).

### The 17 save locations

| # | Name (from ROM) | Map | Submap | Room (community `Map ID's` sheet) |
|---|---|---|---|---|
| 0 | Melrode Inn | 0x0F | 0x05 | Melrode Inn |
| 1 | Dondoran Inn | 0x10 | 0x07 | Dondoran Inn |
| 2 | Dondoran Flats Inn | 0x10 | 0x0F | Don Flats Inn |
| 3 | West Carmagh Inn | 0x12 | 0x06 | Efna's Place |
| 4 | Larapool Inn | 0x11 | 0x00 | Larapool Hotel Tavern |
| 5 | Cull Hazard Cottage | 0x12 | 0x07 | Dorothy's Place |
| 6 | Normoon Inn | 0x14 | 0x05 | Normoon Inn |
| 7 | Limelin Wharf | 0x16 | 0x16 | Sabastian's Place |
| 8 | Limelin Inn | 0x16 | 0x0E | Limelin Inn |
| 9 | Greenoch Inn | 0x17 | 0x00 | A Burnt Town's Inn |
| 10 | Dindom Dries Hut | 0x17 | 0x14 | Walt's Desert House |
| 11 | Baragoon Moor House | 0x17 | 0x16 | Bation's Place |
| 12 | Brannoch Inn | 0x17 | 0x08 | Brannoch Inn |
| 13 | Epona's Cottage | 0x1A | 0x03 | Epona's Cave House |
| 14 | Seagoing Ship | 0x19 | 0x03 | Downstairs (ship) |
| 15 | Brannoch Castle | 0x1E | 0x0F | "Prince Leonardo Arrived" (castle room) |
| 16 | Floating Monastery | 0x0F | 0x0D | Epona's House (Mammon) |

Names decoded from the pointer table at ROM `0x6B800` (letters are 0=A ... 25=Z, `0x7F` = space, `0xFF` = end; each
starts with formatting codes `A0 C0 81`). Map/submap/spawn from ROM `0x54C48`. All 17 are indoor rooms (inns and
houses). The list's order is the order of the spawn table; the name lookup in `func_8002C818` uses its own switch, so
the two lists are separate data that mostly match. One possible mismatch: for map 0x17 the switch checks submap 0x13
for "Dindom Dries Hut", but the spawn table has 0x14 (`0x8002CB6C`), so that save may show the wrong name in the list (not tested). For reference, a new game starts in map 0x0D, submap 0x11
("First Room (Grand Abbott)"), which is not a save point.

### Save menu steps (checked 2026-10-20)

`save_menu_update` runs one step per frame, picked by the low 3 bits of `gSaveMenuState` through the jump table at
`0x80071764` (read from the ROM). **The order is not the order the calls appear in the code:**

| Step | Calls | What it does |
|---|---|---|
| 0 | `save_menu_draw_slots(0)` | Slot list |
| 1 | `func_8002DBE0` | (not traced) |
| 2 | `save_load_slot` | **Load** the selected slot |
| 3 | `save_write_slot` | **Write** the selected slot |
| 4 | `save_delete_slot` | Delete the selected slot |
| 5 | `func_8002BCA0` | (not traced) |
| 6 | `func_8002D5D4` | (not traced) |

Setting the step by hand to 3 thinking it was "load" overwrote a real save on 2026-10-20. To load without the menu,
call `pak_scan_notes`, `save_read_slot_list` and `save_load_slot` directly (`Quest64-ModTemplate/examples/quick-load`).

## 4. Where can overhaul data go?

What the overhaul needs, packed tightly (rough guess, to adjust when the design settles):

| Data | Bytes |
|---|---|
| Header: magic, version, checksum | 8 |
| Gold (u32) | 4 |
| Spirit Points: current, max (u16 each) | 4 |
| Unlocked spells: 4 elements x up to 16 spells, 1 bit each | 8 |
| Charms/sigils: owned bits for up to 64 | 8 |
| Charms/sigils: equipped slots (up to 8, 1 byte each) | 8 |
| Shop state: "bought/sold out" bits, up to 256 entries | 32 |
| Spare for later | 88 |
| **Total** | **160** |

Per-item shop stock counts (1 byte each) would not fit next to everything else; use bits or the extra file (option C).

### Options

**A. The unused tail of each note (offset `0x160-0x1FF`, 160 bytes). Recommended.**
Change the game's save routine to also write the overhaul block at note offset 0x160, and the load routine to read it.
Patch points: after the second write in `func_8002D8BC` (`0x8002DAEC`) and after the read in `func_8002D748`
(`0x8002D780`); the copy-in/copy-out can sit next to `func_8002E234`/`func_8002E404`.
Good: one file per save, so the new data is always saved, copied, deleted and backed up together with the vanilla data;
the vanilla game ignores those bytes; old saves load fine (all zero = "no overhaul data yet", so start a new block).
Limit: 160 bytes. The 28 + 24 junk bytes can be reclaimed if the writes are trimmed to 0x64 and 0xC8 bytes, for
212 bytes in total, but that touches the vanilla layout and is not needed yet.

**B. Unused bytes inside the vanilla blocks.** Very little: 3 bytes at summary 0x35-0x37 (no direct use found, but
not proven unused), maybe the `stone` u32 at 0x14 (it changes after battles, so probably in use), spare bits in the
chest flags (20 of 32 bytes may be unused) and spirit flags (about 30 spare bits). Too small and too risky for
anything but a "save made by the overhaul" marker. Not recommended.

**C. A PC-side extra file (recomp save API).** Add C++ in the runtime (next to `pak.cpp`) that writes, for example,
`overhaul_slot_<n>.sav` when note n is written and reads it when it is loaded. No size limit, and it can use any format.
Risks: two files per save can get out of step (a deleted or re-created note reuses the number `n`, so a stale extra
file would attach to a new save), and copying only the `.sav` loses overhaul progress. If used, key it by a random ID
stored in the note tail (option A), not by the note number.

**D. A second pak note per slot.** Not recommended: notes are limited to 16 in total, so it halves the slot count, and
the game's slot scan (`func_80031574`) would list any note with the QUEST64 name as a save slot unless the name or
code is changed.

**Recommendation:** A, with a version byte and a simple checksum in its 8-byte header (the vanilla save has none, so
the overhaul should protect its own data). Keep the overhaul's live values in one RAM struct so the save/load patch is
a single copy, like the vanilla code does. Move to A + C only if the data outgrows 160 bytes.

## Proposed function names

**Merged into `Quest64Syms/names.toml` on 2026-10-20** (commit 778bbb4). Where the four write-ups proposed different names for the same address, names.toml wins. Changes from this table:

- `spell_type4_return` → `spell_type_return`, `spell_type6_exit` → `spell_type_exit`
- `gLastRoomMap` (+2, +4) → `gRespawnMap`, `gRespawnSubmap`, `gRespawnEntrance` (the NPC write-up showed it's the inn respawn point)
- `gBossFlags` → `gBossDefeatedFlags`

| Address | Proposed name | Evidence |
|---|---|---|
| `0x8002E234` | `save_gather_slot_data` | Copies stats, day, map, settings into the slot's 100-byte summary and the 200-byte world buffer |
| `0x8002E404` | `save_apply_slot_data` | Exact reverse copy of `func_8002E234` |
| `0x8002D8BC` | `save_write_slot` | Creates a note if the slot is new, calls `func_8002E234`, writes 0x80 bytes at 0 and 0xE0 at 0x80 |
| `0x8002D748` | `save_load_slot` | Reads 0xE0 bytes at note offset 0x80, then calls `func_8002E404` |
| `0x8002DD18` | `save_read_slot_list` | Builds the 16-entry slot list, reads each note's 0x80-byte summary |
| `0x8002D614` | `save_delete_slot` | Calls `func_8003195C` (`osPfsDeleteFile`) |
| `0x8002B510` | `save_menu_open` | Sets menu flag 0x4000 plus a mode, resets save menu state; called for inns (mode 4) and the title screen |
| `0x8002B57C` | `save_menu_update` | State machine that calls the scan, draw, load, save and delete functions above |
| `0x8002C818` | `save_menu_draw_slots` | Draws the slot list; picks the location name from `gSaveLocationNameTable` by the slot's map/submap |
| `0x80002EA0` | `player_place_at_save_point` | Finds `gNextMap`/`gNextSubmap` in `gSaveSpawnLocationTbl`, sets Brian's position |
| `0x80031574` | `pak_scan_notes` | `osPfsInitPak`, then `osPfsFileState` on all 16 notes, matching "NETE"/"78"/"QUEST64" |
| `0x80031800` | `pak_find_free_extension` | Returns the first letter A-P not used by an existing note (no callers found) |
| `0x80031890` | `pak_create_note` | `osPfsAllocateFile(0x3738, "NETE", "QUEST64", ext, 0x200)` |
| `0x8003195C` | `pak_delete_note` | `osPfsDeleteFile` with the same identity |
| `0x800319E0` | `pak_read` | `osPfsReadWriteFile(PFS_READ, ...)` wrapper (already in the decomp) |
| `0x80031A44` | `pak_write` | `osPfsFreeBlocks` check, then `osPfsReadWriteFile(PFS_WRITE, ...)` |
| `0x80031BB0` | `pak_error_to_code` | Maps PFS errors to -1..-6 (already in the decomp) |
| `0x800169C8` | `spell_type4_return` | Return spell handler; warps to `0x80084EF8`/`EFC`/`F00` |
| `0x80016B18` | `spell_type6_exit` | Exit spell handler; warps to `0x80084F04`/`F08`/`F0C` |
| `0x8000BD10` | `update_return_destination` | Writes `0x80084EF8`/`EFC`/`F00` from the current map, submap and entrance on room changes (partly read) |

Proposed data names: `0x8008FD58` `gSaveSlotSummaries` (16 x 100 bytes), `0x80090398` `gSaveWorldBuffer`,
`0x80090460` `gSaveSummaryReadBuf`, `0x800904E0` `gSaveWorldReadBuf`, `0x8008FD30` `gSaveSlotList`,
`0x8008FD2A` `gSaveSlotCursor`, `0x8008FD0C` `gMenuFlags`, `0x8008FD20` `gSaveMenuState`,
`0x80084EF8` `gReturnMap` (+4 submap, +8 entrance), `0x80084F04` `gExitMap` (+4, +8),
`0x8007BA60` `gLastRoomMap` (+2 submap, +4 entrance), `0x800869D8` `gChestFlags`, `0x80086AE8` `gSpiritFlags`,
`0x8007D19C` `gBossFlags`, `0x8005FA00` `gMessageSpeed`, `0x8005F010` `gBgmVolume`.

## Still unknown

- Which code sets `gGameState` bit `0x400` to open the save menu (most likely the innkeeper's event script).
- What summary offsets `0x00`, `0x02`, `0x35-0x37` hold, and whether `0x14` (`stone`) is total EXP.
- The exact meaning of save menu modes 0, 4 and 8 inside `func_8002B57C` (4 = save from an inn; 0 and 8 come from the title screen).
- How many of the 32 chest-flag bytes and 128 spirit bits are really used (would tell how many spare bits exist).
