#include "eo.h"

// The calendar (game design document, "Shop hours and market days"). The game counts days but never uses
// them, and its 7-day week (0x8008C634) is an unused leftover; the overhaul makes the week real.
// Times of day come from the game's own lighting table (ROM, RAM 0x8004C3F8): night until tick 5625, day until
// 20250, evening until 25875, then night again (research/time).

#define DAWN 5625
#define NIGHTFALL 25875

static const char* const weekdays[7] = {
    "Fireday", "Earthday", "Waterday", "Windday", "Spiritday", "Moonday", "Sunday",
};

const char* eo_weekday_name(u32 day) {
    return weekdays[day % 7];
}

int eo_is_night(void) {
    return gCurrentTime < DAWN || gCurrentTime >= NIGHTFALL;
}

// ---- Inns wake you at dawn ----
// The rest itself adds 9000 ticks through time_set (research/npcs, "The inn"); once that has run, the clock is
// moved to dawn of the next morning instead (or of this morning, when resting after midnight).

static int resting = 0;
static u16 wake_day;

void eo_calendar_on_rest(void) {
    wake_day = gCurrentTime < DAWN ? gCurrentDay : (u16)(gCurrentDay + 1);
    resting = 1;
}

RECOMP_HOOK_RETURN("time_set") void eo_calendar_after_time_set(void) {
    if (!resting) {
        return;
    }
    resting = 0;
    gCurrentDay = wake_day;
    gCurrentTime = DAWN;
}
