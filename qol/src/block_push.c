/* Faster native push/pull steps (#294). The box keeps its own collision, cosine
 * path and final grid snap. Link's setPushPullKeepData follows that actual box
 * delta, so do not synthesize Link movement or edit the shared block table.
 * Public CC0 source: ZeldaWWHDDecomp/wwhd a874e1a9, d_a_obj_movebox.cpp
 * mode_walk (02379834), d_a_player_main_07.cpp setPushPullKeepData (0243CEE0).
 *
 * HD offsets, not the GameCube ones the decomp's field names still carry:
 * - the box's walk countdown is daObjMovebox::Act_c::m644 (GameCube name) at HD +0x75C:
 *   mode_walk 023798C8 lha r10,0x75C(r26) / 02379928 sth r10,0x75C(r26), r26 = the box.
 * - Link's procedure is daPy_lk_c.mCurProc (s32, HD +0x65F0; commonProcInit 023E00BC stw).
 *   PUSH_MOVE 0x33 and PULL_MOVE 0x34 (d_a_player_main_07.h).
 * Link's fields come from the SDK (wwhd/link.h, wwhd/animation.h); the movebox has no SDK view.
 */
#ifndef BP_OFFSET
#define BP_OFFSET(type, field) ((u32)__builtin_offsetof(type, field))
#endif
#define BP_BOX_WALK_COUNT 0x75Cu            /* daObjMovebox::Act_c m644, s16 */
#define BP_PROF_NAME 0xEu                   /* base_process_class mProfName, s16 */
#define BP_PROC_PLAYER 0xA8
#define BP_PROC_PUSH_MOVE 0x33
#define BP_PROC_PULL_MOVE 0x34
#define BP_LINK_PROC BP_OFFSET(daPy_lk_c, mCurProc)
/* mDoExt_AnmRatioPack[0].mpAnmTransform (+4): animeUpdate 023DD068/023DD070 lwz 0x581C/0x57FC */
#define BP_LINK_UNDER_PACK (BP_OFFSET(daPy_lk_c, mAnmRatioUnder) + 4)
#define BP_LINK_UPPER_PACK (BP_OFFSET(daPy_lk_c, mAnmRatioUpper) + 4)
#define BP_LINK_UNDER_CTRL BP_OFFSET(daPy_lk_c, mFrameCtrlUnder)
#define BP_LINK_UPPER_CTRL BP_OFFSET(daPy_lk_c, mFrameCtrlUpper)
#define BP_CTRL_RATE BP_OFFSET(J3DFrameCtrl, mRate)
#define BP_CTRL_ATTRIBUTE BP_OFFSET(J3DFrameCtrl, mAttribute)
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
    const s16 remaining = BP_AT(s16, box + BP_BOX_WALK_COUNT);
    if (remaining <= 1) return; /* Native final step must reach exactly zero. */
    /* Two counts per tick at 2x. At 1.5x, consume three counts in two ticks.
     * Actor-local countdown phase needs no pointer cache or extra saved state. */
    if (block_push_choice == 2 || remaining % 3 == 0)
        BP_AT(s16, box + BP_BOX_WALK_COUNT) = remaining - 1;
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
    if (BP_AT(s16, link + BP_PROF_NAME) != BP_PROC_PLAYER) return;
    const s32 procedure = BP_AT(s32, link + BP_LINK_PROC);
    if (procedure != BP_PROC_PUSH_MOVE && procedure != BP_PROC_PULL_MOVE) return;
    const u32 under = BP_AT(u32, link + BP_LINK_UNDER_PACK);
    const u32 upper = BP_AT(u32, link + BP_LINK_UPPER_PACK);
    block_push_rate.link = link;
    for (unsigned i = 0; i < 2; ++i) {
        const u32 control = link + (i ? BP_LINK_UPPER_CTRL : BP_LINK_UNDER_CTRL);
        block_push_rate.rates[i].active = 0;
        if (i && (!upper || upper == under)) continue;
        if (BP_AT(u8, control + BP_CTRL_ATTRIBUTE) != 2) continue;
        const f32 rate = BP_AT(f32, control + BP_CTRL_RATE);
        if (!(rate > 0.0f && rate <= 8.0f)) continue;
        block_push_rate.rates[i].active = 1;
        block_push_rate.rates[i].original = rate;
        block_push_rate.rates[i].written = rate * (block_push_choice == 1 ? 1.5f : 2.0f);
        BP_AT(f32, control + BP_CTRL_RATE) = block_push_rate.rates[i].written;
        block_push_rate.depth = 1;
    }
}
WWHD_HOOK_RETURN(0x023DD054, qol_block_push_animation_after, (void* actor)) {
    const u32 link = (u32)actor;
    if (!block_push_rate.depth || block_push_rate.link != link) return;
    if (--block_push_rate.depth) return;
    for (unsigned i = 0; i < 2; ++i) if (block_push_rate.rates[i].active) {
        const u32 control = link + (i ? BP_LINK_UPPER_CTRL : BP_LINK_UNDER_CTRL);
        if (BP_AT(f32, control + BP_CTRL_RATE) == block_push_rate.rates[i].written)
            BP_AT(f32, control + BP_CTRL_RATE) = block_push_rate.rates[i].original;
    }
}
