/* SPDX-License-Identifier: MPL-2.0
 * Actual synchronous shape drawing, not model preparation/queue insertion.
 * J3DShapeSubmission.cpp: 027F16E8 submitShape, 027F1FA8 prepareShape;
 * J3DPacketAdapter.h: ShapePacket::mModel at +9c. The native calls consume
 * material state/texture descriptors and submit GPU commands before returning.
 * Never leave an FMAT edit or a pointer into the mod arena across this boundary:
 * frame-boundary savestates and later model instances see native resources.
 */
#pragma once
typedef struct { u32 model, material; } DLShapeScope;
static DLShapeScope dl_shape_scopes[DL_DRAW_DEPTH];
static u32 dl_shape_depth;
static void dl_shape_apply(DLShapeScope scope) {
    if (!scope.model || !dl_enabled || !dl_state.active || !dl_texture_model(scope.model)) return;
    dl_texture_apply((void*)scope.model,scope.material);
    if (scope.model==dl_word(dl_owner,0x5700) && dl_aura_active(dl_owner))
        dl_aura_blend(dl_owner,scope.material);
}
static void dl_shape_begin(void* packet) {
    /* Inactive mods do not touch bindings, packet memory or the player lookup.
     * Existing nested scopes must still balance if activation changes inside. */
    if (!dl_shape_depth && (!dl_enabled || !dl_state.active)) return;
    if (dl_shape_depth) dl_texture_restore_all();
    u32 depth=dl_shape_depth++;
    if (depth>=DL_DRAW_DEPTH) return;
    DLShapeScope scope={0};
    if (dl_enabled && dl_state.active && dl_game_span((u32)packet,0xa0)) {
        u32 resource=dl_word(packet,0x98);
        u32 header=dl_game_span(resource,4)?*(u32*)resource:0;
        /* J3DShapePacketResources.h: header binding index at +0x0e.
         * Apply only the material the native call is about to consume. */
        if (dl_game_span(header,0x10)) {
            scope.model=dl_word(packet,0x9c);
            scope.material=*(u16*)(header+0x0e);
        }
    }
    dl_shape_scopes[depth]=scope;
    if (scope.model) dl_shape_apply(scope);
}
static void dl_shape_end(void) {
    if (!dl_shape_depth) return;
    dl_texture_restore_all();
    --dl_shape_depth;
    if (dl_shape_depth && dl_shape_depth<=DL_DRAW_DEPTH)
        dl_shape_apply(dl_shape_scopes[dl_shape_depth-1]);
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
