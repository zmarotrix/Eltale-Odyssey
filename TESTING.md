# What to test

Built while you were away. Launch with **Quick Load (Dev)** (it boots straight into save file 1). Everything
below is in the `eltale_odyssey` mod; the Blue Fire and test-window example mods were moved to
`%LOCALAPPDATA%\Quest64Recompiled\mods-disabled` (move them back to turn them on again).

Your save is backed up in the dev panel slots (`before-eltale-0-1`, from before any Eltale testing).

"Checked by Claude" means I drove the game through the dev panel and looked at screenshots.

## Gold and inns

- [x] Inn price window and dialogue, refusal line (checked by you)
- [ ] **Innkeeper's lines** vary by day ("Happy Waterday! A room's 40 gold..."); a second night in a row
      explains the higher price; short of gold, she says by how much. Natural enough?
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
- [ ] Each shopkeeper has their own greetings (alternating by day) and closed line; buying gives varied
      thank-yous, "YOU'RE N GOLD SHORT", and leaving says goodbye before the window closes.
- [ ] Bought items show up in your item panel and work as usual.

## Combat slice

- [ ] **Enemy spells are separate** from Brian's (checked: monsters cast from their own copies).
- [ ] **Element rebalance** (Brian only): Fire Ball 200, Rock 247 (Earth now 0.85x vanilla across the board, it was
      still the only right answer), Water Pillar 285, Wind Cutter 2 shots of 150.
      Enemies keep the vanilla numbers. How does each element feel in Holy Plains?
- [ ] **Burn**: hit something with a Fire spell; from your next turn it takes extra damage numbers each turn for
      3 turns, stacking up to 3 hits. Burn can kill.
- [x] **Burn visuals**: a burning enemy pulses red; small embers rise from it when burn damage lands (ticks and
      ignition only). (Checked by you.)
- [ ] **Fire Bomb** on a burning enemy sets off all its remaining Burn at once.
- [ ] **Poison**: Man Eaters and Bumbershoots can poison you (about 1 hit in 3). Brian pulses purple, 5% max HP per turn for 4 turns, also ticks outside battle, never below 1 HP. Water healing or an inn
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

## Icons in the windows (new)

- [ ] Spirits window: element gems, five tier pips per element (purple = attuned), points as a purple pip; the
      next-tier line is green when you meet it, red when you don't. (Checked by Claude.)
- [ ] Equipment window: slot icons (staff, robe, charm, sigil), a lock on closed slots. (Checked by Claude.)
- [ ] Gold notice: coin and total, the change in green (won) or red (spent). Inn price window: coins after the
      amounts. (Checked by Claude.)
- [ ] Shop window: a coin after each price and before your gold (not checked yet).

## Battle display: everything on the models (new)

No floating icons or plates over enemies, and no explanation of what Boost does: find out by trying.

- [ ] **Spell window**: element gem and description, then the MP cost (red if you can't pay). (Checked by Claude.)
- [ ] **Shield hit**: a weakness hit throws white glints off the enemy; nothing says how many shields are left.
- [ ] **Break sound**: sound 0x35 three times in quick succession (4 frames apart) when an enemy breaks.
- [ ] **Breaking**: the enemy's model flickers dark for a moment (a white flash isn't possible with the game's
      lighting). Not checked by Claude yet.
- [ ] **Broken**: a bigger burst of white and pale gold glints; while Broken the enemy is a little darker, glints
      drift around its head, and its idle animation crawls at about 1/8 speed. (Checked by Claude, forced.)
- [ ] **Poisoned** (you): Brian pulses purple, in battle and in the field; no icon. (Checked by Claude, forced;
      it's subtle, say if it needs to be stronger.)
- [ ] **Ember Staff flare**: a staff hit sets the enemy burning; a staff hit on an enemy that's already burning
      flares: embers burst out and its strongest Burn spreads to every enemy within reach (about a pack's
      width). (Spread checked by Claude with a test trigger; you saw the flare in play.)

## Dev tools

- **Sound browser**: hold L to see a sound ID at the left of the screen; while holding L, D-pad right / left
  plays the next / previous sound, D-pad up replays it. Use it to pick sounds (e.g. a weak-point hit).
  Starts at 18, the game's normal hit sound.

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

## Status colors and one status at a time (new)

- [ ] Every status has its own color, pulsing on the model through fog: Burn red, Poison purple, Compression
      yellow, Vampire crimson, Power Staff orange, Freeze/Restriction ice blue, Movement green, Agility teal,
      Silence white, Soul Searcher gold, Barrier pale blue, MP absorb pink, Defense brown. (Checked by Claude:
      red and white, forced.) Broken enemies pulse dark and move slowly.
- [ ] **One status at a time**: a fighter with a status gets no new one until it wears off (spell statuses,
      Burn, Freeze, Poison; Brian included, so a buffed Brian can't be poisoned and a poisoned Brian can't buff
      himself). Burn still stacks on a burning foe. Break isn't a status. Not checked in play yet.

## Talents (new)

The Spirits window (D-pad up) has five pages: **ATTUNE** and one talent tree per element (FIRE, EARTH, WATER,
WIND); D-pad left / right switches. Each tree has 9 talents with ranks (pips); deeper rows need a higher element
level (1, 3, 6, 10, 12, 16, 20, 25, 30). Each rank costs 2 Spirit Points, capstones 5: about 150 points fill
every tree, on top of 92 for full attunement, out of 98 in the game.

| Fire | Earth | Water | Wind |
|---|---|---|---|
| Smoulder 2: Burn +1 turn | Bedrock 2: +1 starting charge | Deep Freeze 2: Freeze +1 turn | Crosswind 2: Wind Boost +1 shot per charge |
| Fanned Flames 3: Burn +5% of the hit | Weight 3: Earth +4% | Wellspring 3: Water healing +10% | Lock-On 1: hold Z to lock on (see below) |
| Kindling 3: Fire +4% | Stoneskin 3: Defense +2 | Tidal Force 3: Water +4% | Tailwind 2: weakness charge twice a turn, then every hit |
| Conflagration 2: ignition +25% | Shatter 2: Broken +15% | Still Waters 2: +1 MP every other turn / every turn | Shifting Winds 1: your statuses ignore the limit |
| Firebrand 1: any staff hit burns | Fault Line 1: Earth weak hits take 2 shields | Undertow 1: Water healing ends your status | Eye of the Storm 2: your statuses +1 turn |
| Wildfire 2: a burning foe's death spreads its Burn (50/100%) | Aftershock 2: Earth Boost +10% per charge | Frostbite 2: frozen foes +15% | Cyclone 3: Wind hits 5% chance to crack any shield |
| Searing 3: burning foes +5% | Bulwark 1: first hit each fight halved | Riptide 1: freezing gives 1 charge | Second Wind 1: Boost 3 gives 1 charge back |
| Heat Haze 1: Burn ticks give 1 MP | Landslide 1: a Break cracks every other foe | Undercurrent 3: Water spells 10% free | Updraft 2: hold 1 more charge |
| Inferno (cap): Burn stacks to 4 | Epicentre (cap): Broken one more turn | Glacier (cap): freezing cracks a shield | Maelstrom (cap): 2 charge a turn |

(Numbers are per rank.)

**Lock-On** (Wind, Lv 3): in battle, hold **Z** and Brian locks onto an enemy (the one most straight ahead) and
keeps facing it while you move, which aims your spells. Tap Z again to switch to the next enemy. A pale-gold
sparkle floats over the target. Z doesn't swing or cast while the talent is learned (A and R still do).
- [ ] Facing and the marker checked by Claude; moving while locked and switching targets not checked yet.

- [ ] Pages draw (checked by Claude). Learning ranks and every effect: not checked yet (needs points and levels;
      the dev panel's Brian tab can raise element levels, and Spirit Points can be set from the Memory tab).

## Equipment (new, only partly checked)

Only Brian's starting staff (no staff equipped) gives MP back on staff hits; every other staff doesn't.

**D-pad down** in the field opens the **Equipment** window: staff, robe, charm and sigil slots. You can look
anywhere but change equipment only at an inn. The charm slot opens after Solvaring, the sigil slot after Zelse.

| Piece | Slot | Effect | Where |
|---|---|---|---|
| Heavy Staff | Staff | Staff damage 12 more | Dondoran shop (always) |
| Ember Staff | Staff | Staff 6 more, Fire 1.1x; hits Burn, and flare (spread Burn) on a burning foe | Dondoran / Greenoch rotating |
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
