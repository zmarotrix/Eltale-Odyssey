#include "eo.h"

// The D-pad hint (playtest, 2026-10-21: "a little d-pad that slides in and out along with the rest of the hud, and
// icons that show what it does"). In the field, under the element gems where the charge diamonds sit in battle:
// the D-pad with its up arm lit and a Spirit Point pip (the Spirits window), then with its down arm lit and a staff
// (the Equipment window). It slides with the gems. The up arm blinks while Brian has unspent Spirit Points, as a
// prompt to spend them.

#define HINT_Y 216            // the charge diamonds' row (combat.c)
#define HINT_UP_X 34
#define HINT_DOWN_X 74
#define HINT_GAP 13           // from a D-pad to its icon
#define BLINK_FRAMES 20
#define HUD_GEM_SLIDE (*(s32*)0x8008C664)   // the gems are drawn at x 28 minus this (0 when shown)

extern u16 gBattleState;

static int frames = 0;

static int field_quiet(void) {
    return gGameMode == 1 && !(gBattleState & 0x1) && !(gGameState & 0x3) && !eo_shop_is_open() &&
           !eo_spirits_window_open() && !eo_equipment_window_open() && !eo_combat_charge_shown() &&
           gEo.spirits_ready;
}

static void eo_hint_draw_body(void) {
    frames++;
    if (!field_quiet()) {
        return;
    }
    s32 slide = HUD_GEM_SLIDE;
    int blink = gEo.spirit_points > 0 && (frames / BLINK_FRAMES) % 2 != 0;
    eo_icons_begin();
    eo_icon_draw(blink ? ICON_DPAD : ICON_DPAD_UP, HINT_UP_X - slide, HINT_Y);
    eo_icon_draw(ICON_PIP_FULL, HINT_UP_X + HINT_GAP - slide, HINT_Y);
    eo_icon_draw(ICON_DPAD_DOWN, HINT_DOWN_X - slide, HINT_Y);
    eo_icon_draw(ICON_STAFF, HINT_DOWN_X + HINT_GAP - slide, HINT_Y);
}

// Drawn into the mod's own display list (ui.c).
RECOMP_HOOK_RETURN("hud_draw_main") void eo_hint_draw(void) {
    eo_ui_begin();
    eo_hint_draw_body();
    eo_ui_end();
}
