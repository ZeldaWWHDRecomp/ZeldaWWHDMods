/* Faster native push/pull steps (#294). The box keeps its own collision, cosine
 * path and final grid snap. Link's setPushPullKeepData follows that actual box
 * delta, so do not synthesize Link movement or edit the shared block table.
 * Public CC0 source: ZeldaWWHDDecomp/wwhd a874e1a9, d_a_obj_movebox.cpp
 * mode_walk (02379834), d_a_player_main_07.cpp setPushPullKeepData (0243CEE0).
 */
#ifndef BP_AT
#define BP_AT(t, a) (*(volatile t*)(a))
#endif
static int block_push_choice;
static struct {
    u32 link; unsigned depth;
    struct { int active; f32 original, written; } rates[2];
} block_push_rate;

static void block_push_option_changed(void) {
    char value[16] = {0};
    wwhd_config_string("block_push_speed", value, sizeof value);
    block_push_choice = value[0] == '1' ? 1 : value[0] == '2' ? 2 : 0;
}

WWHD_HOOK(0x02379834, qol_block_push_tick, (void* actor)) {
    if (!block_push_choice) return;
    const u32 box = (u32)actor;
    const s16 remaining = BP_AT(s16, box + 0x644);
    if (remaining <= 1) return; /* Native final step must reach exactly zero. */
    /* Two counts per tick at 2x. At 1.5x, consume three counts in two ticks.
     * Actor-local countdown phase needs no pointer cache or extra saved state. */
    if (block_push_choice == 2 || remaining % 3 == 0)
        BP_AT(s16, box + 0x644) = remaining - 1;
}

/* Link-only animeUpdate captures pack pointers, then advances Under0 and (when
 * distinct/present) Upper0 without rewriting their rates first. Both controllers
 * are embedded in Link. The intervening FrameCtrl updates touch only their own controller
 * (their sole helper is floating-point remainder); frame copies target
 * animation packs, not these controllers. Scope the rates
 * around this Link-only method, never the global J3DFrameCtrl hot path.
 * CC0 d_a_player_main_01.cpp animeUpdate 023DD054; J3DFrameCtrl.cpp update.
 * Restore only unchanged injected rates, so native stops/changes survive.
 * A nested call on the same Link shares the outer scope without multiplying
 * twice; unrelated nested Link calls cannot consume the outer return. */
WWHD_HOOK(0x023DD054, qol_block_push_animation_before, (void* actor)) {
    const u32 link = (u32)actor;
    if (block_push_rate.depth) {
        if (block_push_rate.link == link) ++block_push_rate.depth;
        return;
    }
    if (!block_push_choice || !qol_guest_ptr(link)) return;
    if (BP_AT(u16, link + 0xE) != 0xA8) return;
    const s16 procedure = BP_AT(s16, link + 0x69AC);
    if (procedure != 0x33 && procedure != 0x34) return;
    const u32 under = BP_AT(u32, link + 0x57FC);
    const u32 upper = BP_AT(u32, link + 0x581C);
    block_push_rate.link = link;
    for (unsigned i = 0; i < 2; ++i) {
        const u32 control = link + (i ? 0x58B8 : 0x5898);
        block_push_rate.rates[i].active = 0;
        if (i && (!upper || upper == under)) continue;
        if (BP_AT(u8, control + 0xE) != 2) continue;
        const f32 rate = BP_AT(f32, control);
        if (!(rate > 0.0f && rate <= 8.0f)) continue;
        block_push_rate.rates[i].active = 1;
        block_push_rate.rates[i].original = rate;
        block_push_rate.rates[i].written = rate * (block_push_choice == 1 ? 1.5f : 2.0f);
        BP_AT(f32, control) = block_push_rate.rates[i].written;
        block_push_rate.depth = 1;
    }
}
WWHD_HOOK_RETURN(0x023DD054, qol_block_push_animation_after, (void* actor)) {
    const u32 link = (u32)actor;
    if (!block_push_rate.depth || block_push_rate.link != link) return;
    if (--block_push_rate.depth) return;
    for (unsigned i = 0; i < 2; ++i) if (block_push_rate.rates[i].active) {
        const u32 control = link + (i ? 0x58B8 : 0x5898);
        if (BP_AT(f32, control) == block_push_rate.rates[i].written)
            BP_AT(f32, control) = block_push_rate.rates[i].original;
    }
}
