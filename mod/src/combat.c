#include "eo.h"

// The combat system (game design document, "Combat resource"): Spirit Charge, plus enemy shields and Break.
//
// Spirit Charge: Brian starts a battle with 0 charge and gains 1 each time an enemy hurts him (max 5; playtest,
// 2026-10-23: only when hit). Before casting, D-pad up/down picks a Boost of 0-3 (no more than his charge); it's
// spent when the spell launches. Every damaging spell hits harder for it (spells.c, eo_spells_boost), on top of:
//   Fire  +1 Burn stack per charge (status.c); at 3, the hit first ignites the target's existing Burn
//   Earth more damage than the others
//   Water +30% healing per charge; a damaging Water spell Freezes its target for 1 turn per charge
//   Wind  more shots instead of more damage
//
// Shields and Break: each enemy has shields (2-4 by toughness, bosses 6-10) and a weakness, the element
// opposite its own (the game's own x1.25 rule), hidden until hit. Each weakness hit from Brian removes a
// shield; at 0 the enemy is Broken: it loses its next turn (the game's own Restriction status) and takes 100%
// more damage until Brian's next turn is over (his Break window). Then its shields come back; a boss's grow by one
// each Break.

#define MAX_ENEMIES 6
#define MAX_CHARGE 5
#define MAX_BOOST 3
#define START_CHARGE 0   // every battle starts empty (playtest, 2026-10-21); Bedrock adds
#define BROKEN_BONUS_PERCENT 100   // double damage: worth spending the Break turn on, or on healing instead (playtest, 2026-10-21)
// Talents (talents.c) bending these rules, per rank.
#define MOMENTUM_PERCENT 5         // Momentum: Earth Boost per charge, a rank
#define STEADY_PERMILLE 70         // Steady Footing: Settled Earth spells, a rank
#define FAULT_LINE_CHANCE 33       // Fault Line: an extra crack on an Earth weakness hit, a rank
#define MULTI_HIT_PERCENT 8        // Squall, Shear Force: more hits for many-hit spells, a rank
#define CYCLONE_CHANCE 5
#define STONE_MEMORY_PERCENT 15
// Bosses (game design document, "Keeping 1v1 boss fights from becoming a rotation"): besides growing shields and
// the game's random attack choice, every boss adapts to an element Brian keeps casting: from the ADAPT_AFTER + 1th
// spell of one element in a row it takes ADAPT_PERCENT less from it, until he casts another. Beigis alone changes
// element (playtest, 2026-10-21: one boss's mechanic, not every boss's): he starts each fight as Fire, and each
// time he comes back from a Break he takes one of the four elements at random, 25% each (his current one
// included); his weakness becomes its opposite and his model glows in its color (fx.c). His reward, the Prism
// Sigil, pays off switching elements.
#define ADAPT_AFTER 3
#define ADAPT_PERCENT 40
#define SHIFTING_BOSS 7   // Beigis (CURRENT_BOSS counts from 1, Solvaring)
#define STONE_GUARD_STEP 3
#define STONE_GUARD_MAX 12
#define BRIAN_DEFENSE_NOW (*(u16*)0x8007BBC0)   // in-battle defense
#define PRISM_PERCENT 20
#define GLASS_PERCENT 25
#define RESIST_SOFTEN_PERCENT 50   // 0.5x resisted hits become 0.75x
#define SETTLE_PERMILLE 400        // Earth spells while Brian is Settled deal 0.4x (0.7x was never worth a pass; playtest, 2026-10-24)
#define FINISHER_CHARGE 2          // tier-5 spells need this much Charge and spend all of it
#define BOSS_FREEZE_IMMUNE 2       // a thawed boss can't be frozen again for this many of its turns
#define BLOODSTONE_PERCENT 20      // Bloodstone: spells while Brian is under half HP
#define EXECUTIONER_PERCENT 30     // Executioner's Charm: foes under a quarter of their HP
#define STORM_GLASS_PERCENT 30     // Storm Glass: Wind spells on frozen foes
#define LANTERN_PERCENT 20         // Ember Lantern: spells on burning foes
#define GLACIAL_PIN_PERCENT 25     // Glacial Pin: spells on frozen foes
#define ECHO_STONE_PERCENT 50      // Echo Stone: the first spell of each battle
#define MISER_PERCENT 10           // Miser's Purse: more damage taken
#define SCARRED_PERCENT 25         // Scarred Leather: less damage taken under half HP
#define STOKE_FRESH_PERCENT 70     // a stoking Fire spell on a foe that isn't burning
#define STOKE_BURNING_PERCENT 140  // ... and on one that is
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
    u8 broken;     // BROKEN_WAITING until its turn is skipped, then BROKEN_WINDOW for Brian's next turn
} Guard;
#define BROKEN_WAITING 1
#define BROKEN_WINDOW 2

static Guard guards[MAX_ENEMIES];
static u8 charge = 0;
static u8 boost = 0;           // chosen for the next cast
static u8 reflected_this_battle = 0;  // Mirror Shard
static u32 turns_taken = 0;           // Brian's turns this battle (Still Waters)
static u32 cast_turn = 0;             // turns_taken + 1 when Brian last cast (one cast a turn)
static u32 turn_swing_used = 0;       // turns_taken + 1 when Brian last swung (first swing a turn)
static u8 earth_streak = 0;           // Earth spells cast in a row (Stone Memory)
static u8 last_cast_school = 0xFF;    // element of Brian's last spell (Prism Sigil)
static u8 element_changed = 0;
static u8 stone_guard = 0;
static u8 element_streak = 0;         // spells of the same element cast in a row (boss adaptation)
static u32 first_cast_turn = 0;       // turns_taken + 1 of the battle's first spell (Echo Stone)

// Settle (Spell Trees doc, Earth): an Earth attack Settles Brian from the turn after; while Settled his Earth spells
// deal 0.4x. Only a turn he ends with A (no spell, no swing; moving is fine) clears it: one turn per level, and
// Megalith Settles 2 deep. The attack's own turn is never Settled, so every rock of it lands at full strength.
static u8 settled = 0;
static u8 settle_pending = 0;
// Settle depth by Earth spell slot: Sandblast (Earth+Wind) and Rockstorm (Earth+Wind+Wind) don't Settle, nor do
// the guard and control spells; Megalith (Earth+Earth+Earth) Settles 2.
static const u8 earth_settle[15] = { 1, 1, 0, 1, 0, 2, 1, 0, 1, 0, 0, 1, 0, 1, 0 };

// Freeze (Spell Trees doc, Water): a frozen foe skips its turn and stays frozen until the start of its next one, so
// Brian always gets a turn against the ice (Shatter). The game thaws a foe at the end of its skipped turn; the bit
// goes back on until Brian's turn ends. A thawed boss resists freezing for 2 of its turns.
static u8 frozen_before[MAX_ENEMIES];
static u8 ice_window[MAX_ENEMIES];
static u8 freeze_immune[MAX_ENEMIES];
static u8 ending_enemy_turn = 0;

static int in_battle(void);

// Charge a tier-5 spell of `school` needs (Storm Caller: Wind's need 1 less).
static int finisher_charge(int school) {
    return FINISHER_CHARGE - (school == WIND && eo_talent(TALENT_STORM_CALLER) ? 1 : 0);
}

int eo_combat_finisher_ready_for(int school) {
    return !in_battle() || charge >= finisher_charge(school);
}

int eo_combat_finisher_ready(void) {
    return eo_combat_finisher_ready_for(FIRE);
}

// How long a thawed boss resists freezing: Winter's Grip shortens it; with Glacier Heart it doesn't.
static u8 boss_freeze_immunity(void) {
    if (eo_talent(TALENT_GLACIER_HEART)) {
        return 0;
    }
    return (u8)(BOSS_FREEZE_IMMUNE - eo_talent(TALENT_WINTERS_GRIP));
}

// Avalanche (Earth capstone): every third Earth attack of a battle deals double.
// Turns with more than one phase (Second Wind, Hit and Run): what Brian already did this turn, and whether he is
// in Hit and Run's moving phase (he can't cast or swing; A ends it).
static u8 acted_earlier = 0;
static u8 run_phase = 0;
static f32 move_base = 0.0f;   // Brian's move multiplier from gear and Fleet (0: not read yet this battle)

#define BRIAN_STATUS_BITS (*(u16*)0x8007BB38)
#define BRIAN_SILENCE 0x040
#define BRIAN_SILENCE_COUNTDOWN (*(u8*)0x8007BB40)
#define BRIAN_MOVE_MULTIPLIER (*(f32*)0x8007BBC8)

// Jet Stream: Brian moves farther until he casts. The base is read the first time it's needed in a battle, once
// gear and Fleet have set it (player_battle_init).
static void set_jet_stream(int casting_done) {
    int jet = eo_talent(TALENT_JET_STREAM);
    if (jet == 0) {
        return;
    }
    if (move_base == 0.0f) {
        move_base = BRIAN_MOVE_MULTIPLIER;
    }
    BRIAN_MOVE_MULTIPLIER = move_base * (casting_done ? 1.0f : 1.0f + 0.25f * (f32)jet);
}

int eo_combat_settled(void);
void eo_combat_gain_charge(int n);
// Brace: a turn ended with A while Settled halves the hits until Brian's next turn.
static u8 braced = 0;            // defense the Stone Staff has added this fight

// The boost of the spell being cast right now (0 when none), and what was changed for it.
static u8 active_boost = 0;
static u8 active_school = 0;
static u8* active_spell = 0;
static u16 saved_base;
static u16 saved_shots;

// Most charge Brian can hold.
static u8 max_charge(void) {
    return MAX_CHARGE;
}

static void gain_charge(int amount) {
    int c = charge + amount;
    charge = (u8)(c > max_charge() ? max_charge() : c);
}

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

int eo_combat_broken(int slot) {
    return slot >= 0 && slot < MAX_ENEMIES && guards[slot].broken;
}

int eo_combat_active_boost(void) {
    return active_boost;
}

int eo_combat_active_school(void) {
    return active_spell != 0 ? active_school : -1;
}

// ---- Battle start ----

static u8 opposite_element(u16 element);

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
        if (boss && CURRENT_BOSS == SHIFTING_BOSS) {
            *(u16*)(record + RECORD_ELEMENT) = FIRE;   // Beigis starts every fight as Fire
        }
        u16 element = *(u16*)(record + RECORD_ELEMENT);
        g->weakness = opposite_element(element);
        g->max_shields = g->shields = shields_for(record, boss);
    }
    charge = 0;
    gain_charge(START_CHARGE + eo_talent(TALENT_SECOND_BREATH) + (eo_equipment_worn(EQ_HOURGLASS_CHARM) ? 2 : 0));
    braced = 0;
    acted_earlier = 0;
    run_phase = 0;
    move_base = 0.0f;
    first_cast_turn = 0;
    settled = settle_pending = 0;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        frozen_before[i] = ice_window[i] = freeze_immune[i] = 0;
    }
    turns_taken = 0;
    cast_turn = turn_swing_used = 0;
    earth_streak = 0;
    last_cast_school = 0xFF;
    element_changed = 0;
    stone_guard = 0;
    element_streak = 0;
    boost = 0;
    active_boost = 0;
    active_spell = 0;
    reflected_this_battle = 0;
}

// ---- Break ----

// The Break sound (playtest, 2026-10-21): sound 0x35 three times in quick succession.
#define SFX_BREAK 0x35
#define BREAK_SOUNDS 3
#define BREAK_SOUND_GAP 4   // frames between them
static int break_sounds_left = 0;
static int break_sound_timer = 0;

// For fx.c: the element of a foe that changes element (Beigis), or -1.
int eo_combat_shifting_element(int slot) {
    if (!(gBattleState & BATTLE_BOSS) || CURRENT_BOSS != SHIFTING_BOSS || slot < 0 || slot >= MAX_ENEMIES ||
        ENEMY_RECORD(slot) == 0) {
        return -1;
    }
    u16 element = *(u16*)(ENEMY_RECORD(slot) + RECORD_ELEMENT);
    return element < 4 ? element : -1;
}

static void play_break_sounds(void) {
    if (break_sounds_left > 0 && --break_sound_timer <= 0) {
        eo_sfx(SFX_BREAK);
        break_sounds_left--;
        break_sound_timer = BREAK_SOUND_GAP;
    }
}

static void break_enemy(int i) {
    Guard* g = &guards[i];
    g->broken = BROKEN_WAITING;
    u8* status = *(u8**)(ENEMY_BATTLE_DATA(i) + 0x68);
    *(u16*)status |= STATUS_RESTRICTION_BIT;   // its next turn is skipped by the game itself
    status[STATUS_RESTRICTION_DURATION] = 1;
    break_sounds_left = BREAK_SOUNDS;   // played by eo_combat_draw, a few frames apart
    break_sound_timer = 0;
}

// Sound for a shield knocked off without a Break (playtest pick, 2026-10-21). It plays alongside the game's own hit
// sound (0x18, from enemy_take_damage, which this runs inside).
#define SFX_SHIELD_HIT 0x27

static u8 opposite_element(u16 element) {
    return element == FIRE ? WATER : element == WATER ? FIRE : element == EARTH ? WIND : element == WIND ? EARTH :
           NO_WEAKNESS;
}

// Beigis's element change: a new element at random (the game's own x1.25 / x0.5 rule follows it, since it reads
// the record), the weakness its opposite, hidden again until hit. Guilty and Mammon have no element and never get a
// weakness (playtest, 2026-10-21): their shields only fall to the Titan Staff, Cyclone or Glacier.
static void shift_element(int i) {
    Guard* g = &guards[i];
    u8* record = ENEMY_RECORD(i);
    if (record == 0) {
        return;
    }
    u16 element = (u16)get_rand(4);
    *(u16*)(record + RECORD_ELEMENT) = element;
    g->weakness = opposite_element(element);
    g->revealed = 0;
    eo_fx_shield_hit(i, 0);   // a glint as it changes form
}

// Knocks shields off a foe (a weakness hit, Cyclone, Glacier); at 0 it Breaks.
static void crack_shields(int i, int count) {
    Guard* g = &guards[i];
    if (g->broken || g->shields == 0) {
        return;
    }
    g->shields = (u8)(g->shields > count ? g->shields - count : 0);
    if (g->shields == 0) {
        break_enemy(i);
    } else if (SFX_SHIELD_HIT != 0) {
        eo_sfx(SFX_SHIELD_HIT);
    }
    eo_fx_shield_hit(i, g->broken);
}

static void freeze_enemy(int i, int turns) {
    u8* status = *(u8**)(ENEMY_BATTLE_DATA(i) + 0x68);
    if (gBattleState & BATTLE_BOSS) {
        if (freeze_immune[i] > 0) {
            return;   // still shaking off the last freeze
        }
        turns = 1;  // bosses resist control effects
    } else if (eo_talent(TALENT_PERMAFROST) >= 3) {
        turns++;    // Permafrost
    }
    *(u16*)status |= STATUS_RESTRICTION_BIT;
    if (status[STATUS_RESTRICTION_DURATION] < turns) {
        status[STATUS_RESTRICTION_DURATION] = (u8)turns;
    }
}

// ---- For spells.c ----

static int frozen_now(int i) {
    u8* status = *(u8**)(ENEMY_BATTLE_DATA(i) + 0x68);
    return !guards[i].broken && !eo_spells_confused(i) && status != 0 && (*(u16*)status & STATUS_RESTRICTION_BIT);
}

int eo_combat_settled(void) {
    return settled > 0;
}

void eo_combat_gain_charge(int n) {
    if (in_battle()) {
        gain_charge(n);
    }
}

int eo_combat_frozen(int slot) {
    return slot >= 0 && slot < MAX_ENEMIES && ENEMY_HP(slot) > 0 && frozen_now(slot);
}

void eo_combat_freeze(int slot, int turns) {
    if (slot >= 0 && slot < MAX_ENEMIES && ENEMY_HP(slot) > 0 && !guards[slot].broken) {
        freeze_enemy(slot, turns);
    }
}

// A frozen foe hit colder stays frozen a turn longer (up to 3; bosses never past 1).
void eo_combat_freeze_longer(int slot) {
    if (!eo_combat_frozen(slot) || (gBattleState & BATTLE_BOSS)) {
        return;
    }
    u8* status = *(u8**)(ENEMY_BATTLE_DATA(slot) + 0x68);
    if (status[STATUS_RESTRICTION_DURATION] < 3) {
        status[STATUS_RESTRICTION_DURATION]++;
    }
}

// Fire, steam and Shatter end a Freeze; a boss then resists freezing for a while.
void eo_combat_thaw(int slot) {
    if (!eo_combat_frozen(slot)) {
        return;
    }
    u8* status = *(u8**)(ENEMY_BATTLE_DATA(slot) + 0x68);
    *(u16*)status &= (u16)~STATUS_RESTRICTION_BIT;
    status[STATUS_RESTRICTION_DURATION] = 0;
    ice_window[slot] = 0;
    frozen_before[slot] = 0;
    if (gBattleState & BATTLE_BOSS) {
        freeze_immune[slot] = boss_freeze_immunity();
    }
}

// Confusion: the foe loses its next turns (the Restriction bit, like Break; no ice).
void eo_combat_skip_turn(int slot, int turns) {
    if (slot < 0 || slot >= MAX_ENEMIES || ENEMY_HP(slot) == 0) {
        return;
    }
    u8* status = *(u8**)(ENEMY_BATTLE_DATA(slot) + 0x68);
    *(u16*)status |= STATUS_RESTRICTION_BIT;
    if (status[STATUS_RESTRICTION_DURATION] < turns) {
        status[STATUS_RESTRICTION_DURATION] = (u8)turns;
    }
}

void eo_combat_crack(int slot, int count) {
    if (slot >= 0 && slot < MAX_ENEMIES && ENEMY_HP(slot) > 0) {
        crack_shields(slot, count);
    }
}

void eo_combat_reveal(int slot) {
    if (slot >= 0 && slot < MAX_ENEMIES) {
        guards[slot].revealed = 1;
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
static u8 hit_was_frozen;
static u8 hit_was_burning;
static u8 hit_was_weak;
static u8 hit_was_resisted;
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
    u8* target_status = *(u8**)(ENEMY_BATTLE_DATA(slot) + 0x68);
    hit_was_frozen = !g->broken && !eo_spells_confused(slot) && target_status != 0 &&
                     (*(u16*)target_status & STATUS_RESTRICTION_BIT);
    hit_was_burning = eo_status_burn_stacks(slot) > 0;
    // The game's element rule: the opposite element hits x1.25 (the guard's weakness), the same one x0.5.
    u16 own = ENEMY_RECORD(slot) != 0 ? *(u16*)(ENEMY_RECORD(slot) + RECORD_ELEMENT) : 0xFF;
    hit_was_weak = hit_kind == HIT_SPELL && g->weakness != NO_WEAKNESS && element == g->weakness;
    // The game also halves spells on a foe with no element (4); the overhaul softens both to 0.75x below.
    hit_was_resisted = hit_kind == HIT_SPELL && element < 4 && (element == own || own == 4);
    if (hit_kind != HIT_SPELL || g->broken) {
        return;
    }
    int index = last_brian_spell & 0xFF;
    int heavy_hit = element == EARTH && index < 15 && earth_settle[index] > 0;   // an Earth attack that Settles
    if (g->weakness == NO_WEAKNESS || element != g->weakness) {
        // Cyclone: a Wind hit may crack a shield anyway. Seismic: so does a heavy Earth hit, always.
        if (element == WIND && get_rand(100) < CYCLONE_CHANCE * eo_talent(TALENT_CYCLONE)) {
            crack_shields(slot, 1);
        } else if (heavy_hit && eo_talent(TALENT_SEISMIC)) {
            crack_shields(slot, 1);
        }
        return;
    }
    g->revealed = 1;
    // Mountain Staff: Earth weakness hits take 2 shields; Fault Line: a third of the time a rank, 1 more.
    int cracks = 1 + eo_spells_weak_extra_cracks();
    if (element == EARTH && eo_equipment_worn(EQ_MOUNTAIN_STAFF)) {
        cracks++;
    }
    if (element == EARTH && get_rand(100) < FAULT_LINE_CHANCE * eo_talent(TALENT_FAULT_LINE)) {
        cracks++;
    }
    crack_shields(slot, cracks);
}

// Damage numbers: 9 slots of 0x1C bytes at 0x800873D0 (+0x00 the fighter's battle data, +0x04 digits, +0x0C timer,
// starting at 45 when spawned, +0x14 digit count), filled by damage_popup_spawn through count_digits (which also
// writes the digits).
#define POPUPS 0x800873D0
#define POPUP_COUNT 9
#define POPUP_SIZE 0x1C
#define POPUP_NEW_TIMER 0x2D
s32 count_digits(s32 value, u8* digits, s32 arg);

// Colors (playtest, 2026-10-21: "Mechanically weakened damage can be blue, mechanically buffed damage can be orange or
// red"). A popup's kind (+0x10) indexes the RGBA table at 0x8004CFA4 (0 white, 1 green, 2 pale blue; game data
// follows it). The mod's colors live in the console's spare 4 MB (see icons.c) and are reached with a kind large
// enough to index there.
#define POPUP_KIND 0x10
#define POPUP_COLOR_TABLE 0x8004CFA4
#define POPUP_COLORS ((u32*)0x807F2000)
enum { POPUP_PLAIN, POPUP_WEAKENED, POPUP_BUFFED };
static const u32 popup_colors[] = { 0xFFFFFFFF, 0x5AA0FFFF, 0xFF9A30FF };

// Sets the number just spawned over an enemy (the game shows the hit before the mod adjusts it) and its color.
static void set_popup(int slot, u32 amount, int look) {
    for (int i = 0; i < POPUP_COUNT; i++) {
        u8* p = (u8*)(POPUPS + i * POPUP_SIZE);
        if (*(u8**)p == ENEMY_BATTLE_DATA(slot) && *(u16*)(p + 0x0C) == POPUP_NEW_TIMER) {
            *(s32*)(p + 0x14) = count_digits((s32)amount, p + 0x04, 1);
            if (look != POPUP_PLAIN) {
                POPUP_COLORS[look] = popup_colors[look];
                *(u32*)(p + POPUP_KIND) = ((u32)&POPUP_COLORS[look] - POPUP_COLOR_TABLE) / 4;
            }
            return;
        }
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
    // Hit and Run's moving phase: no swinging (the hit is undone).
    if (hit_kind == HIT_STAFF && run_phase) {
        ENEMY_HP(slot) = hit_hp;
        return;
    }
    // Staff hits deal 0.76x like spells (progression.c; spells get it through their base): the rest comes back.
    if (hit_kind == HIT_STAFF) {
        u32 back = hit * (1000 - DAMAGE_SCALE_THOUSANDTHS) / 1000;
        if (back > 0 && hp > 0) {
            u16 max_hp = *(u16*)(ENEMY_RECORD(slot) + RECORD_MAX_HP);
            hp = (u16)(hp + back > max_hp ? max_hp : hp + back);
            ENEMY_HP(slot) = hp;
            hit -= back;
        }
    }
    // Extra damage on top of the hit: a Broken foe takes double, the staff adds its flat bonus, and a staff
    // that favours the spell's element adds 10%.
    u32 extra = 0;
    u32 percent = 0;
    if (hit_was_broken) {
        percent += BROKEN_BONUS_PERCENT;
    }
    // Same element or no element: the game's 0.5x becomes 0.75x (missing the weakness already costs the shield cracks).
    if (hit_was_resisted) {
        percent += RESIST_SOFTEN_PERCENT;
    }
    // Gear and talents: what they add counts as plain (white) unless a mechanic also pushed the hit.
    u32 plain = 0;
    if (hit_kind == HIT_STAFF) {
        extra += (u32)eo_equip_staff_bonus();
        plain = 0;
    } else if (hit_element < 4) {
        plain = (u32)eo_equip_element_percent(hit_element) +
                (u32)eo_talents_spell_percent(hit_element);
        percent += plain;

        // Stone Memory: each Earth spell cast in a row hits 15% harder (to 30%).
        if (hit_element == EARTH && eo_equipment_worn(EQ_STONE_MEMORY)) {
            percent += STONE_MEMORY_PERCENT * (u32)(earth_streak > 1 ? (earth_streak - 1 > 2 ? 2 : earth_streak - 1) : 0);
        }
        // Prism Sigil: a spell of a different element from the last one hits 20% harder.
        if (eo_equipment_worn(EQ_PRISM_SIGIL) && element_changed) {
            percent += PRISM_PERCENT;
        }
        u16* brian = (u16*)gPlayerMainData;
        if (eo_equipment_worn(EQ_BLOODSTONE) && brian[2] * 2 < brian[3]) {
            percent += BLOODSTONE_PERCENT;
        }
        if (eo_equipment_worn(EQ_STORM_GLASS) && hit_element == WIND && hit_was_frozen) {
            percent += STORM_GLASS_PERCENT;
        }
        if (eo_equipment_worn(EQ_EMBER_LANTERN) && hit_was_burning) {
            percent += LANTERN_PERCENT;
        }
        if (eo_equipment_worn(EQ_GLACIAL_PIN) && hit_was_frozen) {
            percent += GLACIAL_PIN_PERCENT;
        }
        if (eo_equipment_worn(EQ_ECHO_STONE) && cast_turn == first_cast_turn) {
            percent += ECHO_STONE_PERCENT;
        }
    }
    // Executioner's Charm: a foe under a quarter of its HP takes 30% more from anything.
    if (eo_equipment_worn(EQ_EXECUTIONERS_CHARM) && ENEMY_RECORD(slot) != 0 &&
        (u32)hit_hp * 4 < *(u16*)(ENEMY_RECORD(slot) + RECORD_MAX_HP)) {
        percent += EXECUTIONER_PERCENT;
    }
    // Glass Charm: everything Brian deals is 25% more.
    if (eo_equipment_worn(EQ_GLASS_CHARM)) {
        percent += GLASS_PERCENT;
    }
    // The spell's own bonus (Shatter on ice, Fan the Flames on a burning foe); Juggernaut's swing.
    if (hit_kind == HIT_SPELL) {
        percent += (u32)eo_spells_hit_percent(slot, hit_was_frozen, hit_was_burning);
    } else if (hit_kind == HIT_STAFF) {
        percent += (u32)eo_spells_staff_percent(slot);
    }
    // Which way the mechanics pushed this hit, for the number's color: a weakness hit, a Broken, burning or frozen
    // bonus or a stoke on a burning foe buffs it; the foe resisting the element or a stoke on a foe that isn't
    // burning weakens it. Gear bonuses alone leave it white.
    int buffed = hit_was_weak || hit_was_broken || percent > plain + (hit_was_resisted ? RESIST_SOFTEN_PERCENT : 0);
    int weakened = hit_was_resisted;
    u32 shown = hit;
    // A boss adapted to the element Brian keeps casting gives back part of the hit.
    if (hit_kind == HIT_SPELL && (gBattleState & BATTLE_BOSS) && element_streak > ADAPT_AFTER &&
        hit_element == last_cast_school && hp > 0) {
        weakened = 1;
        u32 back = hit * ADAPT_PERCENT / 100;
        u16 max_hp = *(u16*)(ENEMY_RECORD(slot) + RECORD_MAX_HP);
        hp = (u16)(hp + back > max_hp ? max_hp : hp + back);
        ENEMY_HP(slot) = hp;
        shown = shown > back ? shown - back : 0;
    }
    extra += hit * percent / 100;
    if (extra > 0 && hp > 0) {
        u32 dealt = extra >= hp ? hp : extra;
        ENEMY_HP(slot) = (u16)(hp - dealt);
        shown += dealt;   // one number for the whole hit
    }
    set_popup(slot, shown, buffed ? POPUP_BUFFED : weakened ? POPUP_WEAKENED : POPUP_PLAIN);
    // Ember Staff: a staff hit on a burning foe detonates it, spreading the Burn to the enemies around it (status.c);
    // it never starts a Burn itself (playtest, 2026-10-21: it was far ahead of the other element staves).
    // Phoenix Staff and Firebrand: staff hits set foes burning.
    if (hit_kind == HIT_STAFF && turn_swing_used != turns_taken + 1) {
        turn_swing_used = turns_taken + 1;   // the first swing this turn
        if (eo_equipment_worn(EQ_SPELLSTRIKE)) {
            gain_charge(1);   // Spellstrike
        }
        if (eo_equipment_worn(EQ_TITAN_STAFF) && ENEMY_HP(slot) > 0) {
            crack_shields(slot, 1);   // Titan Staff
        }
    }
    if (hit_kind == HIT_STAFF) {
        if (eo_status_burn_stacks(slot) > 0) {
            if (eo_equipment_worn(EQ_EMBER_STAFF)) {
                eo_status_flare(slot);
            }
        } else if (ENEMY_HP(slot) > 0 && eo_equipment_worn(EQ_PHOENIX_STAFF)) {
            eo_status_add_burn(slot, (u16)(hit * (u32)eo_status_burn_percent() / 100 + 1));
        }
        // Stone Staff: every swing hardens Brian, +3 defense for the rest of the fight (up to +12).
        if (eo_equipment_worn(EQ_STONE_STAFF) && stone_guard < STONE_GUARD_MAX) {
            stone_guard = (u8)(stone_guard + STONE_GUARD_STEP);
            BRIAN_DEFENSE_NOW = (u16)(BRIAN_DEFENSE_NOW + STONE_GUARD_STEP);
        }
        // Tide Staff: swings heal Brian for half the damage they deal.
        if (eo_equipment_worn(EQ_TIDE_STAFF)) {
            u16* stats = (u16*)gPlayerMainData;
            u32 healed = stats[2] + (shown + 1) / 2;
            stats[2] = (u16)(healed > stats[3] ? stats[3] : healed);
        }
    }
    // Leech Pillar: Water Pillar (Lv1-3) heals Brian for a quarter of the damage it deals.
    if (hit_kind == HIT_SPELL && eo_equipment_worn(EQ_LEECH_PILLAR) &&
        (last_brian_spell == 0x200 || last_brian_spell == 0x201 || last_brian_spell == 0x204)) {
        u16* stats = (u16*)gPlayerMainData;
        u32 healed = stats[2] + (hit + extra) / 4;
        stats[2] = (u16)(healed > stats[3] ? stats[3] : healed);
    }
    // What the spell does besides damage (spells.c, next frame).
    if (hit_kind == HIT_SPELL) {
        eo_spells_on_hit(slot, shown, hit_was_weak, hit_was_frozen);
    }
}

// ---- Mirror Shard: the first hit Brian takes in a fight is reflected at the attacker ----

static int mirror_slot = -1;
static u16 mirror_hp;

static u16 bulwark_hp;

RECOMP_HOOK("player_take_damage") void eo_combat_before_brian_damage(u16 power, u16 flags, void* attacker) {
    bulwark_hp = ((u16*)gPlayerMainData)[2];
    mirror_slot = -1;
    if (!in_battle() || reflected_this_battle || power == 0 || !is_enemy_battle_data(attacker) ||
        !eo_equipment_worn(EQ_MIRROR_SHARD)) {
        return;
    }
    mirror_slot = (int)(((u32)attacker - 0x24 - ENEMY_SLOTS) / ENEMY_SLOT_SIZE);
    mirror_hp = ((u16*)gPlayerMainData)[2];
}

RECOMP_HOOK_RETURN("player_take_damage") void eo_combat_after_brian_damage(void) {
    // Glass Charm: what Brian takes is 25% more (never below 1 HP from the extra).
    u16* hp_now = (u16*)gPlayerMainData + 2;
    if (in_battle() && eo_equipment_worn(EQ_GLASS_CHARM) && *hp_now < bulwark_hp && *hp_now > 0) {
        u16 more = (u16)((bulwark_hp - *hp_now) * GLASS_PERCENT / 100);
        *hp_now = (u16)(*hp_now > more ? *hp_now - more : 1);
    }
    // Miser's Purse: what Brian takes is 10% more (never below 1 HP from the extra).
    if (in_battle() && eo_equipment_worn(EQ_MISERS_PURSE) && *hp_now < bulwark_hp && *hp_now > 0) {
        u16 more = (u16)((bulwark_hp - *hp_now) * MISER_PERCENT / 100);
        *hp_now = (u16)(*hp_now > more ? *hp_now - more : 1);
    }
    // Scarred Leather: a hit taken while under half HP is 25% smaller.
    u16 max_hp = ((u16*)gPlayerMainData)[3];
    if (in_battle() && eo_equipment_worn(EQ_SCARRED_LEATHER) && *hp_now < bulwark_hp && bulwark_hp * 2 < max_hp) {
        u16 back = (u16)((bulwark_hp - *hp_now) * SCARRED_PERCENT / 100);
        *hp_now = (u16)(*hp_now + back);
    }
    // Brace: braced on a turn ended with A while Settled, hits until his next turn are halved.
    if (in_battle() && braced && *hp_now < bulwark_hp) {
        *hp_now = (u16)(*hp_now + (bulwark_hp - *hp_now) / 2);
    }
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
    ending_enemy_turn = (gBattleState & BATTLE_ENEMY_TURN) != 0;
    if (!(gBattleState & BATTLE_ENEMY_TURN)) {
        // Settle: a turn ended with A (no spell, no swing) clears a level; this turn's Earth attack Settles him now.
        int acted = acted_earlier || cast_turn == turns_taken + 1 || turn_swing_used == turns_taken + 1;
        acted_earlier = (u8)acted;
        braced = 0;
        if (!acted && settled > 0) {
            braced = (u8)eo_talent(TALENT_BRACE);
            gain_charge(eo_talent(TALENT_PLANTED_FEET));
            eo_arena_settled_pass();   // Tremor, Mountain's Patience
            settled--;
        }
        if (run_phase) {
            run_phase = 0;
            BRIAN_STATUS_BITS &= (u16)~BRIAN_SILENCE;   // Hit and Run's phase is over
        }
        if (settle_pending > settled) {
            settled = settle_pending;
        }
        settle_pending = 0;
        // The ice window ends with Brian's turn: the foe thaws and acts; a boss resists freezing for a while.
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (!ice_window[i]) {
                continue;
            }
            ice_window[i] = 0;
            u8* status = *(u8**)(ENEMY_BATTLE_DATA(i) + 0x68);
            if (!guards[i].broken) {
                *(u16*)status &= (u16)~STATUS_RESTRICTION_BIT;
            }
            if (gBattleState & BATTLE_BOSS) {
                freeze_immune[i] = boss_freeze_immunity();
            }
        }
        // Brian's turn is over: undo this turn's boost.
        restore_active_spell();
        boost = 0;
        return;
    }
    // Which foes are frozen (not Broken) as this enemy turn ends: one the game thaws now gets its ice window.
    for (int i = 0; i < MAX_ENEMIES; i++) {
        u8* status = *(u8**)(ENEMY_BATTLE_DATA(i) + 0x68);
        frozen_before[i] = ENEMY_HP(i) > 0 && !guards[i].broken && !eo_spells_confused(i) && status != 0 &&
                           (*(u16*)status & STATUS_RESTRICTION_BIT) != 0;
        if (freeze_immune[i] > 0) {
            freeze_immune[i]--;
        }
    }
    turns_taken++;
    acted_earlier = 0;
    set_jet_stream(0);
}

// Brian's turn after a Break is over: the foe recovers before it acts (playtest, 2026-10-23: it had stayed dark and
// slowed through the turn it acted in), and its shields come back.
static void end_break_windows(void) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        Guard* g = &guards[i];
        if (g->broken != BROKEN_WINDOW) {
            continue;
        }
        g->broken = 0;
        if (gBattleState & BATTLE_BOSS) {
            g->max_shields++;
            if (CURRENT_BOSS == SHIFTING_BOSS) {
                shift_element(i);
            }
        }
        g->shields = g->max_shields;
    }
}

// The game ticks a fighter's statuses inside battle_end_turn: a foe it just thawed keeps its ice until Brian's
// turn ends (the window above).
RECOMP_HOOK_RETURN("battle_end_turn") void eo_combat_after_turn_end(void) {
    if (!in_battle()) {
        return;
    }
    if (!ending_enemy_turn) {
        // The game just handed the turn to an enemy; Second Wind and Hit and Run take it back once each.
        // Second Wind: Brian acts twice every turn. Hit and Run: after a cast he may still move, but not cast or
        // swing (silenced; swings do nothing), until he ends the turn with A.
        static u32 second_wind_turn = 0;
        static u32 run_turn = 0;
        int cast_now = cast_turn == turns_taken + 1;
        if (!(gBattleState & BATTLE_ENEMY_TURN)) {
            return;
        }
        if (eo_talent(TALENT_SECOND_WIND) && second_wind_turn != turns_taken + 1) {
            second_wind_turn = turns_taken + 1;
            gBattleState &= (u16)~BATTLE_ENEMY_TURN;
            cast_turn = 0;                 // a fresh action: it may cast again
            turn_swing_used = 0;
            set_jet_stream(0);
        } else if (eo_talent(TALENT_HIT_AND_RUN) && cast_now && run_turn != turns_taken + 1) {
            run_turn = turns_taken + 1;
            gBattleState &= (u16)~BATTLE_ENEMY_TURN;
            run_phase = 1;
            BRIAN_STATUS_BITS |= BRIAN_SILENCE;
            BRIAN_SILENCE_COUNTDOWN = 1;
            set_jet_stream(1);
        }
        if (gBattleState & BATTLE_ENEMY_TURN) {
            end_break_windows();   // the turn really passes to the enemies
        }
        return;
    }
    for (int i = 0; i < MAX_ENEMIES; i++) {
        u8* status = *(u8**)(ENEMY_BATTLE_DATA(i) + 0x68);
        if (frozen_before[i] && ENEMY_HP(i) > 0 && status != 0 && !(*(u16*)status & STATUS_RESTRICTION_BIT)) {
            *(u16*)status |= STATUS_RESTRICTION_BIT;
            ice_window[i] = 1;
        }
        frozen_before[i] = 0;
    }
    // A Broken enemy that has now lost its turn opens Brian's Break window. Checked after the game ticks statuses
    // (before it, the skipped turn's Restriction bit is still on).
    for (int i = 0; i < MAX_ENEMIES; i++) {
        u8* status = *(u8**)(ENEMY_BATTLE_DATA(i) + 0x68);
        if (guards[i].broken == BROKEN_WAITING && status != 0 && !(*(u16*)status & STATUS_RESTRICTION_BIT)) {
            guards[i].broken = BROKEN_WINDOW;
        }
    }
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
    // Once per cast (the first shot of the turn): Stone Memory's streak and Prism Sigil's element change.
    int new_cast = cast_turn != turns_taken + 1;
    if (new_cast) {
        cast_turn = turns_taken + 1;
        if (first_cast_turn == 0) {
            first_cast_turn = cast_turn;
        }
        earth_streak = school == EARTH ? earth_streak + 1 : 0;
        element_streak = school == last_cast_school ? (u8)(element_streak + 1) : 1;
        element_changed = last_cast_school != 0xFF && school != last_cast_school;
        last_cast_school = (u8)school;
    }
    u8* stats = SPELL_STAT_TABLES[school] + index * SPELL_STAT_SIZE;
    u16 type = *(u16*)(stats + SPELL_TYPE);
    // A finisher (tier 5) spends all of Brian's Charge: 2 to cast it, and each one past 2 is its Boost.
    int finisher = eo_spirits_tier(school, index) == 5;
    int used = boost;
    if (finisher && new_cast) {
        int need = finisher_charge(school);
        used = charge > need ? charge - need : 0;
        used = used > MAX_BOOST ? MAX_BOOST : used;
        boost = 0;
        charge = 0;
    }
    if (new_cast) {
        eo_spells_on_cast(school, index, used);

    }
    if (school == EARTH && type == SPELL_TYPE_DAMAGE && earth_settle[index] > settle_pending) {
        settle_pending = earth_settle[index];
    }
    int echo = school == WATER && type == SPELL_TYPE_DAMAGE && eo_equipment_worn(EQ_TIDAL_ECHO);
    int wellspring = 0;
    int leviathan = school == WATER && type == SPELL_TYPE_HEAL && eo_equipment_worn(EQ_LEVIATHAN_STAFF);
    int tether = school == WIND && type == SPELL_TYPE_DAMAGE && eo_equipment_worn(EQ_GALE_TETHER);
    int curve = type == SPELL_TYPE_DAMAGE;   // every damaging spell: the overhaul's power curve (progression.c)
    if (used == 0 && !echo && !wellspring && !leviathan && !tether && !curve) {
        boost = 0;
        return;
    }
    active_spell = stats;
    active_school = (u8)school;
    active_boost = (u8)used;
    saved_base = *(u16*)(stats + SPELL_BASE);
    saved_shots = *(u16*)(stats + SPELL_SHOTS);
    u16 base = saved_base;
    u16 shots = saved_shots;
    if (echo) {
        // Tidal Echo: every Water spell is cast twice, each at 65% (1.3x in all; 75% made it a flat 1.5x).
        shots = (u16)(shots * 2);
        base = (u16)(base * 65 / 100);
    }
    // Boost by spell (spells.c): more damage a charge (Earth most), Wind +40% shots (at least one more a charge),
    // and the spells that boost like another school.
    int damage_pct;
    int shots_pct;
    eo_spells_boost(school, index, &damage_pct, &shots_pct);
    if (damage_pct > 0) {
        if (school == EARTH) {
            damage_pct += MOMENTUM_PERCENT * eo_talent(TALENT_MOMENTUM);
        }
        base = (u16)(base * (100 + damage_pct * used) / 100);
    }
    if (shots_pct > 0) {
        // +10 points with each Crosswind rank and the Skyreed Staff. A flat +2 a charge made a 3-charge Wind Cutter
        // 4x (balance pass, 2026-10-21).
        u32 pct = (u32)shots_pct + (eo_equipment_worn(EQ_SKYREED_STAFF) ? 10 : 0);
        u32 extra_shots = ((u32)saved_shots * pct * used + 50) / 100;
        if (used > 0 && extra_shots < (u32)used) {
            extra_shots = used;
        }
        shots = (u16)(shots + extra_shots);
    }
    if (tether) {
        shots++;
    }
    // Squall and Shear Force: many-hit spells hit more often (the fraction of a hit is a chance at one more).
    u32 more = MULTI_HIT_PERCENT * (u32)eo_talent(TALENT_SQUALL);
    if (type == SPELL_TYPE_DAMAGE && saved_shots >= 2 && more > 0) {
        u32 hundredths = (u32)shots * more;
        shots = (u16)(shots + hundredths / 100 + ((u32)get_rand(100) < hundredths % 100 ? 1 : 0));
    }
    if (school == WATER && type == SPELL_TYPE_HEAL) {
        base = (u16)(base * (100 + 10 * used + 10 * wellspring + (leviathan ? 25 : 0)) / 100);
    }
    if (curve) {
        u32 scaled = (u32)base * eo_progression_spell_scale(school) / 1000;
        if (school == EARTH && settled) {
            scaled = scaled * (SETTLE_PERMILLE + STEADY_PERMILLE * (u32)eo_talent(TALENT_STEADY_FOOTING)) / 1000;
        }
        base = (u16)(scaled > 0 ? (scaled > 0xFFFF ? 0xFFFF : scaled) : 1);
    }
    *(u16*)(stats + SPELL_BASE) = base;
    *(u16*)(stats + SPELL_SHOTS) = shots;
    if (boost > 0) {
        charge -= boost;
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
        eo_sfx(SFX_CONFIRM);
    } else if ((gPadButtonsNew & BTN_DDOWN) && boost > 0) {
        boost--;
        eo_sfx(SFX_CANCEL);
    }
    if (boost > most) {
        boost = most;
    }
}

// ---- Battle display ----
// No lists, counters or floating icons (playtests, 2026-10-20/21): Brian's charge is a row of diamonds next to
// the HUD, and the selected spell gets a window with its element, description and MP cost. What a Boost does is
// left for the player to find out. Enemy statuses, shield hits and Break show on the enemies themselves (fx.c).

#define CHARGE_X 40       // centered under the element gems, bottom left (playtest, 2026-10-21)
#define CHARGE_Y 216      // clear of the gems' level numbers
#define CHARGE_STEP 11
// The element gems slide in and out with battles: the HUD draws them at x 28 minus this (0 when shown); the
// diamonds under them follow.
#define HUD_GEM_SLIDE (*(s32*)0x8008C664)
#define DESC_X 52         // under the game's selected-spell bar
#define DESC_Y 77
#define DESC_ROW 12       // a line of the spell window (fits the 12-pixel icons)

#define BRIAN_SPELL_PRESSES ((u8*)0x8007BBD1)   // element of each press so far
#define BRIAN_SPELL_COUNT (*(u16*)0x8007BBD4)   // presses so far
s32 spell_find_by_inputs(u16 count, u8* inputs);

static void draw_charge(void) {
    for (int i = 0; i < max_charge(); i++) {
        int icon = i < charge - boost ? ICON_CHARGE_FULL : i < charge ? ICON_CHARGE_SPEND : ICON_CHARGE_EMPTY;
        eo_icon_draw(icon, CHARGE_X - HUD_GEM_SLIDE + i * CHARGE_STEP, CHARGE_Y);
    }
    // Settled: an Earth diamond a level, after a gap (playtest, 2026-10-24: Settle was invisible).
    for (int i = 0; in_battle() && i < settled; i++) {
        eo_icon_draw(ICON_EARTH, CHARGE_X - HUD_GEM_SLIDE + (max_charge() + 1 + i) * CHARGE_STEP, CHARGE_Y);
    }
}

static void draw_spell_description(void) {
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

    // MP: each spell's own cost (spells.c); Pilgrim's Soul halves it, Gambler's Coin makes it a gamble. Red when
    // Brian can't pay. A finisher also says it needs 2 Charge.
    char mp[32];
    u32 cost = valid ? eo_spells_mp_cost(school, index) : 0;
    if (eo_equipment_worn(EQ_PILGRIMS_SOUL)) {
        cost = (cost + 1) / 2;
    }
    if (eo_equipment_worn(EQ_GAMBLERS_COIN)) {
        eo_append(mp, "MP ?");
    } else {
        eo_format_u32(mp, "MP ", cost, "");
    }
    if (valid && eo_spirits_tier(school, index) == 5) {
        eo_append(mp + eo_strlen(mp), "  FINISHER: 2 CHARGE");
    } else if (valid && school == EARTH && settled && *(u16*)(SPELL_STAT_TABLES[school] + index * SPELL_STAT_SIZE +
                                                                SPELL_TYPE) == SPELL_TYPE_DAMAGE) {
        eo_append(mp + eo_strlen(mp), "  SETTLED: 0.4X");
    }
    int mp_short = ((u16*)gPlayerMainData)[4] < cost;

    // Line 1: element gem and description. Line 2: the MP cost. (The HUD's text routine stops drawing past the
    // right of the safe area, so nothing goes after the longest descriptions.)
    s32 text_x = DESC_X + SMALL_PAD + ICON_W + 2;
    s32 longest = eo_strlen(text) > eo_strlen(mp) ? eo_strlen(text) : eo_strlen(mp);
    s32 w = text_x + longest * SMALL_W + SMALL_PAD - DESC_X;
    s32 h = DESC_ROW * (valid ? 2 : 1) + 4;
    eo_ui_window(DESC_X, DESC_Y, w, h);
    s32 y = DESC_Y + 4;
    eo_ui_small(text_x, y, TEXT_NORMAL, text);
    if (valid) {
        eo_ui_small(text_x, y + DESC_ROW, mp_short ? TEXT_WARN : TEXT_NORMAL, mp);
    }
    eo_icons_begin();
    if (valid) {
        eo_icon_draw(ICON_FIRE + school, DESC_X + SMALL_PAD, y + ICON_TEXT_DY);
    }
}

static void eo_combat_draw_body(void);

// Drawn into the mod's own display list (ui.c).
RECOMP_HOOK_RETURN("hud_draw_main") void eo_combat_draw(void) {
    eo_ui_begin();
    eo_combat_draw_body();
    eo_ui_end();
}

// After a battle the diamonds stay until the gems have slid away (the battle flag clears first).
#define SLIDE_GONE 100   // gems this far left are off screen
#define LINGER_FRAMES 90 // give up if they never slide

static int linger = 0;

// For the field's D-pad hint (hints.c), which takes the diamonds' place under the gems outside battle.
int eo_combat_charge_shown(void) {
    return in_battle() || linger > 0;
}

static void eo_combat_draw_body(void) {
    if (!in_battle()) {
        if (linger > 0) {
            linger--;
            if (HUD_GEM_SLIDE >= SLIDE_GONE) {
                linger = 0;
            } else {
                eo_icons_begin();
                draw_charge();
            }
        }
        return;
    }
    linger = LINGER_FRAMES;
    eo_fx_update();
    play_break_sounds();
    draw_spell_description();
    eo_icons_begin();
    draw_charge();
}

// ---- Item drops ----
// Enemies drop items half as often (playtest, 2026-10-21): 1 battle in 8 instead of 1 in 4. Bosses still always do.
// Same as the game's battle_roll_item_drop otherwise: a random enemy's item (monster record +0x34), 0xFF for none.
#define DROP_ODDS 8
#define ENEMY_COUNT (*(u16*)0x8008C59A)
#define RECORD_DROP_ITEM 0x34

RECOMP_PATCH s32 battle_roll_item_drop(void) {
    if (!(gBattleState & BATTLE_BOSS) && get_rand(DROP_ODDS) != 0) {
        return 0xFF;
    }
    u8* record = ENEMY_RECORD(get_rand(ENEMY_COUNT));
    return record != 0 ? record[RECORD_DROP_ITEM] : 0xFF;
}
