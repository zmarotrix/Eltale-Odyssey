# What to test

Built while you were away. Launch with **Quick Load (Dev)** (it boots straight into save file 1). Everything
below is in the `eltale_odyssey` mod; the Blue Fire and test-window example mods were moved to
`%LOCALAPPDATA%\Quest64Recompiled\mods-disabled` (move them back to turn them on again).

Your save is backed up in the dev panel slots (`before-eltale-0-1`, from before any Eltale testing).

"Checked by Claude" means I drove the game through the dev panel and looked at screenshots.

## Early chests (new)

The Monastery's and Dondoran Castle's chests hold gold instead of bread and drops (the gold notice shows it):
Monastery dining room 25, courtyard 20, barn 30 and 35; Dondoran Castle King's Room 40, 40, 50, Flora's Room 30, 30,
40, Treasure Room 80 and 100, and a **Traveler's Robe** in the Treasure Room's middle chest (was a Replica).
- [ ] Open one: "Brian found 40 gold!" and the gold notice. Not checked in game yet.

## Getting started (new)

- **D-pad hint**: in the field, under the element gems: a D-pad with its up arm lit and a Spirit Point pip (the
  Spirits window), then one with its down arm lit and a staff (the Equipment window). It slides with the gems and
  gives way to the charge diamonds in battle. The up arm blinks while you have unspent Spirit Points.
  Checked by Claude: shown in the field, replaced by the diamonds in battle. Blinking not seen yet.
- [ ] **Melrode townsfolk teach the new systems**: after their usual line, Edgar (on the beach) explains spirits,
  tiers and talents; Frank shields, Break and Charge; Billy equipment and inns. Not checked in game yet (scripted
  talking didn't trigger the conversation); talk to them and tell me if the text fits its box.

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
- [ ] Each shop lists its own town's Wings first, always, and never another town's (Greenoch sells none: the game
      has no Greenoch Wings). Each shop also has staples plus 3 rotating items that change each in-game day.
- [ ] D-pad or stick moves the hand cursor; A buys; B leaves. Gold and the effect line update.
- [ ] **Daily stock**: the number on each row is how many the SHOP has left today (SOLD when out, OWNED for gear
      you have). Restocks each in-game day; saving keeps today's counts. Per day: own Wings 5, Fresh Bread and Dew
      Drop 3, Honey Bread, Mint Leaves, Healing Potion and Replica 2, the rest 1 (Spirit Light, Dragon's Potion,
      Heroes Drink, amulets, tools, equipment). (Checked by Claude: counts, SOLD, "Sold out for today".)
- [ ] **Item drops**: enemies drop an item after 1 battle in 8 (was 1 in 4); bosses always.
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
- [ ] **Kindle and stoke**: only kindling Fire spells set foes burning: Ember, Twin Flame, Seeker, Blaze, Flame
      Volley (checked by Claude: Ember). Stoking spells don't burn but deal 70% to a foe that isn't burning and
      140% to one that is: Scald, Pyre, Magma, Searing Steam (not checked in battle yet). Detonate sets off the
      Burn already there.
- [x] **Damage numbers**: one number per hit (bonuses included), colored: orange when a mechanic boosted it
      (weakness hit, Broken, burning or frozen bonus, stoking a burning foe), blue when one weakened it (the foe
      resists the element, a stoke on a foe that isn't burning), white otherwise. (Checked by you: blue and orange.)
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

- [ ] Charge starts at 0 every battle and goes up by 1 each of your turns (max 5), but not on the turn after you spend some.
      Your first weakness hit in a turn gives +1. (Checked by Claude.)
- [ ] **D-pad up/down** sets Boost (0-3, no more than your charge) before you cast; it's spent when the spell fires.
      Earth: +40% damage per charge. Wind: +2 shots per charge. Water: +30% healing, or Freezes the target for
      1 turn per charge. Fire: +1 Burn stack per charge; at 3 it also sets off the Burn already on the target.
- [ ] Each weakness hit removes a shield and reveals the weakness. At 0 the enemy is **Broken**: it skips its next
      turn and takes double damage until then (was 50% more); then its shields come back (bosses get one more each time).
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
- Tier 1 spells are known as soon as your element level reaches their old level requirement. Spells in tiers
  you attune later show as **"???"** in the spell wheel until you cast their combination once, which teaches
  the spell ("Learned Fire Ball Lv2"). Spells in tiers you haven't attuned stay EMPTY.
- Checked by Claude: the window, attuning (points spent, spells unlocked), "???" in the wheel, learning on cast.
- [ ] Collect a spirit: "Spirit Point gained".
- [ ] Do the tier costs and level gates feel right? Decisions for you: should Brian be able to master everything
      by the end (current numbers allow it only near the end), and can points be refunded?
- [ ] Existing saves get points for spirits already found and keep the spells their levels already allowed.

- [ ] The charge diamonds slide in and out with the element gems (battle start and end, and around enemy turns).

## Spell names (new)

Every spell has its own name, no levels (Rock, Boulder, ...); the list is in mod/src/spell_text.c.
- [ ] The spell bar and the "Learned ..." notice use them (spell bar checked by Claude). Names you'd change?

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
While locked, the stick moves Brian any way it would normally (strafing no longer pulls him toward the target), and
the camera swings round behind him to look at the target.
- [x] Camera turn and strafing: checked by you (2026-10-21). Since then the camera turns a little faster, and
      standing still keeps him facing the target for casts (loads on the next launch).
- [ ] Switching targets with a second tap: not checked yet.

- [ ] Pages draw (checked by Claude). Learning ranks and every effect: not checked yet (needs points and levels;
      the dev panel's Brian tab can raise element levels, and Spirit Points can be set from the Memory tab).

## Equipment (new, only partly checked)

**Gear colors**: Brian's cape takes his robe's color (none: red, Traveler's Robe: brown, Tidecloth Robe: deep
blue) and his staff its staff's (starting staff: wood, Ember: red-orange, Heavy: iron grey, Stone: amber, Tide:
blue, Gale: green). Checked by Claude: Tide Staff + Tidecloth Robe. Swap gear at an inn to see the others.
The red collar on Brian's chest follows the robe too, and the gem in its middle follows the charm (Pilgrim's Soul
green, Gambler's Coin gold, Mirror Shard violet; no charm keeps the game's). Checked by Claude in memory only.

- [ ] **Wool Cloak** from the shepherd's hut chest ("Brian found a Wool Cloak!"; checked by Claude). Saves that
      already opened that chest get the cloak on load.

Only Brian's starting staff (no staff equipped) gives MP back on staff hits; every other staff doesn't.

**D-pad down** in the field opens the **Equipment** window: staff, robe, charm and sigil slots. Brian starts with **Brian's Staff** (the only staff whose hits restore MP) and **Brian's Robe** (no bonuses). You can look
anywhere but change equipment only at an inn. The charm slot opens after Solvaring, the sigil slot after Zelse.

| Piece | Slot | Effect | Where |
|---|---|---|---|
| Heavy Staff | Staff | Staff damage 12 more | Dondoran shop (always) |
| Ember Staff | Staff | Swings 6 more, Fire 1.1x; a swing on a burning foe detonates it, spreading its Burn (no longer starts a Burn) | Dondoran / Greenoch rotating |
| Stone Staff | Staff | Swings 6 more, Earth 1.1x; each swing +3 defense for the fight (up to +12) | Larapool rotating |
| Tide Staff | Staff | Swings 6 more, Water 1.1x; swings heal you for half their damage | Normoon rotating |
| Gale Staff | Staff | Swings 6 more, Wind 1.1x; move 30% farther in battle | Limelin rotating |
| Traveler's Robe | Robe | Defense 4 more | Melrode shop (always), Larapool rotating |
| Tidecloth Robe | Robe | Defense 8 more, immune to Poison | Limelin / Greenoch / Brannoch rotating |
| Gambler's Coin | Charm | Each cast costs no MP or double | Solvaring |
| Pilgrim's Soul | Charm | Spells cost half, staff hits give no MP | Normoon rotating (rare) |
| Mirror Shard | Charm | The first hit you take each fight is reflected | Guilty |
| Leech Pillar | Sigil | Water Pillar heals you a quarter of its damage | Blue Cave chest #3 (was Heroes Drink) |
| Tidal Echo | Sigil | Water spells cast twice at 0.65x | Nepty |
| Wool Cloak | Robe | Defense 2 more, Wind 1.1x, cream cape | Chest in Melrode's shepherd's hut (was Fresh Bread) |
| Gale Tether | Sigil | Wind spells fire one more shot | Zelse |
| Lead Mantle | Robe | Defense 10 more, Earth 1.1x | Glencoe Forest cabin chest (was Silent Flute) |
| Glass Charm | Charm | Deal and take 25% more damage | Cull Hazard chest #5 (was Silent Flute) |
| Stone Memory | Sigil | Earth spells cast in a row: +15% each, up to +30% | Isle of Skye ship chest (was Dragon's Potion) |
| Hunter's Charm | Charm | Battles pay half again in gold | Dindom Dries hut chest (was Giant's Shoes) |
| Spellstrike | Charm | First staff swing each turn gives a charge | Limelin Castle treasury chest #1 (was Silver Amulet) |
| Prism Staff | Staff | Staff 6 more, all magic 1.1x | Limelin Castle treasury chest #2 (was Celine's Bell) |
| Ember Heart | Sigil | Kindling adds one more Burn stack | Shamwood, Lavaar's room chest (was Golden Amulet) |
| Mountain Staff | Staff | Staff 8 more, Earth 1.25x, Earth weakness hits take 2 shields | Baragoon Tunnel chest #6 (was Silver Amulet) |
| Phoenix Staff | Staff | Staff 8 more, Fire 1.25x, swings Burn | Boil Hole chest #5 (was Dragon's Potion) |
| Knight's Mantle | Robe | Defense 16 more | Baragoon Moor inn chest #6 (was Celine's Bell) |
| Leviathan Staff | Staff | Staff 8 more, Water 1.25x, heals 25% more | Brannoch Castle cannon room chest #2 (was Heroes Drink) |
| Titan Staff | Staff | Staff 30 more; first swing a turn cracks a shield | Brannoch Castle chest (was Golden Amulet) |
| Skyreed Staff | Staff | Staff 8 more, Wind 1.25x, Wind Boost +10% more shots per charge | Leonardo section chest #2 (was Celine's Bell) |
| Dragon Robe | Robe | Defense 24 more, immune to Poison | Leonardo section chest #4 (was Silver Amulet) |
| Prism Sigil | Sigil | A spell of a different element from the last: 20% more | Beigis |

The pieces are built around the six styles: Pyromancer (Ember Staff, Ember Heart, Phoenix Staff), Stonewall
(Lead Mantle, Stone Memory, Mountain Staff), Tidecaller (Leech Pillar, Tidal Echo, Leviathan Staff), Tempest
(Gale Tether, Pilgrim's Soul, Skyreed Staff), Staff Brawler (Heavy Staff, Spellstrike, Titan Staff) and
Elementalist (Wool Cloak, Prism Staff, Prism Sigil); wild cards: Gambler's Coin, Hunter's Charm, Glass Charm,
Mirror Shard. Chest pieces are given on load to saves that already opened those chests.

- [ ] Not checked in game yet: any of the new pieces. Easiest check: open one of those chests (the message
      should say "Brian found ...") and look in the Equipment window.

- Checked by Claude: the Equipment window opens and shows the slots (charm and sigil locked on your save).
- [ ] Not checked yet: equipping, each effect in battle, buying pieces in shops, boss rewards ("Found ...").

## Hint Scrolls (new)

Every shop can roll a **Hint Scroll** (150 gold, one a day) into its rotating slots. Buying one teaches a spell
you can already cast but haven't discovered (lowest tier first): the shopkeeper shows how to cast it ("CAST IT:
EARTH EARTH") and the notice says "Learned ...". With nothing left to teach, the shop refuses the sale.
- Checked by Claude: Melrode, bought with 1000 gold set in RAM; it taught Boulder (game closed, not saved).

## Restocks by beat (new)

After Zelse, Melrode and Dondoran can roll Healing Potions, Heroes Drinks and Giant's Shoes; after Shilf, Melrode,
Dondoran, Larapool and Normoon can roll Spirit Light, Dragon's Potion and Golden Amulets. They join the rotating
pool, so they still compete for the three daily slots.
- Checked by Claude: shelves unchanged on a save before Zelse. Not yet seen after a boss.

## Attunement tiers (changed)

Tier gates now match the level vanilla needed for each tier's first spell: Lv 7 / 16 / 25 / 36, costing
3 / 6 / 10 / 15 Spirit Points (was Lv 6 / 14 / 22 / 30 for 2 / 4 / 7 / 10). One element to tier 5 is 34 points;
the game has 98 and talents share them. Tiers you already attuned stay attuned.
Epicentre no longer makes a Broken boss lose two turns (bosses lose one, like Freeze).

## Boss fights (new)

From the design rule that every boss uses at least three tricks (game design document, "Keeping 1v1 boss fights
from becoming a rotation"):
- **Growing shields** (already in): a boss gets one more shield each time it comes back from a Break.
- **Beigis changes element**: he starts every fight as Fire (weak to Water). Each time he comes back from a Break
  he takes one of the four elements at random, 25% each (the same one can come up again). His weakness becomes the
  opposite element (Fire/Water, Earth/Wind), hidden until hit, and damage follows his new element too. His model
  glows steadily in his element's color: orange Fire, tan Earth, blue Water, green Wind (statuses pulse over it).
  Every other boss keeps its element and weakness all fight. Guilty and Mammon have no weakness: Break them with
  the Titan Staff, Cyclone or Glacier, or win without Breaking.
- **Adaptation**: casting the same element 4 or more times in a row, the boss takes 40% less from it (blue number)
  until you cast a different element. Three in a row is free, so Stone Memory still reaches +30%.
- **Random attacks** (the game's own boss AI).
- Checked by Claude with a test build that gave Zelse Beigis's mechanic: he started as Fire with the orange glow;
  six Breaks rolled Earth, Wind, Earth, Fire, Wind, Fire, each with the right opposite weakness and glow. Not
  checked in Beigis's own fight (warping a fresh save straight to Brannoch crashes the game).
- [ ] Adaptation: not checked yet (needs real spell casts). Watch for blue numbers on the 4th same-element cast.

## Element staves (changed)

The Ember Staff was far ahead of the other three, so all four now give swings +6 and their element 1.1x, plus a
perk each (table above). Detonations (Ember Staff swings and Wildfire) play the Ember spell's sound (0x0F).
- Checked by Claude in battle: Gale move 1.3x; Stone defense 5 to 17 over four hits (capped); Tide healed 5 from a
  10-damage swing; an Ember swing on a burning foe spread a Burn stack to the foe next to it. The sound wasn't
  heard (no audio in tests).

## Balance pass (new, 2026-10-21)

Full reasoning in `research/balance/README.md`. In short: the new systems make a focused Brian deal about
1.15x vanilla damage per turn early and 2.2x at the end, so enemies got more HP to match, a bit more than that.

- Normal monsters: HP x1.2 (Holy Plains) up to x2.1 (Mammon's World); attack x1.0 up to x1.2.
- Bosses: Solvaring 240 HP, Zelse 1054, Nepty 1496, Shilf 1800, Fargo 2850, Guilty 3600, Beigis 3895,
  Mammon 4945 (vanilla 200 / 680 / 880 / 1000 / 1500 / 1800 / 1900 / 2300).
- Wind Boost: 40% more shots per charge instead of 2 more (Crosswind and Skyreed add 10% each).
- Burn: 20% of the kindling hit per stack per turn (was 30%).
- Tidal Echo: 0.65x twice (was 0.75x). Stone Memory: up to +30% (was 45%).
- Checked by Claude: the boss records and a Holy Plains battle read back scaled HP; scaling does not pile up
      when a map is loaded again.
- [ ] Count Brian's turns per fight. The target is about the same as vanilla, or one more. Tell me which
      fights feel too long or too short.

## Known rough edges

- Burn shows as "B1"-"B3" in the enemy list (checked by Claude, including a Burn tick that kills).
- The enemy list partly covers the game's own target-name box; the battle UI pass will sort out placement.
- The dialogue font has no + % or / (full symbol table in research/menus), so effects read like "Fire 1.1x".
- Changing gold with the dev panel shows up as "Paid"/"Won" in the gold notice.
