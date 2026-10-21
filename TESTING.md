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
- [ ] **Element rebalance** (Brian only): Fire Ball 200, Rock 247 (Earth now 0.85x vanilla across the board, it was
      still the only right answer), Water Pillar 285, Wind Cutter 2 shots of 150.
      Enemies keep the vanilla numbers. How does each element feel in Holy Plains?
- [ ] **Burn**: hit something with a Fire spell; from your next turn it takes extra damage numbers each turn for
      3 turns, stacking up to 3 hits. Burn can kill.
- [ ] **Fire Bomb** on a burning enemy sets off all its remaining Burn at once.
- [ ] **Poison**: Man Eaters and Bumbershoots can poison you (about 1 hit in 3). a purple drop icon right of the HP
      bar, 5% max HP per turn for 4 turns, also ticks outside battle, never below 1 HP. Water healing or an inn
      night cures it.

## Spirit Charge, shields and Break (new)

**Charge** shows as five diamonds right of the HP bar: purple = charge you have, gold = charge your Boost is about
to spend, dark = empty. The enemy list is gone: icons float above each enemy instead (a white burst = Broken,
a flame = burning, a coloured diamond = its weakness once you've revealed it). No durations are shown; track
them yourself. For now the weakness is always the element opposite the monster's own.

- [ ] While picking a spell, a one-line **description** of the selected spell shows under the spell name
      ("UNKNOWN - CAST IT TO LEARN IT." for "???" spells). (Checked by Claude: diamonds, gold spend colour,
      markers, description.)

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

## Smaller windows

- [ ] Shop, Spirits, Equipment, the inn's price window and the gold notice now use the HUD's small font, so they
      take about a quarter less room. (Checked by Claude: inn price, Spirits, Equipment.) Too small to read?

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

## Equipment (new, only partly checked)

**D-pad down** in the field opens the **Equipment** window: staff, robe, charm and sigil slots. You can look
anywhere but change equipment only at an inn. The charm slot opens after Solvaring, the sigil slot after Zelse.

| Piece | Slot | Effect | Where |
|---|---|---|---|
| Heavy Staff | Staff | Staff damage 12 more | Dondoran shop (always) |
| Ember Staff | Staff | Staff 6 more, Fire 1.1x, staff hits may Burn | Dondoran / Greenoch rotating |
| Stone / Tide / Gale Staff | Staff | Staff 3 more, Earth / Water / Wind 1.1x | Larapool / Normoon / Limelin rotating |
| Traveler's Robe | Robe | Defense 4 more | Melrode shop (always), Larapool rotating |
| Tidecloth Robe | Robe | Defense 8 more, immune to Poison | Limelin / Greenoch / Brannoch rotating |
| Gambler's Coin | Charm | Each cast costs no MP or double | Solvaring |
| Pilgrim's Soul | Charm | Spells cost half, staff hits give no MP | Normoon rotating (rare) |
| Mirror Shard | Charm | The first hit you take each fight is reflected | Guilty |
| Leech Pillar | Sigil | Water Pillar heals you a quarter of its damage | Zelse |
| Tidal Echo | Sigil | Water spells cast twice at 0.75x | Nepty |

- Checked by Claude: the Equipment window opens and shows the slots (charm and sigil locked on your save).
- [ ] Not checked yet: equipping, each effect in battle, buying pieces in shops, boss rewards ("Found ...").

## Known rough edges

- Burn shows as "B1"-"B3" in the enemy list (checked by Claude, including a Burn tick that kills).
- The enemy list partly covers the game's own target-name box; the battle UI pass will sort out placement.
- The dialogue font has no + % or / (full symbol table in research/menus), so effects read like "Fire 1.1x".
- Changing gold with the dev panel shows up as "Paid"/"Won" in the gold notice.
