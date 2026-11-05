/* SPDX-License-Identifier: MPL-2.0
 * Actual synchronous shape drawing, not model preparation/queue insertion.
 * J3DShapeSubmission.cpp: 027F16E8 submitShape, 027F1FA8 prepareShape;
 * J3DPacketAdapter.h: ShapePacket::mModel at +9c. The native calls consume
 * material state/texture descriptors and submit GPU commands before returning.
 * Never leave an FMAT edit or a pointer into the mod arena across this boundary:
 * frame-boundary savestates and later model instances see native resources.
 */
#pragma once
static u32 dl_shape_models[DL_DRAW_DEPTH], dl_shape_depth;
static void dl_shape_apply(u32 model) {
    if (!dl_texture_model(model)) return;
    dl_texture_apply((void*)model);
    if (dl_owner && model==dl_word(dl_owner,0x5700) && dl_aura_active(dl_owner))
        dl_aura_blend(dl_owner);
}
static void dl_shape_begin(void* packet) {
    /* Suspend the outer scope before a nested/unrelated draw, including when
     * both models share resource data. Reapply its current textures on return. */
    dl_texture_restore_all();
    u32 depth=dl_shape_depth++;
    if (depth>=DL_DRAW_DEPTH) return;
    u32 model=dl_game_span((u32)packet,0xa0)?dl_word(packet,0x9c):0;
    dl_shape_models[depth]=model;
    dl_shape_apply(model);
}
static void dl_shape_end(void) {
    dl_texture_restore_all();
    if (!dl_shape_depth) return;
    --dl_shape_depth;
    if (dl_shape_depth && dl_shape_depth<=DL_DRAW_DEPTH)
        dl_shape_apply(dl_shape_models[dl_shape_depth-1]);
}
WWHD_HOOK(0x027F16E8, dl_shape_before, (void* packet, void* input)) {
    (void)input; dl_shape_begin(packet);
}
WWHD_HOOK_RETURN(0x027F16E8, dl_shape_after, (void* packet, void* input)) {
    (void)packet; (void)input; dl_shape_end();
}
WWHD_HOOK(0x027F1FA8, dl_shape_prepare_before, (void* packet, void* input)) {
    (void)input; dl_shape_begin(packet);
}
WWHD_HOOK_RETURN(0x027F1FA8, dl_shape_prepare_after, (void* packet, void* input)) {
    (void)packet; (void)input; dl_shape_end();
}
