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
static u32 block_push_link;
static struct { u32 controller; unsigned depth; f32 original, written; } block_push_rate;

static void block_push_option_changed(void) {
    char value[16] = {0};
    wwhd_config_string("block_push_speed", value, sizeof value);
    block_push_choice = value[0] == '1' ? 1 : value[0] == '2' ? 2 : 0;
}
static void block_push_link_step(u32 link) { block_push_link = link; }

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

/* Only the two native walking-push/pull controllers advance faster. The rate
 * exists for this one update call and is restored unless native code changed it
 * (for example, stopping). Other animations and actors are not accelerated. */
WWHD_HOOK(0x027F2FC4, qol_block_push_animation_before, (void* control)) {
    const u32 at = (u32)control;
    if (block_push_rate.depth) {
        if (block_push_rate.controller == at) ++block_push_rate.depth;
        return;
    }
    if (!block_push_choice || !qol_guest_ptr(block_push_link)) return;
    if (at != block_push_link + 0x5898 && at != block_push_link + 0x58B8) return;
    if (BP_AT(u16, block_push_link + 0xE) != 0xA8) return;
    const s16 procedure = BP_AT(s16, block_push_link + 0x69AC);
    if (procedure != 0x33 && procedure != 0x34) return;
    if (BP_AT(u8, at + 0xE) != 2) return; /* Ordinary looping movement only. */
    const f32 rate = BP_AT(f32, at);
    if (!(rate > 0.0f && rate <= 8.0f)) return;
    block_push_rate.controller = at;
    block_push_rate.original = rate;
    block_push_rate.written = rate * (block_push_choice == 1 ? 1.5f : 2.0f);
    block_push_rate.depth = 1;
    BP_AT(f32, at) = block_push_rate.written;
}
WWHD_HOOK_RETURN(0x027F2FC4, qol_block_push_animation_after, (void* control)) {
    const u32 at = (u32)control;
    if (!block_push_rate.depth || block_push_rate.controller != at) return;
    if (--block_push_rate.depth) return;
    if (BP_AT(f32, at) == block_push_rate.written)
        BP_AT(f32, at) = block_push_rate.original;
}
