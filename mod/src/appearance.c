#include "eo.h"

// Brian's staff and cape take the colors of what he wears (user request, 2026-10-21). Research: research/models.
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

typedef struct { u8 r, g, b; } Color;

// Robe colors (cape), by piece; anything else keeps the red.
static u32 cape_color_for(int robe) {
    switch (robe) {
        case EQ_TRAVELERS_ROBE: return 0x8A5A2AFF;   // brown
        case EQ_TIDECLOTH_ROBE: return 0x1878C0FF;   // deep blue
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

static u32* cape_prim[CAPE_PIECES];   // the color word of each cape piece's SetPrimColor
static u16* shaft = 0;                // the staff's textures in the model
static u16* head = 0;
static u16 shaft_original[SHAFT_TEXELS];
static u16 head_original[HEAD_TEXELS];
static u32 shaft_written = 0;         // checksums of what the mod last wrote (0 = the original is there)
static u32 head_written = 0;
static int applied_robe = -2;
static int applied_staff = -2;

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

// Finds the cape's color words and the staff's textures. 1 if all were found.
static int find_model(void) {
    int capes = 0;
    shaft = head = 0;
    for (u32* w = (u32*)MODEL_AREA_START; w < (u32*)MODEL_AREA_END - 4; w += 2) {
        if (capes < CAPE_PIECES && w[0] == CAPE_COMBINE_W0 && w[1] == CAPE_COMBINE_W1) {
            // The color comes a few commands later (after the texture and vertex loads).
            for (u32* c = w + 2; c < w + 2 + 2 * 6; c += 2) {
                if (c[0] == GFX_SET_PRIM && (c[1] == CAPE_RED || c[1] == cape_color_for(applied_robe))) {
                    cape_prim[capes++] = &c[1];
                    break;
                }
            }
        } else if (head == 0 && w[0] == GFX_SET_TIMG_RGBA16_W16 && w[2] >> 24 == 0xF5 && w[3] == GFX_STAFF_HEAD_TILE) {
            // The staff's head; its shaft is loaded a few commands before it, in the same display list (other parts
            // of Brian use the shaft's tile layout too).
            for (u32* c = w - 2; c > w - 2 * 24 && c > (u32*)MODEL_AREA_START; c -= 2) {
                if (c[0] == GFX_SET_TIMG_RGBA16_W16 && c[2] >> 24 == 0xF5 && c[3] == GFX_STAFF_SHAFT_TILE) {
                    shaft = (u16*)c[1];
                    head = (u16*)w[1];
                    break;
                }
            }
        }
        if (capes == CAPE_PIECES && shaft != 0 && head != 0) {
            return 1;
        }
    }
    return 0;
}

// Still where we found it? (A room change can reload the model.)
static int model_intact(void) {
    if (shaft == 0 || head == 0) {
        return 0;
    }
    for (int i = 0; i < CAPE_PIECES; i++) {
        if (cape_prim[i] == 0 || cape_prim[i][-1] != GFX_SET_PRIM) {
            return 0;
        }
    }
    // Reloaded in place: the cape's color or the staff's pixels aren't what the mod last set.
    if (applied_robe != -2 && *cape_prim[0] != cape_color_for(applied_robe)) {
        return 0;
    }
    if (shaft_written != 0 && checksum(shaft, SHAFT_TEXELS) != shaft_written) {
        return 0;
    }
    return 1;
}

static void recolor(u16* tex, const u16* original, int n, const Color* c) {
    for (int i = 0; i < n; i++) {
        u16 px = original[i];
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
        tex[i] = (u16)((nr << 11) | (ng << 6) | (nb << 1) | (px & 1));
    }
}

// Copies the staff's current pixels as the originals, unless they are what the mod itself wrote.
static void remember_staff(void) {
    if (checksum(shaft, SHAFT_TEXELS) != shaft_written || shaft_written == 0) {
        for (int i = 0; i < SHAFT_TEXELS; i++) {
            shaft_original[i] = shaft[i];
        }
        shaft_written = 0;
    }
    if (checksum(head, HEAD_TEXELS) != head_written || head_written == 0) {
        for (int i = 0; i < HEAD_TEXELS; i++) {
            head_original[i] = head[i];
        }
        head_written = 0;
    }
}

RECOMP_HOOK_RETURN("frame_build") void eo_appearance_frame(void) {
    if (gGameMode != 1 || !gEo.spirits_ready) {
        return;
    }
    if (!model_intact()) {
        if (!find_model()) {
            shaft = head = 0;
            return;
        }
        remember_staff();
        applied_robe = applied_staff = -2;
    }
    int robe = worn(EQ_TRAVELERS_ROBE, EQ_TIDECLOTH_ROBE);
    int staff = worn(EQ_EMBER_STAFF, EQ_GALE_STAFF);
    if (robe != applied_robe) {
        u32 color = cape_color_for(robe);
        for (int i = 0; i < CAPE_PIECES; i++) {
            *cape_prim[i] = color;
        }
        applied_robe = robe;
    }
    if (staff != applied_staff) {
        const Color* c = staff_color_for(staff);
        if (c != 0) {
            recolor(shaft, shaft_original, SHAFT_TEXELS, c);
            recolor(head, head_original, HEAD_TEXELS, c);
            shaft_written = checksum(shaft, SHAFT_TEXELS);
            head_written = checksum(head, HEAD_TEXELS);
        } else {
            for (int i = 0; i < SHAFT_TEXELS; i++) {
                shaft[i] = shaft_original[i];
            }
            for (int i = 0; i < HEAD_TEXELS; i++) {
                head[i] = head_original[i];
            }
            shaft_written = head_written = 0;
        }
        applied_staff = staff;
    }
}
