/* QoL package: small gameplay extras, each switched on its own (all off by default) while playing.
 * Features (one file each, included here: the packager compiles this one translation unit):
 *   clothes.c         Link's clothes at any time
 *   replay.c          conducted songs played back, as on the GameCube
 *   picto_subjects.c  the album says whether a picto is good for a figurine
 * Adding a feature: its file (an <name>_option_changed and its step or hooks), its option in manifest.json and
 * one line in the table below.
 * Ideas from the GreenNaugahyde Android fork (MPL-2.0), reimplemented as a guest mod. */
#include "qol.h"
#include "clothes.c"
#include "replay.c"
#include "picto_subjects.c"

static qol_feature features[] = {
    {"clothes", clothes_option_changed, 0},
    {"replay_songs", replay_option_changed, 0},
    {"picto_subjects", picto_option_changed, 0},
};

/* the live options: a feature reads its option again when its revision changed (also once at start) */
static void options(void) {
    for (u32 i = 0; i < sizeof features / sizeof *features; i++) {
        unsigned long long r = wwhd_config_changed(features[i].option);
        if (r == features[i].seen) continue;
        features[i].seen = r;
        features[i].changed();
    }
}

/* Link's step (daPy_Execute), at its start */
WWHD_HOOK(WWHD_ADDR_daPy_Execute, qol_link_step, (void* link)) {
    options();
    clothes_step((u32)link);
    replay_step((u32)link);
    picto_step();
}
