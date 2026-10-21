#include "eo.h"

// Brian's staff, cape and collar take the colors of what he wears, and the gem on his chest his charm's (user requests,
// 2026-10-21). Research: research/models.
//
// - The cape is six bone display lists that set its color with SetPrimColor E90000FF (red) a few commands after the
//   combine FC127E03 FFFFF378. Rewriting that color word recolors the cape.
// - The staff is one bone whose display list loads two RGBA16 textures, the shaft (16 x 16, tile line 0x140) and the
//   head (16 x 32, tile line 0x4140). Its colors are texels, so the mod keeps the original pixels and writes recolored
//   ones: each texel's brightness times the staff's color. The starting staff keeps the original wood.
//
// Brian's model is found by those command patterns in the model area and found again whenever the words there
// change (a room change reloads it).

#define MODEL_AREA_START 0x80200000
#define MODEL_AREA_END 0x80280000
#define CAPE_PIECES 6
#define CAPE_COMBINE_W0 0xFC127E03
#define CAPE_COMBINE_W1 0xFFFFF378
#define CAPE_RED 0xE90000FF
#define GFX_SET_PRIM 0xFA000000
#define GFX_SET_TIMG_RGBA16_W16 0xFD10000F   // set texture image: RGBA16, 16 texels wide
#define GFX_STAFF_SHAFT_TILE 0x07010140
#define GFX_STAFF_HEAD_TILE 0x07054140
#define SHAFT_TEXELS (16 * 16)
#define HEAD_TEXELS (16 * 32)
#define GFX_SET_TIMG_RGBA16_W24 0xFD100017   // 24 texels wide
#define GFX_COLLAR_TILE 0x07050150           // the red collar, 24 x 16
#define GFX_GEM_TILE 0x07010040              // the chest gem, 16 x 16
#define COLLAR_TEXELS (24 * 16)
#define GEM_TEXELS (16 * 16)

typedef struct { u8 r, g, b; } Color;

// Robe colors (cape), by piece; anything else keeps the red.
static u32 cape_color_for(int robe) {
    switch (robe) {
        case EQ_TRAVELERS_ROBE: return 0x8A5A2AFF;   // brown
        case EQ_TIDECLOTH_ROBE: return 0x1878C0FF;   // deep blue
        case EQ_WOOL_CLOAK: return 0xE8DCC0FF;       // undyed wool
    }
    return CAPE_RED;
}

// Staff colors; the starting staff (none worn) keeps its wood.
static const Color* staff_color_for(int staff) {
    static const Color ember = { 255, 110, 60 };
    static const Color heavy = { 150, 150, 165 };
    static const Color stone = { 225, 170, 100 };
    static const Color tide = { 90, 150, 255 };
    static const Color gale = { 110, 230, 140 };
    switch (staff) {
        case EQ_EMBER_STAFF: return &ember;
        case EQ_HEAVY_STAFF: return &heavy;
        case EQ_STONE_STAFF: return &stone;
        case EQ_TIDE_STAFF: return &tide;
        case EQ_GALE_STAFF: return &gale;
    }
    return 0;
}

// Robe colors for the red collar (a texture, recolored like the staff); no robe keeps the red.
static const Color* collar_color_for(int robe) {
    static const Color travelers = { 150, 95, 45 };
    static const Color tidecloth = { 30, 120, 200 };
    static const Color wool = { 235, 225, 200 };
    switch (robe) {
        case EQ_TRAVELERS_ROBE: return &travelers;
        case EQ_TIDECLOTH_ROBE: return &tidecloth;
        case EQ_WOOL_CLOAK: return &wool;
    }
    return 0;
}

// Charm colors for the gem on Brian's chest; no charm keeps the game's.
static const Color* gem_color_for(int charm) {
    static const Color pilgrim = { 120, 255, 150 };
    static const Color gambler = { 255, 210, 60 };
    static const Color mirror = { 190, 130, 255 };
    switch (charm) {
        case EQ_PILGRIMS_SOUL: return &pilgrim;
        case EQ_GAMBLERS_COIN: return &gambler;
        case EQ_MIRROR_SHARD: return &mirror;
    }
    return 0;
}

// A texture of Brian's the mod recolors, with its original pixels.
typedef struct {
    u16* tex;
    u16* original;
    int texels;
    u32 written;   // checksum of what the mod last wrote (0 = the original is there)
} Part;

static u16 shaft_original[SHAFT_TEXELS];
static u16 head_original[HEAD_TEXELS];
static u16 collar_original[COLLAR_TEXELS];
static u16 gem_original[GEM_TEXELS];
enum { PART_SHAFT, PART_HEAD, PART_COLLAR, PART_GEM, PART_COUNT };
static Part parts[PART_COUNT] = {
    { 0, shaft_original, SHAFT_TEXELS, 0 },
    { 0, head_original, HEAD_TEXELS, 0 },
    { 0, collar_original, COLLAR_TEXELS, 0 },
    { 0, gem_original, GEM_TEXELS, 0 },
};

static u32* cape_prim[CAPE_PIECES];   // the color word of each cape piece's SetPrimColor
static int applied_robe = -2;
static int applied_staff = -2;
static int applied_charm = -2;

static u32 checksum(const u16* t, int n) {
    u32 sum = 1;
    for (int i = 0; i < n; i++) {
        sum = sum * 31 + t[i];
    }
    return sum;
}

static int worn(int slot_first, int slot_last) {
    for (int p = slot_first; p <= slot_last; p++) {
        if (eo_equipment_worn(p)) {
            return p;
        }
    }
    return -1;
}

static int is_texture_load(const u32* w, u32 timg, u32 tile) {
    return w[0] == timg && w[2] >> 24 == 0xF5 && w[3] == tile;
}

// Finds the cape's color words and the textures. 1 if all were found.
static int find_model(void) {
    int capes = 0;
    for (int i = 0; i < PART_COUNT; i++) {
        parts[i].tex = 0;
    }
    for (u32* w = (u32*)MODEL_AREA_START; w < (u32*)MODEL_AREA_END - 4; w += 2) {
        if (capes < CAPE_PIECES && w[0] == CAPE_COMBINE_W0 && w[1] == CAPE_COMBINE_W1) {
            // The color comes a few commands later (after the texture and vertex loads).
            for (u32* c = w + 2; c < w + 2 + 2 * 6; c += 2) {
                if (c[0] == GFX_SET_PRIM && (c[1] == CAPE_RED || c[1] == cape_color_for(applied_robe))) {
                    cape_prim[capes++] = &c[1];
                    break;
                }
            }
        } else if (parts[PART_HEAD].tex == 0 && is_texture_load(w, GFX_SET_TIMG_RGBA16_W16, GFX_STAFF_HEAD_TILE)) {
            // The staff's head; its shaft is loaded a few commands before it, in the same display list (other parts
            // of Brian use the shaft's tile layout too).
            for (u32* c = w - 2; c > w - 2 * 24 && c > (u32*)MODEL_AREA_START; c -= 2) {
                if (is_texture_load(c, GFX_SET_TIMG_RGBA16_W16, GFX_STAFF_SHAFT_TILE)) {
                    parts[PART_SHAFT].tex = (u16*)c[1];
                    parts[PART_HEAD].tex = (u16*)w[1];
                    break;
                }
            }
        } else if (parts[PART_COLLAR].tex == 0 && is_texture_load(w, GFX_SET_TIMG_RGBA16_W24, GFX_COLLAR_TILE)) {
            // The red collar; the chest gem is loaded just before it, in the same display list.
            for (u32* c = w - 2; c > w - 2 * 16 && c > (u32*)MODEL_AREA_START; c -= 2) {
                if (is_texture_load(c, GFX_SET_TIMG_RGBA16_W16, GFX_GEM_TILE)) {
                    parts[PART_GEM].tex = (u16*)c[1];
                    parts[PART_COLLAR].tex = (u16*)w[1];
                    break;
                }
            }
        }
        int all = capes == CAPE_PIECES;
        for (int i = 0; i < PART_COUNT; i++) {
            all = all && parts[i].tex != 0;
        }
        if (all) {
            return 1;
        }
    }
    return 0;
}

// Still where we found it? (A room change can reload the model.)
static int model_intact(void) {
    for (int i = 0; i < PART_COUNT; i++) {
        if (parts[i].tex == 0) {
            return 0;
        }
        // Reloaded in place: the pixels aren't what the mod last wrote.
        if (parts[i].written != 0 && checksum(parts[i].tex, parts[i].texels) != parts[i].written) {
            return 0;
        }
    }
    for (int i = 0; i < CAPE_PIECES; i++) {
        if (cape_prim[i] == 0 || cape_prim[i][-1] != GFX_SET_PRIM) {
            return 0;
        }
    }
    return applied_robe == -2 || *cape_prim[0] == cape_color_for(applied_robe);
}

// Each texel's brightness times the color.
static void recolor(Part* part, const Color* c) {
    if (c == 0) {
        for (int i = 0; i < part->texels; i++) {
            part->tex[i] = part->original[i];
        }
        part->written = 0;
        return;
    }
    for (int i = 0; i < part->texels; i++) {
        u16 px = part->original[i];
        int r = (px >> 11) & 31;
        int g = (px >> 6) & 31;
        int b = (px >> 1) & 31;
        int lum = (r * 3 + g * 6 + b) / 10;   // 0..31
        int nr = lum * c->r / 180;
        int ng = lum * c->g / 180;
        int nb = lum * c->b / 180;
        nr = nr > 31 ? 31 : nr;
        ng = ng > 31 ? 31 : ng;
        nb = nb > 31 ? 31 : nb;
        part->tex[i] = (u16)((nr << 11) | (ng << 6) | (nb << 1) | (px & 1));
    }
    part->written = checksum(part->tex, part->texels);
}

// Copies each texture's current pixels as its original, unless they are what the mod itself wrote.
static void remember_parts(void) {
    for (int p = 0; p < PART_COUNT; p++) {
        Part* part = &parts[p];
        if (part->written == 0 || checksum(part->tex, part->texels) != part->written) {
            for (int i = 0; i < part->texels; i++) {
                part->original[i] = part->tex[i];
            }
            part->written = 0;
        }
    }
}

RECOMP_HOOK_RETURN("frame_build") void eo_appearance_frame(void) {
    if (gGameMode != 1 || !gEo.spirits_ready) {
        return;
    }
    if (!model_intact()) {
        if (!find_model()) {
            for (int i = 0; i < PART_COUNT; i++) {
                parts[i].tex = 0;
            }
            return;
        }
        remember_parts();
        applied_robe = applied_staff = applied_charm = -2;
    }
    int robe = worn(EQ_TRAVELERS_ROBE, EQ_TIDECLOTH_ROBE);
    if (eo_equipment_worn(EQ_WOOL_CLOAK)) {
        robe = EQ_WOOL_CLOAK;
    }
    int staff = worn(EQ_EMBER_STAFF, EQ_GALE_STAFF);
    int charm = worn(EQ_PILGRIMS_SOUL, EQ_MIRROR_SHARD);
    if (robe != applied_robe) {
        u32 color = cape_color_for(robe);
        for (int i = 0; i < CAPE_PIECES; i++) {
            *cape_prim[i] = color;
        }
        recolor(&parts[PART_COLLAR], collar_color_for(robe));
        applied_robe = robe;
    }
    if (staff != applied_staff) {
        const Color* c = staff_color_for(staff);
        recolor(&parts[PART_SHAFT], c);
        recolor(&parts[PART_HEAD], c);
        applied_staff = staff;
    }
    if (charm != applied_charm) {
        recolor(&parts[PART_GEM], gem_color_for(charm));
        applied_charm = charm;
    }
}
