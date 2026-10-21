#include "eo.h"

// Town shops (game design document, "Shops" and "Daily stock"; prices from the pacing plan).
//
// The Wings "shops" (and Greenoch's item shop) give their item away in vanilla, using text condition 2. When a
// room loads, their NPC records are switched to SHOP_TEXT_CONDITION so the game just loads their text; the
// shopkeeper greets Brian, and once the conversation closes a native shop window opens.
// Every shop has fixed staples plus ROTATING_SLOTS picks from its pool that re-roll each in-game day. The picks
// are seeded by town and day, so reloading a save shows the same shelves.

#define SHOP_TEXT_CONDITION 7
#define ROTATING_SLOTS 3
#define MAX_STAPLES 4
#define MAX_POOL 8
#define MAX_ROWS (MAX_STAPLES + ROTATING_SLOTS)

// Live NPC slots: 16 x 0x84 bytes; +0x80 points at the NPC's map record (+0x02 text condition, +0x08 text ID).
#define NPC_SLOTS 0x8007BD30
#define NPC_SLOT_SIZE 0x84
#define NPC_SLOT_COUNT 16
#define NPC_RECORD(slot) (*(u8**)(NPC_SLOTS + (slot) * NPC_SLOT_SIZE + 0x80))
#define RECORD_CONDITION 0x02
#define RECORD_TEXT_ID   0x08

#define INVENTORY_EMPTY 0xFF

void inventory_add_item(u8 item);
s32 inventory_count_item(u8 item);
void msgwin_open(s32 position);
extern u8 gMessageBuffer[];
extern s32 gMsgChoiceCount;
extern u16 gPadButtonsNew;
extern u16 gPadButtonPressed;
extern s8 gPadStickX;
extern s8 gPadStickY;

enum {
    SPIRIT_LIGHT, FRESH_BREAD, HONEY_BREAD, HEALING_POTION, DRAGONS_POTION, DEW_DROP, MINT_LEAVES, HEROES_DRINK,
    SILENT_FLUTE, CELINES_BELL, REPLICA, GIANTS_SHOES, SILVER_AMULET, GOLDEN_AMULET,
    WHITE_WINGS, YELLOW_WINGS, BLUE_WINGS, GREEN_WINGS, RED_WINGS, BLACK_WINGS, ITEM_COUNT
};

typedef struct {
    const char* name;
    u16 price;
    const char* effect;
} ItemInfo;

// Effects from the game's item records (ROM 0xD86AC0); prices keep money slightly short (about 1 fight of
// gold for a small heal early on) and Wings follow the pacing plan.
static const ItemInfo items[ITEM_COUNT] = {
    [SPIRIT_LIGHT]   = { "Spirit Light",    600, "Restores all HP." },
    [FRESH_BREAD]    = { "Fresh Bread",      25, "Restores 50 HP." },
    [HONEY_BREAD]    = { "Honey Bread",      60, "Restores 100 HP." },
    [HEALING_POTION] = { "Healing Potion",  110, "Restores 150 HP." },
    [DRAGONS_POTION] = { "Dragon's Potion", 700, "Restores all MP." },
    [DEW_DROP]       = { "Dew Drop",         30, "Restores 10 MP." },
    [MINT_LEAVES]    = { "Mint Leaves",      70, "Restores 20 MP." },
    [HEROES_DRINK]   = { "Heroes Drink",    160, "Restores 40 MP." },
    [SILENT_FLUTE]   = { "Silent Flute",    250, "Silences foes in battle." },
    [CELINES_BELL]   = { "Celine's Bell",   250, "Holds foes in place." },
    [REPLICA]        = { "Replica",         150, "Escape from a battle." },
    [GIANTS_SHOES]   = { "Giant's Shoes",   200, "Walk faster for a while." },
    [SILVER_AMULET]  = { "Silver Amulet",   300, "Raises defense in battle." },
    [GOLDEN_AMULET]  = { "Golden Amulet",   800, "Greatly raises defense." },
    [WHITE_WINGS]    = { "White Wings",      40, "Fly to Melrode." },
    [YELLOW_WINGS]   = { "Yellow Wings",     60, "Fly to Dondoran." },
    [BLUE_WINGS]     = { "Blue Wings",      150, "Fly to Larapool." },
    [GREEN_WINGS]    = { "Green Wings",     150, "Fly to Normoon." },
    [RED_WINGS]      = { "Red Wings",       300, "Fly to Limelin." },
    [BLACK_WINGS]    = { "Black Wings",     450, "Fly to Brannoch." },
};

#define END 0xFF

typedef struct {
    u16 text_id;      // the shopkeeper's first text ID (NPC record +0x08), from the NPC Locations sheet
    u8 staples[MAX_STAPLES];
    u8 pool[MAX_POOL];
} Shop;

static const Shop shops[] = {
    { 0x1BB0, { FRESH_BREAD, DEW_DROP, WHITE_WINGS, END },                  // Ingram, Melrode
              { HONEY_BREAD, MINT_LEAVES, REPLICA, SILVER_AMULET, YELLOW_WINGS, END } },
    { 0x36B8, { FRESH_BREAD, HONEY_BREAD, DEW_DROP, YELLOW_WINGS },         // Thom, Dondoran
              { MINT_LEAVES, HEALING_POTION, REPLICA, SILVER_AMULET, CELINES_BELL, WHITE_WINGS, END } },
    { 0x5880, { HONEY_BREAD, MINT_LEAVES, BLUE_WINGS, END },                // Jiryo, Larapool
              { HEALING_POTION, HEROES_DRINK, SILENT_FLUTE, CELINES_BELL, GIANTS_SHOES, SILVER_AMULET, YELLOW_WINGS, END } },
    { 0x6928, { HONEY_BREAD, MINT_LEAVES, GREEN_WINGS, END },               // Anette, Normoon
              { HEALING_POTION, HEROES_DRINK, SILENT_FLUTE, CELINES_BELL, GIANTS_SHOES, GOLDEN_AMULET, SPIRIT_LIGHT, BLUE_WINGS } },
    { 0x7900, { HEALING_POTION, HEROES_DRINK, RED_WINGS, END },             // Ring, Limelin
              { SPIRIT_LIGHT, DRAGONS_POTION, GOLDEN_AMULET, GIANTS_SHOES, SILENT_FLUTE, CELINES_BELL, GREEN_WINGS, END } },
    { 0xB3B0, { HEALING_POTION, HEROES_DRINK, END },                        // Bronze, Greenoch
              { SPIRIT_LIGHT, DRAGONS_POTION, GOLDEN_AMULET, REPLICA, RED_WINGS, END } },
    { 0x87B8, { HEALING_POTION, HEROES_DRINK, BLACK_WINGS, END },           // Tom, Brannoch
              { SPIRIT_LIGHT, DRAGONS_POTION, GOLDEN_AMULET, GIANTS_SHOES, SILENT_FLUTE, CELINES_BELL, RED_WINGS, END } },
};
#define SHOP_COUNT (sizeof(shops) / sizeof(shops[0]))

static int shop_for_text(u16 text_id) {
    for (u32 i = 0; i < SHOP_COUNT; i++) {
        if (shops[i].text_id == text_id) {
            return (int)i;
        }
    }
    return -1;
}

// A room was set up: turn the item givers that run shops into plain talkers (no free item).
RECOMP_HOOK_RETURN("npc_spawn_room") void eo_shop_on_room(void) {
    for (int slot = 0; slot < NPC_SLOT_COUNT; slot++) {
        u8* record = NPC_RECORD(slot);
        if (record != 0 && shop_for_text(*(u16*)(record + RECORD_TEXT_ID)) >= 0) {
            *(u16*)(record + RECORD_CONDITION) = SHOP_TEXT_CONDITION;
        }
    }
}

static int talking_shop(void) {
    u8* record = NPC_RECORD(gTalkTargetNpc);
    if (record == 0 || *(u16*)(record + RECORD_CONDITION) != SHOP_TEXT_CONDITION) {
        return -1;
    }
    return shop_for_text(*(u16*)(record + RECORD_TEXT_ID));
}

// ---- Today's stock ----

static u8 stock[MAX_ROWS];
static int stock_count = 0;

static u32 next_random(u32* state) {
    *state = *state * 1103515245u + 12345u;
    return (*state >> 16) & 0x7FFF;
}

static void build_stock(int shop) {
    const Shop* s = &shops[shop];
    stock_count = 0;
    for (int i = 0; i < MAX_STAPLES && s->staples[i] != END; i++) {
        stock[stock_count++] = s->staples[i];
    }
    int pool_size = 0;
    while (pool_size < MAX_POOL && s->pool[pool_size] != END) {
        pool_size++;
    }
    u8 picked[MAX_POOL] = { 0 };
    u32 seed = (u32)(shop + 1) * 7919u + (u32)gCurrentDay * 104729u;
    for (int n = 0; n < ROTATING_SLOTS && n < pool_size; n++) {
        int pick = (int)(next_random(&seed) % (u32)pool_size);
        while (picked[pick]) {
            pick = (pick + 1) % pool_size;
        }
        picked[pick] = 1;
        stock[stock_count++] = s->pool[pick];
    }
}

// ---- Talking to the shopkeeper ----

#define MSG_WINDOW_POSITION (*(s32*)0x8008C644)
#define TEXT_END 0xFF

static int shop_pending = -1;  // shop to open once the greeting closes
static int shop_open = -1;     // shop whose window is open

// npc_talk_begin just loaded the shopkeeper's text: greet instead.
RECOMP_HOOK_RETURN("npc_talk_begin") void eo_shop_on_greeting(void) {
    int shop = talking_shop();
    if (shop < 0) {
        return;
    }
    eo_text_encode_full(gMessageBuffer, "Welcome, traveler. Have a\nlook at what I have today.");
    shop_pending = shop;
}

// ---- The shop window ----

#define WIN_X 32
#define WIN_Y 24
#define WIN_W 256
#define ROW_H 14
#define TITLE_Y (WIN_Y + 8)
#define LIST_Y (WIN_Y + 26)
#define NAME_X (WIN_X + 26)
#define PRICE_X (WIN_X + 160)
#define OWNED_X (WIN_X + 214)
#define BTN_A 0x8000
#define BTN_B 0x4000
#define BTN_DUP 0x0800
#define BTN_DDOWN 0x0400
#define STICK_THRESHOLD 40
#define MESSAGE_FRAMES 60

static int cursor = 0;
static u16 menu_pressed = 0;
static int stick_held = 0;
static int message_timer = 0;
static const char* message = "";

static void open_shop(int shop) {
    build_stock(shop);
    shop_open = shop;
    cursor = 0;
    message_timer = 0;
    menu_pressed = 0;
}

// While the window is open the game sees no input (Brian can't walk or open menus).
RECOMP_HOOK_RETURN("pad_read") void eo_shop_read_input(void) {
    if (shop_open < 0) {
        return;
    }
    menu_pressed |= gPadButtonsNew;
    // The stick moves the cursor once per push.
    if (gPadStickY > STICK_THRESHOLD || gPadStickY < -STICK_THRESHOLD) {
        if (!stick_held) {
            menu_pressed |= gPadStickY > 0 ? BTN_DUP : BTN_DDOWN;
        }
        stick_held = 1;
    } else {
        stick_held = 0;
    }
    gPadButtonsNew = 0;
    gPadButtonPressed = 0;
    gPadStickX = 0;
    gPadStickY = 0;
}

static void say_message(const char* text) {
    message = text;
    message_timer = MESSAGE_FRAMES;
}

static void handle_input(void) {
    u16 pressed = menu_pressed;
    menu_pressed = 0;
    if (pressed & BTN_B) {
        sfx_play(SFX_CANCEL, 0xFF, 0x40);
        shop_open = -1;
        return;
    }
    if (pressed & BTN_DUP) {
        cursor = (cursor + stock_count - 1) % stock_count;
        message_timer = 0;
    } else if (pressed & BTN_DDOWN) {
        cursor = (cursor + 1) % stock_count;
        message_timer = 0;
    } else if (pressed & BTN_A) {
        u8 item = stock[cursor];
        if (GOLD < items[item].price) {
            sfx_play(SFX_CANCEL, 0xFF, 0x40);
            say_message("Not enough gold.");
        } else if (inventory_count_item(INVENTORY_EMPTY) == 0) {
            sfx_play(SFX_CANCEL, 0xFF, 0x40);
            say_message("You can't carry any more.");
        } else {
            GOLD -= items[item].price;
            inventory_add_item(item);
            eo_gold_resync();  // the shop shows the gold itself
            sfx_play(SFX_CONFIRM, 0xFF, 0x40);
            say_message("Thank you.");
        }
    }
}

static void draw_shop(void) {
    s32 h = 26 + stock_count * ROW_H + 30;
    char line[32];
    eo_ui_window(WIN_X, WIN_Y, WIN_W, h);
    eo_ui_text(WIN_X + 12, TITLE_Y, "Shop");
    eo_format_amount(line, "Gold", GOLD, 11, "");
    eo_ui_text(WIN_X + WIN_W - 12 - 11 * 8, TITLE_Y, line);
    for (int i = 0; i < stock_count; i++) {
        const ItemInfo* item = &items[stock[i]];
        s32 y = LIST_Y + i * ROW_H;
        eo_ui_text(NAME_X, y, item->name);
        eo_format_amount(line, "", item->price, 5, "");
        eo_ui_text(PRICE_X, y, line);
        eo_format_amount(line, "x", (u32)inventory_count_item(stock[i]), 3, "");
        eo_ui_text(OWNED_X, y, line);
    }
    s32 info_y = LIST_Y + stock_count * ROW_H + 6;
    eo_ui_text(WIN_X + 12, info_y, message_timer > 0 ? message : items[stock[cursor]].effect);
    if (message_timer > 0) {
        message_timer--;
    }
    hud_draw_sprite(1, WIN_X + 8, LIST_Y + cursor * ROW_H - 1, 8, 0xD);  // the game's pointing-hand cursor
}

// The shop window draws over the field HUD; the greeting closes first, then the window opens.
RECOMP_HOOK_RETURN("hud_draw_main") void eo_shop_draw(void) {
    if (shop_open < 0 && shop_pending >= 0 && !(gGameState & 0x2) && gMsgWinOpenCount == 0) {
        open_shop(shop_pending);
        shop_pending = -1;
    }
    if (shop_open < 0) {
        return;
    }
    handle_input();
    if (shop_open >= 0) {
        draw_shop();
    }
}
