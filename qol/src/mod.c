/* QoL package: small gameplay extras, each switched on its own (all off by default) while playing.
 * Features (one file each, included here: the packager compiles this one translation unit):
 *   clothes.c         Link's clothes at any time
 *   replay.c          conducted songs played back, as on the GameCube
 *   picto_subjects.c  the album says whether a picto is good for a figurine
 *   songs.c           play buttons and note pads by touch while Link conducts
 *   one_hit.c         every enemy goes down with any hit
 *   kaboom.c          minigames by touch: Sploosh Kaboom
 *   minigames.c       minigames by touch: the Cabana's puzzles, mail sorting, the cannon game, the auction, figurines
 *   pages.c           album pages: more pages of 12 pictos, kept in this package's files
 * Adding a feature: its file (an <name>_option_changed and its step or hooks), its option in manifest.json and
 * one line in the table below.
 * Ideas from the GreenNaugahyde Android fork (MPL-2.0), reimplemented as a guest mod. */
#include "qol.h"
#include "clothes.c"
#include "replay.c"
#include "picto_subjects.c"
#include "songs.c"
#include "one_hit.c"
#include "kaboom.c"
#include "minigames.c"
#include "pages.c"

static qol_feature features[] = {
    {"clothes", clothes_option_changed, 0},
    {"replay_songs", replay_option_changed, 0},
    {"picto_subjects", picto_option_changed, 0},
    {"song_touch", songs_option_changed, 0},
    {"one_hit_kills", one_hit_option_changed, 0},
    {"minigame_touch", kaboom_option_changed, 0},
    {"minigame_touch", mg_option_changed, 0},
    {"album_pages", pg_option_changed, 0},
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

/* one HUD callback, touch callback and input filter per mod: each feature gets its part */
static void hud(u32 list) {
    picto_draw(list);
    pg_draw(list);
    songs_draw(list);
}
static void touch(const wwhd_touch_event* e) {
    if (!kaboom_touch(e) && !mg_touch(e) && !pg_touch(e)) songs_touch(e);
}
static void filter(wwhd_input_state* s) {
    options();  /* (also while Link doesn't step: a minigame's own camera) */
    songs_filter(s);
    kaboom_filter(s);
    mg_filter(s);
    pg_filter(s);
}

/* Link's step (daPy_Execute), at its start */
WWHD_HOOK(WWHD_ADDR_daPy_Execute, qol_link_step, (void* link)) {
    static int registered;
    if (!registered) {
        registered = 1;
        wwhd_hud_register(hud, WWHD_HUD_TV);
        wwhd_touch_register(touch);
        wwhd_input_filter_register(filter);
    }
    options();
    clothes_step((u32)link);
    replay_step((u32)link);
    songs_step((u32)link);
}
