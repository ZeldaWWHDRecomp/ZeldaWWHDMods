/* Enemy sword-hit freeze, requested in port issue #244. Public CC0 WWHD
 * d/d_cc_uty.cpp cc_at_check (025192A8) writes nextPauseTimer (101EACB7)
 * after its damage, spin, critical/finishing-hit effects and sound work.
 * Observe that completed path rather than replacing any combat calculation.
 *
 * Its only early exit is a missing attack actor. at_power_check leaves stale
 * result/actor fields if mpObj is null, so BOTH object and current Link must
 * match before treating this as a new sword pause. A valid object resets the
 * actor/result before that early exit. Hammer/other attacks never qualify.
 * The active pause byte (101EACB6), menus and event pauses are not edited.
 *
 * Durations are native 30-Hz logical ticks. The play scene is unconverted in
 * true60::classify and executes only on full passes: never scale by display Hz.
 * Source: public CC0 wwhd_src, pin a874e1 (d_cc_uty.cpp, d_s_play.cpp). */
static int hitstop_mode; /* 0 original, 1 half (round up), 2 off */
static void hitstop_option_changed(void) {
    char value[24] = {0};
    wwhd_config_string("enemy_hit_freeze", value, sizeof value);
    hitstop_mode = value[0] == 'H' ? 1 : value[0] == 'O' && value[1] == 'f' ? 2 : 0;
}

/* Native CcAtInfo prefix: pointers at 0/4; damage/dead/result bytes at 8/9/A. */
typedef struct { u32 object, actor; u8 damage, dead, result, padding; } qol_hit_info;

WWHD_HOOK_RETURN(0x025192A8, qol_enemy_hit_freeze, (fopAc_ac_c* target, qol_hit_info* hit)) {
    if (!hitstop_mode || !target || target->group != 2 || !hit || !hit->object ||
        !hit->actor || hit->result != 1) return;
    u8* play = qol_play();
    if (!play || play[QOL_EVENT_MODE] || hit->actor != *(u32*)(play + QOL_PLAYER)) return;
    volatile u8* pending = &WWHD_GAME_DATA(0x101EACB7, u8);
    /* Nonpositive signed/debug values retain native semantics. Half retains at
     * least one logical tick; Off alone removes the ordinary one-tick freeze. */
    u8 duration = *pending;
    if (duration == 0 || duration > 127) return;
    *pending = hitstop_mode == 2 ? 0 : (u8)((duration + 1) / 2);
}
