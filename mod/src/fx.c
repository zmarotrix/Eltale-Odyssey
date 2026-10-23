#include "eo.h"

// Status visuals on the models themselves, with no icons (playtests, 2026-10-20/21: "a burned enemy is faintly
// flashing red, and has fire particles coming off them when they take damage"; "There should just be a visual
// on the enemy"; shields: "just some feedback that you have damaged the enemies shield").
//
// Status colors: every status has its own color, pulsed on the model. The models' render mode blends in fog
// ("fog color x shade alpha"), so just before a model with a status is drawn we set the fog color and a constant
// fog amount, and put the game's own fog color and range back right after: any color, white included, on just
// that model. (Lights can only darken: they can't take a model past its own texture colors.)
//   Burn red, Poison purple, then the game's statuses (status_colors below)
// Broken: the enemy's lights pulse dark, glints drift around its head, and its animation crawls. When it breaks
// it also flickers dark for a moment.

// Freeze (playtest, 2026-10-23: "a freeze visual that stops their animation and has an ice block on the frozen
// enemy"): a frozen foe's animation stops dead, and a pale, see-through block of ice is drawn around it, shaded by
// the scene's lights. Broken foes also carry the restriction bit; they keep the Broken look instead.

// Particles: the game's sparkle particles (the ones spell impacts and enemy deaths spray), from the color table:
//   Burn damage  small embers rising around the enemy, in reds and orange. The sparkle's own draw commands mix
//                its color toward white ("texture + (1 - texture) x color"), so embers use a copy that draws
//                "texture x color" instead.
//   Shield hit   white glints flying off the enemy; a Break throws more, bigger, and pale gold, and the
//                enemy's model flickers dark for a moment (lights can't take a model past its own texture
//                colors, so a white flash isn't possible this way).

#define MAX_ENEMIES 6
#define ENEMY_SLOTS 0x8007C998
#define ENEMY_SLOT_SIZE 0x128
#define ENEMY_BATTLE_DATA(i) ((u8*)(ENEMY_SLOTS + (i) * ENEMY_SLOT_SIZE + 0x24))
#define ENEMY_HP(i) (*(u16*)(ENEMY_SLOTS + (i) * ENEMY_SLOT_SIZE + 0x0A))
#define BRIAN_OBJECT ((u8*)0x8007BACC)    // gPlayerData1: what model_draw gets for Brian

#define BATTLE_DATA_RECORD 0x64
#define BATTLE_DATA_SCALE 0x24
#define RECORD_TYPE 0x00          // 0 ground, 1 flying
#define RECORD_WIDTH 0x18
#define RECORD_HEIGHT 0x1C

// The game loads its lights with F3D G_MOVEMEM commands: lights 0-2 directional (16 bytes each), light 3 the
// ambient one (only its color is used). Their colors: r, g, b, 0, then the same again.
#define LIGHT_COUNT 4
static const u32 light_load[LIGHT_COUNT] = { 0x03860010, 0x03880010, 0x038A0010, 0x038C0010 };
static const u32 game_light[LIGHT_COUNT] = { 0x80085770, 0x80085780, 0x80085790, 0x80085768 };
// Our copies have to be in the console's 8 MB for the renderer to read them (see icons.c, which uses the 0x1200
// bytes before them).
#define FX_RAM 0x807F1200
#define TINT_LIGHTS ((u8*)FX_RAM)              // 4 x 16 bytes
#define FLAME_GFX ((u32*)(FX_RAM + 0x40))      // the sparkle's draw commands, recolored
#define ICE_VTX ((u8*)(FX_RAM + 0x100))         // the ice block: 24 vertices x 16 bytes
#define ICE_MTX ((u8*)(FX_RAM + 0x280))         // one 64-byte matrix per enemy

#define PULSE_PERIOD 48           // frames per pulse
#define BROKEN_DARK 70            // percent the lights drop at the peak of a Broken enemy's dark pulse
#define BROKEN_ANIM_STEP 8        // a Broken enemy's animation advances one frame in this many
#define BATTLE_DATA_ANIM_FRAME 0x54  // u16 frames into the current animation (model_draw reads it)
#define BATTLE_DATA_STATS 0x68    // pointer to the fighter's stats; +0x00 is its u16 status bits
#define BRIAN_STATUS (*(u16*)0x8007BB38)
#define STATUS_RESTRICTION 0x008  // also how Break skips the enemy's turn
#define BREAK_FLASH_FRAMES 12     // the enemy's model flickers when it breaks
#define FLICKER_LIGHT 20          // percent of the light on the dark frames
#define HEAD_GLINT_EVERY 6        // frames between glints around a Broken enemy's head

// Fog: F3D G_SETFOGCOLOR (0xF8) and G_MOVEWORD G_MW_FOG (0xBC000008, fog multiplier << 16 | offset). With a zero
// multiplier the fog amount is the offset alone.
#define GFX_FOG_COLOR 0xF8
#define GFX_FOG_FACTOR 0xBC000008
#define FOG_SCAN 4096             // commands searched back for the game's current fog settings
#define FOG_PEAK 0x00A0           // fog offset at the peak of a status pulse

// The ice block, drawn in F3DEX right after the frozen enemy's model, in the models' own 2-cycle mode: the game's
// translucent surface mode (it uses it itself), color = primitive x shade (the scene's lights on the block's
// faces), alpha = primitive. The game's render mode and color mode are put back after it.
#define GFX_MTX_LOAD 0x01020040   // modelview, load, no push (as model_draw_bone does; the view is in projection)
#define GFX_VTX_24 0x0400617F     // 24 vertices from index 0
#define GFX_TRI2 0xB1
#define GFX_PIPE_SYNC 0xE7
#define GFX_RENDER_MODE 0xB900031D
#define ICE_RENDER_MODE 0x0C1849D8     // pass, then translucent surface (z compare, no z update)
#define ICE_COMBINE_W0 0xFC327FFF      // cycle 1: primitive x shade, alpha primitive; cycle 2: pass
#define ICE_COMBINE_W1 0xFFFFF638
#define GFX_PRIM_COLOR 0xFA
#define ICE_COLOR 0xD2F0FFA8           // pale icy blue, about two-thirds opaque
#define ICE_WIDTH 1.35f                // the block's half-width, times the body's
#define ICE_HEIGHT 1.08f               // its height, times the body's
#define ICE_UNIT 100                   // vertex coordinates are +-100; the matrix scales them

typedef struct { u8 r, g, b; } Rgb;
static const Rgb burn_color = { 255, 50, 0 };
// Beigis's element, a steady glow (statuses pulse over it): Fire, Earth, Water, Wind.
static const Rgb element_colors[4] = { { 255, 110, 20 }, { 190, 140, 50 }, { 40, 110, 255 }, { 70, 230, 120 } };
#define ELEMENT_GLOW 0x0070       // fog offset of the element glow
static const Rgb poison_color = { 160, 40, 230 };
// The game's status bits, lowest first (research/battle, "Status effects").
static const Rgb status_colors[12] = {
    { 255, 225, 60 },    // 0x001 Compression: yellow
    { 170, 0, 50 },      // 0x002 Vampire's Touch: crimson
    { 255, 150, 30 },    // 0x004 Power Staff: orange
    { 130, 210, 255 },   // 0x008 Restriction (Freeze): ice blue
    { 170, 235, 255 },   // 0x010 Movement: pale ice blue (Frost, spells.c)
    { 40, 220, 200 },    // 0x020 Agility: teal
    { 255, 255, 255 },   // 0x040 Silence: white
    { 255, 210, 90 },    // 0x080 Soul Searcher: gold
    { 200, 230, 255 },   // 0x100 Magic Barrier: pale blue
    { 255, 120, 200 },   // 0x200 MP absorb: pink
    { 150, 100, 60 },    // 0x400 Defense: brown
    { 255, 255, 255 },   // 0x800 (unused)
};

// Particles: the sparkle template the emitters use, and its fields.
#define SPARKLE_TEMPLATE ((u8*)0x8004CFB0)
#define SPARKLE_GFX ((u32*)0x800C4948)     // ends with G_ENDDL (0xB8)
#define SPARKLE_GFX_WORDS 20
#define P_GFX 0x14                // draw commands (+0x10 is the texture)
#define GFX_SET_COMBINE 0xFC
#define FLAME_COMBINE_W0 0xFC509BFF   // color (ENV - 0) x TEXEL0 + 0; alpha and second cycle as the sparkle's
#define FLAME_COMBINE_W1 0xFFFFFE38
#define PARTICLE_SIZE 0x40
#define P_LIFE 0x00               // u16 frames
#define P_POS 0x18                // x, y, z
#define P_SCALE 0x28              // x, y
#define P_VEL 0x30                // x, y, z (the game slows it by 10% a frame, so it travels about 10 x this)
#define P_COLOR 0x3C              // index in the color table 0x800EBD1C
#define COLOR_RED 2
#define COLOR_YELLOW 6
#define COLOR_WHITE 7
#define COLOR_SCARLET 8           // ff1400
#define COLOR_PALE_GOLD 13        // ffff9b
#define COLOR_ORANGE 15

#define FLAME_FRAMES 16           // how long a burst lasts; one ember every other frame
#define FLAME_SCALE 1.2f          // the game's sparkles are 1.3; it shrinks them to 1.0 near the end
#define FLAME_HEIGHT 0.2f         // spawn this far up the body (0 = feet, 1 = top)
#define GLINTS 8                  // per shield hit
#define BREAK_GLINTS 18
#define GLINT_HEIGHT 0.5f

void* particle_spawn(void* particle);
s32 get_rand(s32 n);
extern u16 gBattleState;

static u32 frame = 0;             // counts Brian's draws: one per frame wherever he is
static u8 flame_frames[MAX_ENEMIES];
static u8 was_broken[MAX_ENEMIES];
static u8 flash_frames[MAX_ENEMIES];
static u16 held_frame[MAX_ENEMIES];  // a Broken or frozen enemy's animation frame
static u8 was_frozen[MAX_ENEMIES];
static int ice_slot = -1;            // the frozen enemy being drawn now

static int enemy_slot_of(u8* object) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (object == ENEMY_BATTLE_DATA(i)) {
            return i;
        }
    }
    return -1;
}

// A smooth-ish triangle pulse, 0..100.
static int pulse(void) {
    int phase = (int)(frame % PULSE_PERIOD);
    return phase < PULSE_PERIOD / 2 ? phase * 200 / PULSE_PERIOD : (PULSE_PERIOD - phase) * 200 / PULSE_PERIOD;
}

static int lighting = 0;
static int fogging = 0;
static u32 game_fog_color;
static u32 game_fog_factor;

// The fog color and range the game last set in this frame's display list.
static int find_game_fog(void) {
    u32* g = gMasterGfxPos;
    int have_color = 0;
    int have_factor = 0;
    for (int i = 1; i <= FOG_SCAN && !(have_color && have_factor); i++) {
        u32* c = g - 2 * i;
        if (!have_color && (c[0] >> 24) == GFX_FOG_COLOR) {
            game_fog_color = c[1];
            have_color = 1;
        }
        if (!have_factor && c[0] == GFX_FOG_FACTOR) {
            game_fog_factor = c[1];
            have_factor = 1;
        }
    }
    return have_color && have_factor;
}

// The color of a fighter's status, if it has one. One status at a time (status.c), so there's only one to show.
static const Rgb* status_color(u16 bits, int burn, int poison, int broken) {
    if (burn) {
        return &burn_color;
    }
    if (poison) {
        return &poison_color;
    }
    if (broken) {
        bits &= (u16)~STATUS_RESTRICTION;   // that's the Break, shown as the dark pulse
    }
    for (int i = 0; i < 12; i++) {
        if (bits & (1 << i)) {
            return &status_colors[i];
        }
    }
    return 0;
}

RECOMP_HOOK("model_draw") void eo_fx_before_model(s32 flags, u8* object, void* animation, void* gfx) {
    const Rgb* color = 0;
    int broken = 0;
    int flicker = 0;   // a dark frame of the Break flicker
    int element = -1;  // a steady element glow instead of a status
    if (object == BRIAN_OBJECT) {
        frame++;
        color = status_color((gBattleState & 0x1) ? BRIAN_STATUS : 0, 0, eo_status_poisoned(), 0);
    } else {
        int slot = enemy_slot_of(object);
        if (slot < 0 || ENEMY_HP(slot) == 0 || !(gBattleState & 0x1)) {
            return;
        }
        broken = eo_combat_broken(slot);
        u8* stats = *(u8**)(object + BATTLE_DATA_STATS);
        color = status_color(stats != 0 ? *(u16*)stats : 0, eo_status_burn_stacks(slot) > 0, 0, broken);
        flicker = flash_frames[slot] > 0 && (flash_frames[slot] & 2) != 0;
        element = color == 0 ? eo_combat_shifting_element(slot) : -1;
        // A Broken enemy's animation is held back: its frame only moves on every BROKEN_ANIM_STEP draws (the
        // game advances it by one after each draw).
        // A frozen one's doesn't move at all.
        int frozen = !broken && !eo_spells_confused(slot) && stats != 0 && (*(u16*)stats & STATUS_RESTRICTION) != 0;
        u16* anim_frame = (u16*)(object + BATTLE_DATA_ANIM_FRAME);
        if ((broken && was_broken[slot] && frame % BROKEN_ANIM_STEP != 0) || (frozen && was_frozen[slot])) {
            *anim_frame = held_frame[slot];
        }
        held_frame[slot] = *anim_frame;
        was_broken[slot] = (u8)broken;
        was_frozen[slot] = (u8)frozen;
        ice_slot = frozen ? slot : -1;
    }
    if (!eo_gfx_fits(0x100)) {
        return;   // no room to tint and restore this model
    }
    int p = pulse();

    // Broken and the Break flicker: darker lights.
    if (broken || flicker) {
        int keep = flicker ? FLICKER_LIGHT : 100 - BROKEN_DARK * p / 100;
        for (int i = 0; i < LIGHT_COUNT; i++) {
            u8* game = (u8*)game_light[i];
            u8* light = TINT_LIGHTS + i * 16;
            for (int k = 0; k < 16; k++) {
                light[k] = game[k];
            }
            for (int k = 0; k < 8; k += 4) {   // the color and its copy
                for (int ch = 0; ch < 3; ch++) {
                    light[k + ch] = (u8)(light[k + ch] * keep / 100);
                }
            }
            eo_gfx(light_load[i], (u32)light);
        }
        lighting = 1;
    }

    // A status: its color as fog, pulsing.
    if (color != 0 && find_game_fog()) {
        eo_gfx(GFX_FOG_COLOR << 24, ((u32)color->r << 24) | ((u32)color->g << 16) | ((u32)color->b << 8) | 0xFF);
        eo_gfx(GFX_FOG_FACTOR, (u32)(FOG_PEAK * p / 100) & 0xFFFF);
        fogging = 1;
    } else if (element >= 0 && find_game_fog()) {
        const Rgb* glow = &element_colors[element];
        eo_gfx(GFX_FOG_COLOR << 24, ((u32)glow->r << 24) | ((u32)glow->g << 16) | ((u32)glow->b << 8) | 0xFF);
        eo_gfx(GFX_FOG_FACTOR, ELEMENT_GLOW);
        fogging = 1;
    }
}

// Builds the ice block's 24 vertices once: four per face, wound counter-clockwise seen from outside, with the
// face's normal for the lights. x and z run -100..100, y 0..100 (feet to top).
static void make_ice_vertices(void) {
    static int ready = 0;
    if (ready) {
        return;
    }
    static const s8 quad[4][2] = { { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 } };
    for (int f = 0; f < 6; f++) {
        int axis = f / 2;
        int sign = (f & 1) ? -1 : 1;
        int u = (axis + 1) % 3;
        int v = (axis + 2) % 3;
        for (int k = 0; k < 4; k++) {
            int c[3];
            c[axis] = sign;
            // u x v points along +axis, so walking the square forwards faces +axis; backwards faces -axis.
            int q = sign > 0 ? k : 3 - k;
            c[u] = quad[q][0];
            c[v] = quad[q][1];
            u8* vtx = ICE_VTX + (f * 4 + k) * 16;
            for (int i = 0; i < 16; i++) {
                vtx[i] = 0;
            }
            *(s16*)(vtx + 0) = (s16)(c[0] * ICE_UNIT);
            *(s16*)(vtx + 2) = (s16)(c[1] > 0 ? ICE_UNIT : 0);
            *(s16*)(vtx + 4) = (s16)(c[2] * ICE_UNIT);
            vtx[12 + axis] = (u8)(s8)(sign * 120);   // normal
            vtx[15] = 0xFF;
        }
    }
    ready = 1;
}

// The game's fixed-point matrix: 16 integer halves, then 16 fraction halves, row by row (row vectors: the
// bottom row is the translation).
static void put_matrix(u8* mtx, const f32 m[4][4]) {
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            s32 fixed = (s32)(m[r][c] * 65536.0f);
            ((u16*)mtx)[r * 4 + c] = (u16)((u32)fixed >> 16);
            ((u16*)mtx)[16 + r * 4 + c] = (u16)((u32)fixed & 0xFFFF);
        }
    }
}

// The last command in this frame's display list with this first word (or this opcode, if `whole` is 0).
static int find_game_command(u32 w0, int whole, u32* out) {
    u32* g = gMasterGfxPos;
    for (int i = 1; i <= FOG_SCAN; i++) {
        u32* c = g - 2 * i;
        if (whole ? c[0] == w0 : (c[0] >> 24) == w0) {
            out[0] = c[0];
            out[1] = c[1];
            return 1;
        }
    }
    return 0;
}

static void draw_ice(int slot) {
    if (!eo_gfx_fits(0x140)) {
        return;
    }
    make_ice_vertices();
    u8* bd = ENEMY_BATTLE_DATA(slot);
    u8* record = *(u8**)(bd + BATTLE_DATA_RECORD);
    f32 scale = *(f32*)(bd + BATTLE_DATA_SCALE);
    f32 width = 6.0f;
    f32 height = 12.0f;
    f32 base = *(f32*)(bd + 0x4);
    if (record != 0) {
        width = *(f32*)(record + RECORD_WIDTH) * scale * 0.5f;
        height = *(f32*)(record + RECORD_HEIGHT) * scale;
        if (*(u16*)(record + RECORD_TYPE) == 1) {
            base = *(f32*)(*(u8**)(bd + 0x68) + 0x94) - height;   // fliers: down from the top of the body
        }
    }
    f32 w = width * ICE_WIDTH / ICE_UNIT;
    f32 h = height * ICE_HEIGHT / ICE_UNIT;
    f32 m[4][4] = {
        { w, 0, 0, 0 },
        { 0, h, 0, 0 },
        { 0, 0, w, 0 },
        { *(f32*)(bd + 0x0), base - height * 0.05f, *(f32*)(bd + 0x8), 1 },
    };
    u8* mtx = ICE_MTX + slot * 64;
    put_matrix(mtx, m);

    u32 game_mode[2];
    u32 game_combine[2];
    u32 game_prim[2];
    int have_mode = find_game_command(GFX_RENDER_MODE, 1, game_mode);
    int have_combine = find_game_command(GFX_SET_COMBINE, 0, game_combine);
    int have_prim = find_game_command(GFX_PRIM_COLOR, 0, game_prim);

    eo_gfx(GFX_PIPE_SYNC << 24, 0);
    eo_gfx(GFX_RENDER_MODE, ICE_RENDER_MODE);
    eo_gfx(ICE_COMBINE_W0, ICE_COMBINE_W1);
    eo_gfx(GFX_PRIM_COLOR << 24, ICE_COLOR);
    eo_gfx(GFX_MTX_LOAD, (u32)mtx);
    eo_gfx(GFX_VTX_24, (u32)ICE_VTX);
    for (int f = 0; f < 6; f++) {
        u32 q = (u32)(f * 4 * 2);   // F3DEX vertex indices are doubled
        eo_gfx((GFX_TRI2 << 24) | (q << 16) | ((q + 2) << 8) | (q + 4), (q << 16) | ((q + 4) << 8) | (q + 6));
    }
    eo_gfx(GFX_PIPE_SYNC << 24, 0);
    if (have_mode) {
        eo_gfx(game_mode[0], game_mode[1]);
    }
    if (have_combine) {
        eo_gfx(game_combine[0], game_combine[1]);
    }
    if (have_prim) {
        eo_gfx(game_prim[0], game_prim[1]);
    }
}

RECOMP_HOOK_RETURN("model_draw") void eo_fx_after_model(void) {
    if (lighting) {
        for (int i = 0; i < LIGHT_COUNT; i++) {
            eo_gfx(light_load[i], game_light[i]);
        }
        lighting = 0;
    }
    if (fogging) {
        eo_gfx(GFX_FOG_COLOR << 24, game_fog_color);
        eo_gfx(GFX_FOG_FACTOR, game_fog_factor);
        fogging = 0;
    }
    if (ice_slot >= 0) {
        draw_ice(ice_slot);
        ice_slot = -1;
    }
}

// Copies the sparkle's draw commands with the ember color mode (once; the game's never change).
static u32* flame_gfx(void) {
    static int ready = 0;
    if (!ready) {
        for (int i = 0; i < SPARKLE_GFX_WORDS; i += 2) {
            FLAME_GFX[i] = SPARKLE_GFX[i];
            FLAME_GFX[i + 1] = SPARKLE_GFX[i + 1];
            if ((SPARKLE_GFX[i] >> 24) == GFX_SET_COMBINE) {
                FLAME_GFX[i] = FLAME_COMBINE_W0;
                FLAME_GFX[i + 1] = FLAME_COMBINE_W1;
            }
        }
        ready = 1;
    }
    return FLAME_GFX;
}

static f32 rand_unit(void) {  // -1..1
    return (f32)(get_rand(201) - 100) / 100.0f;
}

// A point on a ring around the enemy's body, `height` of the way up it (0 = feet, 1 = top), and the ring's
// outward direction there.
static void body_point(int slot, f32 height, f32* pos, f32* out_x, f32* out_z) {
    u8* bd = ENEMY_BATTLE_DATA(slot);
    u8* record = *(u8**)(bd + BATTLE_DATA_RECORD);
    f32 scale = *(f32*)(bd + BATTLE_DATA_SCALE);
    f32 y = *(f32*)(bd + 0x4);
    f32 width = 4.0f;
    if (record != 0) {
        // Fliers: down from the top of the body, as the game's death effect finds it.
        f32 h = *(f32*)(record + RECORD_HEIGHT) * scale;
        if (*(u16*)(record + RECORD_TYPE) == 1) {
            y = *(f32*)(*(u8**)(bd + 0x68) + 0x94) - h * (1.0f - height);
        } else {
            y += h * height;
        }
        width = *(f32*)(record + RECORD_WIDTH) * scale * 0.5f;
    }
    f32 dx = rand_unit();
    f32 dz = rand_unit();
    f32 len = __builtin_sqrtf(dx * dx + dz * dz);
    if (len < 0.01f) {
        dx = 1.0f;
        len = 1.0f;
    }
    *out_x = dx / len;
    *out_z = dz / len;
    pos[0] = *(f32*)(bd + 0x0) + *out_x * width;
    pos[1] = y + rand_unit() * width * 0.2f;
    pos[2] = *(f32*)(bd + 0x8) + *out_z * width;
}

static void spawn(const f32* pos, const f32* vel, f32 scale, u8 color, u16 life, u32* gfx) {
    u8 p[PARTICLE_SIZE] __attribute__((aligned(8)));
    for (int k = 0; k < PARTICLE_SIZE; k++) {
        p[k] = SPARKLE_TEMPLATE[k];
    }
    *(u16*)(p + P_LIFE) = life;
    if (gfx != 0) {
        *(u32**)(p + P_GFX) = gfx;
    }
    for (int k = 0; k < 3; k++) {
        *(f32*)(p + P_POS + k * 4) = pos[k];
        *(f32*)(p + P_VEL + k * 4) = vel[k];
    }
    *(f32*)(p + P_SCALE + 0) = scale;
    *(f32*)(p + P_SCALE + 4) = scale;
    p[P_COLOR] = color;
    particle_spawn(p);
}

static void spawn_ember(int slot) {
    f32 pos[3];
    f32 ox;
    f32 oz;
    body_point(slot, FLAME_HEIGHT, pos, &ox, &oz);
    f32 vel[3] = { rand_unit() * 0.3f, 0.6f + (f32)get_rand(40) / 100.0f, rand_unit() * 0.3f };
    int pick = get_rand(6);
    u8 color = pick < 2 ? COLOR_RED : pick < 4 ? COLOR_SCARLET : pick < 5 ? COLOR_ORANGE : COLOR_YELLOW;
    spawn(pos, vel, FLAME_SCALE, color, (u16)(14 + get_rand(8)), flame_gfx());
}

void eo_fx_burn_hit(int slot) {
    if (slot >= 0 && slot < MAX_ENEMIES) {
        flame_frames[slot] = FLAME_FRAMES;
    }
}

void eo_fx_shield_hit(int slot, int broke) {
    if (slot < 0 || slot >= MAX_ENEMIES) {
        return;
    }
    if (broke) {
        flash_frames[slot] = BREAK_FLASH_FRAMES;
    }
    int count = broke ? BREAK_GLINTS : GLINTS;
    for (int i = 0; i < count; i++) {
        f32 pos[3];
        f32 ox;
        f32 oz;
        body_point(slot, GLINT_HEIGHT, pos, &ox, &oz);
        f32 speed = (broke ? 1.6f : 1.1f) + (f32)get_rand(60) / 100.0f;
        f32 vel[3] = { ox * speed, 0.2f + (f32)get_rand(60) / 100.0f, oz * speed };
        u8 color = broke && (i & 1) ? COLOR_PALE_GOLD : COLOR_WHITE;
        spawn(pos, vel, broke ? 2.2f : 1.5f, color, (u16)(14 + get_rand(10)), 0);
    }
}

// Ember Staff flare: embers burst outward from the enemy along the ground, out to about the spread's reach.
#define FLARE_EMBERS 24

void eo_fx_flare(int slot) {
    if (slot < 0 || slot >= MAX_ENEMIES) {
        return;
    }
    for (int i = 0; i < FLARE_EMBERS; i++) {
        f32 pos[3];
        f32 ox;
        f32 oz;
        body_point(slot, FLAME_HEIGHT, pos, &ox, &oz);
        f32 speed = 3.5f + (f32)get_rand(150) / 100.0f;   // travels about 10 x this
        f32 vel[3] = { ox * speed, 0.2f + (f32)get_rand(30) / 100.0f, oz * speed };
        int pick = get_rand(4);
        u8 color = pick < 2 ? COLOR_ORANGE : pick < 3 ? COLOR_SCARLET : COLOR_YELLOW;
        spawn(pos, vel, 1.6f, color, (u16)(16 + get_rand(8)), flame_gfx());
    }
}

// Lock-On's marker: a pale-gold sparkle bobbing just over the target's head.
void eo_fx_lockon_marker(int slot) {
    if (slot < 0 || slot >= MAX_ENEMIES) {
        return;
    }
    f32 pos[3];
    f32 ox;
    f32 oz;
    body_point(slot, 1.0f, pos, &ox, &oz);
    u8* bd = ENEMY_BATTLE_DATA(slot);
    pos[0] = *(f32*)(bd + 0x0);   // centered, not on the body's ring
    pos[2] = *(f32*)(bd + 0x8);
    pos[1] += 6.0f;
    f32 vel[3] = { 0.0f, 0.15f, 0.0f };
    spawn(pos, vel, 3.0f, COLOR_PALE_GOLD, 16, 0);   // the game shrinks sparkles to 1.0 below 10 frames left
}

// A slow glint drifting around a Broken enemy's head.
static void spawn_head_glint(int slot) {
    f32 pos[3];
    f32 ox;
    f32 oz;
    body_point(slot, 1.0f, pos, &ox, &oz);
    f32 vel[3] = { -oz * 0.35f, 0.1f, ox * 0.35f };   // sideways around the head
    spawn(pos, vel, 1.1f, COLOR_WHITE, (u16)(20 + get_rand(10)), 0);
}

// Every frame in battle (combat.c).
void eo_fx_update(void) {
    static u32 ticks = 0;
    ticks++;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (flash_frames[i] > 0) {
            flash_frames[i]--;
        }
        if (ENEMY_HP(i) > 0 && eo_combat_broken(i) && ticks % HEAD_GLINT_EVERY == 0) {
            spawn_head_glint(i);
        }
        if (flame_frames[i] == 0) {
            continue;
        }
        flame_frames[i]--;
        if (flame_frames[i] & 1) {
            spawn_ember(i);
        }
    }
}
