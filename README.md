# Eltale Odyssey

A mechanics overhaul of **Quest 64** (USA 1.0), built as a mod for
[Quest 64: Recompiled](https://github.com/Rainchus/Quest64-Recomp). The story, world and progression stay; magic,
stats, items, economy and combat are rebuilt around Quest 64's own quirks, with old-school RPG friction on purpose.

**This is a playtest build.** Expect rough edges, and expect numbers to change.

## What's different

- **Combat:** Spirit Charge (build it up, spend it to empower spells) and enemy shields: hit a foe's weakness to
  crack them and **Break** it, so it loses a turn and takes double damage.
- **Magic:** each element has a job (Fire burns, Earth hits hard, Water heals and freezes, Wind hits many times).
  Spirits give Spirit Points to attune higher spell tiers and buy **talents** (a page per element). New spells
  show as "???" until you cast them; Hint Scrolls in shops teach one.
- **Statuses:** Burn stacks and spreads; every fighter carries one status at a time, shown as a color on the model.
- **Equipment:** staff, robe, charm and sigil slots, with about 30 pieces built around six playstyles (Pyromancer,
  Stonewall, Tidecaller, Tempest, Staff Brawler, Elementalist). Many are hidden in chests that used to hold
  common items; bosses give the rest. Change gear at inns.
- **Economy:** gold from battles, town shops with daily stock, inns that charge (more for nights in a row).
- **Balance:** spells and enemies are rebalanced so the game keeps roughly its original difficulty curve with
  the new options. Notes in [research/balance](research/balance/README.md).
- **Lock-On** (a Wind talent): hold Z in battle to target an enemy, Ocarina of Time style.

[TESTING.md](TESTING.md) is the full list of features and what has been checked.

## Playing the playtest build

You need your own **Quest 64 (USA 1.0)** ROM. Nothing from the game is included.

1. Download `EltaleOdyssey-playtest-*.zip` from [Releases](../../releases) and unzip it anywhere.
2. Run `Quest64Recompiled.exe` and select your ROM when it asks (first launch only).
3. The mod is already in the `mods` folder and enabled. The build runs in portable mode (`portable.txt`), so its
   saves and settings stay in that folder and never touch a normal Quest 64: Recompiled install.

Controls in the field: **D-pad up** opens the Spirits window (tiers and talents), **D-pad down** the Equipment
window.

Feedback is welcome in [Issues](../../issues): what felt too easy or too hard (how many turns a fight took
helps a lot), bugs, and crashes (what you were doing just before).

## Building the mod

The mod is in `mod/`. It builds with `make` (clang with MIPS support and `ld.lld`, as for the
[Quest 64 mod template](https://github.com/zmarotrix/Quest64-ModTemplate)) against the symbol files in
[zmarotrix/Quest64Syms](https://github.com/zmarotrix/Quest64Syms), expected at `../Quest64-Recomp/Quest64Syms`
(see `mod/mod.toml`). The engine fork with the dev panel used for testing is
[zmarotrix/Quest64-Recomp](https://github.com/zmarotrix/Quest64-Recomp). `tools/test/` holds the scripts that drive
the game through that dev panel.

## Research

`research/` holds the reverse-engineering notes behind the mod: spells, battle, enemies, stats, saves, menus,
NPCs, time and balance. ROM-derived files (sprite renders, raw data dumps) are not included; the scripts that
make them are.

## Credits

- **The Eltale Monsters Discord community**, whose Quest 64 modding notes this research builds on.
- **Rainchus** and contributors for Quest 64: Recompiled, and **Wiseguy** and contributors for N64Recomp and
  N64ModernRuntime.
- The **Quest 64 decomp** contributors on decomp.me, and the **Merrow** randomizer.

Quest 64 is a trademark of its owners. This project is not affiliated with them and contains none of the game's
data.
