#include "eo.h"

// Saves and loads the overhaul's own data together with each vanilla save.
// The game writes a slot as two pak_write calls (summary at 0x00, world block at 0x80, see research/saves);
// when the second one goes out, the overhaul block is written to the note's unused tail first. Loading reads
// the world block with one pak_read, so the overhaul block is read alongside it.

#define WORLD_OFFSET 0x80
#define WORLD_SIZE   0xE0

EoSaveBlock gEo;

static u16 checksum(const EoSaveBlock* block) {
    const u8* bytes = (const u8*)block;
    u16 sum = 0;
    for (u32 i = 8; i < sizeof(EoSaveBlock); i++) {
        sum = (u16)(sum * 31 + bytes[i]);
    }
    return sum;
}

void eo_reset(void) {
    u8* bytes = (u8*)&gEo;
    for (u32 i = 0; i < sizeof(EoSaveBlock); i++) {
        bytes[i] = 0;
    }
    gEo.magic = EO_SAVE_MAGIC;
    gEo.version = EO_SAVE_VERSION;
}

u32 eo_now(void) {
    return (u32)gCurrentDay * DAY_LENGTH + (u32)gCurrentTime;
}

RECOMP_HOOK("pak_write") void eo_save_on_pak_write(s32 pak, s32 file, s32 offset, s32 size, void* buf) {
    if (offset != WORLD_OFFSET || size != WORLD_SIZE) {
        return;
    }
    gEo.magic = EO_SAVE_MAGIC;
    gEo.version = EO_SAVE_VERSION;
    gEo.checksum = checksum(&gEo);
    s32 err = pak_write(pak, file, EO_SAVE_OFFSET, sizeof(EoSaveBlock), &gEo);
    if (err != 0) {
        recomp_printf("Eltale Odyssey: writing the save block failed (%d)\n", err);
    }
}

RECOMP_HOOK("pak_read") void eo_save_on_pak_read(s32 pak, s32 file, s32 offset, s32 size, void* buf) {
    if (offset != WORLD_OFFSET || size != WORLD_SIZE) {
        return;
    }
    static EoSaveBlock loaded;
    s32 err = pak_read(pak, file, EO_SAVE_OFFSET, sizeof(EoSaveBlock), &loaded);
    if (err == 0 && loaded.magic == EO_SAVE_MAGIC && loaded.version == EO_SAVE_VERSION &&
        loaded.checksum == checksum(&loaded)) {
        gEo = loaded;
    } else {
        // A vanilla save (zeros), a damaged block or a read error: start fresh.
        if (err == 0 && loaded.magic == EO_SAVE_MAGIC) {
            recomp_printf("Eltale Odyssey: the save block failed its check; starting a new one\n");
        }
        eo_reset();
    }
}

// The loaded save's values (gold included) are copied into place here; don't announce them as new gold.
RECOMP_HOOK_RETURN("save_apply_slot_data") void eo_save_on_apply(void) {
    eo_gold_resync();
}

RECOMP_HOOK_RETURN("new_game_init") void eo_save_on_new_game(void) {
    eo_reset();
    eo_gold_resync();
}
