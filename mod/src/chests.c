#include "eo.h"

// Chests that hold the overhaul's equipment or some gold instead of their vanilla item (playtest, 2026-10-21: "There's
// a chest in the house in the sheep farm ... it has Fresh Bread. We can replace that with equipment"; 2026-10-22: the
// Monastery's and Dondoran Castle's storeroom chests hold "a bit of gold and maybe an equip").
//
// Opening a chest runs func_8000496C: chest_item_message(item) writes "<item> ..." into gMessageBuffer, the message
// window opens, then inventory_add_item(item). For a chest listed here the message is replaced, and the item it added
// is taken back and the equipment given instead. Chest bits and contents: the modding notes' "Chest Locations" sheet.

#define CHEST_FLAGS ((u8*)0x800869D8)     // gChestFlags: bit n = byte n / 8, bit n % 8
#define CHEST_COUNT (*(u32*)0x800869A0)   // chests loaded in this room
#define CHEST_OBJECTS 0x800862E0
#define CHEST_SIZE 0x6C
#define CHEST_BIT_ID 0x62
#define CHEST_ITEM 0x50
#define INVENTORY ((u8*)0x8008CF78)
#define INVENTORY_SIZE 0x96
#define INVENTORY_EMPTY 0xFF

#define GOLD_ONLY 0xFF   // a chest with gold in place of equipment

typedef struct {
    u8 chest_bit;    // the chest's bit in gChestFlags
    u8 vanilla_item; // what the game puts in it
    u8 piece;        // the equipment it holds instead, or GOLD_ONLY
    u16 gold;        // gold it holds instead (GOLD_ONLY chests)
    const char* found;
} EquipmentChest;

static const EquipmentChest chests[] = {
    // The Monastery and Dondoran Castle (2026-10-22): the first chest with each kind of item keeps it, so players
    // meet the items ("it's important for players to discover some items"); repeats hold a little gold instead
    // (an early fight pays about 30, Melrode's inn costs 40), and the treasure room holds a robe.
    // Kept: Monastery dining room (Honey Bread), courtyard (Dew Drop); Dondoran King's room #1 (Fresh Bread),
    // Flora's room #3 (Mint Leaves), treasure room #1 (Healing Potion) and #3 (Heroes Drink).
    { 0x03, 0x05, GOLD_ONLY, 30, "Brian found %a gold!" },             // Monastery barn #1 (Dew Drop)
    { 0x04, 0x05, GOLD_ONLY, 35, "Brian found %a gold!" },             // Monastery barn #2 (Dew Drop)
    { 0x06, 0x01, GOLD_ONLY, 40, "Brian found %a gold!" },             // King's room #2 (Fresh Bread)
    { 0x07, 0x05, GOLD_ONLY, 50, "Brian found %a gold!" },             // King's room #3 (Dew Drop)
    { 0x08, 0x05, GOLD_ONLY, 30, "Brian found %a gold!" },             // Flora's room #1 (Dew Drop)
    { 0x09, 0x05, GOLD_ONLY, 30, "Brian found %a gold!" },             // Flora's room #2 (Dew Drop)
    { 0x0C, 0x0A, EQ_TRAVELERS_ROBE, 0, "Brian found a Traveler's Robe!" }, // treasure room #2 (Replica)
    { 0x0E, 0x01, EQ_WOOL_CLOAK, 0, "Brian found a Wool Cloak!" },          // Melrode, the shepherd's hut (Fresh Bread)
    { 0x55, 0x08, EQ_LEAD_MANTLE, 0, "Brian found a Lead Mantle!" },        // Glencoe Forest cabin (Silent Flute)
    { 0x32, 0x07, EQ_LEECH_PILLAR, 0, "Brian found the Leech Pillar!" },    // Blue Cave #3 (Heroes Drink)
    { 0x39, 0x08, EQ_GLASS_CHARM, 0, "Brian found a Glass Charm!" },        // Cull Hazard #5 (Silent Flute)
    { 0x2F, 0x04, EQ_STONE_MEMORY, 0, "Brian found the Stone Memory!" },    // the Isle of Skye ship (Dragon's Potion)
    { 0x1E, 0x0B, EQ_HUNTERS_CHARM, 0, "Brian found a Hunter's Charm!" },   // Dindom Dries hut (Giant's Shoes)
    { 0x18, 0x0C, EQ_SPELLSTRIKE, 0, "Brian found the Spellstrike!" },      // Limelin Castle treasury #1 (Silver Amulet)
    { 0x19, 0x09, EQ_PRISM_STAFF, 0, "Brian found a Prism Staff!" },        // Limelin Castle treasury #2 (Celine's Bell)
    { 0x25, 0x0D, EQ_EMBER_HEART, 0, "Brian found the Ember Heart!" },      // Shamwood, Lavaar's room (Golden Amulet)
    { 0x3F, 0x0C, EQ_MOUNTAIN_STAFF, 0, "Brian found a Mountain Staff!" },  // Baragoon Tunnel #6 (Silver Amulet)
    { 0x45, 0x04, EQ_PHOENIX_STAFF, 0, "Brian found a Phoenix Staff!" },    // Boil Hole #5 (Dragon's Potion)
    { 0x24, 0x09, EQ_KNIGHTS_MANTLE, 0, "Brian found a Knight's Mantle!" }, // Baragoon Moor inn #6 (Celine's Bell)
    { 0x49, 0x07, EQ_LEVIATHAN_STAFF, 0, "Brian found a Leviathan Staff!" },// Brannoch Castle cannon room #2 (Heroes Drink)
    { 0x4B, 0x0D, EQ_TITAN_STAFF, 0, "Brian found a Titan Staff!" },        // Brannoch Castle, Hi Dad Bye Dad #2 (Golden Amulet)
    { 0x4D, 0x09, EQ_SKYREED_STAFF, 0, "Brian found a Skyreed Staff!" },    // Leonardo to the rescue #2 (Celine's Bell)
    { 0x4F, 0x0C, EQ_DRAGON_ROBE, 0, "Brian found a Dragon Robe!" },        // Leonardo to the rescue #4 (Silver Amulet)
    // Odd finds (2026-10-22), in rough story order; each replaces a repeat of a common item.
    { 0x54, 0x0B, EQ_SHEPHERDS_CROOK, 0, "Brian found a Shepherd's Crook!" },     // Connor Forest hut (Giant's Shoes)
    { 0x57, 0x02, EQ_IRON_PLATE, 0, "Brian found an Iron Plate!" },              // Windward Forest cabin (Honey Bread)
    { 0x11, 0x01, EQ_HOURGLASS_CHARM, 0, "Brian found an Hourglass Charm!" },    // Normoon 1st mill #2 (Fresh Bread)
    { 0x0F, 0x0C, EQ_BLOODSTONE, 0, "Brian found a Bloodstone!" },               // Larapool well room (Silver Amulet)
    { 0x34, 0x06, EQ_EXECUTIONERS_CHARM, 0, "Brian found the Executioner's Charm!" }, // Blue Cave #5 (Mint Leaves)
    { 0x14, 0x02, EQ_SMITHS_HAMMER, 0, "Brian found a Smith's Hammer!" },        // Limelin Castle armory #1 (Honey Bread)
    { 0x16, 0x06, EQ_SCARRED_LEATHER, 0, "Brian found Scarred Leather!" },       // Limelin Castle armory #3 (Mint Leaves)
    { 0x37, 0x02, EQ_STORM_GLASS, 0, "Brian found the Storm Glass!" },           // Cull Hazard #3 (Honey Bread)
    { 0x26, 0x08, EQ_EMBER_LANTERN, 0, "Brian found an Ember Lantern!" },        // Shamwood #1 (Silent Flute)
    { 0x21, 0x0B, EQ_MISERS_PURSE, 0, "Brian found a Miser's Purse!" },          // Baragoon Moor inn #3 (Giant's Shoes)
    { 0x43, 0x0B, EQ_GLACIAL_PIN, 0, "Brian found a Glacial Pin!" },             // Boil Hole #3 (Giant's Shoes)
    { 0x47, 0x06, EQ_ECHO_STONE, 0, "Brian found an Echo Stone!" },              // Brannoch Castle hallway #2 (Mint Leaves)
};
#define CHEST_ENTRIES (sizeof(chests) / sizeof(chests[0]))

void msgwin_open(s32 position);
extern u8 gMessageBuffer[];

static const EquipmentChest* pending = 0;

static int chest_opened(u8 bit) {
    return (CHEST_FLAGS[bit / 8] >> (bit % 8)) & 1;
}

// The listed chest in this room holding `item`, if any.
static const EquipmentChest* chest_here(u8 item) {
    for (u32 c = 0; c < CHEST_COUNT && c < 8; c++) {
        u8* chest = (u8*)(CHEST_OBJECTS + c * CHEST_SIZE);
        for (u32 i = 0; i < CHEST_ENTRIES; i++) {
            if (*(u16*)(chest + CHEST_BIT_ID) == chests[i].chest_bit && item == chests[i].vanilla_item) {
                return &chests[i];
            }
        }
    }
    return 0;
}

RECOMP_HOOK("chest_item_message") void eo_chest_before_message(s32 item) {
    pending = chest_here((u8)item);
}

RECOMP_HOOK_RETURN("chest_item_message") void eo_chest_after_message(void) {
    if (pending != 0) {
        char line[40];
        eo_text_encode_full(gMessageBuffer, eo_text_fill(line, pending->found, 0, pending->gold, 0));
    }
}

RECOMP_HOOK_RETURN("inventory_add_item") void eo_chest_after_add(void) {
    if (pending == 0) {
        return;
    }
    // Take back the vanilla item it just added (the last one in the bag).
    for (int i = INVENTORY_SIZE - 1; i >= 0; i--) {
        if (INVENTORY[i] == pending->vanilla_item) {
            INVENTORY[i] = INVENTORY_EMPTY;
            break;
        }
    }
    if (pending->piece == GOLD_ONLY) {
        GOLD += pending->gold;   // the gold notice shows it
    } else {
        eo_equipment_give(pending->piece);
    }
    pending = 0;
}

// Saves that opened one of these chests before the overhaul (and got the vanilla item) get the piece once; gold
// chests already paid out their item, so they give nothing more.
void eo_chests_on_load(void) {
    for (u32 i = 0; i < CHEST_ENTRIES; i++) {
        if (chests[i].piece != GOLD_ONLY && chest_opened(chests[i].chest_bit) && !eo_equipment_owned(chests[i].piece)) {
            eo_equipment_give(chests[i].piece);
        }
    }
}
