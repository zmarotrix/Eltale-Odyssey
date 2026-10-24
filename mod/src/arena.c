#include "eo.h"

// The battle arena as a place (Spell Trees doc, "Talent trees, take 3", 2026-10-23: "the 3D battle space"): things
// that sit on the ground and things that move foes around.
//
//   Ground fire  Burst and Blast leave it (Firewall: longer, fireballs too; Living Flame: foes on 3 stacks drop it).
//                A foe that crosses it or stands in it catches a Burn stack, once per enemy turn. Sun Heart: it
//                never goes out. Embers rise from it.
//   Ice floor    Water spells leave it with Ice Floor: foes moving across it slide further than they meant to.
//   Hailcloud    Hailstorm lingers: at the start of Brian's turns hail falls on foes under it.
//   Vortex       Whirlwind drifts toward the nearest foe for a few turns, cutting and pulling what it passes.
//   Pillars      Monolith: Rock and Boulder leave a stone pillar. It blocks enemy spells aimed past it, foes walk
//                around it, and with Quarry Brian can hurl it.
//   Stones       Mountain's Patience: each turn ended with A while Settled stores a stone circling Brian; stones
//                stop enemy spells, and his next Earth attack hurls them all.
//   Smoke        Burning foes smoke: their attacks on Brian can miss. Smoke (talent) spreads it to foes near a
//                burning one and hides Brian when he stands near one. Ash Cloud blinds a foe whose Burn ran out.
//   Knockback    Impact: Earth hits push foes away from Brian; a foe slammed into another foe, a pillar or the edge
//                of the arena takes the hit again (bosses too). Gravity Well pulls foes in before a hit.
//   Tremor       A turn ended with A while Settled slows foes near Brian (rank 2: and pushes them back).
//   Zephyr Step  Moving at least half his circle makes Brian harder to hit until his next turn.
//
// Positions are battle data x, y, z (y up); only x and z matter for reach.

#define MAX_ENEMIES 6
#define ENEMY_SLOTS 0x8007C998
#define ENEMY_SLOT_SIZE 0x128
#define ENEMY_BATTLE_DATA(i) ((u8*)(ENEMY_SLOTS + (i) * ENEMY_SLOT_SIZE + 0x24))
#define ENEMY_HP(i) (*(u16*)(ENEMY_SLOTS + (i) * ENEMY_SLOT_SIZE + 0x0A))
#define BRIAN_POS ((f32*)0x8007BACC)
#define BRIAN_MOVE_RANGE (*(f32*)0x8007BAC4)        // gPlayer + 0x0C: Agility x 0.2857 + 17
#define BRIAN_MOVE_MULTIPLIER (*(f32*)0x8007BBC8)
#define STATS(i) (*(u8**)(ENEMY_BATTLE_DATA(i) + 0x68))
#define STATS_RANGE 0x90                             // f32 move range multiplier
#define BATTLE_ACTIVE 0x1
#define BATTLE_ENEMY_TURN 0x2
#define BATTLE_BOSS 0x100
#define CURRENT_BOSS (*(s32*)0x8008C570)
#define BOSS_ARENA_RADIUS ((f32*)0x8004D28C)         // one per boss (research/enemies)
#define MONSTER_ARENA_RADIUS 150.0f

#define COLOR_YELLOW 6
#define COLOR_WHITE 7
#define COLOR_SCARLET 8
#define COLOR_PALE_GOLD 13
#define COLOR_ORANGE 15

#define FIRE_RADIUS 28.0f
#define WIDE_FIRE_RADIUS 42.0f
#define ICE_RADIUS 22.0f
#define HAIL_RADIUS 45.0f
#define VORTEX_RADIUS 35.0f
#define VORTEX_DRIFT 30.0f
#define PILLAR_RADIUS 5.0f
#define PILLAR_HEIGHT 24.0f
#define PILLAR_TURNS 2
#define BODY_RADIUS 12.0f       // two foes this close have collided
#define SMOKE_MISS 20           // percent an attack from a burning (smoking) foe misses
#define SMOKE_REACH 25.0f       // Smoke talent: per rank, how far smoke reaches from a burning foe
#define SMOKE_HIDE 10           // Smoke talent: per rank, misses on Brian while he stands in smoke
#define ZEPHYR_MISS 20          // Zephyr Step: per rank
#define TREMOR_RADIUS 60.0f
#define TREMOR_PUSH 20.0f
#define QUARRY_REACH 22.0f
#define MAX_STONES 5
#define STONE_ORBIT 14.0f

extern u16 gBattleState;
s32 get_rand(s32 n);

enum { Z_NONE, Z_FIRE, Z_ICE, Z_HAIL, Z_VORTEX };

typedef struct {
    u8 type;
    u8 turns;
    f32 x, y, z, r;
} Zone;

#define MAX_ZONES 16
static Zone zones[MAX_ZONES];

typedef struct {
    u8 turns;
    f32 x, y, z;
} Pillar;

#define MAX_PILLARS 4
static Pillar pillars[MAX_PILLARS];

static u8 stones;                    // Mountain's Patience
static u8 blind[MAX_ENEMIES];        // Ash Cloud: turns left
static u8 caught[MAX_ENEMIES];       // took a stack from ground fire this enemy turn
static u8 slowed[MAX_ENEMIES];       // Tremor
static f32 last_x[MAX_ENEMIES], last_z[MAX_ENEMIES];
static f32 walked[MAX_ENEMIES];      // since the last Living Flame drop
static f32 center_x, center_z, edge;
static f32 brian_start_x, brian_start_z;
static u8 zephyr;
static u32 frames;

static int in_battle(void) {
    return (gBattleState & BATTLE_ACTIVE) != 0;
}

static int alive(int i) {
    return i >= 0 && i < MAX_ENEMIES && ENEMY_HP(i) > 0;
}

static f32* pos(int i) {
    return (f32*)ENEMY_BATTLE_DATA(i);
}

static f32 sq(f32 v) {
    return v * v;
}

static f32 dist2(f32 ax, f32 az, f32 bx, f32 bz) {
    return sq(ax - bx) + sq(az - bz);
}

// Distance squared from point p to the segment a-b, on the ground.
static f32 seg_dist2(f32 ax, f32 az, f32 bx, f32 bz, f32 px, f32 pz) {
    f32 dx = bx - ax;
    f32 dz = bz - az;
    f32 len = dx * dx + dz * dz;
    f32 t = len > 0.01f ? ((px - ax) * dx + (pz - az) * dz) / len : 0.0f;
    t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
    return dist2(ax + dx * t, az + dz * t, px, pz);
}

// ---- Battle start ----

RECOMP_HOOK_RETURN("battle_build_turn_order") void eo_arena_on_battle_start(void) {
    for (int i = 0; i < MAX_ZONES; i++) {
        zones[i].type = Z_NONE;
    }
    for (int i = 0; i < MAX_PILLARS; i++) {
        pillars[i].turns = 0;
    }
    stones = 0;
    zephyr = 0;
    // The arena: centered between Brian and the foes as the fight opens; bosses have their own radius.
    f32 sx = BRIAN_POS[0];
    f32 sz = BRIAN_POS[2];
    int n = 1;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        blind[i] = caught[i] = slowed[i] = 0;
        walked[i] = 0.0f;
        if (alive(i)) {
            sx += pos(i)[0];
            sz += pos(i)[2];
            n++;
            last_x[i] = pos(i)[0];
            last_z[i] = pos(i)[2];
        }
    }
    center_x = sx / n;
    center_z = sz / n;
    edge = MONSTER_ARENA_RADIUS;
    if ((gBattleState & BATTLE_BOSS) && CURRENT_BOSS >= 1 && CURRENT_BOSS <= 8) {
        edge = BOSS_ARENA_RADIUS[CURRENT_BOSS - 1];
    }
    brian_start_x = BRIAN_POS[0];
    brian_start_z = BRIAN_POS[2];
}

// ---- Zones ----

static void add_zone(int type, f32 x, f32 y, f32 z, f32 r, int turns) {
    int slot = 0;
    for (int i = 0; i < MAX_ZONES; i++) {
        if (zones[i].type == Z_NONE) {
            slot = i;
            break;
        }
        if (zones[i].turns < zones[slot].turns) {
            slot = i;   // full: the one closest to running out goes
        }
    }
    zones[slot] = (Zone){ (u8)type, (u8)(turns > 0 ? turns : 1), x, y, z, r };
}

void eo_arena_ground_fire(f32 x, f32 y, f32 z, int wide, int turns) {
    add_zone(Z_FIRE, x, y, z, wide ? WIDE_FIRE_RADIUS : FIRE_RADIUS, turns + eo_talent(TALENT_FIREWALL));
}

void eo_arena_ice_floor(f32 x, f32 y, f32 z) {
    int rank = eo_talent(TALENT_ICE_FLOOR);
    if (rank > 0) {
        add_zone(Z_ICE, x, y, z, ICE_RADIUS + 8.0f * rank, 2);
    }
}

void eo_arena_hailcloud(f32 x, f32 y, f32 z) {
    int rank = eo_talent(TALENT_HAILCLOUD);
    if (rank > 0) {
        add_zone(Z_HAIL, x, y, z, HAIL_RADIUS, rank);
    }
}

void eo_arena_vortex(f32 x, f32 y, f32 z) {
    int rank = eo_talent(TALENT_VORTEX);
    if (rank > 0) {
        add_zone(Z_VORTEX, x, y, z, VORTEX_RADIUS, rank);
    }
}

static int in_zone(int type, f32 x, f32 z) {
    for (int i = 0; i < MAX_ZONES; i++) {
        if (zones[i].type == type && dist2(x, z, zones[i].x, zones[i].z) <= sq(zones[i].r)) {
            return 1;
        }
    }
    return 0;
}

int eo_arena_in_fire(f32 x, f32 z) {
    return in_zone(Z_FIRE, x, z);
}

// Does the line from a to b pass over a zone of this type (Kiln, Crosswind)?
static int crosses(int type, f32 ax, f32 az, f32 bx, f32 bz) {
    for (int i = 0; i < MAX_ZONES; i++) {
        if (zones[i].type == type && seg_dist2(ax, az, bx, bz, zones[i].x, zones[i].z) <= sq(zones[i].r)) {
            return 1;
        }
    }
    return 0;
}

int eo_arena_fire_between(f32 ax, f32 az, f32 bx, f32 bz) {
    return crosses(Z_FIRE, ax, az, bx, bz);
}

int eo_arena_ice_between(f32 ax, f32 az, f32 bx, f32 bz) {
    if (crosses(Z_ICE, ax, az, bx, bz)) {
        return 1;
    }
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (alive(i) && eo_combat_frozen(i) && seg_dist2(ax, az, bx, bz, pos(i)[0], pos(i)[2]) <= sq(BODY_RADIUS) &&
            dist2(bx, bz, pos(i)[0], pos(i)[2]) > 1.0f) {
            return 1;
        }
    }
    return 0;
}

// ---- Pillars and stones ----

void eo_arena_pillar(f32 x, f32 y, f32 z) {
    int slot = 0;
    for (int i = 0; i < MAX_PILLARS; i++) {
        if (pillars[i].turns == 0) {
            slot = i;
            break;
        }
        if (pillars[i].turns < pillars[slot].turns) {
            slot = i;
        }
    }
    pillars[slot] = (Pillar){ PILLAR_TURNS, x, y, z };
}

// Quarry: a pillar Brian stands next to is hurled with his Earth spell (returns 1 and removes it).
int eo_arena_take_pillar_near_brian(void) {
    for (int i = 0; i < MAX_PILLARS; i++) {
        if (pillars[i].turns > 0 && dist2(BRIAN_POS[0], BRIAN_POS[2], pillars[i].x, pillars[i].z) <= sq(QUARRY_REACH)) {
            pillars[i].turns = 0;
            return 1;
        }
    }
    return 0;
}

int eo_arena_take_stones(void) {
    int n = stones;
    stones = 0;
    return n;
}

// An enemy spell from `attacker` to Brian: does a pillar, a frozen statue or an orbiting stone stop it?
int eo_arena_blocks_shot(void* attacker) {
    int from = -1;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (attacker == ENEMY_BATTLE_DATA(i)) {
            from = i;
        }
    }
    if (from < 0) {
        return 0;
    }
    f32 ax = pos(from)[0];
    f32 az = pos(from)[2];
    f32 bx = BRIAN_POS[0];
    f32 bz = BRIAN_POS[2];
    if (stones > 0) {
        stones--;
        return 1;
    }
    for (int i = 0; i < MAX_PILLARS; i++) {
        if (pillars[i].turns > 0 && seg_dist2(ax, az, bx, bz, pillars[i].x, pillars[i].z) <= sq(PILLAR_RADIUS + 3.0f)) {
            return 1;
        }
    }
    if (eo_talent(TALENT_FROZEN_STATUE) >= 2) {
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (i != from && alive(i) && eo_combat_frozen(i) &&
                seg_dist2(ax, az, bx, bz, pos(i)[0], pos(i)[2]) <= sq(BODY_RADIUS)) {
                return 1;
            }
        }
    }
    return 0;
}

// ---- Smoke, blinding, Zephyr Step: does an attack on Brian miss? ----

static int burning(int i) {
    return alive(i) && eo_status_burn_total(i) > 0;
}

int eo_arena_miss_percent(void* attacker) {
    int from = -1;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (attacker == ENEMY_BATTLE_DATA(i)) {
            from = i;
        }
    }
    int miss = zephyr ? ZEPHYR_MISS * eo_talent(TALENT_ZEPHYR_STEP) : 0;
    if (from < 0) {
        return miss;
    }
    if (blind[from]) {
        return 100;
    }
    int smoke = eo_talent(TALENT_SMOKE);
    f32 reach = SMOKE_REACH * smoke;
    int in_smoke = burning(from);
    int brian_hidden = 0;
    for (int i = 0; i < MAX_ENEMIES && smoke > 0; i++) {
        if (!burning(i)) {
            continue;
        }
        if (dist2(pos(i)[0], pos(i)[2], pos(from)[0], pos(from)[2]) <= sq(reach)) {
            in_smoke = 1;
        }
        if (dist2(pos(i)[0], pos(i)[2], BRIAN_POS[0], BRIAN_POS[2]) <= sq(reach)) {
            brian_hidden = 1;
        }
    }
    return miss + (in_smoke ? SMOKE_MISS : 0) + (brian_hidden ? SMOKE_HIDE * smoke : 0);
}

void eo_arena_blind(int slot) {
    if (slot >= 0 && slot < MAX_ENEMIES && eo_talent(TALENT_ASH_CLOUD)) {
        blind[slot] = 1;
    }
}

// ---- Knockback and pulls ----

// Pushes a foe `dist` away from (fx, fz). If it slams into another foe, a pillar or the arena's edge, it takes
// `hit` again (and so does the foe it hit).
void eo_arena_knock(int slot, f32 fx, f32 fz, f32 dist, u32 hit) {
    if (!alive(slot) || dist <= 0.0f) {
        return;
    }
    f32* p = pos(slot);
    f32 dx = p[0] - fx;
    f32 dz = p[2] - fz;
    f32 len = __builtin_sqrtf(dx * dx + dz * dz);
    if (len < 0.1f) {
        return;
    }
    dx /= len;
    dz /= len;
    int slammed = 0;
    int other = -1;
    // Step along the push; stop at the first thing in the way.
    for (f32 moved = 0.0f; moved < dist; moved += 2.0f) {
        f32 nx = p[0] + dx * 2.0f;
        f32 nz = p[2] + dz * 2.0f;
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (i != slot && alive(i) && dist2(nx, nz, pos(i)[0], pos(i)[2]) <= sq(BODY_RADIUS)) {
                other = i;
            }
        }
        for (int i = 0; i < MAX_PILLARS; i++) {
            if (pillars[i].turns > 0 && dist2(nx, nz, pillars[i].x, pillars[i].z) <= sq(PILLAR_RADIUS + 4.0f)) {
                slammed = 1;
            }
        }
        if (dist2(nx, nz, center_x, center_z) > sq(edge)) {
            slammed = 1;   // the arena wall
        }
        if (slammed || other >= 0) {
            break;
        }
        p[0] = nx;
        p[2] = nz;
    }
    if (slammed || other >= 0) {
        eo_fx_shield_hit(slot, 0);
        eo_sfx(0x27);
        eo_status_fixed_damage(slot, (u16)(hit > 0xFFFF ? 0xFFFF : hit));
        if (other >= 0) {
            eo_status_fixed_damage(other, (u16)(hit > 0xFFFF ? 0xFFFF : hit));
        }
    }
}

// Gravity Well: foes within `radius` of a point drift `share` of the way to it.
void eo_arena_pull(f32 x, f32 z, f32 radius, f32 share) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (alive(i) && dist2(pos(i)[0], pos(i)[2], x, z) <= sq(radius)) {
            pos(i)[0] += (x - pos(i)[0]) * share;
            pos(i)[2] += (z - pos(i)[2]) * share;
        }
    }
}

// ---- Turns ----

// Brian passed (ended a turn with A, no spell or swing) while Settled: Tremor and Mountain's Patience.
void eo_arena_settled_pass(void) {
    int tremor = eo_talent(TALENT_TREMOR);
    if (tremor > 0) {
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (!alive(i) || dist2(pos(i)[0], pos(i)[2], BRIAN_POS[0], BRIAN_POS[2]) > sq(TREMOR_RADIUS)) {
                continue;
            }
            u8* st = STATS(i);
            if (st != 0 && !eo_spells_frosted(i)) {
                *(f32*)(st + STATS_RANGE) = 0.5f;
                slowed[i] = 1;
            }
            if (tremor >= 2) {
                eo_arena_knock(i, BRIAN_POS[0], BRIAN_POS[2], TREMOR_PUSH, 0);
            }
            eo_fx_shield_hit(i, 0);
        }
    }
    if (eo_talent(TALENT_MOUNTAINS_PATIENCE) && stones < MAX_STONES) {
        stones++;
    }
}

// The start of Brian's turn (spells.c): zones age, hail falls, the vortex drifts, Bonfire, Zephyr Step resets.
void eo_arena_brian_turn(void) {
    int forever = eo_talent(TALENT_SUN_HEART);
    for (int z = 0; z < MAX_ZONES; z++) {
        Zone* zn = &zones[z];
        if (zn->type == Z_HAIL) {
            for (int i = 0; i < MAX_ENEMIES; i++) {
                if (alive(i) && dist2(pos(i)[0], pos(i)[2], zn->x, zn->z) <= sq(zn->r)) {
                    eo_status_fixed_damage(i, (u16)(eo_spells_estimate(2, 70) * 2 + 1));
                    if (get_rand(100) < 40) {
                        eo_spells_chill(i, 1);
                    }
                }
            }
        } else if (zn->type == Z_VORTEX) {
            // Drift toward the nearest foe, cutting and pulling what it passes.
            int near = -1;
            f32 best = 1e9f;
            for (int i = 0; i < MAX_ENEMIES; i++) {
                f32 d = alive(i) ? dist2(pos(i)[0], pos(i)[2], zn->x, zn->z) : 1e9f;
                if (d < best) {
                    best = d;
                    near = i;
                }
            }
            if (near >= 0) {
                f32 d = __builtin_sqrtf(best);
                f32 step = d < VORTEX_DRIFT ? d : VORTEX_DRIFT;
                if (d > 0.1f) {
                    zn->x += (pos(near)[0] - zn->x) / d * step;
                    zn->z += (pos(near)[2] - zn->z) / d * step;
                }
                eo_arena_pull(zn->x, zn->z, zn->r * 1.5f, 0.4f);
                for (int i = 0; i < MAX_ENEMIES; i++) {
                    if (alive(i) && dist2(pos(i)[0], pos(i)[2], zn->x, zn->z) <= sq(zn->r)) {
                        eo_status_fixed_damage(i, (u16)(eo_spells_estimate(3, 100) * 2 + 1));
                    }
                }
            }
        }
        if (zn->type != Z_NONE && !(zn->type == Z_FIRE && forever) && --zn->turns == 0) {
            zn->type = Z_NONE;
        }
    }
    for (int i = 0; i < MAX_PILLARS; i++) {
        if (pillars[i].turns > 0) {
            pillars[i].turns--;
        }
    }
    // Bonfire: starting a turn standing in his own fire.
    int bonfire = eo_talent(TALENT_BONFIRE);
    if (bonfire > 0 && in_zone(Z_FIRE, BRIAN_POS[0], BRIAN_POS[2])) {
        eo_combat_gain_charge(1);
        if (bonfire >= 2) {
            eo_spells_heal(5);
        }
    }
    for (int i = 0; i < MAX_ENEMIES; i++) {
        caught[i] = 0;
    }
    zephyr = 0;
    brian_start_x = BRIAN_POS[0];
    brian_start_z = BRIAN_POS[2];
}

// The end of Brian's turn: did he move at least half his circle (Zephyr Step)?
static void brian_turn_ends(void) {
    f32 range = BRIAN_MOVE_RANGE * BRIAN_MOVE_MULTIPLIER;
    zephyr = eo_talent(TALENT_ZEPHYR_STEP) > 0 &&
             dist2(BRIAN_POS[0], BRIAN_POS[2], brian_start_x, brian_start_z) >= sq(range * 0.5f);
}

// The end of an enemy's turn: Sun Heart burns foes near Brian; Ash Cloud and Tremor wear off.
static void enemy_turn_ends(int i) {
    if (blind[i] > 0) {
        blind[i]--;
    }
    if (slowed[i]) {
        slowed[i] = 0;
        u8* st = STATS(i);
        if (st != 0 && !eo_spells_frosted(i)) {
            *(f32*)(st + STATS_RANGE) = 1.0f;
        }
    }
    if (alive(i) && eo_talent(TALENT_SUN_HEART) &&
        dist2(pos(i)[0], pos(i)[2], BRIAN_POS[0], BRIAN_POS[2]) <= sq(FIRE_RADIUS * 1.5f)) {
        eo_spells_burn(i, eo_spells_ember_stack());
    }
}

RECOMP_HOOK("battle_end_turn") void eo_arena_on_turn_end(u8* fighter) {
    if (!in_battle()) {
        return;
    }
    if (!(gBattleState & BATTLE_ENEMY_TURN)) {
        brian_turn_ends();
        return;
    }
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (fighter == ENEMY_BATTLE_DATA(i)) {
            enemy_turn_ends(i);
        }
    }
}

// ---- Every frame: foes walking through things, and what the arena looks like ----

// sin and cos for effects (the mod has no math library): Bhaskara's approximation, good to about 0.002.
static f32 approx_sin(f32 a) {
    const f32 pi = 3.14159265f;
    while (a > pi) {
        a -= 2.0f * pi;
    }
    while (a < -pi) {
        a += 2.0f * pi;
    }
    f32 s = a < 0.0f ? -1.0f : 1.0f;
    f32 x = a * s;
    return s * 16.0f * x * (pi - x) / (5.0f * pi * pi - 4.0f * x * (pi - x));
}

static f32 approx_cos(f32 a) {
    return approx_sin(a + 1.5707963f);
}

static f32 rnd(void) {   // -1..1
    return (f32)(get_rand(201) - 100) / 100.0f;
}

static void zone_looks(Zone* zn) {
    f32 p[3];
    f32 v[3] = { 0.0f, 0.0f, 0.0f };
    switch (zn->type) {
        case Z_FIRE:
            // A patch of flames: two embers a frame, spread over the patch.
            for (int k = 0; k < 2; k++) {
                p[0] = zn->x + rnd() * zn->r * 0.85f;
                p[1] = zn->y + 1.0f;
                p[2] = zn->z + rnd() * zn->r * 0.85f;
                v[1] = 0.4f + (f32)get_rand(50) / 100.0f;
                eo_fx_particle(p, v, 1.8f, get_rand(3) ? COLOR_ORANGE : COLOR_SCARLET, 14 + get_rand(10), 1);
            }
            break;
        case Z_ICE:
            eo_fx_box(zn->x, zn->y - 0.5f, zn->z, zn->r * 0.75f, 1.0f, 0xC8EBFF70);
            if (frames % 10 == 0) {
                p[0] = zn->x + rnd() * zn->r * 0.7f;
                p[1] = zn->y + 1.5f;
                p[2] = zn->z + rnd() * zn->r * 0.7f;
                eo_fx_particle(p, v, 1.2f, COLOR_WHITE, 12, 0);
            }
            break;
        case Z_HAIL:
            if (frames % 2 == 0) {
                p[0] = zn->x + rnd() * zn->r;
                p[1] = zn->y + 45.0f;
                p[2] = zn->z + rnd() * zn->r;
                v[1] = -2.5f;
                eo_fx_particle(p, v, 1.0f, COLOR_WHITE, 20, 0);
            }
            break;
        case Z_VORTEX:
            if (frames % 2 == 0) {
                f32 a = (f32)(frames % 60) * 0.1047f;
                f32 s = approx_sin(a);
                f32 c = approx_cos(a);
                p[0] = zn->x + s * zn->r * 0.6f;
                p[1] = zn->y + 5.0f + (f32)get_rand(20);
                p[2] = zn->z + c * zn->r * 0.6f;
                v[0] = c * 1.2f;
                v[2] = -s * 1.2f;
                v[1] = 0.3f;
                eo_fx_particle(p, v, 1.3f, COLOR_WHITE, 14, 0);
            }
            break;
    }
}

RECOMP_HOOK_RETURN("frame_build") void eo_arena_frame(void) {
    if (!in_battle()) {
        return;
    }
    frames++;
    int enemy_turn = (gBattleState & BATTLE_ENEMY_TURN) != 0;
    int living = eo_talent(TALENT_LIVING_FLAME);
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!alive(i)) {
            continue;
        }
        f32* p = pos(i);
        f32 mx = p[0] - last_x[i];
        f32 mz = p[2] - last_z[i];
        f32 step2 = mx * mx + mz * mz;
        if (enemy_turn && step2 > 0.0001f && step2 < 100.0f) {
            // Ice: it keeps sliding the way it was going.
            if (in_zone(Z_ICE, p[0], p[2])) {
                p[0] += mx * 0.8f;
                p[2] += mz * 0.8f;
            }
            // Living Flame: a foe on 3 or more stacks drops fire as it walks.
            walked[i] += __builtin_sqrtf(step2);
            if (living > 0 && eo_status_burn_stacks(i) >= 3 && walked[i] > 25.0f) {
                walked[i] = 0.0f;
                add_zone(Z_FIRE, p[0], p[1], p[2], FIRE_RADIUS * 0.7f, living);
            }
        }
        // Ground fire: crossing or standing in it, once per enemy turn.
        if (enemy_turn && !caught[i] && in_zone(Z_FIRE, p[0], p[2])) {
            caught[i] = 1;
            eo_spells_burn(i, eo_spells_ember_stack());
        }
        // Pillars and frozen statues are solid.
        for (int k = 0; k < MAX_PILLARS; k++) {
            if (pillars[k].turns == 0) {
                continue;
            }
            f32 dx = p[0] - pillars[k].x;
            f32 dz = p[2] - pillars[k].z;
            f32 d = __builtin_sqrtf(dx * dx + dz * dz);
            if (d < PILLAR_RADIUS + 6.0f && d > 0.01f) {
                p[0] = pillars[k].x + dx / d * (PILLAR_RADIUS + 6.0f);
                p[2] = pillars[k].z + dz / d * (PILLAR_RADIUS + 6.0f);
            }
        }
        if (eo_talent(TALENT_FROZEN_STATUE) > 0) {
            for (int k = 0; k < MAX_ENEMIES; k++) {
                if (k == i || !alive(k) || !eo_combat_frozen(k) || eo_combat_frozen(i)) {
                    continue;
                }
                f32 dx = p[0] - pos(k)[0];
                f32 dz = p[2] - pos(k)[2];
                f32 d = __builtin_sqrtf(dx * dx + dz * dz);
                if (d < BODY_RADIUS && d > 0.01f) {
                    p[0] = pos(k)[0] + dx / d * BODY_RADIUS;
                    p[2] = pos(k)[2] + dz / d * BODY_RADIUS;
                }
            }
        }
        last_x[i] = p[0];
        last_z[i] = p[2];
        // Smoke rising from burning foes.
        if (burning(i) && frames % 6 == (u32)i % 6) {
            f32 sp[3] = { p[0] + rnd() * 4.0f, p[1] + 10.0f, p[2] + rnd() * 4.0f };
            f32 sv[3] = { rnd() * 0.2f, 0.4f, rnd() * 0.2f };
            eo_fx_particle(sp, sv, 1.6f + 0.3f * eo_talent(TALENT_SMOKE), COLOR_PALE_GOLD, 22, 0);
        }
    }
    for (int z = 0; z < MAX_ZONES; z++) {
        zone_looks(&zones[z]);
    }
    for (int k = 0; k < MAX_PILLARS; k++) {
        if (pillars[k].turns > 0) {
            eo_fx_box(pillars[k].x, pillars[k].y - 2.0f, pillars[k].z, PILLAR_RADIUS, PILLAR_HEIGHT, 0x9C8C78F0);
        }
    }
    for (int s = 0; s < stones; s++) {
        f32 a = (f32)frames * 0.05f + (f32)s * 1.2566f;
        eo_fx_box(BRIAN_POS[0] + approx_sin(a) * STONE_ORBIT, BRIAN_POS[1] + 8.0f,
                  BRIAN_POS[2] + approx_cos(a) * STONE_ORBIT, 2.5f, 5.0f, 0x8C7C6CF0);
    }
}
