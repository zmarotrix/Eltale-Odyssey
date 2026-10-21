# Models, lighting and particles

Findings from the burn and Break visuals (2026-10-20/21). Function names are in Quest64Syms.

## Drawing models

- `model_draw` (`0x8001DB38`, `(flags, object, animation, gfx)`) draws one model's bone tree; `model_draw_bone`
  (`0x8001DC78`) does each bone. Callers: `enemy_draw_all` (`0x80008CF4`, object = enemy slot + 0x24, its battle
  data), Brian (`0x80003870`, object = `0x8007BACC`), NPCs (`0x80007F18`), bosses (`0x8000B618`) and field objects
  (`0x80011D28`).
- Battle data `+0x52` is the animation index, `+0x54` the frame within it (u16; the game advances it after each
  draw), `+0x56` a running counter, `+0x58` passed to the per-frame animation update.
- Models are lit: geometry mode includes `G_LIGHTING`, combine is mostly `TEXEL0 x SHADE`. The game loads three
  directional lights and an ambient one with F3D `G_MOVEMEM` (`0x03860010`.. `0x038C0010`) from `0x80085770`,
  `0x80085780`, `0x80085790` and `0x80085768` (ambient). Loading changed copies before one model's draw and the
  originals after it tints just that model. The lights already add up past full brightness, so tints must cut
  channels (pulling toward a color barely shows) and nothing can go brighter than the texture (no white flash).
- Display lists, textures and lights the renderer reads must be in the console's 8 MB; mod memory is above it
  (the mod uses `0x807F0000`-`0x807FFFFF`, unused by the game: icons, tint lights, the dev sound mailbox at
  `0x807F1F00`, and its own UI display lists from `0x807F4000`).

## Brian's model (field and battle, loaded around `0x8020xxxx` on the test save)

- **Cape**: six bone display lists (`0x8020BB40`, `...BBF0`, `...BCA0`, `...BD50`, `...BE00`, `...BEB0`) with an IA8
  texture (`0x80209220`), combine `FC127E03 FFFFF378` and `SetPrimColor E90000FF`: the cape's red is that prim
  color. Changing it recolors the cape (tested: green).
- Brown pieces (hair or boots) use prim `4B2500FF`; skin-tone pieces `D5B94BFF` / `D5B63CFF`.
- **Staff**: one bone (`0x8020BFE0` on the test save) loading two RGBA16 textures: the shaft (`0x80209340`, 16 x 16,
  tile word `07010140`) then the head (`0x80209540`, 16 x 32, tile word `07054140`). Other parts of Brian share the
  shaft's tile layout (`0x80207D60`), so find the head first and take the shaft load just before it.
- Cape pieces: the SetPrimColor comes a few commands after the combine (texture and vertex loads in between).
- **Collar and chest gem**: the chest bone loads the gem (`0x80207F60`, RGBA16 16 x 16, tile `07010040`) and right
  after it the red collar (`0x80208160`, RGBA16 24 x 16, timg `FD100017`, tile `07050150`). The white shirt is
  `0x80207D60`.
- appearance.c recolors the cape, staff, collar and gem from Brian's equipment.

## Particles

- `particle_spawn` (`0x8001B108`) copies a 0x40-byte particle into a 256-entry pool (`0x80088428`): `+0x00` life
  (u16 frames), `+0x04` update callback, `+0x10` texture, `+0x14` draw commands, `+0x18` position, `+0x28` scale,
  `+0x30` velocity (damped 10% a frame by the sparkle callback), `+0x3C` color index into the RGBA table at
  `0x800EBD1C` (2 red, 6 yellow, 7 white, 8 scarlet, 13 pale gold, 15 orange).
- The sparkle template is at `0x8004CFB0` (texture `0x800C4808`, a 16x16 I8 star; draw commands `0x800C4948`
  with combine `TEXEL0 + (1 - TEXEL0) x ENV`, which washes colors toward white).
- `particle_emitter_add` (`0x80019354`) queues one of 7 emitters (`0x800874E8`) that spray sparkles: the enemy death
  puff and spell impacts use it.

## Sounds

Constant sound IDs the game plays (through `sfx_play_scaled`, sound in the second argument): `0x18` a hit
(`enemy_take_damage`, `player_take_damage`), `0x0A` an enemy's hurt reaction, `0x17` (near enemy death and player
damage code), `0x21` (Brian's spawn/idle), `0x3B` the item jingle, `0x01`/`0x02` menu confirm/cancel. The mod's
sound browser (hold L, D-pad) plays any ID for picking by ear.
