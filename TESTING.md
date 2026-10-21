# What to test

Built while you were away. Launch with **Quick Load (Dev)** (it boots straight into save file 1). Everything
below is in the `eltale_odyssey` mod; the Blue Fire and test-window example mods were moved to
`%LOCALAPPDATA%\Quest64Recompiled\mods-disabled` (move them back to turn them on again).

Your save is backed up in the dev panel slots (`before-eltale-0-1`, from before any Eltale testing).

"Checked by Claude" means I drove the game through the dev panel and looked at screenshots.

## Gold and inns

- [x] Inn price window and dialogue, refusal line (checked by you)
- [ ] **Innkeeper's last page** now says "That will be N gold. Today is Waterday." (checked by Claude)
- [ ] **Resting wakes you at dawn** of the next morning (checked by Claude: day 2 morning → day 3 dawn)
- [ ] **Boss gold**: Zelse 2,000, Nepty 1,500, Shilf 3,000, Fargo 3,000, Guilty 4,500, Beigis 5,000 (on top of the
      normal reward). Not tested; needs a boss fight.
- [ ] **Defeat costs half your gold**: lose a battle on purpose; back at the inn you should see "Gold N / Lost M".

## Shops (new)

The six Wings shops and Greenoch's item shop sell things now instead of giving one item away:
Ingram (Melrode), Thom (Dondoran), Jiryo (Larapool), Anette (Normoon), Ring (Limelin), Bronze (Greenoch), Tom (Brannoch).

- [ ] Talk to Ingram in Melrode's Wings shop: a greeting, then a shop window (checked by Claude with a test NPC).
- [ ] D-pad or stick moves the hand cursor; A buys; B leaves. Gold, owned count and the effect line update.
- [ ] Three of the items rotate each day: rest at the inn and visit again.
- [ ] At night (after evening turns dark) the shopkeeper says they're closed.
- [ ] Bought items show up in your item panel and work as usual.

## Combat slice

- [ ] **Enemy spells are separate** from Brian's (checked: monsters cast from their own copies).
- [ ] **Element rebalance** (Brian only): Fire Ball 200, Rock 320, Water Pillar 285, Wind Cutter 2 shots of 143.
      Enemies keep the vanilla numbers. How does each element feel in Holy Plains?
- [ ] **Burn**: hit something with a Fire spell; from your next turn it takes extra damage numbers each turn for
      3 turns, stacking up to 3 hits. Burn can kill.
- [ ] **Fire Bomb** on a burning enemy sets off all its remaining Burn at once.
- [ ] **Poison**: Man Eaters and Bumbershoots can poison you (about 1 hit in 3). "Poisoned" window under the HP
      bar, 5% max HP per turn for 4 turns, also ticks outside battle, never below 1 HP. Water healing or an inn
      night cures it.

## Known rough edges

- Enemies show no Burn icon yet (damage numbers only); icons come with the battle UI work.
- "!" isn't in the dialogue font table we decoded, so new text avoids it.
- Changing gold with the dev panel shows up as "Paid"/"Won" in the gold notice.
