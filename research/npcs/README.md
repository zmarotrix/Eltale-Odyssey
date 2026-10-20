# How NPC interaction works (Quest 64 USA 1.0)

Found 2026-10-20 by reading the recompiled code's original MIPS instructions (`Quest64-Recomp/RecompiledFuncs/*.c`),
checked against the ROM and the community sheet `research/modding-notes-text/NPC Locations.tsv`.
Static analysis only: nothing here was tested in the running game. How text and windows are *drawn* is left to the
menus/rendering research (`research/menus/`).

Address convention: RAM addresses are `0x8XXXXXXX`; ROM offsets are plain hex. Main code: ROM = RAM − 0x80000400 + 0x1000.

## The short version

- **Every NPC is one 44-byte record** (0x2C) stored in the room's map data. It says which room (submap) the NPC is in,
  where they stand, which model and name to use, how they behave (stand, wander, face you or not), and **two dialogue
  numbers** plus a **"text condition"** that decides which of the two you get.
- **A dialogue number is just a byte offset** into one big block of all NPC text in the ROM (starting at ROM `0xD305E0`).
  When you talk, the game copies 1 KB from there into a message buffer and opens a window on it.
- **What an NPC says can only change in two ways**: whether you *own a certain item* (condition 2), or whether *a certain
  boss is dead* (condition 5). Nothing looks at the time of day, the day count, or any other story flag.
- **Condition 2 is also how "item givers" work**: if you don't own the item, you hear text 1 and are handed the item; if you
  already have one, you hear text 2. The four Wings "shops" are really this: free wings, one at a time.
- **Condition 6 is the inn**: after the text, a Yes/No box appears. Choosing **Yes** fades the screen, **refills HP and MP**,
  **adds 9000 ticks** (one third of a day) to the clock through `time_set`, **records this room as your respawn point**,
  reloads the room, and then opens the **save screen**. All of this is hard-coded for condition 6; there is no general
  "run this script after Yes" system.
- **The text itself has a few control bytes** that the window code reacts to: `0xF0` waits for a button and starts a new
  page, `0xF2` pops up a second little window with the choices, `0xFF` ends. The answer is stored as a number
  (0 = first option, "Yes"; 1 = "No") at `0x800905C4`, and the code that ends the conversation reads it there.
- **Best place to add new services** (shop, identifier, curse-lifter...): give NPCs **new condition numbers** (7, 8, ...)
  and patch two functions: the one that *starts* a conversation (`func_800086E4`, to pick text and set up choices)
  and the one that *ends* it (`func_80004260`, to read the answer and open our menu). Existing NPCs can be re-assigned
  in RAM right after a room loads, so no ROM editing is needed. Details in section 5.
- **Note for the design:** Quest 64 has no money at all. "Inns that cost gold" needs a new currency the mod owns
  (stored in the mod's own save data); the hook for "can you pay?" is the same spot where the inn's Yes is read.

---

## 1. NPC data

### Where NPCs come from

| Step | Function | What happens |
|---|---|---|
| Entering a map with no battles (towns, interiors) | `func_80007A50` (called from the field loop `func_80001800` when a map loads) | Skipped if `gAllowBattles & 1`. DMAs ROM `0x871900–0x873F20` to `0x8020C0D0` (shared NPC assets), ROM `0xA725D0–0xA7A190` to `0x802A0000` (globally used NPCs such as Shannon), and the **town's NPC asset block** to `0x8020E6F0`. The town is picked by `(*D_80084F1C).0x12` indexing the table at RAM `0x800547F0` = ROM `0x553F0` (7 entries of `{rom_start, rom_end}`: Melrode `0x873F20`, Dondoran `0x8D3B90`, Larapool `0x928E40`, Normoon `0x95F730`, Limelin `0x98A7E0`, Brannoch/desert `0x9E3930`, Isle of Skye `0xA16420`). These blocks hold the models, animations and names the records point to. |
| Spawning | `func_800080E8` (called by `func_80007A50`) | Reads `gMapDataPtr->0x38` → `{u16 count; ...; Record* records at +4}`. For every record whose `submap == gNextSubmap`, fills the next free slot of `gNPCData` and counts it in `D_8007BCE4` (NPCs in this room). |
| Special scenes | `func_80007A50` | If `D_8007B2E8 == 1` it opens `gMsg_Grand_Abbott_intro` (`0x80054828`); if `== 4`, `gMsg_Shannon_Mammon_has_been_destroyed` (`0x80054C04`). These bypass the record system via `func_80008AD8`. |
| Every frame | `func_80007B64` (from the field loop) | Runs each NPC's little state machine (below). |

The records live inside the **map data**, not the town block: e.g. Melrode's outdoor NPCs at ROM `0x135E70`, Melrode inn at
`0x49F830`. Model and name fields are absolute RAM pointers into the town block (Melrode names at `0x8026E210…` = ROM `0x8D3A40…`).

### The 0x2C-byte NPC record (ROM/RAM, big-endian)

Verified on Agnes, Melrode inn, ROM `0x49F8E0`: `0005 0006 0101 0000 1B20 0000 00000000 80219BF0 8026E2B4 00000000 41E80000 00000000 BFC90FF9 3D8F5C29`.

| Offset | Size | Sheet column | Meaning (code evidence) |
|---|---|---|---|
| 0x00 | u16 | Submap ID | Room this NPC appears in; compared with `gNextSubmap` in `func_800080E8` |
| 0x02 | u16 | Text Condition | How dialogue is chosen, see section 2 (`func_800086E4` switch: 2, 5, 6, anything else) |
| 0x04 | u8 | Behavior (high byte) | Movement: `2` = wanders (only value `func_80007B64` / `func_80008948` treat specially); `1` = stands; `0`/`3` also occur and behave like standing as far as the code shows |
| 0x05 | u8 | Behavior (low byte) | Bit flags: `0x01` = can be talked to from further away (sets NPC flag `0x1000` at RAM +0x74; used for shop/inn keepers behind counters, see `func_80005A10`); `0x02` = does not turn to face Brian (horse, Bart) |
| 0x06 | u16 | Event/Item Trigger | Item ID (condition 2, low byte read at +0x07) or boss index 0–7 (condition 5) |
| 0x08 | u16 | Text ID | Dialogue 1: byte offset from ROM `0xD305E0` |
| 0x0A | u16 | 2nd Text ID | Dialogue 2 (used by conditions 2 and 5) |
| 0x0C | u32 | Unknown | Always 0 in the sheet; no reads found |
| 0x10 | ptr | Load Model From Ram | Model/animation header (`+0x2` and `+0xE` are frame counts used for idle/walk timers) |
| 0x14 | ptr | Load Name From Ram | 32-byte name shown in the window; 0 = no name (`func_80008A00` writes `0xFF`) |
| 0x18 | u16 (+pad) | Unknown | Animation played while the NPC talks (`func_80007B64` state 2 passes it to `func_8001D8B0`) |
| 0x1C | f32 | X Coordinate | Position X (also the wander centre) |
| 0x20 | f32 | Y Coordinate | Actually **Z** (ground plane); height is found from the floor by `func_8000EE60` |
| 0x24 | f32 | Direction Facing | Yaw in radians; also the angle to turn back to after talking |
| 0x28 | f32 | Model Size | Scale |

### The live NPC slot in RAM: `gNPCData` (`0x8007BD30`, 16 slots × 0x84)

Layout as in the partial decomp (`Quest64-Decomp/include/8650.h`), with what this pass confirmed:

| Offset | Meaning |
|---|---|
| 0x00 | state: 0 idle, 1 walking, 2 turning to Brian / talking, 3 turning back |
| 0x02 | state timer |
| 0x04, 0x08 | wander centre X/Z (from record) |
| 0x0C | yaw to return to after talking |
| 0x10 | turn amount while turning |
| 0x14–0x28 | position/rotation (X at 0x14, Y at 0x18, Z at 0x1C, yaw at 0x24) |
| 0x38 | scale; 0x3C collision radius (set to 50.0) |
| 0x74 | flags; `0x1000` = long talk range |
| 0x80 | pointer to the NPC's record |

## 2. Talking: from the button to the window

1. **Who can I talk to?** Every frame the player update `func_80002F60` calls `func_80005A10(&player, &playerPos)` (only when
   not in battle and `gGameState & 0x87 == 0`). It checks each NPC in the list at `D_8007BCF0` (count `D_8007BCE8`): height
   difference small, horizontal distance² < 225 (15 units) or the larger `D_800710C0` for long-range NPCs, and the NPC must
   be in front of Brian. It returns **NPC index + 1**, or 0, stored in `D_8007BA74`. (If no NPC, it next checks other
   targets via `func_80012358` and `func_80011E78`, likely chests/doors; not followed.)
2. **Button:** if `D_80092876` (buttons pressed this frame) has `0xA000` (A or Z) and `D_8007BA74 != 0`, `func_80002F60`
   sets Brian's state to 7 ("talking"), plays his idle animation, calls **`func_800086E4(npcIndex)`**, then
   `func_8002E768(style)` to open the window, and sets `gGameState |= 0x2`.
3. **`func_800086E4` — choose the text.** Sets the NPC to state 2 and (unless record bit `0x02`) computes the turn toward Brian.
   Then it switches on the record's **Text Condition**:

| Condition | Who uses it (sheet) | What the code does |
|---|---|---|
| 2 | Item givers, Wings shops (items 0x0E–0x13), Shannon in places | `func_80021240(item)` counts that item in `gInventory` (`0x8008CF78`, 0x96 bytes). Owned → **text 2**. Not owned → **text 1**, then `func_800212A0(item)` puts it in the first empty slot, and bit `0x40` is set at `0x8007BAC0` so the "got an item" jingle (sound 0x3B) plays when the talk ends |
| 5 | Story NPCs whose lines change after a boss | `func_8000B9D8(boss)` tests bit `boss` of **`D_8007D19C`** (one byte, 8 bosses, cleared on new game, saved). Dead → **text 2**, else **text 1**. Extra: if the boss index is **7** and it is dead, sets `gGameState |= 0x8080` and fade timer `D_8007B2EC = 0x16`: a scene change after the talk (likely the ending) |
| 6 | Innkeepers and other "rest here" people | Loads **text 1**, then fills the choice list: `D_800905C8 = 2`, options `gMsg_Yes` (`0x8004C280`) and `gMsg_No` (`0x8004C288`) at `0x800905D0/D4`, sets the answer `D_800905C4 = -1`, and `gGameState |= 0x10` ("a choice is pending") |
| 1, anything else | Everyone else | **text 1** |

4. **`func_80008A00(npc, textId)`** DMAs 0x400 bytes from ROM `0xD305E0 + textId` into `gMessageBuffer` (`0x8007C570`) and
   copies the 32-byte name into `gNPCName` (`0x8007C970`). Its sibling **`func_80008AD8(npc, ramPtr)`** does the same from a
   RAM pointer: the game uses it for the scripted scenes, and **a mod can use it to show its own text**.

Verified by decoding the ROM: Edgar `0x1300` → "Halt, who goes there?…", Agnes `0x1B20` → "…Please rest here for the night…
Will you stay?" followed by `F2 FF`, Ingram (Wings shop) `0x1BB0` "…We'll give you some White Wings…" and `0x1CA0`
"…If you need another pair, return here." (the second text, used once you own them).

**Time of day / weekday / other flags:** not used by any of this. (Matches `research/time`: nothing reads `gCurrentDay`.)

## 3. Dialogue flow

### Windows

- `func_8002E768(style)` **opens a window**: takes one of 4 slots at `0x800905E0` (each 0x89C bytes), copies `gNPCName`,
  `gMessageBuffer` and the choice list (`D_800905C8` count, `D_800905CC`, option text pointers) into it, picks the window
  rectangle from the table at `0x8005F9B8` (8 bytes per style; negative styles are used for choice boxes), increments
  **`D_800905C0` (open window count)** and sets `gGameState |= 0x2`.
- `func_8002EAA0` **updates all windows** every frame (called from the draw/update function `func_800011DC`).
- `func_8002E628(slot, keep)` resets a window slot.

### Stepping through the text (inside `func_8002EAA0`)

Characters are revealed one at a time (delay counter at slot +0x10, read position at slot +0x12); holding A/Z sets a
fast-forward flag. Byte by byte:

| Byte | Effect (code at) |
|---|---|
| `0xE0` | New line; if the window is full, wait for a button (flag `0x04000000`) (`0x8002F6F0`) |
| `0xF0` | Page break: stop and wait for A/Z, then clear and continue (`0x8002F928`) |
| `0xF2` | Set flag `0x00400000`, then at the end of this frame **open a second window** `func_8002E768(-1)` (or `-3`) that shows the choice list (`0x8002F8F8`, `0x8002FA84`) |
| `0xFF` | End of text: flag `0x02000000`, show the "done" marker; A/Z closes the window (`0x8002F940`, `0x8002F9B4`) |

### The choice box and how the answer is read

- In the choice window, stick up/down (`gPadStickY` beyond ±20) moves the cursor (slot +0x15), wrapping at the ends;
  each option is drawn with `ui_draw_text` (`0x80030EA0`).
- **A or Z** confirms (B does nothing here): sound 1, and the cursor index is written to **`D_800905C4`**
  (`0x8002F06C`). For windows opened with style flag `0x80000` (menus), it goes to `D_8008FD28` instead.
- When the last window closes, `D_800905C0` drops to 0 and `gGameState &= ~0x2`.
- Brian's "talking" state handler **`func_80004260`** (entry 7 of the player state table at `0x8004C230`) waits for
  `D_800905C0 == 0`, then: ends the NPC's talk (`func_80008948`: wanderers go idle with a random delay, others turn back),
  clears Brian's state and `gGameState` bit `0x2`, and **if `gGameState & 0x10` (a choice was pending)** clears it and,
  **only for text condition 6 with answer 0 (Yes)**, starts the inn (section 4). Otherwise, if the "got an item" bit is set,
  it plays the item jingle.

So the "special NPC logic" is in exactly two places: **`func_800086E4`** (before the text: item check/give, boss check,
inn setup) and **`func_80004260`** (after the text: inn on Yes, item jingle).

Observation (not tested): `D_800905C8` (number of choices) is only ever written by the inn setup, so after visiting an inn
it stays 2. An `F2` in any later text would show Yes/No, but only condition 6 acts on the answer.

## 4. The inn

| When | Where | What |
|---|---|---|
| Yes chosen | `func_80004260` `0x80004324` | `gGameState |= 0x480` (0x400 = "resting", 0x80 = fade out), fade timer `D_8007B2EC = 0x23` (35 frames), `D_8007B2F2 = 0x1E`; saves the current `gNextMap`/`gNextSubmap` to **`D_8007BA60`/`D_8007BA62`** (respawn point); Brian goes to state 3 (timer 0x41) |
| Fade finished | field loop `func_80001800` `0x80001D50–0x80001DE0` | Because `0x400` is set: **HP = max HP** (player stats `+0x4 ← +0x6`), **MP = max MP** (`+0x8 ← +0xA`); **`gCurrentTime += 9000`** then `time_set(gCurrentTime)` (the immediate 0x2328 is at ROM `0x2996`, as the sheet says); respawn point also gets `D_8007BA64 ← 0x80085370` (entrance); sets fade-in type 2 and `gGameState |= 0x100` |
| Room reload | `func_80001800` `0x80001A38` | `0x100` → `func_8002684C(gNextMap, gNextSubmap, 0)` reloads the room (you "wake up") |
| Save | end of `func_80001800` `0x80002070` | Still `0x400` → `func_8002B510(4)`: switches `gGameMode` to 2 (system menu) with request flag 4, which opens the save screen (menu internals belong to the save research) |
| Death | `func_80001800` start, flag `0x800` | Copies `D_8007BA60/62` back into `gNextMap/gNextSubmap`: you return to the **last inn you slept at** (inference from the flag use; not traced from the game-over code) |

9000 ticks out of a 27000-tick day is 1/3 day. The time is added, not set, so sleeping always moves the clock forward by
the same amount and can roll the day over through `time_set`.

## 5. Hook points for new services

The recomp can replace any function with a C version (`RECOMP_PATCH`, as `patches/main.c` already does for
`model_draw_bone`). Recommended plan:

1. **Mark which NPCs get which service, at load time.** Patch (or hook the end of) **`func_800080E8`**: after it fills
   `gNPCData`, look up each NPC (key: `gCurrentMap`/`gNextMap` + submap + record's text ID, or the record's name pointer)
   in a mod table, and **overwrite the record's Text Condition in RAM** with a new value (7 = shop, 8 = identifier,
   9 = curse-lifter, ...). The record is in RAM map data, so this changes nothing on disk and is redone each room load.
   Keep the original text IDs, or point `+0x08/+0x0A` at new offsets if we add text.
2. **Before the talk: `func_800086E4`.** Add cases for the new conditions. Typical: load the NPC's greeting with
   `func_80008A00` (ROM text) or our own text with `func_80008AD8(npc, ourBuffer)` (text must end with `F2 FF`), then fill
   the choice list exactly like the inn does: `D_800905C8 = n`, pointers at `0x800905D0 + 4*i`, `D_800905C4 = -1`,
   `gGameState |= 0x10`. More than two options should work (the window copies `count` entries of 0x18 bytes) but the
   window height for more than 2 is untested.
3. **After the talk: `func_80004260`.** Where it checks `condition == 6 && D_800905C4 == 0`, add our conditions: read
   `D_800905C4` and open our own shop/identify menu (drawing is the rendering session's work), or show a follow-up line by
   calling `func_80008AD8` + `func_8002E768` again and keeping Brian in state 7.
4. **Inn changes** (cost, weekday): the gold check goes in the same `func_80004260` branch (refuse → show "not enough"
   text instead of setting `0x480`). The weekday/notice can be shown by the innkeeper's greeting (custom text in step 2) or
   after waking, in the `0x400` branch of `func_80001800`, right after `time_set`. Changing the 9000-tick sleep length is
   one immediate (ROM `0x2996`) or a patch of that branch.
5. **Simplest possible first test:** change one NPC's condition to 6 in RAM; they will offer Yes/No and Yes will act like an
   inn. That proves the RAM-override approach before writing new cases.

What would need patching, in short: `func_800080E8` (assign services), `func_800086E4` (start), `func_80004260` (result).
Nothing in the text engine needs changing.

## Still unknown

- Who fills the talk-candidate list `D_8007BCF0`/`D_8007BCE8` (probably the collision/draw code; it holds pointers to
  `gNPCData[i] + 0x14`).
- The exact meaning of `D_800905CC` (copied to window +0x3A next to the choice count; maybe the default cursor).
- Behaviour byte values `0` and `3` (Amber, Marmaduke): no special code path found; they act like standing NPCs.
- What `func_80012358` / `func_80011E78` detect when no NPC is near (chests, signs?) and what game mode the
  `0x8080` "boss 7" ending transition goes to.
- Text encoding details (0xC0 at the start of every line, punctuation codes 0x0A/0x0B/0x10/0x11/0x1A...) are left to the
  rendering research.

## Proposed function names

| Address | Proposed name | Evidence |
|---|---|---|
| `0x80001800` | `field_mode_loop` | Runs while `gGameMode == 1`; per-frame calls to player, NPC, time updates; map load/inn/fade handling |
| `0x80002F60` | `player_field_update` | Talk/chest detection, button handling, then calls the player state table `0x8004C230` |
| `0x80004260` | `player_state_talking` | Entry 7 of the player state table; waits for windows to close, reads `D_800905C4`, starts the inn |
| `0x80005A10` | `npc_find_talk_target` | Distance/facing test over NPC list; returns index+1 into `D_8007BA74` |
| `0x80007A50` | `npc_load_town` | DMAs town NPC blocks from table `0x800547F0`, calls spawn, opens scripted scene text |
| `0x800080E8` | `npc_spawn_room` | Fills `gNPCData` from `gMapDataPtr->0x38` records matching `gNextSubmap` |
| `0x80007B64` | `npc_update_all` | Per-frame state machine over `D_8007BCE4` NPCs (idle/walk/turn/turn back) |
| `0x80008290` | `npc_walk_step` | Moves an NPC by a step with collision (`func_8000835C`) and floor height (`func_8000EE60`) |
| `0x800086E4` | `npc_talk_begin` | Sets talk state, switches on text condition 2/5/6, loads dialogue |
| `0x80008948` | `npc_talk_end` | After talk: wanderers idle with random delay, others turn back |
| `0x80008A00` | `npc_load_dialogue` | DMA 0x400 bytes from ROM `0xD305E0 + textId` to `gMessageBuffer`, copy name |
| `0x80008AD8` | `npc_set_dialogue_ptr` | Same as above from a RAM pointer (already C in decomp `src/8650.c`) |
| `0x8002E768` | `msgwin_open` | Allocates one of 4 windows at `0x800905E0`, copies text/name/choices, `D_800905C0++` |
| `0x8002EAA0` | `msgwin_update_all` | Text reveal, `E0/F0/F2/FF` handling, choice cursor, writes `D_800905C4` |
| `0x8002E628` | `msgwin_reset` | Clears a window slot's state and buffers to `0xFF` |
| `0x8000B9D8` | `boss_is_defeated` | Tests bit N of `D_8007D19C` with mask table `0x8004D740` |
| `0x8000BA0C` | `boss_set_defeated` (tentative) | Takes the address of `D_8007D19C` and reads the same mask table; only other writer is the new-game clear `func_8000B520`. Not read instruction by instruction |
| `0x8000B530` | `boss_spawn_for_room` | Loops 8 boss entries at `0x803A9AD0`, skips defeated ones, DMAs boss data |
| `0x8000BA4C` | `boss_intro_start` | Copies boss field name/dialogue pointers into `gNPCName`/`gMessageBuffer` |
| `0x80021240` | `inventory_count_item` | Counts matches of an item ID in `gInventory` |
| `0x800212A0` | `inventory_add_item` | Writes the item to the first `0xFF` slot of `gInventory` (no "full" check) |
| `0x8002B510` | `sysmenu_open` | Sets `gGameMode = 2` and menu request flags in `D_8008FD0C` |
| `0x800268D4` | `sfx_play` (tentative) | `(channel, soundId, volume)`; called with 0x3B for the item jingle and 1 for menu confirm |

## Proposed data names

| Address | Name | Evidence |
|---|---|---|
| `0x800547F0` | `gTownNpcRomTable` | 7 × `{rom_start, rom_end}`; ROM `0x553F0` |
| `0x8007BA74` | `gTalkTargetNpc` | NPC index + 1, 0 = none |
| `0x8007BCE4` | `gNPCCount` | NPCs spawned in this room |
| `0x8007BA60` / `0x8007BA62` / `0x8007BA64` | `gInnMap` / `gInnSubmap` / `gInnEntrance` | Set on inn Yes / wake; read back on flag `0x800` |
| `0x8007D19C` | `gBossDefeatedFlags` | 1 byte, bit per boss; saved/loaded by `func_8002E234`/`func_8002E404` |
| `0x8007B2EC` | `gFadeTimer` | Counts down; at 0 the field loop performs the pending transition |
| `0x800905C0` | `gMsgWinOpenCount` | Incremented by `msgwin_open`, decremented on close |
| `0x800905C4` | `gMsgChoiceResult` | -1 pending, else chosen option index (0 = Yes) |
| `0x800905C8` | `gMsgChoiceCount` | Number of options; option text pointers at `0x800905D0 + 4*i` |
| `0x800905E0` | `gMsgWindows` | 4 slots × 0x89C bytes |

### `gGameState` (`0x8007B2E4`) bits seen here

| Bit | Meaning |
|---|---|
| `0x2` | Dialogue/message open |
| `0x10` | Choice pending (set by inn setup, consumed by `player_state_talking`) |
| `0x80` | Fade-out / transition in progress |
| `0x100` | Reload room (`gNextMap`/`gNextSubmap`) |
| `0x400` | Inn rest in progress (heal + time + save) |
| `0x800` | Return to last inn (respawn) |
| `0x8000` | Set together with `0x80` by the boss-7 condition-5 NPC (scene change, likely the ending) |
