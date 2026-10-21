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

## Spirit Charge, shields and Break (new)

In battle there are two new panels: **Charge / Boost** in the bottom right corner, and an **enemy list** (top right) with
each enemy's shields and weakness ("?" until you hit it with the right element; for now the weakness is always the
element opposite the monster's own).

- [ ] Charge starts at 1 and goes up by 1 each of your turns (max 5), but not on the turn after you spend some.
      Your first weakness hit in a turn gives +1. (Checked by Claude.)
- [ ] **D-pad up/down** sets Boost (0-3, no more than your charge) before you cast; it's spent when the spell fires.
      Earth: +40% damage per charge. Wind: +2 shots per charge. Water: +30% healing, or Freezes the target for
      1 turn per charge. Fire: +1 Burn stack per charge; at 3 it also sets off the Burn already on the target.
- [ ] Each weakness hit removes a shield and reveals the weakness. At 0 the enemy is **Broken**: it skips its next
      turn and takes 50% more damage until then; then its shields come back (bosses get one more each time).
      (Checked by Claude: shields, reveal, charge bonus, Break and the skipped-turn status.)
- [ ] Does Boost feel worth saving charge for? Are shield counts (2-4, bosses 6-10) right?

Note: Holy Plains enemies die before they can be Broken; Break matters from tougher areas on.

## Spirits: points, tiers and discovering spells (new)

- **D-pad up** in the field opens the **Spirits** window: each element's level and attuned tier, and your
  Spirit Points. Each spirit you collect now also gives 1 Spirit Point.
- Each element's 15 spells are in 5 tiers (by their old level requirement). Tier 1 is open. **A** attunes the next
  tier: tier 2 needs element Lv 6 and 2 points, tier 3 Lv 14 and 4, tier 4 Lv 22 and 7, tier 5 Lv 30 and 10.
  All four elements fully attuned cost 92 points; the game has 98 spirits.
- Spells in attuned tiers show as **"???"** in the spell wheel until you cast their combination once, which teaches
  the spell ("Learned Fire Ball Lv2"). Spells in tiers you haven't attuned stay EMPTY.
- Checked by Claude: the window, attuning (points spent, spells unlocked), "???" in the wheel, learning on cast.
- [ ] Collect a spirit: "Spirit Point gained".
- [ ] Do the tier costs and level gates feel right? Decisions for you: should Brian be able to master everything
      by the end (current numbers allow it only near the end), and can points be refunded?
- [ ] Existing saves get points for spirits already found and keep the spells their levels already allowed.

## Known rough edges

- Burn shows as "B1"-"B3" in the enemy list (checked by Claude, including a Burn tick that kills).
- The enemy list partly covers the game's own target-name box; the battle UI pass will sort out placement.
- "!" isn't in the dialogue font table we decoded, so new text avoids it.
- Changing gold with the dev panel shows up as "Paid"/"Won" in the gold notice.
