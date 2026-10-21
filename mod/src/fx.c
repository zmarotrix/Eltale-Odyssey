#include "eo.h"

// Status visuals on the models themselves, with no icons (playtests, 2026-10-20/21: "a burned enemy is faintly
// flashing red, and has fire particles coming off them when they take damage"; "There should just be a visual
// on the enemy"; shields: "just some feedback that you have damaged the enemies shield").
//
// Tints: models are lit by the game's lights (three directional and an ambient one, loaded once before the
// models are drawn). Just before a model with a status is drawn we load changed copies of the four lights,
// and put the game's own back right after, so only that model changes:
//   Burn    the enemy pulses red
//   Broken  the enemy is slightly darker, glints drift around its head, and its animation crawls
//   Poison  Brian pulses purple (in the field too)
//
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

#define PULSE_PERIOD 48           // frames per pulse
#define BURN_RED 75               // percent of the way to full red at the peak
#define BURN_DIM 90               // percent green and blue drop at the peak
#define BROKEN_LIGHT 55           // percent of the normal light while Broken (the lights add up past full,
                                  // so a gentle cut barely shows)
#define BROKEN_ANIM_STEP 8        // a Broken enemy's animation advances one frame in this many
#define BATTLE_DATA_ANIM_FRAME 0x54  // u16 frames into the current animation (model_draw reads it)
#define BREAK_FLASH_FRAMES 12     // the enemy's model flickers when it breaks
#define FLICKER_LIGHT 20          // percent of the light on the dark frames
#define HEAD_GLINT_EVERY 6        // frames between glints around a Broken enemy's head
// Poison scales each channel down at the peak (pulling toward a color wouldn't show: the lights add up past
// full brightness), keeping most red and blue: purple.
static const u8 poison_keep[3] = { 75, 15, 85 };   // percent of each channel at the peak

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

static int tinting = 0;
static u32 frame = 0;             // counts Brian's draws: one per frame wherever he is
static u8 flame_frames[MAX_ENEMIES];
static u8 was_broken[MAX_ENEMIES];
static u8 flash_frames[MAX_ENEMIES];
static u16 held_frame[MAX_ENEMIES];  // a Broken enemy's animation frame

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

// A smooth-ish triangle pulse, 0..100.
static int pulse(void) {
    int phase = (int)(frame % PULSE_PERIOD);
    return phase < PULSE_PERIOD / 2 ? phase * 200 / PULSE_PERIOD : (PULSE_PERIOD - phase) * 200 / PULSE_PERIOD;
}

RECOMP_HOOK("model_draw") void eo_fx_before_model(s32 flags, u8* object, void* animation, void* gfx) {
    int burn = 0;
    int broken = 0;
    int poison = 0;
    int flicker = 0;   // a dark frame of the Break flicker
    if (object == BRIAN_OBJECT) {
        frame++;
        poison = eo_status_poisoned();
    } else {
        int slot = enemy_slot_of(object);
        if (slot < 0 || ENEMY_HP(slot) == 0 || !(gBattleState & 0x1)) {
            return;
        }
        burn = eo_status_burn_stacks(slot) > 0;
        broken = eo_combat_broken(slot);
        flicker = flash_frames[slot] > 0 && (flash_frames[slot] & 2) != 0;
        // A Broken enemy's animation is held back: its frame only moves on every BROKEN_ANIM_STEP draws (the
        // game advances it by one after each draw).
        u16* anim_frame = (u16*)(object + BATTLE_DATA_ANIM_FRAME);
        if (broken && was_broken[slot] && frame % BROKEN_ANIM_STEP != 0) {
            *anim_frame = held_frame[slot];
        }
        held_frame[slot] = *anim_frame;
        was_broken[slot] = (u8)broken;
    }
    if (!burn && !broken && !poison && !flicker) {
        return;
    }
    int p = pulse();
    for (int i = 0; i < LIGHT_COUNT; i++) {
        u8* game = (u8*)game_light[i];
        u8* light = TINT_LIGHTS + i * 16;
        for (int k = 0; k < 16; k++) {
            light[k] = game[k];
        }
        for (int k = 0; k < 8; k += 4) {   // the color and its copy
            u8* c = light + k;
            if (broken) {
                for (int ch = 0; ch < 3; ch++) {
                    c[ch] = (u8)(c[ch] * BROKEN_LIGHT / 100);
                }
            }
            if (burn) {
                c[0] = toward(c[0], 255, BURN_RED * p / 100);
                c[1] = toward(c[1], 0, BURN_DIM * p / 100);
                c[2] = toward(c[2], 0, BURN_DIM * p / 100);
            }
            if (poison) {
                for (int ch = 0; ch < 3; ch++) {
                    c[ch] = toward(c[ch], (u8)(c[ch] * poison_keep[ch] / 100), p);
                }
            }
            for (int ch = 0; ch < 3 && flicker; ch++) {
                c[ch] = (u8)(c[ch] * FLICKER_LIGHT / 100);
            }
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
