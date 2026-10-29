/* One-hit kills: every enemy goes down with any hit. Option "one_hit_kills" (Combat), off by default.
 * Every actor's step goes through fopAc_Execute (025D475C); an enemy (fopAc_ac_c group 2, fopAc_ENEMY_e) keeps its
 * health at 1, so any hit takes its last point. Bosses that count hits themselves are not changed by it.
 * Idea from the GreenNaugahyde Android fork (MPL-2.0), mods/cheats.cpp; reimplemented as a guest mod. */
#define ONE_HIT_ENEMY 2

static int one_hit_on;
static u32 one_hit_count;  /* enemies held at 1 so far (log) */

static void one_hit_option_changed(void) { one_hit_on = wwhd_config_bool("one_hit_kills", 0); }

WWHD_HOOK(WWHD_ADDR_fopAc_Execute, qol_actor_step, (void* actor)) {
    if (!one_hit_on || !actor) return;
    fopAc_ac_c* a = (fopAc_ac_c*)actor;
    if (a->group != ONE_HIT_ENEMY || a->health <= 1) return;
    a->health = 1;
    if (one_hit_count++ < 3) wwhd_log("one-hit kills: an enemy is down to one point");
}
