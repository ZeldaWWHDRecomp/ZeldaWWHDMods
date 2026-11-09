/* Link's look: a dyer on Windfall who changes the colours of Link's hair, skin, eyes, tunic and its
 * details.
 *
 * Everything is data in manifest.json: the dyer (a placed Windfall resident, the game's own character
 * read from the player's game) and her talk, whose "appearance" action opens the port's colour page
 * (manager 1.11.0: Link in a close camera, a colour for each area with a live preview; the look is kept
 * in the save file when the game saves). Only colours change: the port recolours Link's own textures in
 * memory; this package ships no art. The code only notes in the log when a save file is played, which
 * helps while testing. All texts are original to this mod. */
#include "wwhd_guest.h"
#include "wwhd/functions.h"
#include "wwhd/save.h"

static u32 steps;
static u32 told;

WWHD_HOOK(WWHD_ADDR_daPy_Execute, link_appearance_step, (void* link)) {
    (void)link;
    if (told || ++steps % 30) return; /* twice a second until told */
    u16 max_life = dComIfGs_player_status_a.max_life;
    if (max_life < 12 || max_life > 80) return; /* no save file loaded */
    told = 1;
    wwhd_log("link-appearance: the dyer waits in Windfall's town square");
}
