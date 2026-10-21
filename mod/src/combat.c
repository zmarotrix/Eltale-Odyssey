#include "eo.h"

// The combat system (game design document, "Combat resource"): Spirit Charge, plus enemy shields and Break.
//
// Spirit Charge: Brian starts a battle with 1 charge and gains 1 at the start of each of his turns (max 5),
// except the turn after he spent some. His first weakness hit in a turn gives +1. Before casting, D-pad up/down
// picks a Boost of 0-3 (no more than his charge); it's spent when the spell launches:
//   Fire  +1 Burn stack per charge (status.c); at 3, the hit first ignites the target's existing Burn
//   Earth +40% damage per charge
//   Water +30% healing per charge; a damaging Water spell Freezes its target for 1 turn per charge
//   Wind  +2 shots per charge
//
// Shields and Break: each enemy has shields (2-4 by toughness, bosses 6-10) and a weakness, the element
// opposite its own (the game's own x1.25 rule), hidden until hit. Each weakness hit from Brian removes a
// shield; at 0 the enemy is Broken: it loses its next turn (the game's own Restriction status) and takes 50%
// more damage until then. Then its shields come back; a boss's grow by one each Break.

#define MAX_ENEMIES 6
#define MAX_CHARGE 5
#define MAX_BOOST 3
#define START_CHARGE 1
#define BROKEN_BONUS_PERCENT 50
#define NO_WEAKNESS 0xFF

#define ENEMY_SLOTS 0x8007C998
#define ENEMY_SLOT_SIZE 0x128
#define ENEMY_SLOT(i) ((u8*)(ENEMY_SLOTS + (i) * ENEMY_SLOT_SIZE))
#define ENEMY_HP(i) (*(u16*)(ENEMY_SLOT(i) + 0x0A))
#define ENEMY_RECORD(i) (*(u8**)(ENEMY_SLOT(i) + 0x20))
#define ENEMY_BATTLE_DATA(i) (ENEMY_SLOT(i) + 0x24)
#define RECORD_MODEL_INDEX 0x02  // also the index into the name table
#define RECORD_MAX_HP 0x06
#define RECORD_ELEMENT 0x26

// Status block (battle data +0x68): u16 status bits, then one duration byte per status at +2.
#define STATUS_RESTRICTION_BIT 0x8
#define STATUS_RESTRICTION_DURATION 0x5

#define BATTLE_ACTIVE     0x1
#define BATTLE_ENEMY_TURN 0x2
#define BATTLE_BOSS       0x100
#define CURRENT_BOSS (*(s32*)0x8008C570)
#define NAME_TABLE (*(char***)0x8007D0AC)

#define SPELL_STAT_TABLES ((u8**)0x800C1B14)
#define SPELL_STAT_SIZE 0x44
#define SPELL_BASE   0x0C
#define SPELL_TYPE   0x10
#define SPELL_SHOTS  0x20
#define SPELL_TYPE_DAMAGE 0
#define SPELL_TYPE_HEAL 1

enum { FIRE, EARTH, WATER, WIND };

s32 enemy_take_damage(s32 slot, u16 power, u16 element, u16 flags, void* attacker);
void damage_popup_spawn(void* battle_data, s32 kind, s32 amount);
s32 get_rand(s32 n);  // 0..n-1
extern u16 gBattleState;
extern u16 gPadButtonsNew;

typedef struct {
    u8 shields;
    u8 max_shields;
    u8 weakness;
    u8 revealed;
    u8 broken;
} Guard;

static Guard guards[MAX_ENEMIES];
static u8 charge = 0;
static u8 boost = 0;           // chosen for the next cast
static u8 spent_last_turn = 0;
static u8 spent_this_turn = 0;
static u8 weakness_bonus_given = 0;
static u8 reflected_this_battle = 0;  // Mirror Shard

// The boost of the spell being cast right now (0 when none), and what was changed for it.
static u8 active_boost = 0;
static u8 active_school = 0;
static u8* active_spell = 0;
static u16 saved_base;
static u16 saved_shots;

static int in_battle(void) {
    return (gBattleState & BATTLE_ACTIVE) != 0;
}

static int brians_turn(void) {
    return in_battle() && !(gBattleState & BATTLE_ENEMY_TURN);
}

static int is_enemy_battle_data(void* p) {
    u32 a = (u32)p;
    return a >= ENEMY_SLOTS && a < ENEMY_SLOTS + MAX_ENEMIES * ENEMY_SLOT_SIZE;
}

int eo_combat_active_boost(void) {
    return active_boost;
}

int eo_combat_active_school(void) {
    return active_spell != 0 ? active_school : -1;
}

// ---- Battle start ----

static u8 shields_for(u8* record, int boss) {
    if (boss) {
        return (u8)(6 + (CURRENT_BOSS >= 0 && CURRENT_BOSS < 8 ? CURRENT_BOSS / 2 : 0));
    }
    u16 hp = *(u16*)(record + RECORD_MAX_HP);
    return hp < 60 ? 2 : hp < 200 ? 3 : 4;
}

RECOMP_HOOK("battle_build_turn_order") void eo_combat_on_battle_start(void) {
    int boss = (gBattleState & BATTLE_BOSS) != 0;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        Guard* g = &guards[i];
        *g = (Guard){ 0 };
        u8* record = ENEMY_RECORD(i);
        if (record == 0 || ENEMY_HP(i) == 0) {
            continue;
        }
        u16 element = *(u16*)(record + RECORD_ELEMENT);
        g->weakness = element == FIRE ? WATER : element == WATER ? FIRE : element == EARTH ? WIND :
                      element == WIND ? EARTH : NO_WEAKNESS;
        g->max_shields = g->shields = shields_for(record, boss);
    }
    charge = START_CHARGE;
    boost = 0;
    spent_last_turn = spent_this_turn = 0;
    weakness_bonus_given = 0;
    active_boost = 0;
    active_spell = 0;
    reflected_this_battle = 0;
}

// ---- Break ----

static void break_enemy(int i) {
    Guard* g = &guards[i];
    g->broken = 1;
    u8* status = *(u8**)(ENEMY_BATTLE_DATA(i) + 0x68);
    *(u16*)status |= STATUS_RESTRICTION_BIT;   // its next turn is skipped by the game itself
    status[STATUS_RESTRICTION_DURATION] = 1;
    sfx_play(SFX_CONFIRM, 0xFF, 0x40);
}

static void freeze_enemy(int i, int turns) {
    u8* status = *(u8**)(ENEMY_BATTLE_DATA(i) + 0x68);
    if (gBattleState & BATTLE_BOSS) {
        turns = 1;  // bosses resist control effects
    }
    *(u16*)status |= STATUS_RESTRICTION_BIT;
    if (status[STATUS_RESTRICTION_DURATION] < turns) {
        status[STATUS_RESTRICTION_DURATION] = (u8)turns;
    }
}

// Hits from Brian: remember the target's HP, count weakness hits. The staff calls enemy_take_damage with
// flags 0x8004; burn ticks and reflected hits use plain 0x8000 and are ignored here.
#define STAFF_FLAGS 0x8004
enum { HIT_NONE, HIT_SPELL, HIT_STAFF };
static int hit_slot = -1;
static int hit_kind = HIT_NONE;
static u16 hit_hp;
static u16 hit_element;
static u8 hit_was_broken;
static u16 last_brian_spell = 0xFFFF;

RECOMP_HOOK("enemy_take_damage") void eo_combat_before_enemy_damage(s32 slot, u16 power, u16 element, u16 flags, void* attacker) {
    hit_slot = -1;
    hit_kind = HIT_NONE;
    if (!in_battle() || slot < 0 || slot >= MAX_ENEMIES || power == 0 || is_enemy_battle_data(attacker)) {
        return;
    }
    if (flags & 0x8000) {
        if (flags != STAFF_FLAGS) {
            return;
        }
        hit_kind = HIT_STAFF;
    } else {
        hit_kind = HIT_SPELL;
    }
    hit_slot = slot;
    hit_hp = ENEMY_HP(slot);
    hit_element = element;
    Guard* g = &guards[slot];
    hit_was_broken = g->broken;
    if (hit_kind != HIT_SPELL || g->weakness == NO_WEAKNESS || element != g->weakness || g->broken) {
        return;
    }
    g->revealed = 1;
    if (!weakness_bonus_given && charge < MAX_CHARGE) {
        charge++;
        weakness_bonus_given = 1;
    }
    if (g->shields > 0 && --g->shields == 0) {
        break_enemy(slot);
    }
}

RECOMP_HOOK_RETURN("enemy_take_damage") void eo_combat_after_enemy_damage(void) {
    int slot = hit_slot;
    hit_slot = -1;
    if (slot < 0) {
        return;
    }
    u16 hp = ENEMY_HP(slot);
    u32 hit = hp < hit_hp ? (u32)(hit_hp - hp) : 0;
    if (hit == 0) {
        return;
    }
    // Extra damage on top of the hit: a Broken foe takes 50% more, the staff adds its flat bonus, and a staff
    // that favours the spell's element adds 10%.
    u32 extra = 0;
    if (hit_was_broken) {
        extra += hit * BROKEN_BONUS_PERCENT / 100;
    }
    if (hit_kind == HIT_STAFF) {
        extra += (u32)eo_equip_staff_bonus();
    } else if (hit_element < 4) {
        extra += hit * (u32)eo_equip_element_percent(hit_element) / 100;
    }
    if (extra > 0 && hp > 0) {
        ENEMY_HP(slot) = extra >= hp ? 0 : (u16)(hp - extra);
        damage_popup_spawn(ENEMY_BATTLE_DATA(slot), 0, (s32)extra);
    }
    // Ember Staff: staff hits may set the foe burning.
    if (hit_kind == HIT_STAFF && ENEMY_HP(slot) > 0 && get_rand(100) < eo_equip_staff_burn_chance()) {
        eo_status_add_burn(slot, (u16)(hit * 30 / 100 + 1));
    }
    // Leech Pillar: Water Pillar (Lv1-3) heals Brian for a quarter of the damage it deals.
    if (hit_kind == HIT_SPELL && eo_equipment_worn(EQ_LEECH_PILLAR) &&
        (last_brian_spell == 0x200 || last_brian_spell == 0x201 || last_brian_spell == 0x204)) {
        u16* stats = (u16*)gPlayerMainData;
        u32 healed = stats[2] + (hit + extra) / 4;
        stats[2] = (u16)(healed > stats[3] ? stats[3] : healed);
    }
    // A boosted damaging Water spell freezes its target.
    if (active_boost > 0 && active_school == WATER && ENEMY_HP(slot) > 0) {
        freeze_enemy(slot, active_boost);
    }
}

// ---- Mirror Shard: the first hit Brian takes in a fight is reflected at the attacker ----

static int mirror_slot = -1;
static u16 mirror_hp;

RECOMP_HOOK("player_take_damage") void eo_combat_before_brian_damage(u16 power, u16 flags, void* attacker) {
    mirror_slot = -1;
    if (!in_battle() || reflected_this_battle || power == 0 || !is_enemy_battle_data(attacker) ||
        !eo_equipment_worn(EQ_MIRROR_SHARD)) {
        return;
    }
    mirror_slot = (int)(((u32)attacker - 0x24 - ENEMY_SLOTS) / ENEMY_SLOT_SIZE);
    mirror_hp = ((u16*)gPlayerMainData)[2];
}

RECOMP_HOOK_RETURN("player_take_damage") void eo_combat_after_brian_damage(void) {
    if (mirror_slot < 0) {
        return;
    }
    u16* stats = (u16*)gPlayerMainData;
    if (stats[2] < mirror_hp) {
        u16 damage = (u16)(mirror_hp - stats[2]);
        stats[2] = mirror_hp;
        reflected_this_battle = 1;
        eo_status_fixed_damage(mirror_slot, damage);
    }
    mirror_slot = -1;
}

// ---- Charge, boost and the turn clock ----

static void restore_active_spell(void) {
    if (active_spell != 0) {
        *(u16*)(active_spell + SPELL_BASE) = saved_base;
        *(u16*)(active_spell + SPELL_SHOTS) = saved_shots;
        active_spell = 0;
    }
    active_boost = 0;
}

RECOMP_HOOK("battle_end_turn") void eo_combat_on_turn_end(void) {
    if (!in_battle()) {
        return;
    }
    if (!(gBattleState & BATTLE_ENEMY_TURN)) {
        // Brian's turn is over: undo this turn's boost, remember whether he spent.
        restore_active_spell();
        spent_last_turn = spent_this_turn;
        spent_this_turn = 0;
        boost = 0;
        return;
    }
    // An enemy's turn is over, so Brian's begins: a Broken enemy that has now lost its turn recovers.
    for (int i = 0; i < MAX_ENEMIES; i++) {
        Guard* g = &guards[i];
        u8* status = *(u8**)(ENEMY_BATTLE_DATA(i) + 0x68);
        if (g->broken && !(*(u16*)status & STATUS_RESTRICTION_BIT)) {
            g->broken = 0;
            if (gBattleState & BATTLE_BOSS) {
                g->max_shields++;
            }
            g->shields = g->max_shields;
        }
    }
    if (!spent_last_turn && charge < MAX_CHARGE) {
        charge++;
    }
    spent_last_turn = 0;
    weakness_bonus_given = 0;
}

// Brian launches a spell: spend the chosen boost on it. projectile_init runs for every shot; the boost is
// applied on the first one of the turn and undone when his turn ends.
RECOMP_HOOK("projectile_init") void eo_combat_on_projectile(void* caster, u16 spell) {
    if (is_enemy_battle_data(caster)) {
        return;
    }
    last_brian_spell = spell & 0x0FFF;
    if (!brians_turn() || active_spell != 0) {
        return;
    }
    u16 school = (spell >> 8) & 0xF;
    u16 index = spell & 0xFF;
    if (school > WIND || index >= 15) {
        return;
    }
    u8* stats = SPELL_STAT_TABLES[school] + index * SPELL_STAT_SIZE;
    u16 type = *(u16*)(stats + SPELL_TYPE);
    int echo = school == WATER && type == SPELL_TYPE_DAMAGE && eo_equipment_worn(EQ_TIDAL_ECHO);
    if (boost == 0 && !echo) {
        return;
    }
    active_spell = stats;
    active_school = (u8)school;
    active_boost = boost;
    saved_base = *(u16*)(stats + SPELL_BASE);
    saved_shots = *(u16*)(stats + SPELL_SHOTS);
    u16 base = saved_base;
    u16 shots = saved_shots;
    if (echo) {
        // Tidal Echo: every Water spell is cast twice, each at 75%.
        shots = (u16)(shots * 2);
        base = (u16)(base * 3 / 4);
    }
    if (school == EARTH && type == SPELL_TYPE_DAMAGE) {
        base = (u16)(base * (100 + 40 * boost) / 100);
    } else if (school == WATER && type == SPELL_TYPE_HEAL) {
        base = (u16)(base * (100 + 30 * boost) / 100 + boost);
    } else if (school == WIND && type == SPELL_TYPE_DAMAGE) {
        shots = (u16)(shots + 2 * boost);
    }
    *(u16*)(stats + SPELL_BASE) = base;
    *(u16*)(stats + SPELL_SHOTS) = shots;
    if (boost > 0) {
        charge -= boost;
        spent_this_turn = 1;
    }
    boost = 0;
}

// D-pad up/down picks the boost on Brian's turn.
#define BTN_DUP 0x0800
#define BTN_DDOWN 0x0400

RECOMP_HOOK_RETURN("pad_read") void eo_combat_read_input(void) {
    if (!brians_turn() || active_spell != 0) {
        return;
    }
    u8 most = charge < MAX_BOOST ? charge : MAX_BOOST;
    if ((gPadButtonsNew & BTN_DUP) && boost < most) {
        boost++;
        sfx_play(SFX_CONFIRM, 0xFF, 0x40);
    } else if ((gPadButtonsNew & BTN_DDOWN) && boost > 0) {
        boost--;
        sfx_play(SFX_CANCEL, 0xFF, 0x40);
    }
    if (boost > most) {
        boost = most;
    }
}

// ---- Battle display ----
// No lists or counters (playtest, 2026-10-20): Brian's charge is a row of diamonds next to the HUD; each enemy
// has a small plate above it with its statuses (no turn counts) and its shields as pips; the selected spell
// gets a window with its element, description, MP cost and what the chosen Boost will do.

#define CHARGE_X 112      // right of the HUD's HP/MP box
#define CHARGE_Y 24
#define CHARGE_STEP 11
#define MARKER_STEP 13    // status icons on an enemy's plate
#define PIP_STEP 7        // shield pips
#define PLATE_PAD 2
#define MARKER_RISE 0     // plate bottom above the top of the enemy (the record's height already leaves a gap)
#define DESC_X 52         // under the game's selected-spell bar
#define DESC_Y 77
#define DESC_ROW 12       // a line of the spell window (fits the 12-pixel icons)
#define SPELL_BAR_TOP 60  // the game's selected-spell bar, above the spell window

// Bottom of the spell window this frame (0 when hidden). Enemy plates that would cover it or the spell bar
// are left out while Brian picks a spell.
static s32 desc_bottom = 0;

#define BATTLE_DATA_POS_X 0x00
#define BATTLE_DATA_POS_Y 0x04
#define BATTLE_DATA_POS_Z 0x08
#define BATTLE_DATA_SCALE 0x24
#define BATTLE_DATA_RECORD 0x64
#define RECORD_TYPE 0x00        // 0 ground, 1 flying, 2 Mammon
#define RECORD_HEIGHT 0x1C

#define BRIAN_SPELL_PRESSES ((u8*)0x8007BBD1)   // element of each press so far
#define BRIAN_SPELL_COUNT (*(u16*)0x8007BBD4)   // presses so far
s32 spell_find_by_inputs(u16 count, u8* inputs);

static void draw_charge(void) {
    for (int i = 0; i < MAX_CHARGE; i++) {
        int icon = i < charge - boost ? ICON_CHARGE_FULL : i < charge ? ICON_CHARGE_SPEND : ICON_CHARGE_EMPTY;
        eo_icon_draw(icon, CHARGE_X + i * CHARGE_STEP, CHARGE_Y);
    }
}

// Where to draw markers over an enemy: the top of its body, the way the game places damage numbers.
static int marker_origin(int i, s32* sx, s32* sy) {
    u8* bd = ENEMY_BATTLE_DATA(i);
    u8* record = *(u8**)(bd + BATTLE_DATA_RECORD);
    f32 x = *(f32*)(bd + BATTLE_DATA_POS_X);
    f32 y = *(f32*)(bd + BATTLE_DATA_POS_Y);
    f32 z = *(f32*)(bd + BATTLE_DATA_POS_Z);
    if (record != 0 && *(u16*)(record + RECORD_TYPE) == 0) {
        y += *(f32*)(record + RECORD_HEIGHT) * *(f32*)(bd + BATTLE_DATA_SCALE);
    } else if (record != 0 && *(u16*)(record + RECORD_TYPE) == 1) {
        y = *(f32*)(*(u8**)(bd + 0x68) + 0x94);
    }
    return eo_screen_pos(x, y, z, sx, sy);
}

static void draw_enemy_markers(void) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (ENEMY_RECORD(i) == 0 || ENEMY_HP(i) == 0) {
            continue;
        }
        Guard* g = &guards[i];
        int icons[3];
        int n = 0;
        if (g->broken) {
            icons[n++] = ICON_BROKEN;
        }
        if (eo_status_burn_stacks(i) > 0) {
            icons[n++] = ICON_BURN;
        }
        if (g->revealed && g->weakness != NO_WEAKNESS) {
            icons[n++] = ICON_FIRE + g->weakness;
        }
        s32 sx, sy;
        if (!marker_origin(i, &sx, &sy)) {
            continue;
        }
        // The plate: a row of status icons (if any) over a row of shield pips (none while Broken; lost
        // shields show dark).
        int pips = g->broken ? 0 : g->max_shields;
        s32 icons_w = n * MARKER_STEP;
        s32 pips_w = pips * PIP_STEP + 3;
        s32 w = (icons_w > pips_w ? icons_w : pips_w) + 2 * PLATE_PAD;
        s32 h = (n > 0 ? 12 : 0) + (pips > 0 ? 9 : 0) + 2 * PLATE_PAD;
        if (n == 0 && pips == 0) {
            continue;
        }
        s32 x = sx - w / 2;
        s32 y = sy - MARKER_RISE - h;
        if (desc_bottom != 0 && y < desc_bottom && y + h > SPELL_BAR_TOP) {
            continue;
        }
        eo_ui_window(x, y, w, h);
        eo_icons_begin();
        s32 row_y = y + PLATE_PAD;
        for (int k = 0; k < n; k++) {
            eo_icon_draw(icons[k], sx - icons_w / 2 + k * MARKER_STEP, row_y);
        }
        if (pips > 0) {
            row_y += n > 0 ? 12 : 0;
            for (int k = 0; k < pips; k++) {   // pip art sits 3 pixels down its cell
                eo_icon_draw(k < g->shields ? ICON_PIP_STEEL : ICON_PIP_EMPTY, sx - pips_w / 2 + k * PIP_STEP - 2,
                             row_y - 2);
            }
        }
    }
}

// The spell Brian has selected so far (on his turn, while choosing), as the game's spell wheel finds it.
// "BOOST 2: DAMAGE X1.8" and the like, in the HUD font (no + or %). Matches eo_combat_on_projectile and
// status.c's Burn.
static void boost_text(char* out, int school, u16 type, int b) {
    char head[12];
    eo_format_u32(head, "BOOST ", (u32)b, ": ");
    char* o = eo_append(out, head);
    if (school == EARTH && type == SPELL_TYPE_DAMAGE) {
        u32 pct = 100 + 40 * (u32)b;
        eo_text_fill(o, "DAMAGE X%a.%b", 0, pct / 100, pct / 10 % 10);
    } else if (school == WATER && type == SPELL_TYPE_HEAL) {
        u32 pct = 100 + 30 * (u32)b;
        eo_text_fill(o, "HEALING X%a.%b", 0, pct / 100, pct / 10 % 10);
    } else if (school == WATER && type == SPELL_TYPE_DAMAGE) {
        eo_text_fill(o, b == 1 ? "FREEZES FOR 1 TURN" : "FREEZES FOR %a TURNS", 0, (u32)b, 0);
    } else if (school == WIND && type == SPELL_TYPE_DAMAGE) {
        eo_text_fill(o, "%a MORE SHOTS", 0, 2 * (u32)b, 0);
    } else if (school == FIRE && type == SPELL_TYPE_DAMAGE) {
        eo_text_fill(o, b == 3 ? "IGNITES - THEN 3 BURN STACKS" : b == 1 ? "1 MORE BURN STACK" : "%a MORE BURN STACKS",
                     0, (u32)b, 0);
    } else {
        eo_append(o, "NO EFFECT ON THIS SPELL");
    }
}

static void draw_spell_description(void) {
    desc_bottom = 0;
    if (!brians_turn() || active_spell != 0 || BRIAN_SPELL_COUNT == 0 || BRIAN_SPELL_COUNT > 3) {
        return;
    }
    s32 spell = spell_find_by_inputs(BRIAN_SPELL_COUNT, BRIAN_SPELL_PRESSES);
    int school = (spell >> 8) & 0xF;
    int index = spell & 0xFF;
    int valid = spell >= 0 && school <= WIND && index < 15;
    const char* text;
    if (!valid) {
        text = "NO SPELL";
    } else if (!eo_spirits_known(school, index)) {
        text = "??? - CAST IT TO LEARN IT.";
    } else {
        text = eo_spell_description(school, index);
    }
    if (text == 0) {
        return;
    }

    // MP: one per element press (research/battle, "MP cost"); Pilgrim's Soul halves it, Gambler's Coin
    // makes it a gamble. Red when Brian can't pay.
    char mp[8];
    u32 cost = BRIAN_SPELL_COUNT;
    if (eo_equipment_worn(EQ_PILGRIMS_SOUL)) {
        cost = (cost + 1) / 2;
    }
    if (eo_equipment_worn(EQ_GAMBLERS_COIN)) {
        eo_append(mp, "MP ?");
    } else {
        eo_format_u32(mp, "MP ", cost, "");
    }
    int mp_short = ((u16*)gPlayerMainData)[4] < cost;

    // What the chosen Boost will do to this spell.
    char boost_line[48];
    boost_line[0] = '\0';
    if (valid && boost > 0) {
        u8* stats = SPELL_STAT_TABLES[school] + index * SPELL_STAT_SIZE;
        boost_text(boost_line, school, *(u16*)(stats + SPELL_TYPE), boost);
    }

    // Line 1: element gem and description. Line 2: MP cost, then the Boost's effect. (The HUD's text routine
    // stops drawing past the right of the safe area, so nothing goes after the longest descriptions.)
    s32 text_x = DESC_X + SMALL_PAD + ICON_W + 2;
    s32 boost_icon_x = text_x + (eo_strlen(mp) + 2) * SMALL_W;
    s32 boost_x = boost_icon_x + ICON_W + 1;
    s32 line1_end = text_x + eo_strlen(text) * SMALL_W;
    s32 line2_end = boost_line[0] != '\0' ? boost_x + eo_strlen(boost_line) * SMALL_W : boost_icon_x;
    s32 w = (line1_end > line2_end ? line1_end : line2_end) + SMALL_PAD - DESC_X;
    s32 h = DESC_ROW * (valid ? 2 : 1) + 4;
    eo_ui_window(DESC_X, DESC_Y, w, h);
    s32 y = DESC_Y + 4;
    eo_ui_small(text_x, y, TEXT_NORMAL, text);
    if (valid) {
        eo_ui_small(text_x, y + DESC_ROW, mp_short ? TEXT_WARN : TEXT_NORMAL, mp);
    }
    if (boost_line[0] != '\0') {
        eo_ui_small(boost_x, y + DESC_ROW, TEXT_GOOD, boost_line);
    }
    eo_icons_begin();
    if (valid) {
        eo_icon_draw(ICON_FIRE + school, DESC_X + SMALL_PAD, y + ICON_TEXT_DY);
    }
    if (boost_line[0] != '\0') {
        eo_icon_draw(ICON_CHARGE_SPEND, boost_icon_x, y + DESC_ROW + ICON_TEXT_DY);
    }
    desc_bottom = DESC_Y + h + 3;
}

RECOMP_HOOK_RETURN("hud_draw_main") void eo_combat_draw(void) {
    if (!in_battle()) {
        return;
    }
    eo_fx_update();
    draw_spell_description();
    eo_icons_begin();
    draw_charge();
    draw_enemy_markers();
}
