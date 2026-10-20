# Eltale Odyssey

A mechanics overhaul of Quest 64 (USA 1.0). The story, world and progression stay; magic, stats, items,
economy and combat are rebuilt around Quest 64's own quirks, with old-school RPG friction on purpose.

This repository is **local only** for now.

## Design

- [Game design document](https://claude.ai/code/artifact/7f4165c7-63c1-4fa0-a848-8931684641ec)
- [Pacing plan](https://claude.ai/code/artifact/3a27b6a6-f54f-4cf1-8dc3-5d60a3e34fe7)

## Layout

| Folder | What it holds |
| --- | --- |
| `mod/` | The overhaul itself, built as a mod for Quest 64: Recompiled (empty until phase 1) |
| `research/` | Reverse-engineering notes: spells, stats, time, menus, saves, battle, enemies, NPCs |
| `tools/` | Analysis scripts (spell table dump, stat logger, spreadsheet converter) |

## Related projects (separate folders next to this one)

| Folder | Role |
| --- | --- |
| `Quest64-Recomp` | The engine: our fork of Quest 64: Recompiled (build fixes, dev panel, engine features) |
| `Quest64-Recomp/Quest64Syms` | Shared function and data names (`names.toml` + `tools/apply_names.py`) |
| `Quest64-ModTemplate` | Mod template and examples for other modders |
| `Quest64-Decomp` | The community decompilation, used as a read-only reference |

## Credits

Research builds on the Eltale Monsters Discord community's modding notes, the Quest 64 decomp contributors on
decomp.me, and the Merrow randomizer.
