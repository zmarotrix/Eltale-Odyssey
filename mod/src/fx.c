#include "eo.h"

// Burn visuals (playtest, 2026-10-20: "a burned enemy is faintly flashing red, and has fire particles coming off
// them when they take damage").
//
// Red flash: enemy models are lit by the game's lights (three directional and an ambient one, loaded once before
// the enemies are drawn). Just before a burning enemy is drawn we load reddened copies of the four lights, and
// put the game's own back right after, so only that enemy changes.
//
// Flames: the game's sparkle particles (the ones spell impacts and enemy deaths spray), made bigger, drifting
// upwards and colored from the game's own color table: orange, yellow and red. The sparkle's own draw commands
// mix its color toward white ("texture + (1 - texture) x color"), so flames use a copy that draws
// "texture x color" instead.

#define MAX_ENEMIES 6
#define ENEMY_SLOTS 0x8007C998
#define ENEMY_SLOT_SIZE 0x128
#define ENEMY_BATTLE_DATA(i) ((u8*)(ENEMY_SLOTS + (i) * ENEMY_SLOT_SIZE + 0x24))
#define ENEMY_HP(i) (*(u16*)(ENEMY_SLOTS + (i) * ENEMY_SLOT_SIZE + 0x0A))

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

#define FLASH_PERIOD 48           // frames per pulse
#define FLASH_RED 55              // percent of the way to full red at the peak
#define FLASH_DIM 70              // percent green and blue drop at the peak

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
#define P_VEL 0x30                // x, y, z
#define P_COLOR 0x3C              // index in the color table 0x800EBD1C
#define COLOR_RED 2
#define COLOR_SCARLET 8          // ff1400
#define COLOR_YELLOW 6
#define COLOR_ORANGE 15

#define FLAME_FRAMES 16           // how long a burst lasts; one ember every other frame
#define FLAME_SCALE 1.2f          // the game's sparkles are 1.3; it shrinks them to 1.0 near the end
#define FLAME_HEIGHT 0.2f         // spawn this far up the body (0 = feet, 1 = top)

void* particle_spawn(void* particle);
s32 get_rand(s32 n);

static int tinting = 0;
static u32 frame = 0;
static u8 flame_frames[MAX_ENEMIES];

static int enemy_slot_of(u8* object) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (object == ENEMY_BATTLE_DATA(i)) {
            return i;
        }
    }
    return -1;
}

static u8 toward(u8 from, u8 to, int percent) {
    return (u8)(from + ((int)to - from) * percent / 100);
}

RECOMP_HOOK("model_draw") void eo_fx_before_model(s32 flags, u8* object, void* animation, void* gfx) {
    int slot = enemy_slot_of(object);
    if (slot < 0 || ENEMY_HP(slot) == 0 || eo_status_burn_stacks(slot) == 0) {
        return;
    }
    // A smooth-ish triangle pulse, 0..100.
    int phase = (int)(frame % FLASH_PERIOD);
    int pulse = phase < FLASH_PERIOD / 2 ? phase * 200 / FLASH_PERIOD : (FLASH_PERIOD - phase) * 200 / FLASH_PERIOD;
    for (int i = 0; i < LIGHT_COUNT; i++) {
        u8* game = (u8*)game_light[i];
        u8* light = TINT_LIGHTS + i * 16;
        for (int k = 0; k < 16; k++) {
            light[k] = game[k];
        }
        for (int k = 0; k < 8; k += 4) {   // the color and its copy
            light[k + 0] = toward(game[k + 0], 255, FLASH_RED * pulse / 100);
            light[k + 1] = toward(game[k + 1], 0, FLASH_DIM * pulse / 100);
            light[k + 2] = toward(game[k + 2], 0, FLASH_DIM * pulse / 100);
        }
        eo_gfx(light_load[i], (u32)light);
    }
    tinting = 1;
}

RECOMP_HOOK_RETURN("model_draw") void eo_fx_after_model(void) {
    if (tinting) {
        for (int i = 0; i < LIGHT_COUNT; i++) {
            eo_gfx(light_load[i], game_light[i]);
        }
        tinting = 0;
    }
}

// Copies the sparkle's draw commands with the flame color mode (once; the game's never change).
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

static void spawn_flame(int slot) {
    u8* bd = ENEMY_BATTLE_DATA(slot);
    u8* record = *(u8**)(bd + BATTLE_DATA_RECORD);
    f32 scale = *(f32*)(bd + BATTLE_DATA_SCALE);
    f32 x = *(f32*)(bd + 0x0);
    f32 y = *(f32*)(bd + 0x4);
    f32 z = *(f32*)(bd + 0x8);
    f32 width = 4.0f;
    if (record != 0) {
        // Low on the body. Fliers: down from the top of the body, as the game's death effect finds it.
        f32 height = *(f32*)(record + RECORD_HEIGHT) * scale;
        if (*(u16*)(record + RECORD_TYPE) == 1) {
            y = *(f32*)(*(u8**)(bd + 0x68) + 0x94) - height * (1.0f - FLAME_HEIGHT);
        } else {
            y += height * FLAME_HEIGHT;
        }
        width = *(f32*)(record + RECORD_WIDTH) * scale * 0.5f;
    }

    u8 p[PARTICLE_SIZE] __attribute__((aligned(8)));
    for (int k = 0; k < PARTICLE_SIZE; k++) {
        p[k] = SPARKLE_TEMPLATE[k];
    }
    *(u16*)(p + P_LIFE) = (u16)(12 + get_rand(8));
    *(u32**)(p + P_GFX) = flame_gfx();
    *(f32*)(p + P_POS + 0) = x + rand_unit() * width;
    *(f32*)(p + P_POS + 4) = y + rand_unit() * width * 0.2f;
    *(f32*)(p + P_POS + 8) = z + rand_unit() * width;
    *(f32*)(p + P_SCALE + 0) = FLAME_SCALE;
    *(f32*)(p + P_SCALE + 4) = FLAME_SCALE;
    *(f32*)(p + P_VEL + 0) = rand_unit() * 0.3f;
    *(f32*)(p + P_VEL + 4) = 0.4f + (f32)get_rand(40) / 100.0f;
    *(f32*)(p + P_VEL + 8) = rand_unit() * 0.3f;
    int pick = get_rand(6);
    p[P_COLOR] = pick < 2 ? COLOR_RED : pick < 4 ? COLOR_SCARLET : pick < 5 ? COLOR_ORANGE : COLOR_YELLOW;
    particle_spawn(p);
}

void eo_fx_burn_hit(int slot) {
    if (slot >= 0 && slot < MAX_ENEMIES) {
        flame_frames[slot] = FLAME_FRAMES;
    }
}

// Every frame in battle (combat.c).
void eo_fx_update(void) {
    frame++;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (flame_frames[i] == 0) {
            continue;
        }
        flame_frames[i]--;
        if (flame_frames[i] & 1) {
            spawn_flame(i);
        }
    }
}
