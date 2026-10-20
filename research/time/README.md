# Time, days and the unused week (Quest 64 USA 1.0)

Found 2026-10-20 from the recompiled code's disassembly. Data names are from Quest64Syms/data_dump.toml.

| RAM | Name | Set at new game | Notes |
|---|---|---|---|
| `0x800859D0` | `gCurrentTime` | 7875 (0x1EC3) | Ticks since the start of the day |
| `0x800859D4` | `gDayLength` | 27000 (0x6978) | Ticks per day |
| `0x800859E0` | `gCurrentDay` (u16) | 1 | Day counter, saved at offset 0x3A of the save block |
| `0x800859E2` | `D_800859E2` (u16) | 0 | Likely play time in minutes: +1 every 1801 ticks of `0x80085388`, capped at 59939; saved at 0x38 |
| `0x8008C634` | `D_8008C634` | 7 | The "week length" from the modding notes. Written once, **never read** by any recompiled code |

## How time moves

- `time_tick` (`0x800102D0`): adds 1 to `gCurrentTime`; when it reaches `gDayLength` it wraps and adds 1 to `gCurrentDay`, **capped at 999**.
- `time_set` (`0x80010510`): sets `gCurrentTime` (likely used by the inn); on overflow it adds 1 to `gCurrentDay`, **capped at 9999**. The two caps disagree.
- Save (`func_8002E234`) and load (`func_8002E404`) copy `gCurrentDay` and `D_800859E2`.

## What uses the day

Nothing found besides the save code: no shop, NPC or event reads `gCurrentDay`, and the week value is never read.
Caveat: code could reach these through a pointer passed in from data we haven't decoded, but every recompiled
function was searched for references to this block.

If the play-time counter's 1801 ticks are one minute, a tick is one 30 fps frame and a day is 27000 frames:
**15 minutes of real time** (unconfirmed; check with the dev panel by watching `gCurrentTime`).
