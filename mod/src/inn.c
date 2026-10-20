#include "eo.h"

// Inns cost gold. Each extra night in a row at the same inn costs 50% more (up to 4 extra nights);
// the streak ends once a full day passes after a night, or when you sleep somewhere else.
//
// How it hooks in (research/npcs, "The inn"): when the Yes/No box closes, player_state_talking checks
// "choice pending, NPC text condition 6, answer 0 (Yes)" and starts the rest. This hook runs just before
// that check with the same test: if you can't pay, the answer is changed to No and the game does what it
// always does for No; if you can, the night is paid for.

#define INN_TEXT_CONDITION 6
#define MAX_EXTRA_NIGHTS 4

// The live NPC slots are 0x84 bytes; +0x80 points at the NPC's map record, whose +0x02 is the text condition.
#define NPC_RECORD_PTRS 0x8007BDB0
#define NPC_SLOT_SIZE   0x84

// The price window, centered at the top or bottom of the screen, whichever the game's windows leave free
// (the dialogue box moves between them depending on where Brian stands).
#define PRICE_COLUMNS 15  // label + right-aligned amount; " gold" follows
#define PRICE_WIN_W 180  // 20 characters of the 8 px dialogue font, plus margins
#define PRICE_WIN_H 36
#define PRICE_WIN_X ((320 - PRICE_WIN_W) / 2)
#define PRICE_WIN_TOP_Y    16
#define PRICE_WIN_BOTTOM_Y (240 - 16 - PRICE_WIN_H - 3)

// Message window slots (research/menus, ui_draw_window): flags at +0 (nonzero while in use), then s16 x, y, w, h.
#define MSG_WINDOWS     0x800905E0
#define MSG_WINDOW_SIZE 0x89C
#define MSG_WINDOW_COUNT 4

typedef struct {
    u8 map;
    u8 submap;
    u16 price;
} InnPrice;

// The 17 save points from the game's save spawn table (research/saves), in table order.
// First-pass prices: about 1-2 normal fights of the game's own gold in that area; tune in playtests.
static const InnPrice inn_prices[] = {
    { 0x0F, 0x05,  40 },  //  0 Melrode Inn
    { 0x10, 0x07,  40 },  //  1 Dondoran Inn
    { 0x10, 0x0F,  80 },  //  2 Dondoran Flats Inn
    { 0x12, 0x06,  80 },  //  3 West Carmagh Inn
    { 0x11, 0x00, 100 },  //  4 Larapool Inn
    { 0x12, 0x07,  80 },  //  5 Cull Hazard Cottage
    { 0x14, 0x05, 100 },  //  6 Normoon Inn
    { 0x16, 0x16, 200 },  //  7 Limelin Wharf
    { 0x16, 0x0E, 200 },  //  8 Limelin Inn
    { 0x17, 0x00, 250 },  //  9 Greenoch Inn
    { 0x17, 0x14, 250 },  // 10 Dindom Dries Hut
    { 0x17, 0x16, 400 },  // 11 Baragoon Moor House
    { 0x17, 0x08, 400 },  // 12 Brannoch Inn
    { 0x1A, 0x03, 150 },  // 13 Epona's Cottage
    { 0x19, 0x03, 150 },  // 14 Seagoing Ship
    { 0x1E, 0x0F, 500 },  // 15 Brannoch Castle
    { 0x0F, 0x0D, 500 },  // 16 Floating Monastery
};
#define INN_COUNT (sizeof(inn_prices) / sizeof(inn_prices[0]))
#define UNKNOWN_INN_PRICE 100

static int current_inn(void) {
    for (u32 i = 0; i < INN_COUNT; i++) {
        if (inn_prices[i].map == gNextMap && inn_prices[i].submap == gNextSubmap) {
            return (int)i;
        }
    }
    return -1;
}

// Extra nights in a row this night would be, given where and when the last paid night was.
static u32 nights_in_row(int inn) {
    if (gEo.last_inn != (u8)(inn + 1)) {
        return 0;
    }
    // The previous night itself adds a third of a day, so "in a row" means less than a day after it started.
    // Saves keep the day but not the time of day, so after loading "now" can be earlier than the last night;
    // count that as no time passed.
    u32 now = eo_now();
    if (now > gEo.last_rest_time && now - gEo.last_rest_time >= DAY_LENGTH) {
        return 0;
    }
    u32 extra = gEo.inn_nights_in_row + 1u;
    return extra > MAX_EXTRA_NIGHTS ? MAX_EXTRA_NIGHTS : extra;
}

static u32 night_price(int inn, u32 extra) {
    u32 price = inn >= 0 ? inn_prices[inn].price : UNKNOWN_INN_PRICE;
    for (u32 i = 0; i < extra; i++) {
        price = price * 3 / 2;
    }
    return price;
}

// True while the inn's Yes/No box is up (or answered and about to be read).
static int inn_choice_active(void) {
    if (!(gGameState & GAME_STATE_CHOICE_PENDING)) {
        return 0;
    }
    u8* record = *(u8**)(NPC_RECORD_PTRS + gTalkTargetNpc * NPC_SLOT_SIZE);
    return record != 0 && *(u16*)(record + 2) == INN_TEXT_CONDITION;
}

RECOMP_HOOK("player_state_talking") void eo_inn_on_answer(void) {
    // Same test the game makes next: all windows closed, inn choice pending, answered Yes.
    if (gMsgWinOpenCount != 0 || !inn_choice_active() || gMsgChoiceResult != 0) {
        return;
    }
    int inn = current_inn();
    u32 extra = nights_in_row(inn);
    u32 price = night_price(inn, extra);
    char line1[24];
    char line2[24];

    if (GOLD < price) {
        gMsgChoiceResult = 1;  // the game now treats it as No: no rest, no heal, no save
        sfx_play(SFX_CANCEL, 0xFF, 0x40);
        eo_format_amount(line1, "Gold", GOLD, NOTICE_COLUMNS, "");
        eo_format_amount(line2, "Need", price, NOTICE_COLUMNS, "");
        eo_gold_notice(line1, line2);
        return;
    }

    gEo.last_inn = (u8)(inn + 1);
    gEo.inn_nights_in_row = (u8)extra;
    gEo.last_rest_time = eo_now();
    GOLD -= price;  // shown by the gold notice once the HUD is back
}

// True if a price window at height y would cover any open message window (shadows included).
static int overlaps_game_window(s32 y) {
    for (int i = 0; i < MSG_WINDOW_COUNT; i++) {
        u8* slot = (u8*)(MSG_WINDOWS + i * MSG_WINDOW_SIZE);
        if (*(u32*)slot == 0) {
            continue;
        }
        s32 wx = *(s16*)(slot + 4);
        s32 wy = *(s16*)(slot + 6);
        s32 ww = *(s16*)(slot + 8) + 3;
        s32 wh = *(s16*)(slot + 10) + 3;
        if (PRICE_WIN_X < wx + ww && wx < PRICE_WIN_X + PRICE_WIN_W + 3 && y < wy + wh && wy < y + PRICE_WIN_H + 3) {
            return 1;
        }
    }
    return 0;
}

// While the inn's Yes/No box is open, show the price of tonight and what you have.
// The dialogue frame doesn't run hud_draw_main, so this draws after the message windows instead.
RECOMP_HOOK_RETURN("msgwin_update_all") void eo_inn_draw_price(void) {
    if (!inn_choice_active() || gMsgWinOpenCount == 0) {
        return;
    }
    int inn = current_inn();
    u32 price = night_price(inn, nights_in_row(inn));
    char line1[24];
    char line2[24];
    eo_format_amount(line1, "One night", price, PRICE_COLUMNS, " gold");
    eo_format_amount(line2, "You have", GOLD, PRICE_COLUMNS, " gold");
    // A small window in the dialogue box's own style and font, drawn right after the game's windows.
    s32 y = overlaps_game_window(PRICE_WIN_TOP_Y) ? PRICE_WIN_BOTTOM_Y : PRICE_WIN_TOP_Y;
    eo_ui_window(PRICE_WIN_X, y, PRICE_WIN_W, PRICE_WIN_H);
    eo_ui_text(PRICE_WIN_X + 10, y + 7, line1);
    eo_ui_text(PRICE_WIN_X + 10, y + 21, line2);
}
