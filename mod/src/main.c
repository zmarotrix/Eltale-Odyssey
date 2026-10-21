#include "eo.h"

// Eltale Odyssey: boot order. The game has just copied its static data (spell tables included) into RAM.
// Enemies get their copies of the vanilla spells first, then Brian's spells are rebalanced, then the
// spirit system remembers each spell's vanilla level and name before it starts changing them.
RECOMP_HOOK_RETURN("boot_load_static_data") void eo_boot(void) {
    eo_enemy_spells_init();
    eo_balance_apply();
    eo_spirits_init_tables();
    eo_status_init();
    eo_icons_init();
}
