#include "eo.h"

// Lock-On, Wind's Z-targeting talent (user idea, 2026-10-20; "like Zelda OoT and MM"). In battle, hold Z: Brian locks
// onto an enemy and keeps facing it while he moves, and since spells fly the way he faces, that aims them. Tapping Z
// again switches to the next enemy. A small pale-gold sparkle floats over the target (fx.c), in place of Navi.
// The camera swings round behind Brian to look at the target too (playtest, 2026-10-21), as Z-targeting does.
//
// Z has no battle action of its own (the game treats it as a second A), so while the talent is learned Z presses are
// taken away from the game in battle. R still opens the spell input; A still swings.

#define MAX_ENEMIES 6
#define ENEMY_SLOTS 0x8007C998
#define ENEMY_SLOT_SIZE 0x128
#define ENEMY_HP(i) (*(u16*)(ENEMY_SLOTS + (i) * ENEMY_SLOT_SIZE + 0x0A))
#define ENEMY_POS(i) ((f32*)(ENEMY_SLOTS + (i) * ENEMY_SLOT_SIZE + 0x24))   // battle data: x, y, z
#define BRIAN_DATA ((u8*)0x8007BACC)
#define BRIAN_POS ((f32*)BRIAN_DATA)
#define BRIAN_YAW (*(f32*)(BRIAN_DATA + 0x10))   // facing: forward is (sin yaw, cos yaw) on x, z (measured walking)
#define BTN_Z 0x2000
#define MARKER_EVERY 3   // frames between marker sparkles

// The camera (0x80086DC0) follows Brian at whatever angle its eye (+0x0C x, +0x14 z) already has around him
// (func_80013008 keeps that angle), so turning it means moving the eye round him. A spell's camera sets flag 0x40
// and is left alone.
#define CAMERA ((u8*)0x80086DC0)
#define CAMERA_FLAGS (*(u16*)(CAMERA + 0x08))
#define CAMERA_EYE_X (*(f32*)(CAMERA + 0x0C))
#define CAMERA_EYE_Z (*(f32*)(CAMERA + 0x14))
#define CAMERA_SPELL_SNAP 0x40
#define CAMERA_TURN 0.2f    // share of the remaining angle the camera turns each frame

extern u16 gPadButtonsNew;
extern u16 gPadButtonPressed;   // held
extern u16 gBattleState;
extern s8 gPadStickX;
extern s8 gPadStickY;
#define STICK_DEADZONE 10

static int target = -1;
static int held = 0;
static int ticks = 0;
static f32 move_yaw;          // the direction the game steers Brian in while he faces the target
static int move_yaw_ready = 0;

int eo_lockon_target(void) {
    return held ? target : -1;
}

// atan2 for the facing angle (no math library in the mod): about 0.005 rad error.
static f32 approx_atan2(f32 y, f32 x) {
    const f32 pi = 3.14159265f;
    f32 ax = x < 0 ? -x : x;
    f32 ay = y < 0 ? -y : y;
    if (ax < 1e-6f && ay < 1e-6f) {
        return 0.0f;
    }
    f32 a = ax > ay ? ay / ax : ax / ay;
    f32 s = a * a;
    f32 r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
    if (ay > ax) {
        r = pi / 2 - r;
    }
    if (x < 0) {
        r = pi - r;
    }
    return y < 0 ? -r : r;
}

// sin for the facing direction: wrapped to -pi..pi, then a parabola fit (about 0.001 error).
static f32 approx_sin(f32 x) {
    const f32 pi = 3.14159265f;
    while (x > pi) {
        x -= 2 * pi;
    }
    while (x < -pi) {
        x += 2 * pi;
    }
    f32 y = 1.27323954f * x - 0.405284735f * x * (x < 0 ? -x : x);
    return 0.225f * (y * (y < 0 ? -y : y) - y) + y;
}

static f32 approx_cos(f32 x) {
    return approx_sin(x + 1.57079633f);
}

static int alive(int i) {
    return i >= 0 && i < MAX_ENEMIES && ENEMY_HP(i) > 0 && *(u32*)(ENEMY_SLOTS + i * ENEMY_SLOT_SIZE + 0x20) != 0;
}

// The first press picks the enemy closest to straight ahead; later presses go to the next enemy in slot order.
static int pick_target(void) {
    if (alive(target)) {
        for (int k = 1; k <= MAX_ENEMIES; k++) {
            int i = (target + k) % MAX_ENEMIES;
            if (alive(i)) {
                return i;
            }
        }
        return target;
    }
    f32 fx = approx_sin(BRIAN_YAW);
    f32 fz = approx_cos(BRIAN_YAW);
    int best = -1;
    f32 best_score = -1e9f;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!alive(i)) {
            continue;
        }
        f32 dx = ENEMY_POS(i)[0] - BRIAN_POS[0];
        f32 dz = ENEMY_POS(i)[2] - BRIAN_POS[2];
        f32 d = __builtin_sqrtf(dx * dx + dz * dz) + 1.0f;
        f32 score = (dx * fx + dz * fz) / d - d * 0.002f;   // in front first, then near
        if (score > best_score) {
            best_score = score;
            best = i;
        }
    }
    return best;
}

RECOMP_HOOK_RETURN("pad_read") void eo_lockon_input(void) {
    if (!(gBattleState & 0x1)) {
        held = 0;
        target = -1;
        return;
    }
    int pressed = (gPadButtonsNew & BTN_Z) != 0;
    held = (gPadButtonPressed & BTN_Z) != 0;
    gPadButtonsNew &= (u16)~BTN_Z;
    gPadButtonPressed &= (u16)~BTN_Z;
    if (pressed) {
        int next = pick_target();
        if (next >= 0) {
            target = next;
            eo_sfx(SFX_CONFIRM);
        }
    }
    if (!alive(target)) {
        target = -1;
    }
}

static f32 wrap_angle(f32 a) {
    const f32 pi = 3.14159265f;
    while (a > pi) {
        a -= 2 * pi;
    }
    while (a < -pi) {
        a += 2 * pi;
    }
    return a;
}

// The game turns Brian a little toward the stick each frame and walks him along his facing. Facing the target every
// frame would make him walk at it whichever way the stick points (playtest, 2026-10-21: "strafing moves me closer"),
// so while locked on the game steers its own direction (restored here) and he only shows the target facing. Only
// while the stick is pushed: standing still, he keeps the target facing for whatever he casts.
RECOMP_HOOK("player_update") void eo_lockon_before_move(void) {
    int moving = gPadStickX > STICK_DEADZONE || gPadStickX < -STICK_DEADZONE || gPadStickY > STICK_DEADZONE ||
                 gPadStickY < -STICK_DEADZONE;
    if (eo_lockon_target() < 0 || !moving) {
        move_yaw_ready = 0;
        return;
    }
    if (move_yaw_ready) {
        BRIAN_YAW = move_yaw;
    }
}

// Rotates the camera's eye round Brian toward the side opposite the target, so the camera looks past him at it.
static void turn_camera(f32 dx, f32 dz) {
    if (CAMERA_FLAGS & CAMERA_SPELL_SNAP) {
        return;
    }
    f32 ex = CAMERA_EYE_X - BRIAN_POS[0];
    f32 ez = CAMERA_EYE_Z - BRIAN_POS[2];
    f32 dist = __builtin_sqrtf(ex * ex + ez * ez);
    if (dist < 1.0f) {
        return;
    }
    f32 now = approx_atan2(ex, ez);
    f32 want = approx_atan2(-dx, -dz);
    f32 angle = now + wrap_angle(want - now) * CAMERA_TURN;
    CAMERA_EYE_X = BRIAN_POS[0] + approx_sin(angle) * dist;
    CAMERA_EYE_Z = BRIAN_POS[2] + approx_cos(angle) * dist;
}

// After Brian's update each frame: remember where the game is steering him, then face the target.
RECOMP_HOOK_RETURN("player_update") void eo_lockon_face(void) {
    int t = eo_lockon_target();
    if (t < 0) {
        return;
    }
    move_yaw = BRIAN_YAW;
    move_yaw_ready = 1;   // (cleared again by eo_lockon_before_move when the stick is let go)
    f32 dx = ENEMY_POS(t)[0] - BRIAN_POS[0];
    f32 dz = ENEMY_POS(t)[2] - BRIAN_POS[2];
    BRIAN_YAW = approx_atan2(dx, dz);
    turn_camera(dx, dz);
    if (++ticks % MARKER_EVERY == 0) {
        eo_fx_lockon_marker(t);
    }
}
