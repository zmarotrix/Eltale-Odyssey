#include "eo.h"

// Small icons drawn by the game's own texture routine: an 8-bit indexed atlas built at boot, with our own
// palette, drawn through hud_draw_tex_region (the routine the HUD uses for its pieces; research/menus).
// Each icon is a 12 x 12 cell in a 16-pixel-wide atlas, one cell per row of 12 lines.

#define ATLAS_W 16
#define CELL 12

// Palette indices.
enum {
    C_CLEAR, C_OUTLINE, C_SHINE,
    C_EMPTY, C_PURPLE, C_GOLD, C_RED, C_AMBER, C_BLUE, C_GREEN,
    C_FLAME_OUT, C_FLAME_IN, C_CRACK, C_POISON, C_POISON_DARK,
    C_GOLD_DARK, C_WOOD, C_STEEL,
    C_COUNT
};

// The renderer reads textures from the console's 8 MB of RAM, and the mod itself is loaded above it, so the
// atlas and palette live in the top 64 KB of the Expansion Pak's 4 MB, which the game never touches (checked
// zero through play).
#define ICON_RAM 0x807F0000
#define atlas ((u8*)ICON_RAM)
#define palette ((u16*)(ICON_RAM + 0x1000))
#define ATLAS_SIZE (ATLAS_W * CELL * ICON_COUNT)
_Static_assert(ATLAS_SIZE <= 0x1000, "the atlas must fit below the palette");
static u32 descriptor[4] __attribute__((aligned(8)));  // pointer, unused, width, height (like the game's)

static u16 rgba5551(int r, int g, int b) {
    return (u16)(((r >> 3) << 11) | ((g >> 3) << 6) | ((b >> 3) << 1) | 1);
}

static void put(int icon, int x, int y, u8 c) {
    if (x >= 0 && x < CELL && y >= 0 && y < CELL) {
        atlas[(icon * CELL + y) * ATLAS_W + x] = c;
    }
}

static int iabs(int v) {
    return v < 0 ? -v : v;
}

// A diamond like the HUD's element gems: outline, fill, a little shine at the top left.
static void make_diamond(int icon, u8 fill) {
    for (int y = 0; y < CELL; y++) {
        for (int x = 0; x < CELL; x++) {
            // Distance from the centre (5.5, 5.5) in half-pixels.
            int d = iabs(2 * x - 11) + iabs(2 * y - 11);
            if (d <= 9) {
                put(icon, x, y, fill);
            } else if (d <= 12) {
                put(icon, x, y, C_OUTLINE);
            }
        }
    }
    put(icon, 4, 4, C_SHINE);
    put(icon, 5, 3, C_SHINE);
    put(icon, 4, 5, C_SHINE);
}

static void make_flame(int icon) {
    static const char* const art[CELL] = {
        "     #      ",
        "    ##      ",
        "    ###  #  ",
        "   ####  ## ",
        "  #####.### ",
        "  ###..#### ",
        " ###...#### ",
        " ##.....### ",
        " ##.....### ",
        "  ##...###  ",
        "   ######   ",
        "            ",
    };
    for (int y = 0; y < CELL; y++) {
        for (int x = 0; x < CELL; x++) {
            char ch = art[y][x];
            if (ch == '#') put(icon, x, y, C_FLAME_OUT);
            if (ch == '.') put(icon, x, y, C_FLAME_IN);
        }
    }
}

// A burst for Broken: a white star with a dark edge.
static void make_broken(int icon) {
    static const char* const art[CELL] = {
        "     ##     ",
        " #   ##   # ",
        "  # #..# #  ",
        "   #....#   ",
        "  #......#  ",
        "##...##...##",
        "##...##...##",
        "  #......#  ",
        "   #....#   ",
        "  # #..# #  ",
        " #   ##   # ",
        "     ##     ",
    };
    for (int y = 0; y < CELL; y++) {
        for (int x = 0; x < CELL; x++) {
            char ch = art[y][x];
            if (ch == '#') put(icon, x, y, C_OUTLINE);
            if (ch == '.') put(icon, x, y, C_CRACK);
        }
    }
}

// A poison drop.
static void make_poison(int icon) {
    static const char* const art[CELL] = {
        "     ##     ",
        "     ##     ",
        "    #..#    ",
        "    #..#    ",
        "   #....#   ",
        "  #......#  ",
        "  #......#  ",
        " #...,,...# ",
        " #...,,...# ",
        "  #......#  ",
        "   ######   ",
        "            ",
    };
    for (int y = 0; y < CELL; y++) {
        for (int x = 0; x < CELL; x++) {
            char ch = art[y][x];
            if (ch == '#') put(icon, x, y, C_POISON_DARK);
            if (ch == '.') put(icon, x, y, C_POISON);
            if (ch == ',') put(icon, x, y, C_SHINE);
        }
    }
}

// A small pointer for menus (the game's hand is too big for 10-pixel rows).
static void make_pointer(int icon) {
    static const char* const art[CELL] = {
        "            ",
        "            ",
        " ##         ",
        " #.##       ",
        " #...##     ",
        " #.....#    ",
        " #...##     ",
        " #.##       ",
        " ##         ",
        "            ",
        "            ",
        "            ",
    };
    for (int y = 0; y < CELL; y++) {
        for (int x = 0; x < CELL; x++) {
            char ch = art[y][x];
            if (ch == '#') put(icon, x, y, C_OUTLINE);
            if (ch == '.') put(icon, x, y, C_GOLD);
        }
    }
}

// Paints pixel art into a cell: each character of `keys` stands for the color at the same place in `colors`;
// anything else is clear. Art is drawn from the cell's top left plus (dx, dy).
static void paint(int icon, const char* const* art, int rows, int dx, int dy, const char* keys, const u8* colors) {
    for (int y = 0; y < rows; y++) {
        for (int x = 0; art[y][x] != '\0'; x++) {
            for (int k = 0; keys[k] != '\0'; k++) {
                if (art[y][x] == keys[k]) {
                    put(icon, x + dx, y + dy, colors[k]);
                }
            }
        }
    }
}

// The menu icons are 8-10 pixels, so they sit on a 10-pixel text row (drawn 2 pixels above the text).

static void make_coin(int icon) {
    static const char* const art[] = {
        "  ####  ",
        " #yyyy# ",
        "#y*yyyd#",
        "#yyyyyd#",
        "#yyyyyd#",
        "#yyyydd#",
        " #dddd# ",
        "  ####  ",
    };
    static const u8 colors[] = { C_GOLD_DARK, C_GOLD, C_SHINE, C_AMBER };
    paint(icon, art, 8, 2, 2, "#y*d", colors);
}

// A small diamond: tier pips and Spirit Points.
static void make_pip(int icon, u8 fill) {
    static const char* const art[] = {
        "   #   ",
        "  #.#  ",
        " #...# ",
        "#.....#",
        " #...# ",
        "  #.#  ",
        "   #   ",
    };
    const u8 colors[] = { C_OUTLINE, fill };
    paint(icon, art, 7, 2, 3, "#.", colors);
}

static void make_lock(int icon) {
    static const char* const art[] = {
        "  ####  ",
        " #    # ",
        " #    # ",
        "########",
        "#ssssss#",
        "#sss#ss#",
        "#sss#ss#",
        "#ssssss#",
        "########",
    };
    static const u8 colors[] = { C_OUTLINE, C_STEEL };
    paint(icon, art, 9, 2, 2, "#s", colors);
}

static void make_staff(int icon) {
    static const char* const art[] = {
        "       ## ",
        "      #oo#",
        "      #o*#",
        "     #w## ",
        "    #w#   ",
        "   #w#    ",
        "  #w#     ",
        " #w#      ",
        "#w#       ",
        "##        ",
    };
    static const u8 colors[] = { C_OUTLINE, C_BLUE, C_SHINE, C_WOOD };
    paint(icon, art, 10, 1, 1, "#o*w", colors);
}

static void make_robe(int icon) {
    static const char* const art[] = {
        "  ##  ##  ",
        " #bb##bb# ",
        "#bbbbbbbb#",
        "#b#bbbb#b#",
        "## #bb# ##",
        "   #bb#   ",
        "  #bbbb#  ",
        "  #bbbb#  ",
        " #bbbbbb# ",
        " ######## ",
    };
    static const u8 colors[] = { C_OUTLINE, C_RED };
    paint(icon, art, 10, 1, 1, "#b", colors);
}

static void make_charm(int icon) {
    static const char* const art[] = {
        "#      #",
        " #    # ",
        "  #  #  ",
        "   ##   ",
        "  #gg#  ",
        " #g*gg# ",
        " #gggg# ",
        "  #gg#  ",
        "   ##   ",
    };
    static const u8 colors[] = { C_GOLD_DARK, C_GREEN, C_SHINE };
    paint(icon, art, 9, 2, 1, "#g*", colors);
}

static void make_sigil(int icon) {
    static const char* const art[] = {
        "  ####  ",
        " #pppp# ",
        "#pp..pp#",
        "#p.pp.p#",
        "#p.pp.p#",
        "#pp..pp#",
        " #pppp# ",
        "  ####  ",
    };
    static const u8 colors[] = { C_OUTLINE, C_PURPLE, C_GOLD };
    paint(icon, art, 8, 2, 2, "#p.", colors);
}

void eo_icons_init(void) {
    for (u32 i = 0; i < ATLAS_SIZE; i++) {
        atlas[i] = C_CLEAR;
    }
    for (int i = 0; i < 256; i++) {
        palette[i] = 0;
    }
    palette[C_CLEAR] = 0;
    palette[C_OUTLINE] = rgba5551(24, 20, 28);
    palette[C_SHINE] = rgba5551(250, 250, 250);
    palette[C_EMPTY] = rgba5551(64, 60, 70);
    palette[C_PURPLE] = rgba5551(160, 70, 220);
    palette[C_GOLD] = rgba5551(250, 200, 40);
    palette[C_RED] = rgba5551(230, 50, 40);
    palette[C_AMBER] = rgba5551(220, 150, 40);
    palette[C_BLUE] = rgba5551(50, 110, 240);
    palette[C_GREEN] = rgba5551(60, 200, 110);
    palette[C_FLAME_OUT] = rgba5551(230, 70, 20);
    palette[C_FLAME_IN] = rgba5551(255, 210, 60);
    palette[C_CRACK] = rgba5551(250, 250, 240);
    palette[C_POISON] = rgba5551(150, 70, 190);
    palette[C_POISON_DARK] = rgba5551(60, 20, 80);
    palette[C_GOLD_DARK] = rgba5551(120, 80, 10);
    palette[C_WOOD] = rgba5551(150, 95, 45);
    palette[C_STEEL] = rgba5551(205, 210, 225);

    make_diamond(ICON_CHARGE_EMPTY, C_EMPTY);
    make_diamond(ICON_CHARGE_FULL, C_PURPLE);
    make_diamond(ICON_CHARGE_SPEND, C_GOLD);
    make_diamond(ICON_FIRE, C_RED);
    make_diamond(ICON_EARTH, C_AMBER);
    make_diamond(ICON_WATER, C_BLUE);
    make_diamond(ICON_WIND, C_GREEN);
    make_flame(ICON_BURN);
    make_broken(ICON_BROKEN);
    make_poison(ICON_POISON);
    make_pointer(ICON_POINTER);
    make_coin(ICON_COIN);
    make_pip(ICON_PIP_EMPTY, C_EMPTY);
    make_pip(ICON_PIP_FULL, C_PURPLE);
    make_pip(ICON_PIP_STEEL, C_STEEL);
    make_lock(ICON_LOCK);
    make_staff(ICON_STAFF);
    make_robe(ICON_ROBE);
    make_charm(ICON_CHARM);
    make_sigil(ICON_SIGIL);

    descriptor[0] = (u32)atlas;
    descriptor[1] = 0;
    descriptor[2] = ATLAS_W;
    descriptor[3] = CELL * ICON_COUNT;
}

void hud_draw_tex_region(void* texture, s32 x, s32 y, s32 w, s32 h, s32 s, s32 t, s32 dsdx, s32 dtdy);

// Loads our palette; call once before a batch of eo_icon_draw.
void eo_icons_begin(void) {
    if (!eo_gfx_fits(0x60)) {
        return;
    }
    eo_gfx(0xE7000000, 0x00000000);                // pipe sync
    eo_gfx(0xBA000E02, 0x00008000);                // texture LUT: RGBA16
    eo_gfx(0xE7000000, 0x00000000);
    eo_gfx(0xFD100000, (u32)palette);              // set texture image: our palette
    eo_gfx(0xE8000000, 0x00000000);                // tile sync
    eo_gfx(0xF5000100, 0x07000000);                // tile 7 at TMEM 0x100
    eo_gfx(0xE6000000, 0x00000000);                // load sync
    eo_gfx(0xF0000000, 0x073FC000);                // load TLUT, 256 colors
    eo_gfx(0xE7000000, 0x00000000);
}

void eo_icon_draw(int icon, s32 x, s32 y) {
    if (!eo_gfx_fits(0x80)) {
        return;
    }
    hud_draw_tex_region(descriptor, x, y, CELL, CELL, 0, icon * CELL, 0x400, 0x400);
}

// ---- Screen position of a point in the 3D scene (the routine that places damage numbers) ----

s32 world_to_screen(f32 x, f32 y, f32 z, s32* sx, s32* sy);

int eo_screen_pos(f32 x, f32 y, f32 z, s32* sx, s32* sy) {
    return world_to_screen(x, y, z, sx, sy) != 0;
}
