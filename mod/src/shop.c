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
#define MAX_POOL 11
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
// A hint scroll isn't an item either: it teaches a spell on the spot (spirits.c). Pacing plan: the Elementalist's
// starter, on every shop's shelf from the first beat, one a day.
#define HINT_SCROLL 0x3F
static const ItemInfo hint_scroll = { "Hint Scroll", 150, "Teaches a spell you can cast." };
// Goods at EQ_BASE and up are the overhaul's equipment (equipment.c), not game items.
#define EQ_BASE 0x40
#define EQ(piece) (EQ_BASE + (piece))

typedef struct {
    u16 text_id;      // the shopkeeper's first text ID (NPC record +0x08), from the NPC Locations sheet
    u8 staples[MAX_STAPLES];
    u8 pool[MAX_POOL];
} Shop;

// Each shop sells its own town's Wings, first and always, and never another town's (playtest, 2026-10-21).
// Greenoch has no Wings item in the game, so it sells none.
static const Shop shops[] = {
    { 0x1BB0, { WHITE_WINGS, FRESH_BREAD, DEW_DROP, EQ(EQ_TRAVELERS_ROBE) },                      // Ingram, Melrode
              { HONEY_BREAD, MINT_LEAVES, REPLICA, SILVER_AMULET, HINT_SCROLL, END } },
    { 0x36B8, { YELLOW_WINGS, FRESH_BREAD, HONEY_BREAD, EQ(EQ_HEAVY_STAFF) },                    // Thom, Dondoran
              { DEW_DROP, MINT_LEAVES, HEALING_POTION, REPLICA, SILVER_AMULET, CELINES_BELL, EQ(EQ_EMBER_STAFF),
                HINT_SCROLL, END } },
    { 0x5880, { BLUE_WINGS, HONEY_BREAD, MINT_LEAVES, END },                                      // Jiryo, Larapool
              { HEALING_POTION, HEROES_DRINK, SILENT_FLUTE, CELINES_BELL, GIANTS_SHOES, SILVER_AMULET,
                EQ(EQ_STONE_STAFF), EQ(EQ_TRAVELERS_ROBE), HINT_SCROLL, END } },
    { 0x6928, { GREEN_WINGS, HONEY_BREAD, MINT_LEAVES, END },                                     // Anette, Normoon
              { HEALING_POTION, HEROES_DRINK, SILENT_FLUTE, CELINES_BELL, GIANTS_SHOES, GOLDEN_AMULET, SPIRIT_LIGHT,
                EQ(EQ_TIDE_STAFF), EQ(EQ_PILGRIMS_SOUL), HINT_SCROLL, END } },
    { 0x7900, { RED_WINGS, HEALING_POTION, HEROES_DRINK, END },                                   // Ring, Limelin
              { SPIRIT_LIGHT, DRAGONS_POTION, GOLDEN_AMULET, GIANTS_SHOES, SILENT_FLUTE, CELINES_BELL,
                EQ(EQ_GALE_STAFF), EQ(EQ_TIDECLOTH_ROBE), HINT_SCROLL, END } },
    { 0xB3B0, { HEALING_POTION, HEROES_DRINK, END },                                              // Bronze, Greenoch
              { SPIRIT_LIGHT, DRAGONS_POTION, GOLDEN_AMULET, REPLICA, EQ(EQ_TIDECLOTH_ROBE), EQ(EQ_EMBER_STAFF),
                HINT_SCROLL, END } },
    { 0x87B8, { BLACK_WINGS, HEALING_POTION, HEROES_DRINK, END },                                 // Tom, Brannoch
              { SPIRIT_LIGHT, DRAGONS_POTION, GOLDEN_AMULET, GIANTS_SHOES, SILENT_FLUTE, CELINES_BELL,
                EQ(EQ_HEAVY_STAFF), EQ(EQ_TIDECLOTH_ROBE), HINT_SCROLL, END } },
};
#define SHOP_COUNT (sizeof(shops) / sizeof(shops[0]))

// How many of each good a shop has each day (playtest, 2026-10-21: "items should be limited per day, especially
// mana and HP recovery"). Equipment: one.
static const u8 daily_stock[ITEM_COUNT] = {
    [SPIRIT_LIGHT] = 1, [FRESH_BREAD] = 3, [HONEY_BREAD] = 2, [HEALING_POTION] = 2, [DRAGONS_POTION] = 1,
    [DEW_DROP] = 3, [MINT_LEAVES] = 2, [HEROES_DRINK] = 1,
    [SILENT_FLUTE] = 1, [CELINES_BELL] = 1, [REPLICA] = 2, [GIANTS_SHOES] = 1, [SILVER_AMULET] = 1,
    [GOLDEN_AMULET] = 1,
    [WHITE_WINGS] = 5, [YELLOW_WINGS] = 5, [BLUE_WINGS] = 5, [GREEN_WINGS] = 5, [RED_WINGS] = 5, [BLACK_WINGS] = 5,
};

static int is_equipment(u8 good) {
    return good >= EQ_BASE && good != END;
}

static const char* good_name(u8 good) {
    return good == HINT_SCROLL ? hint_scroll.name : is_equipment(good) ? eo_equipment_info(good - EQ_BASE)->name :
           items[good].name;
}

static u32 good_price(u8 good) {
    return good == HINT_SCROLL ? hint_scroll.price : is_equipment(good) ? eo_equipment_info(good - EQ_BASE)->price :
           items[good].price;
}

static const char* good_effect(u8 good) {
    return good == HINT_SCROLL ? hint_scroll.effect :
           is_equipment(good) ? eo_equipment_info(good - EQ_BASE)->effect : items[good].effect;
}

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

// Each shopkeeper's own voice (playtest, 2026-10-20: "make the dialogue more natural"): two greetings that
// alternate by day, and a line for when the shop is shut. Within 26 characters a line, two lines.
typedef struct {
    const char* greetings[2];
    const char* closed;
} Shopkeeper;

static const Shopkeeper keepers[] = {
    { { "Welcome! Small town, but\nI keep good stock.",                       // Ingram, Melrode
        "Ah, Brian! Off on another\nadventure? Stock up first." },
      "Brian? It's the middle of\nthe night. Come back later." },
    { { "Welcome to Dondoran! Have\na look around.",                         // Thom, Dondoran
        "Back from the Flats? You\nlook like you need bread." },
      "Shop's shut. Dondoran\nsleeps early, friend." },
    { { "Welcome to Larapool. Take\nyour time looking.",                     // Jiryo, Larapool
        "Ah, a customer! Not many\ncome by the lake lately." },
      "We're closed. Come back\nwhen the sun's up." },
    { { "Oh, hello! Everything here\nis good for the road.",                 // Anette, Normoon
        "Welcome! I've got a few\nnew things in today." },
      "Sorry, dear, I've closed\nup for the night." },
    { { "Welcome. You'll want good\nsupplies past this town.",               // Ring, Limelin
        "Back again? Have a look,\nI've got what you need." },
      "Closed. Try me again in\nthe morning." },
    { { "Hm. A customer. Look,\nbut don't drop anything.",                   // Bronze, Greenoch
        "Greenoch doesn't see many\nvisitors. Buying?" },
      "It's late. Go to bed and\ncome back tomorrow." },
    { { "Welcome to Brannoch. Have\na look around.",                         // Tom, Brannoch
        "The road ahead is rough.\nBest stock up now." },
      "We're closed, traveler.\nCome back at dawn." },
};
_Static_assert(sizeof(keepers) / sizeof(keepers[0]) == SHOP_COUNT, "one shopkeeper per shop");

static int shop_pending = -1;  // shop to open once the greeting closes
static int shop_open = -1;     // shop whose window is open

// npc_talk_begin just loaded the shopkeeper's text: greet instead.
RECOMP_HOOK_RETURN("npc_talk_begin") void eo_shop_on_greeting(void) {
    int shop = talking_shop();
    if (shop < 0) {
        return;
    }
    const Shopkeeper* keeper = &keepers[shop];
    if (eo_is_night()) {
        // Old-school shop hours: closed at night, open again at dawn.
        eo_text_encode_full(gMessageBuffer, keeper->closed);
        return;
    }
    eo_text_encode_full(gMessageBuffer, keeper->greetings[gCurrentDay % 2]);
    shop_pending = shop;
}

// ---- The shop window ----

// Small font layout: pointer, name (16), price (5) and a coin, the shop's stock left today ("x 2", or SOLD); the
// bottom line holds up to 31 characters.
#define WIN_W (31 * SMALL_W + 2 * SMALL_PAD)
#define WIN_X ((320 - WIN_W) / 2)
#define WIN_Y 24
#define ROW_H SMALL_LINE
#define TITLE_Y (WIN_Y + SMALL_PAD)
#define LIST_Y (TITLE_Y + SMALL_LINE + 3)
#define NAME_X (WIN_X + SMALL_PAD + 2 * SMALL_W)
#define PRICE_X (NAME_X + 17 * SMALL_W)
#define COIN_X (PRICE_X + 5 * SMALL_W - 1)   // a coin after each price
#define OWNED_X (COIN_X + ICON_W + 1)
#define BTN_A 0x8000
#define BTN_B 0x4000
#define BTN_DUP 0x0800
#define BTN_DDOWN 0x0400
#define STICK_THRESHOLD 40
#define MESSAGE_FRAMES 60

static int cursor = 0;
static int closing = 0;   // frames left of the goodbye before the window closes
static u16 menu_pressed = 0;
static int stick_held = 0;
static int message_timer = 0;
static const char* message = "";

int eo_shop_is_open(void) {
    return shop_open >= 0;
}

// Today's counts for the open shop, kept in the save so a sold-out shelf stays sold out after saving. A new day
// restocks.
static u8* stock_left(void) {
    return gEo.shop_left[shop_open];
}

static void restock_if_new_day(int shop) {
    if (gEo.shop_day[shop] == (u16)(gCurrentDay + 1)) {
        return;
    }
    gEo.shop_day[shop] = (u16)(gCurrentDay + 1);
    for (int i = 0; i < stock_count && i < 7; i++) {
        u8 good = stock[i];
        gEo.shop_left[shop][i] = is_equipment(good) || good == HINT_SCROLL ? 1 : daily_stock[good];
    }
}

static void open_shop(int shop) {
    build_stock(shop);
    restock_if_new_day(shop);
    shop_open = shop;
    cursor = 0;
    message_timer = 0;
    menu_pressed = 0;
    closing = 0;
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

static int message_palette = TEXT_WARN;
static char message_text[40];
static u32 sales = 0;     // picks the thank-you line

// The shopkeeper's lines inside the window (the small font: uppercase, no commas or !).
static const char* const thanks_lines[] = { "THANK YOU.", "A FINE CHOICE.", "ANYTHING ELSE?", "MUCH OBLIGED." };
static const char* const goodbye_lines[] = { "COME AGAIN SOON.", "SAFE TRAVELS." };
#define GOODBYE_FRAMES 40

static void say_message(const char* text, int palette) {
    message_text[0] = '\0';
    eo_append(message_text, text);
    message = message_text;
    message_palette = palette;
    message_timer = MESSAGE_FRAMES;
}

static void handle_input(void) {
    u16 pressed = menu_pressed;
    menu_pressed = 0;
    if (closing > 0) {
        if (--closing == 0) {
            shop_open = -1;
        }
        return;
    }
    if (pressed & BTN_B) {
        eo_sfx(SFX_CANCEL);
        say_message(goodbye_lines[gCurrentDay % 2], TEXT_GOOD);
        closing = GOODBYE_FRAMES;
        return;
    }
    if (pressed & BTN_DUP) {
        cursor = (cursor + stock_count - 1) % stock_count;
        message_timer = 0;
    } else if (pressed & BTN_DDOWN) {
        cursor = (cursor + 1) % stock_count;
        message_timer = 0;
    } else if (pressed & BTN_A) {
        u8 good = stock[cursor];
        if (stock_left()[cursor] == 0) {
            eo_sfx(SFX_CANCEL);
            say_message("SOLD OUT FOR TODAY.", TEXT_WARN);
        } else if (is_equipment(good) && eo_equipment_owned(good - EQ_BASE)) {
            eo_sfx(SFX_CANCEL);
            say_message("YOU ALREADY HAVE ONE OF THOSE.", TEXT_WARN);
        } else if (GOLD < good_price(good)) {
            eo_sfx(SFX_CANCEL);
            char line[40];
            say_message(eo_text_fill(line, "YOU'RE %a GOLD SHORT.", 0, good_price(good) - GOLD, 0), TEXT_WARN);
        } else if (good == HINT_SCROLL) {
            char how[40];
            if (!eo_spirits_hint(how, (u32)gCurrentDay * 31u + sales)) {
                eo_sfx(SFX_CANCEL);
                say_message("NOTHING IN IT YOU DON'T KNOW.", TEXT_WARN);
                return;
            }
            GOLD -= good_price(good);
            stock_left()[cursor]--;
            eo_spirits_mark_dirty();
            eo_gold_resync();
            eo_sfx(SFX_CONFIRM);
            sales++;
            say_message(how, TEXT_GOOD);
        } else if (!is_equipment(good) && inventory_count_item(INVENTORY_EMPTY) == 0) {
            eo_sfx(SFX_CANCEL);
            say_message("YOUR BAG'S FULL.", TEXT_WARN);
        } else {
            GOLD -= good_price(good);
            stock_left()[cursor]--;
            if (is_equipment(good)) {
                eo_equipment_give(good - EQ_BASE);
            } else {
                inventory_add_item(good);
            }
            eo_gold_resync();  // the shop shows the gold itself
            eo_sfx(SFX_CONFIRM);
            say_message(thanks_lines[sales++ % (sizeof(thanks_lines) / sizeof(thanks_lines[0]))], TEXT_GOOD);
        }
    }
}

static void draw_shop(void) {
    s32 h = (LIST_Y - WIN_Y) + stock_count * ROW_H + 3 + SMALL_LINE + SMALL_PAD - 2;
    char line[32];
    eo_ui_window(WIN_X, WIN_Y, WIN_W, h);
    eo_ui_small(WIN_X + SMALL_PAD, TITLE_Y, TEXT_NORMAL, "SHOP");
    eo_format_u32(line, "", GOLD, "");
    s32 gold_x = WIN_X + WIN_W - SMALL_PAD - eo_strlen(line) * SMALL_W;
    eo_ui_small(gold_x, TITLE_Y, TEXT_NORMAL, line);
    for (int i = 0; i < stock_count; i++) {
        u8 good = stock[i];
        s32 y = LIST_Y + i * ROW_H;
        int palette = i == cursor ? TEXT_GOOD : TEXT_NORMAL;
        eo_ui_small(NAME_X, y, palette, good_name(good));
        eo_format_amount(line, "", good_price(good), 5, "");
        eo_ui_small(PRICE_X, y, palette, line);
        // How many the shop has left today.
        u8 left = stock_left()[i];
        if (is_equipment(good) && eo_equipment_owned(good - EQ_BASE)) {
            eo_ui_small(OWNED_X, y, TEXT_DIM, "OWNED");
        } else if (left == 0) {
            eo_ui_small(OWNED_X, y, TEXT_DIM, "SOLD");
        } else {
            eo_format_amount(line, "X", left, 3, "");
            eo_ui_small(OWNED_X, y, palette, line);
        }
    }
    s32 info_y = LIST_Y + stock_count * ROW_H + 3;
    if (message_timer > 0) {
        eo_ui_small(WIN_X + SMALL_PAD, info_y, message_palette, message);
        message_timer--;
    } else {
        eo_ui_small(WIN_X + SMALL_PAD, info_y, TEXT_NORMAL, good_effect(stock[cursor]));
    }
    eo_icons_begin();
    eo_icon_draw(ICON_COIN, gold_x - ICON_W, TITLE_Y + ICON_TEXT_DY);
    for (int i = 0; i < stock_count; i++) {
        eo_icon_draw(ICON_COIN, COIN_X, LIST_Y + i * ROW_H + ICON_TEXT_DY);
    }
    eo_icon_draw(ICON_POINTER, WIN_X + SMALL_PAD - 1, LIST_Y + cursor * ROW_H + ICON_TEXT_DY);
}

// The shop window draws over the field HUD; the greeting closes first, then the window opens.
static void eo_shop_draw_body(void);

// Drawn into the mod's own display list (ui.c).
RECOMP_HOOK_RETURN("hud_draw_main") void eo_shop_draw(void) {
    eo_ui_begin();
    eo_shop_draw_body();
    eo_ui_end();
}

static void eo_shop_draw_body(void) {
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
