# Brian's stats and growth (Quest 64 USA 1.0)

Measured 2026-10-20 with the dev panel and `stat_logger.py` (polls `/api/mem` and logs every change).
Raw log: `stat_log_2026-10-20.txt`. Addresses below are the real ones; the community RAM notes list fields
smaller than 4 bytes byte-swapped.

## Layout (`0x8007BA84`, same order as the new-game table at ROM `0x54908`)

| RAM | Size | Field |
| --- | --- | --- |
| `0x8007BA84` | u16 | HP |
| `0x8007BA86` | u16 | Max HP |
| `0x8007BA88` | u16 | MP |
| `0x8007BA8A` | u16 | Max MP |
| `0x8007BA8C` | u16 | Agility |
| `0x8007BA8E` | u16 | Defense |
| `0x8007BA90` | u32 | Element EXP (toward the next battle-earned element level) |
| `0x8007BA94` | u32 | Unknown; rose 532 -> 564 when a battle was won (maybe total EXP) |
| `0x8007BAA4`-`A7` | u8 x4 | Fire, Earth, Water, Wind levels |
| `0x8007BAA8` | u16 | HP growth EXP: +1 per hit taken, more for bigger hits |
| `0x8007BAAA` | u16 | MP growth EXP: +1 per spell cast |
| `0x8007BAAC` | u16 | Agility growth EXP: rises while moving |
| `0x8007BAAE` | u16 | Defense growth EXP: rises when hit |
| `0x8007BAB0` | u8 | HP levels gained (Max HP went 51 -> 52 -> 53 as it rose) |
| `0x8007BAB3` | u8 | Defense levels gained (Defense 4 -> 5) |
| `0x8007BAB4` | u8 | Element levels gained through battle |

## Element levels come from battles too

Winning a battle gave **+6 element EXP** (6 -> 12). At the threshold the game offered an element level up and
the player chose where it went (Fire 1 -> 2); element EXP dropped from 12 to 4, so this level cost 8 EXP.
The ROM's element EXP chart (`0x54940`, 100 u32 entries) reads 10, 13, 17, 22, 28, 35, ... 655371, so how the
threshold is chosen is not yet clear (8 isn't in the chart). Losing a battle, even after a kill, gave nothing.

Implication: element levels = spirits (98) + battle levels, and the player chooses the element for both.
