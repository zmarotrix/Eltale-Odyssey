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
void sfx_play(s32 sound, s32 arg1, s32 volume);

// ---- Game data ----
extern u32 gGameState;
extern u32 gPlayerMainData[];   // Brian's stat block; see research/saves (summary offsets 0x00-0x37)
extern s32 gNextMap;
extern s32 gNextSubmap;
extern s32 gCurrentTime;        // ticks since the start of the day
extern u16 gCurrentDay;
extern s32 gTalkTargetNpc;      // NPC slot being talked to
extern s32 gMsgWinOpenCount;
extern s32 gMsgChoiceResult;    // -1 pending, 0 = first option (Yes), 1 = No

extern u32* gMasterGfxPos;      // next free command in the frame's display list

// The HUD's render setup (copy mode, TLUT on, texture-only combine), called by hud_draw_main before its text.
#define HUD_SETUP_DL 0x803A6FC0

// Appends gSPDisplayList(dl) to the frame's display list.
static inline void gfx_call(u32 dl) {
    u32* g = gMasterGfxPos;
    g[0] = 0x06000000;
    g[1] = dl;
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

// ---- Gold display (gold.c) ----
void eo_gold_resync(void);
void eo_gold_notice(const char* line1, const char* line2);
void eo_format_u32(char* out, const char* prefix, u32 value, const char* suffix);

#endif
