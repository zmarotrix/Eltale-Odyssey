#ifndef EO_H
#define EO_H

#include "modding.h"
#include "recomputils.h"
#include "PR/ultratypes.h"

// Shared declarations for Eltale Odyssey. Game names come from Quest64Syms (dump.toml / data_dump.toml);
// the research behind each one is in ../research/<topic>/README.md.

// ---- Game functions ----
s32 pak_read(s32 pak, s32 file, s32 offset, s32 size, void* buf);
s32 pak_write(s32 pak, s32 file, s32 offset, s32 size, void* buf);
void hud_draw_text(s32 x, s32 y, s32 palette, const char* text);
void ui_draw_rect(s32 style, s32 x, s32 y, s32 w, s32 h);
void ui_draw_text(s32 x, s32 y, const u8* text);  // text in the game's own encoding
void sfx_play(s32 sound, s32 arg1, s32 volume);

// ---- Game data ----
extern u32 gGameState;
extern u16 gGameMode;           // 1 = field, 2 = system menu, 3 = title
extern u32 gPlayerMainData[];   // Brian's stat block; see research/saves (summary offsets 0x00-0x37)
extern s32 gNextMap;
extern s32 gNextSubmap;
extern s32 gCurrentTime;        // ticks since the start of the day
extern u16 gCurrentDay;
extern s32 gTalkTargetNpc;      // NPC slot being talked to
extern s32 gMsgWinOpenCount;
extern s32 gMsgChoiceResult;    // -1 pending, 0 = first option (Yes), 1 = No

extern u32* gMasterGfxPos;      // next free command in the frame's display list

// Appends one raw display-list command to the frame.
static inline void eo_gfx(u32 w0, u32 w1) {
    u32* g = gMasterGfxPos;
    g[0] = w0;
    g[1] = w1;
    gMasterGfxPos = g + 2;
}

#define GAME_STATE_CHOICE_PENDING 0x10
#define GAME_STATE_RESTING        0x400

#define DAY_LENGTH 27000

#define SFX_CONFIRM 1
#define SFX_CANCEL  2

// Gold is the game's own unused currency: each monster's "second reward" (record +0x14) is added to
// gPlayerMainData +0x14 after a win, and the vanilla save already stores it (summary offset 0x14).
#define GOLD (gPlayerMainData[0x14 / 4])

// ---- The overhaul's save block ----
// Stored in the 160 bytes at the end of each 512-byte save note, which the vanilla game never writes
// (research/saves, "Where can overhaul data go?"). Old saves have zeros there and get a fresh block.
#define EO_SAVE_OFFSET  0x160
#define EO_SAVE_MAGIC   0x454C544F  // "ELTO"
#define EO_SAVE_VERSION 1

typedef struct {
    u32 magic;
    u16 version;
    u16 checksum;          // sum of the bytes after the header
    u8 last_inn;           // save location index + 1 of the last paid night, 0 = none
    u8 inn_nights_in_row;  // extra nights in a row at last_inn (0 = first night)
    u16 pad;
    u32 last_rest_time;    // day * DAY_LENGTH + time when that night started
    u8 reserved[144];
} EoSaveBlock;

_Static_assert(sizeof(EoSaveBlock) == 160, "the save block must fill the 160 free bytes exactly");

extern EoSaveBlock gEo;

void eo_reset(void);
u32 eo_now(void);

// ---- Native UI pieces (ui.c) ----
void eo_ui_window(s32 x, s32 y, s32 w, s32 h);
void eo_ui_text(s32 x, s32 y, const char* text);  // letters, digits, spaces and . , ? '
// Plain text to the game's encoding ('\n' line break, '\f' page break). _full adds the prefix and end byte.
u8* eo_text_encode(u8* out, const char* text);
void eo_text_encode_full(u8* out, const char* text);
// "<label><amount right-aligned so the two fill `columns` characters><unit>": amounts line up in the
// fixed-width dialogue font.
void eo_format_amount(char* out, const char* label, u32 amount, int columns, const char* unit);

// ---- Boot steps, run in order by main.c ----
void eo_enemy_spells_init(void);
void eo_balance_apply(void);
void eo_status_init(void);

// ---- Status effects (status.c) ----
void eo_status_cure_poison(void);

// ---- Gold display (gold.c) ----
void eo_gold_resync(void);
#define NOTICE_COLUMNS 12  // width of a gold notice line, in characters
void eo_gold_notice(const char* line1, const char* line2);
void eo_format_u32(char* out, const char* prefix, u32 value, const char* suffix);

#endif
