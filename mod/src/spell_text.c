#include "eo.h"

// Spell names and the one-line descriptions shown while a spell is selected in battle. Both live with the spells
// themselves now (spells.c); names are at most 15 characters, lines use the HUD's small font (uppercase, digits
// and : ' - . ? = only; no commas).

const char* eo_spell_name(int school, int index) {
    return eo_spells_name(school, index);
}

const char* eo_spell_description(int school, int index) {
    return eo_spells_text(school, index);
}
