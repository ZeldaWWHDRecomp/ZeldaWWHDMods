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
 * Durations are native 30-Hz logic ticks: the play scene steps once per 30-Hz
 * logic tick at every display rate, so never scale them by the display rate.
 * Source: public CC0 wwhd_src, pin a874e1 (d_cc_uty.cpp, d_s_play.cpp).
 *
 * Kinds (options "hit_freeze_<kind>": which pauses the setting changes): a sword hit is a defeat when the enemy's
 * health is gone, strong when it asks for 4 or more steps (knocked down, a Darknut's armour, a Stalfos' head), else
 * a hit; any of them on a boss is the boss kind. Bosses and signposts also ask for the pause from their own step
 * (d_a_bmd_exec.cpp, d_a_bgn3.cpp, d_a_kanban_exec.cpp): that request is caught around fopAc_Execute by the
 * actor's process name (its profile number, the same in every release) and changed the same way. Every other pause is left alone. With the default kinds
 * (hits, strong hits, defeats and bosses; not signs) the setting changes the sword-hit pauses it changed before,
 * plus the bosses' own pauses. "Original" (the default) changes nothing.
 * Kinds and the boss / signpost list from GreenNaugahyde/ZeldaWWHDRecompAndroid db65d60 (MPL-2.0),
 * mods/turbo.cpp hit_stop_step (there by the USA execute functions; here by the process names of their profiles). */
static int hitstop_mode; /* 0 original, 1 half (round up), 2 off */
enum { HITSTOP_HIT = 1, HITSTOP_STRONG = 2, HITSTOP_DEFEAT = 4, HITSTOP_BOSS = 8, HITSTOP_SIGN = 16 };
static int hitstop_kinds = HITSTOP_HIT | HITSTOP_STRONG | HITSTOP_DEFEAT | HITSTOP_BOSS; /* the kinds it changes */
static void hitstop_option_changed(void) {
    char value[24] = {0};
    wwhd_config_string("enemy_hit_freeze", value, sizeof value);
    hitstop_mode = value[0] == 'H' ? 1 : value[0] == 'O' && value[1] == 'f' ? 2 : 0;
}
static void hitstop_kinds_changed(void) {
    hitstop_kinds = (wwhd_config_bool("hit_freeze_hits", 1) ? HITSTOP_HIT : 0) |
                    (wwhd_config_bool("hit_freeze_strong", 1) ? HITSTOP_STRONG : 0) |
                    (wwhd_config_bool("hit_freeze_defeats", 1) ? HITSTOP_DEFEAT : 0) |
                    (wwhd_config_bool("hit_freeze_bosses", 1) ? HITSTOP_BOSS : 0) |
                    (wwhd_config_bool("hit_freeze_signs", 0) ? HITSTOP_SIGN : 0);
}

/* Native CcAtInfo prefix: pointers at 0/4; damage/dead/result bytes at 8/9/A. */
typedef struct { u32 object, actor; u8 damage, dead, result, padding; } qol_hit_info;

/* base_process_class::mProcName (+0x8, what fpcM_GetName reads): the actor's profile number, the same in USA and EU.
 * Each listed profile is the only one using that actor's execute function (checked in both executables' profile
 * lists). Public CC0 wwhd_src fpcNm_*_e: */
#define HITSTOP_PROC_NAME 0x8
static const s16 hitstop_boss_names[] = {
    0xEE, /* BDK: Helmaroc King (daBdk_Execute) */
    0xEF, /* BDKOBJ: its parts (daBdkobj_Execute) */
    0xF3, /* BGN: Puppet Ganon, first form (daBgn_Execute) */
    0xF4, /* BGN2: second form (daBgn2_Execute) */
    0xF5, /* BGN3: third form (daBgn3_Execute) */
    0xEB, /* BMD: Kalle Demos (daBmd_Execute) */
    0xF0, /* BST: Gohdan (daBst_Execute) */
    0xEA, /* BTD: Gohma (daBtd_Execute) */
    0xD9, /* BWD: Molgera (daBwd_Execute) */
    0xF1, /* FGANON: Phantom Ganon (daFganon_Execute) */
    0xF6, /* GND: Ganondorf (daGnd_Execute) */
};
#define HITSTOP_KANBAN_NAME 0xB6 /* KANBAN: signposts (daKanban_Execute) */
static int hitstop_kind_of_actor(const void* actor) {
    if (!actor) return 0;
    const s16 name = *(const volatile s16*)((const u8*)actor + HITSTOP_PROC_NAME);
    if (name == HITSTOP_KANBAN_NAME) return HITSTOP_SIGN;
    for (u32 i = 0; i < sizeof hitstop_boss_names / sizeof *hitstop_boss_names; i++)
        if (name == hitstop_boss_names[i]) return HITSTOP_BOSS;
    return 0;
}
/* the request for a pause of `kind` as the setting has it (unchanged when that kind is not chosen); `before` is
 * what Off puts back. Nonpositive signed/debug values retain native semantics. Half retains at least one logical
 * tick; Off alone removes the ordinary one-tick freeze. */
static u8 hitstop_apply(int kind, u8 duration, u8 before) {
    if (!hitstop_mode || !(hitstop_kinds & kind) || duration == 0 || duration > 127) return duration;
    return hitstop_mode == 2 ? before : (u8)((duration + 1) / 2);
}

static int hitstop_sword_done; /* the sword-hit path handled the request of the actor step under way */

WWHD_HOOK_RETURN(0x025192A8, qol_enemy_hit_freeze, (fopAc_ac_c* target, qol_hit_info* hit)) {
    if (!hitstop_mode || !target || target->group != 2 || !hit || !hit->object ||
        !hit->actor || hit->result != 1) return;
    u8* play = qol_play();
    if (!play || play[QOL_EVENT_MODE] || hit->actor != *(u32*)(play + QOL_PLAYER)) return;
    volatile u8* pending = &WWHD_GAME_DATA(0x101EACB7, u8);
    u8 duration = *pending;
    if (duration == 0 || duration > 127) return;
    const int kind = hitstop_kind_of_actor(target) == HITSTOP_BOSS ? HITSTOP_BOSS
                   : target->health <= 0 ? HITSTOP_DEFEAT
                   : duration >= 4 ? HITSTOP_STRONG : HITSTOP_HIT;
    *pending = hitstop_apply(kind, duration, 0);
    hitstop_sword_done = 1;
}

/* around each actor's step (fopAc_Execute 025D475C): a boss's or a signpost's own request */
static u8 hitstop_before; /* the request before the step */
static int hitstop_watch;  /* the kind of the actor stepping, while its request is watched */
static void hitstop_actor_before(void* actor) {
    hitstop_watch = 0;
    hitstop_sword_done = 0;
    if (!hitstop_mode || !actor || !(hitstop_kinds & (HITSTOP_BOSS | HITSTOP_SIGN))) return;
    const int kind = hitstop_kind_of_actor(actor);
    if (!(hitstop_kinds & kind) || qol_event()) return;
    hitstop_watch = kind;
    hitstop_before = WWHD_GAME_DATA(0x101EACB7, u8);
}
WWHD_HOOK_RETURN(WWHD_ADDR_fopAc_Execute, qol_hit_freeze_actor_step, (void* actor)) {
    (void)actor;
    const int kind = hitstop_watch;
    hitstop_watch = 0;
    if (!kind || hitstop_sword_done) return;
    volatile u8* pending = &WWHD_GAME_DATA(0x101EACB7, u8);
    const u8 now = *pending;
    if (now == hitstop_before) return; /* the step asked for no pause */
    *pending = hitstop_apply(kind, now, hitstop_before);
}
