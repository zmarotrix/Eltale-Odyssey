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
void hud_draw_sprite(s32 image, s32 x, s32 y, s32 width, s32 height);  // a menu image (1 = hand, 16 x 13)
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
    // Spirits (spirits.c). Zero in saves from before them: spirits_ready = 0 sets them up on load.
    u8 spirits_ready;
    u8 spirit_points;      // unspent
    u8 spirits_counted;    // spirits already turned into points
    u8 pad2;
    u8 tiers[4];           // attuned tier per element, 1-5
    u8 discovered[8];      // 60 bits: school * 15 + index
    // Equipment (equipment.c)
    u32 owned_equipment;   // one bit per piece
    u8 equipped[4];        // staff, robe, charm, sigil: piece + 1, 0 = none
    u8 talent_ranks[40];   // ranks learned per talent (talents.c)
    u8 reserved[80];
} EoSaveBlock;

_Static_assert(sizeof(EoSaveBlock) == 160, "the save block must fill the 160 free bytes exactly");

extern EoSaveBlock gEo;

void eo_reset(void);
u32 eo_now(void);

// ---- Native UI pieces (ui.c) ----
// Small text in the HUD's 6 x 8 font. Palettes on parchment: 0 dark grey (normal), 1 red (warning),
// 2 light grey (unavailable), 3 green (selected, good news).
#define SMALL_W 6        // character width
#define SMALL_LINE 10    // line height
#define SMALL_PAD 5      // window padding
enum { TEXT_NORMAL, TEXT_WARN, TEXT_DIM, TEXT_GOOD };
void eo_ui_small(s32 x, s32 y, int palette, const char* text);
void eo_ui_window(s32 x, s32 y, s32 w, s32 h);
void eo_ui_text(s32 x, s32 y, const char* text);  // letters, digits, spaces and . , ? '
// Plain text to the game's encoding ('\n' line break, '\f' page break). _full adds the prefix and end byte.
u8* eo_text_encode(u8* out, const char* text);
void eo_text_encode_full(u8* out, const char* text);
// "<label><amount right-aligned so the two fill `columns` characters><unit>": amounts line up in the
// fixed-width dialogue font.
void eo_format_amount(char* out, const char* label, u32 amount, int columns, const char* unit);

// Fills a line: %w becomes word, %a and %b the numbers a and b. Returns out.
char* eo_text_fill(char* out, const char* line, const char* word, u32 a, u32 b);

// Copies text to out and returns the new end (out stays zero-terminated).
char* eo_append(char* out, const char* text);
// "WERE HARE" -> "Were Hare" (the game's names are uppercase ASCII), cut to max_chars.
void eo_title_case(char* out, const char* name, int max_chars);

// ---- Calendar (calendar.c) ----
const char* eo_weekday_name(u32 day);
int eo_is_night(void);
void eo_calendar_on_rest(void);  // a paid night: wake at dawn

// ---- Boot steps, run in order by main.c ----
void eo_enemy_spells_init(void);
void eo_balance_apply(void);
void eo_spirits_init_tables(void);
void eo_status_init(void);

// ---- Combat (combat.c) ----
int eo_combat_active_boost(void);   // Spirit Charge spent on the spell being cast, 0 if none
int eo_combat_active_school(void);  // its element (0 Fire .. 3 Wind), -1 if none

// ---- Spirits (spirits.c) ----
void eo_spirits_mark_dirty(void);
void eo_notice(const char* a, const char* b);  // bottom-of-screen notice, a and b joined  // the save block changed: rewrite the spell locks and names

// ---- Icons (icons.c) ----
enum {
    ICON_CHARGE_EMPTY, ICON_CHARGE_FULL, ICON_CHARGE_SPEND,
    ICON_FIRE, ICON_EARTH, ICON_WATER, ICON_WIND,   // element diamonds, in element order
    ICON_BURN, ICON_BROKEN, ICON_POISON, ICON_POINTER,
    ICON_COIN, ICON_PIP_EMPTY, ICON_PIP_FULL, ICON_LOCK,
    ICON_STAFF, ICON_ROBE, ICON_CHARM, ICON_SIGIL,   // equipment slots, in slot order
    ICON_PIP_STEEL,                                  // an enemy's shield
    ICON_COUNT
};
#define ICON_TEXT_DY (-2)   // draw an icon at a small-text line's y + ICON_TEXT_DY to line them up
#define ICON_W 12
void eo_icons_init(void);
void eo_icons_begin(void);                 // loads the icon palette; call before drawing icons
void eo_icon_draw(int icon, s32 x, s32 y);  // 12 x 12
int eo_screen_pos(f32 x, f32 y, f32 z, s32* sx, s32* sy);  // scene point to screen; 0 if off screen
const char* eo_spell_description(int school, int index);
const char* eo_spell_name(int school, int index);
int eo_spirits_known(int school, int index);
s32 eo_strlen(const char* text);  // characters in a zero-terminated string

// ---- Burn visuals (fx.c) ----
void eo_fx_burn_hit(int enemy_slot);  // flames rise from the enemy for a moment
void eo_fx_shield_hit(int enemy_slot, int broke);  // glints fly off the enemy (more when it breaks)
void eo_fx_flare(int enemy_slot);     // a ring of embers bursting out from the enemy
void eo_status_flare(int enemy_slot); // Ember Staff: spread the enemy's Burn to those around it
int eo_combat_broken(int enemy_slot);
int eo_status_poisoned(void);
void eo_fx_update(void);              // every frame in battle

// ---- Talents (talents.c): a tree per element, bought rank by rank with Spirit Points ----
enum {
    // Fire
    TALENT_SMOULDER, TALENT_FANNED_FLAMES, TALENT_KINDLING, TALENT_CONFLAGRATION, TALENT_FIREBRAND,
    TALENT_WILDFIRE, TALENT_SEARING, TALENT_HEAT_HAZE, TALENT_INFERNO,
    // Earth
    TALENT_BEDROCK, TALENT_WEIGHT, TALENT_STONESKIN, TALENT_SHATTER, TALENT_FAULT_LINE,
    TALENT_AFTERSHOCK, TALENT_BULWARK, TALENT_LANDSLIDE, TALENT_EPICENTRE,
    // Water
    TALENT_DEEP_FREEZE, TALENT_WELLSPRING, TALENT_TIDAL_FORCE, TALENT_STILL_WATERS, TALENT_UNDERTOW,
    TALENT_FROSTBITE, TALENT_RIPTIDE, TALENT_UNDERCURRENT, TALENT_GLACIER,
    // Wind
    TALENT_CROSSWIND, TALENT_LOCK_ON, TALENT_TAILWIND, TALENT_SHIFTING_WINDS, TALENT_EYE_OF_THE_STORM,
    TALENT_CYCLONE, TALENT_SECOND_WIND, TALENT_UPDRAFT, TALENT_MAELSTROM,
    TALENT_COUNT
};
#define TALENTS_PER_SCHOOL 9
typedef struct {
    const char* name;
    u8 school;
    u8 level;          // element level needed
    u8 ranks;          // most ranks
    u8 cost;           // Spirit Points per rank
    const char* effect;
} TalentInfo;
const TalentInfo* eo_talent_info(int talent);
int eo_talent(int talent);         // ranks learned, 0 = none
int eo_talent_learn(int talent);   // 1 if a rank was learned now (level and points checked)

// ---- Lock-On (lockon.c): Wind's Z-targeting talent ----
int eo_lockon_target(void);   // enemy slot Brian is locked onto, -1 if none
void eo_fx_lockon_marker(int enemy_slot);   // fx.c: the sparkle over the target's head

// ---- One status at a time (status.c) ----
// 1 if `fighter` (battle data) has a status that keeps a new one off. `by_brian`: the new status is Brian's
// (Shifting Winds lets his through). `except_burn`: the new status is Burn, which stacks on a burning foe.
int eo_status_blocks(void* fighter, int by_brian, int except_burn);
int eo_status_burn_percent(void);

// ---- Equipment (equipment.c) ----
enum {
    EQ_EMBER_STAFF, EQ_HEAVY_STAFF, EQ_STONE_STAFF, EQ_TIDE_STAFF, EQ_GALE_STAFF,
    EQ_TRAVELERS_ROBE, EQ_TIDECLOTH_ROBE,
    EQ_PILGRIMS_SOUL, EQ_GAMBLERS_COIN, EQ_MIRROR_SHARD,
    EQ_TIDAL_ECHO, EQ_LEECH_PILLAR,
    EQUIPMENT_COUNT
};
typedef struct {
    const char* name;
    u8 slot;          // 0 staff, 1 robe, 2 charm, 3 sigil
    u16 price;        // in shops; 0 = not sold
    const char* effect;
} EquipmentInfo;
const EquipmentInfo* eo_equipment_info(int piece);
int eo_equipment_owned(int piece);
int eo_equipment_worn(int piece);
void eo_equipment_give(int piece);
int eo_equipment_window_open(void);
int eo_equip_staff_bonus(void);
int eo_equip_element_percent(int element);
int eo_equip_defense(void);
int eo_equip_poison_immune(void);
int eo_spirits_window_open(void);
int eo_inn_here(void);  // standing in one of the 17 inn/save rooms

// ---- Shops (shop.c) ----
int eo_shop_is_open(void);

// ---- Status effects (status.c) ----
void eo_status_cure_poison(void);
int eo_status_burn_stacks(int enemy_slot);  // 0-3
void eo_status_add_burn(int enemy_slot, u16 amount);
void eo_status_fixed_damage(int enemy_slot, u16 amount);  // ignores element and defense

// ---- Gold display (gold.c) ----
void eo_gold_resync(void);
#define NOTICE_COLUMNS 12  // width of a gold notice line, in characters
void eo_gold_notice(const char* line1, const char* line2);
void eo_format_u32(char* out, const char* prefix, u32 value, const char* suffix);

#endif
