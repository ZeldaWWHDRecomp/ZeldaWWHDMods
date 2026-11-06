/* SPDX-License-Identifier: MPL-2.0
 * Actual synchronous shape drawing, not model preparation/queue insertion.
 * J3DShapeSubmission.cpp: 027F16E8 submitShape, 027F1FA8 prepareShape;
 * J3DPacketAdapter.h: ShapePacket::mModel at +9c. The native calls consume
 * material state/texture descriptors and submit GPU commands before returning.
 * Never leave an FMAT edit or a pointer into the mod arena across this boundary:
 * frame-boundary savestates and later model instances see native resources.
 *
 * Both functions are leaves (they never draw another shape), but the main
 * thread and a sead worker thread run them for Link AT THE SAME TIME (see
 * dl_draw_lock). So there is no per-thread nesting to track: each draw is one
 * scope, found again by its own (packet, input) arguments in the return hook.
 * A binding stays applied while any draw still uses it; the last one restores.
 */
#pragma once
#define DL_SCOPES 32u
#define DL_SCOPE_BINDINGS 8u
typedef struct { u32 packet, input, aura, count, held[DL_SCOPE_BINDINGS]; } DLShapeScope;
static DLShapeScope dl_shape_scopes[DL_SCOPES];
static u32 dl_shape_count;
static void dl_texture_restore_all(void) {
    dl_lock();
    dl_texture_restore_locked();
    dl_aura_restore();
    dl_shape_count = 0; /* draws still in flight find no scope and change nothing */
    dl_unlock();
}
/* Model windows (main thread, visuals.h). 027F55FC copies each material's FMAT
 * render state into the model's packet words (ModelFrameUpdate.cpp), and the
 * shape draws bind those copies (bindShapeDrawState -> 027E220C), not the FMAT.
 * So the body blend and the aura shell blend must also be in place while that
 * copy runs, exactly like the palette; restored when the window returns. */
static void dl_view_patch(u32 depth, void* model, int part, const u32* resources, u32 count) {
    u32 materials = dl_word(model, 0x34), native_count = *(u16*)((u8*)model + 0x2a);
    if (!native_count || native_count > DL_MAX_MATERIALS || !dl_game_span(materials, native_count * 0x3c)) return;
    dl_lock();
    dl_view_held_aura[depth] = part == 7;
    if (part == 7) {
        /* The native Magic Armor shell: only while the aura is shown. */
        for (u32 i = 0; i < native_count && dl_aura_active(dl_owner); ++i) {
            u32 serial = dl_aura_blend(dl_owner, i);
            if (serial) dl_view_held[depth][dl_view_held_count[depth]++] = serial;
        }
    } else {
        for (u32 i = 0; i < count; ++i) {
            u32 resource = resources[i];
            if (!dl_game_span(resource, 0x10)) continue;
            u32 index = *(u16*)(resource + 0xc);
            if (index >= native_count || dl_word((void*)(materials + index * 0x3c), 0) != resource) continue;
            u32 state = dl_body_acquire((u32)model, materials, index, resource);
            if (state) dl_view_held[depth][dl_view_held_count[depth]++] = state;
        }
    }
    dl_unlock();
}
static void dl_view_unpatch(u32 depth) {
    if (!dl_view_held_count[depth]) return;
    dl_lock();
    for (u32 i = 0; i < dl_view_held_count[depth]; ++i) {
        if (dl_view_held_aura[depth]) dl_aura_release(dl_view_held[depth][i]);
        else dl_body_release(dl_view_held[depth][i]);
    }
    dl_view_held_count[depth] = 0;
    dl_unlock();
}
static void dl_shape_begin(void* packet, void* input) {
    /* Inactive mods do not touch bindings, packet memory or the player lookup. */
    if (!dl_enabled || !dl_state.active || !dl_game_span((u32)packet,0xa0)) return;
    u32 resource=dl_word(packet,0x98);
    u32 header=dl_game_span(resource,4)?*(u32*)resource:0;
    /* J3DShapePacketResources.h: header binding index at +0x0e.
     * Apply only the material the native call is about to consume. */
    if (!dl_game_span(header,0x10)) return;
    u32 model=dl_word(packet,0x9c), material=*(u16*)(header+0x0e);
    if (!model || !dl_texture_model(model)) return;
    s32 eyes[DL_EYE_COUNT];
    dl_texture_eyes((void*)model, eyes);
    dl_lock();
    if (dl_enabled && dl_state.active && dl_shape_count < DL_SCOPES) {
        DLShapeScope* scope = &dl_shape_scopes[dl_shape_count];
        *scope = (DLShapeScope){.packet=(u32)packet, .input=(u32)input};
        dl_texture_apply((void*)model, material, eyes, scope->held, &scope->count, DL_SCOPE_BINDINGS);
        if (model==dl_word(dl_owner,0x5700) && dl_aura_active(dl_owner))
            scope->aura = dl_aura_blend(dl_owner, material);
        if (scope->count || scope->aura) ++dl_shape_count;
    }
    dl_unlock();
}
static void dl_shape_end(void* packet, void* input) {
    /* This call's own scope, if any, was published by this same thread. */
    if (!__atomic_load_n(&dl_shape_count, __ATOMIC_ACQUIRE)) return;
    dl_lock();
    for (u32 i = dl_shape_count; i--;) {
        DLShapeScope* scope = &dl_shape_scopes[i];
        if (scope->packet != (u32)packet || scope->input != (u32)input) continue;
        for (u32 h = 0; h < scope->count; ++h) dl_binding_release(scope->held[h]);
        if (scope->aura) dl_aura_release(scope->aura);
        *scope = dl_shape_scopes[--dl_shape_count];
        break;
    }
    dl_unlock();
}
WWHD_HOOK(0x027F16E8, dl_shape_before, (void* packet, void* input)) {
    dl_shape_begin(packet, input);
}
WWHD_HOOK_RETURN(0x027F16E8, dl_shape_after, (void* packet, void* input)) {
    dl_shape_end(packet, input);
}
WWHD_HOOK(0x027F1FA8, dl_shape_prepare_before, (void* packet, void* input)) {
    dl_shape_begin(packet, input);
}
WWHD_HOOK_RETURN(0x027F1FA8, dl_shape_prepare_after, (void* packet, void* input)) {
    dl_shape_end(packet, input);
}
