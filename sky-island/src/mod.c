/* The sky island: a side quest with a stage of its own.
 *
 * Everything is data in manifest.json: the island stage and its arena ("stages", built on the player's machine
 * from the player's own game files and this package's models), the Helmaroc King's flight from Outset's pier
 * ("scenes"), the island far off in Outset's sky ("actors", "models") and the quest with its letter ("quests").
 * The code only logs the quest's step as it changes, which helps while testing. */
#include "wwhd_guest.h"
#include "wwhd/functions.h"
#include "wwhd/save.h"

static u32 steps;
static s32 last = -100;

WWHD_HOOK(WWHD_ADDR_daPy_Execute, sky_island_step, (void* link)) {
    (void)link;
    if (++steps % 30) return; /* twice a second is plenty */
    u16 max_life = dComIfGs_player_status_a.max_life;
    if (max_life < 12 || max_life > 80) return; /* no save file loaded */
    s32 state = wwhd_quest_state("sky");
    if (state != last) {
        last = state;
        wwhd_log_int("sky-island: quest step (255 done)", state);
    }
}
