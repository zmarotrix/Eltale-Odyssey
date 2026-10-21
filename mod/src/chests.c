#include "eo.h"

// Chests that hold the overhaul's equipment instead of their vanilla item (playtest, 2026-10-21: "There's a chest in
// the house in the sheep farm ... it has Fresh Bread. We can replace that with equipment").
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

typedef struct {
    u8 chest_bit;    // the chest's bit in gChestFlags
    u8 vanilla_item; // what the game puts in it
    u8 piece;        // the equipment it holds instead
    const char* found;
} EquipmentChest;

static const EquipmentChest chests[] = {
    { 0x0E, 0x01, EQ_WOOL_CLOAK, "Brian found a Wool Cloak!" },   // Melrode, the shepherd's hut (was Fresh Bread)
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
        eo_text_encode_full(gMessageBuffer, pending->found);
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
    eo_equipment_give(pending->piece);
    pending = 0;
}

// Saves that opened one of these chests before the overhaul (and got the vanilla item) get the piece once.
void eo_chests_on_load(void) {
    for (u32 i = 0; i < CHEST_ENTRIES; i++) {
        if (chest_opened(chests[i].chest_bit) && !eo_equipment_owned(chests[i].piece)) {
            eo_equipment_give(chests[i].piece);
        }
    }
}
