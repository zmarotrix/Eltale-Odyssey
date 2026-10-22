#include "eo.h"

// Teaching the new systems through the world (game design document, "Not designed yet": "teach through the world,
// no pop-up tutorials"; playtest, 2026-10-21: "somewhere very early"). Three Melrode townsfolk every player meets
// first keep their own line and then add a few pages on one system each: Edgar the spirits, tiers and talents,
// Frank shields, Break and Charge, Billy equipment. They talk this way every time, so it can be heard again.
//
// The conversation text is in gMessageBuffer once npc_talk_begin returns (as for the inn, inn.c); the pages are
// added before its end byte. Lines stay within 26 characters, four to a page. The dialogue font has no + or %.

#define NPC_RECORD_PTRS 0x8007BDB0   // live NPC slots (0x84 bytes); +0x80 points at the NPC's map record
#define NPC_SLOT_SIZE 0x84
#define RECORD_TEXT_ID 0x08
#define MESSAGE_BUFFER_SIZE 0x400
#define TEXT_CHOICE_BOX 0xF2
#define TEXT_PAGE_BREAK 0xF0
#define TEXT_END 0xFF

extern u8 gMessageBuffer[];

typedef struct {
    u16 text_id;   // the NPC's first text ID (NPC Locations sheet, Eltale Monsters Discord community)
    const char* pages;
} Lesson;

static const Lesson lessons[] = {
    { 0x1300,   // Edgar, Melrode
      "Have you heard? Every\nspirit you find grants a\nSpirit Point.\f"
      "Press Up on the Control\nPad to open your Spirits.\nSpend points there to\nattune higher tiers.\f"
      "Each element has talents\nto learn too. You can't\nhave them all, so choose\nyour path well.\f"
      "And new spells? Nobody\nknows their names until\nthey're cast. Hint\nScrolls help, they say." },
    { 0x17A0,   // Frank, Melrode
      "Monsters out there guard\nthemselves with shields.\f"
      "Strike with the element\nthey fear and a shield\ncracks. Crack them all\nand the beast reels!\f"
      "A reeling foe loses its\nturn and takes double\ndamage. Heal up, or hit\nit hard. Your choice.\f"
      "Each turn, your spirit\ngathers Charge.\f"
      "Before you cast, press\nUp or Down on the Control\nPad to pour Charge into\nthe spell.\f"
      "Fire burns hotter, Earth\nhits harder, Water heals\nmore and Wind fires more\nshots." },
    { 0x17E8,   // Billy, Melrode
      "Fine gear makes the\ntraveler. Press Down on\nthe Control Pad to see\nwhat you carry.\f"
      "Staff, robe, charm and\nsigil. You can only\nchange them while resting\nat an inn, mind you.\f"
      "Odd trinkets turn up in\nold chests, and bosses\noften carry treasures." },
};
#define LESSON_COUNT (sizeof(lessons) / sizeof(lessons[0]))

RECOMP_HOOK_RETURN("npc_talk_begin") void eo_teach_on_talk(void) {
    u8* record = *(u8**)(NPC_RECORD_PTRS + gTalkTargetNpc * NPC_SLOT_SIZE);
    if (record == 0) {
        return;
    }
    u16 text_id = *(u16*)(record + RECORD_TEXT_ID);
    const Lesson* lesson = 0;
    for (u32 i = 0; i < LESSON_COUNT; i++) {
        if (lessons[i].text_id == text_id) {
            lesson = &lessons[i];
        }
    }
    if (lesson == 0) {
        return;
    }
    // The end of the townsperson's own text; leave anything with a Yes/No box alone.
    int end = -1;
    for (int i = 0; i < MESSAGE_BUFFER_SIZE; i++) {
        if (gMessageBuffer[i] == TEXT_CHOICE_BOX) {
            return;
        }
        if (gMessageBuffer[i] == TEXT_END) {
            end = i;
            break;
        }
    }
    // Encoded text is at most two bytes a character (a mode switch and the letter), plus the page break and end.
    if (end < 0 || end + 2 * eo_strlen(lesson->pages) + 2 > MESSAGE_BUFFER_SIZE) {
        return;
    }
    u8* out = &gMessageBuffer[end];
    *out++ = TEXT_PAGE_BREAK;
    out = eo_text_encode(out, lesson->pages);
    *out = TEXT_END;
}
