# How Quest 64 draws menus, windows and text (USA 1.0)

Status: 2026-10-20, static analysis of the recompiled code. Goal: build new in-engine menus (shops, attunement,
charms and sigils, inn boards, a new battle screen) that look like the game's own.

**Summary.** Menus are drawn every frame as graphics commands added to the game's display list
(`gMasterGfxPos`, the pointer at `0x8007B2FC`). The building blocks we need are all present and callable:
a window drawer (background, 1-pixel border, 3-pixel drop shadow, pop-open animation), two text drawers (a small
uppercase HUD font and a larger mixed-case menu/dialogue font), a sprite drawer for the game's 55 menu images,
a sound-effect call, and a "buttons newly pressed" input variable. Menu headings like "Status" and "Elements"
are pre-drawn images, not text; new headings need new art or the menu font.

## Building blocks

| Function | Proposed name | What it does |
| --- | --- | --- |
| `0x800307D8` | `ui_draw_rect` | `(style, x, y, w, h)`: filled rectangle. Style 0 = window background, 3 = border/shadow color |
| `0x8002FB30` | `ui_draw_window` | Draws a window centered on a point: background, 1 px border on all sides, 3 px drop shadow right and below, scaled by an opening animation |
| `0x80030EA0` | `ui_draw_text` | `(x, y, text)`: draws text in the menu/dialogue font, in the game's own text encoding (below). Used for item names in the inventory and in the save menus |
| `0x80020B4C` | `hud_draw_text` | `(x, y, palette, text)`: draws ASCII text in the small HUD font. Characters: `0-9 : ' - . ? =`, space, `A-Z` |
| `0x80020D18` | `hud_strlen` | Length of a zero-terminated string (used to center HUD text: x = center − 3·length) |
| `0x80029B58` | `hud_draw_sprite` | `(image, x, y, ...)`: draws one of the 55 menu images, with clipping at the screen edge |
| `0x800210FC` | `hud_draw_tex_region` | Draws a rectangle cut from a texture sheet: `(texture, x, y, w, h, s, t, dsdx, dtdy)` |
| `0x80020E2C` | `hud_load_tex_region` | Loads that region into texture memory (CI8 textures) |
| `0x80020F8C` | `hud_tex_rect` | Emits the textured rectangle |
| `0x80025B8C` | `sfx_play` | `(sound, ?, volume)`: plays a sound effect. Menus use 1 = confirm, 2 = cancel, volume `0x40` |
| `0x800314C0` | `pad_read` | Reads the controller (already decompiled): sets `gPadButtonPressed` (held) and `D_80092876` (newly pressed this frame) |

The Controller Pak manager screen (`src/328F0.c`) has its own text code that writes pixels straight into the
framebuffer with the CPU. It's a special boot-time screen; don't build on it.

## Text

**HUD font** (`hud_draw_text`): a 258×8 CI8 sheet at `0x803A2110` (descriptor `0x803A6F80`: pointer, width 258,
height 8), 43 glyphs of 6×8 pixels, 4 colors (transparent, shadow, edge, face). Character order at `0x8004D44C`:
`0123456789:'-.?= ABCDEFGHIJKLMNOPQRSTUVWXYZ`. The `palette` argument picks one of 4 palettes from the table at
`0x803A6FB0`. Spell and item names are stored in uppercase ASCII because of this font.

**Menu/dialogue font** (`ui_draw_text`): a CI8 sheet 152 pixels wide (ROM `0xD3E2C0`, `0xE790` bytes, loaded to
`0x803232A0` by `func_80030E58`; palette ROM `0xD3E240` → `0x80331A30`). It holds digits and punctuation, A–Z,
a–z, and the Japanese kana and kanji left from the Japanese version (`menu_font.png`). Text uses the game's own
encoding (`research/modding-notes-text/Text Information.tsv`): `0x00`–`0x19` letters, `0x80`+ mode switches
(numerals, upper/lower case), `0x7F` space, `0xE0` line break, `0xF0` page break, `0xF2` yes/no box, `0xFF` end,
`0xA0`–`0xA3` colors. The drawer splits each byte with the masks `0x80`, `0x60` and `0x1F`.

### Dialogue font: punctuation and symbols (checked in-game, 2026-10-20)

Rendered every code with a test mod and read the screenshot. Modes: `0x80` digits, `0x81` uppercase, `0x82`
lowercase; `0x7F` is a space in any mode.

| Mode | Code | Glyph |
|---|---|---|
| digits | `0x00`-`0x09` | 0-9 |
| digits | `0x0A` `0x0B` | `!` `?` |
| digits | `0x0C`-`0x0F` | Japanese brackets 「 」 『 』 |
| digits | `0x10` `0x11` `0x12` `0x13` `0x14` `0x15` | `.` `,` `…` `·` `:` `-` |
| digits | `0x16` `0x17` | ♪ ♡ |
| lowercase | `0x1A` `0x1B` `0x1C` | `'` `”` `“` |
| uppercase | `0x1A` | `&` |

There is no `+`, `%` or `/`. Codes `0x18`-`0x1F` (digits) and `0x1D`-`0x1F` (letters) draw nothing.

## Menu images

The game has 55 menu images (`menu_sprites.png`): a table of sizes (width, height bytes) at `0x8005F014` and of
CI8 pixel pointers at `0x8005F0D0` (ROM `0x5FCD0`), all in the main program's data. They include the pointing-hand
cursor (1), left/right arrows (6, 7), a script digit font (10–25), word labels "Status", "Hp", "Mp", "Def", "Agi",
"exp", "Environment", "Message", "BGM", "max", "min", "Map", "Elements", "No.", gauge bars, and the four element
gems (46–49). Entries 2, 3, 31 and 52–54 point at something other than plain pixel data.

## How a menu runs (from the decompiled title screen, `src/overlays/title/DA68F0.c`)

- One state variable drives the menu; each frame the code draws the current state and reacts to input.
- Input is `D_80092876` (newly pressed): `0x8000` A, `0x4000` B, `0x2000` Z, `0x1000` Start. Confirm is
  `0xB000` (A, Z or Start), cancel is `0x4000` (B).
- L opens the game's item panel in the field (found in testing, 2026-10-20), so it isn't free for new menus.
- Every confirm plays `sfx_play(1, 0xFF, 0x40)`, every cancel `sfx_play(2, 0xFF, 0x40)`.
- Fades use an environment color alpha stepped each frame.

## What this means for building new menus

1. A new screen is a function, called once per frame while the screen is open, that adds graphics commands:
   `ui_draw_window` (or `ui_draw_rect` pieces) for boxes, `ui_draw_text` for text, `hud_draw_sprite` for the
   cursor hand and gems, `sfx_play` for confirm/cancel, and `D_80092876` for input.
2. Text for new screens should be stored in the game's encoding (for `ui_draw_text`) or in uppercase ASCII (for
   `hud_draw_text`). A small converter from normal text to the game's encoding will be needed.
3. Headings in the script style ("Shop", "Charms") need new images in the same CI8 format; until then, the menu
   font can stand in.
4. Still unknown: where the pause menu decides which screen is open (the place to add new pages), and how the game
   pauses world updates while a menu is open. Next step: draw a test window with text from a mod or patch, on
   screen, to prove the building blocks work (roadmap gate 1).
