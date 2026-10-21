#include "eo.h"

// Eltale Odyssey: boot order. The game has just copied its static data (spell tables included) into RAM.
// Enemies get their copies of the vanilla spells first, then Brian's spells are rebalanced.
RECOMP_HOOK_RETURN("boot_load_static_data") void eo_boot(void) {
    eo_enemy_spells_init();
    eo_balance_apply();
    eo_status_init();
}
