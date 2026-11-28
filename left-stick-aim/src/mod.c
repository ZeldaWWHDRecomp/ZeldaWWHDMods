/* Left Stick Aim: GameCube-style first-person aiming for Wind Waker HD.
 * While Link is in a first-person view, the left stick is sent to the game as the right stick.
 * Click the left stick (L3) to switch between this and the normal controls. */
#include "wwhd_guest.h"
#include "wwhd/functions.h"

#define BUTTON_STICK_L 0x00040000u /* VPAD left stick click */

static u32 aim_step;
static int aim_seen;
static int registered;
static int enabled = 1;
static u32 l3_was_down;

static void mark_aiming(void)
{
    aim_step = (u32)wwhd_logic_step();
    aim_seen = 1;
}

static void filter(wwhd_input_state* s)
{
    u32 l3 = s->buttons & BUTTON_STICK_L;
    if (l3 && !l3_was_down) {
        enabled = !enabled;
        wwhd_log_int("left-stick-aim: enabled", enabled);
    }
    l3_was_down = l3;
    s->buttons &= ~BUTTON_STICK_L; /* the game never sees the toggle button */

    /* The aim hooks run after the controller read, so the mark is at most 2 steps old. */
    if (!enabled || !aim_seen || (u32)wwhd_logic_step() - aim_step > 2)
        return;
    /* Keep whichever stick is pushed further, so the right stick still aims too. */
    if (s->lx * s->lx + s->ly * s->ly > s->rx * s->rx + s->ry * s->ry) {
        s->rx = s->lx;
        s->ry = s->ly;
    }
    s->lx = 0.0f;
    s->ly = 0.0f;
}

WWHD_HOOK(WWHD_ADDR_daPy_Execute, on_link_step, (void* link))
{
    (void)link;
    if (!registered)
        registered = wwhd_input_filter_register(filter);
}

/* First-person camera (look around, telescope...). */
WWHD_HOOK(WWHD_ADDR_dCamera_c__subjectCamera_025071FC, on_subject_camera, (void* camera, s32 style)) { (void)camera; (void)style; mark_aiming(); }
/* Look, bow, boomerang, hookshot, grappling hook. */
WWHD_HOOK(WWHD_ADDR_daPy_lk_c__procSubjectivity_0241701C, on_look, (void* link)) { (void)link; mark_aiming(); }
WWHD_HOOK(WWHD_ADDR_daPy_lk_c__procBowSubject_024379C0, on_bow, (void* link)) { (void)link; mark_aiming(); }
WWHD_HOOK(WWHD_ADDR_daPy_lk_c__procBoomerangSubject_024376D8, on_boomerang, (void* link)) { (void)link; mark_aiming(); }
WWHD_HOOK(WWHD_ADDR_daPy_lk_c__procHookshotSubject_02437C0C, on_hookshot, (void* link)) { (void)link; mark_aiming(); }
WWHD_HOOK(WWHD_ADDR_daPy_lk_c__procRopeSubject_024350A0, on_rope, (void* link)) { (void)link; mark_aiming(); }
/* The same on the boat. */
WWHD_HOOK(WWHD_ADDR_daPy_lk_c__procShipScope_024327A4, on_ship_scope, (void* link)) { (void)link; mark_aiming(); }
WWHD_HOOK(WWHD_ADDR_daPy_lk_c__procShipBow_0243333C, on_ship_bow, (void* link)) { (void)link; mark_aiming(); }
WWHD_HOOK(WWHD_ADDR_daPy_lk_c__procShipBoomerang_02432B28, on_ship_boomerang, (void* link)) { (void)link; mark_aiming(); }
WWHD_HOOK(WWHD_ADDR_daPy_lk_c__procShipHookshot_02433104, on_ship_hookshot, (void* link)) { (void)link; mark_aiming(); }
