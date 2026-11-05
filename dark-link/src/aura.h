/* Visual-only use of Link's native Ymgcs00 Magic Armor shell.
 * Layout/calls: public d_a_player_main_04{,_aura,_item}, main_01 draw.
 * No shield flag, Tingle timer, magic, rupees or damage state is written. */
#pragma once
WWHD_GAME_FUNC(0x027DF40C, void, dl_animation_notify, (u32*));
typedef struct { u32 brk, animation, channel; f32 frame, control, value, end; } DLAuraFrame;
static DLAuraFrame dl_aura_update_frames[2], dl_aura_draw_frames[2];
static u32 dl_aura_update_owner, dl_aura_draw_owner;
typedef struct { u32 model, material, resource, state, before[3], ours[3]; } DLAuraBlend;
static DLAuraBlend dl_aura_blends[16];
static u32 dl_aura_blend_count;
static int dl_aura_active(daPy_lk_c* link) {
    return dl_enabled && dl_state.active && link == dl_owner && link == dl_player() &&
        dl_actor_id(link) == dl_owner_id && !(dl_word(link, 0x3bc) & 0x800);
}
static int dl_aura_frame(u32 brk, DLAuraFrame* save) {
    u32 a = dl_word((void*)brk, 0x10), v = dl_word((void*)brk, 0x20);
    if (!dl_game_span(a, 8) || !dl_game_span(v, 0x18)) return 0;
    /* Magic Armor calls BRK init with play=FALSE. FrameCtrl.mEnd can be zero.
     * Use the animation's getFrameMax virtual call, exactly as setAuraEffect. */
    u32 table=dl_word((void*)a,4);
    if (!dl_game_span(table,24)) return 0;
    u32 get_end=dl_word((void*)table,20);
    if (get_end<0x02000000 || get_end>=0x04000000) return 0;
    typedef s32 (*GetEnd)(u32);
    s32 end=((GetEnd)get_end)(a);
    if (end < 2 || end > 10000 || dl_word((void*)brk,0x10)!=a) return 0;
    *save = (DLAuraFrame){brk, a, v, *(f32*)a, *(f32*)(brk+4), *(f32*)v, (f32)end};
    return 1;
}
static void dl_aura_set(DLAuraFrame* f, f32 frame) {
    *(f32*)(f->brk+4) = *(f32*)f->animation = frame;
    u32 interpolate = dl_word((void*)f->channel, 0x10);
    if (interpolate >= 0x02000000 && interpolate < 0x04000000) {
        typedef f32 (*Interpolate)(u32, f32, f32, f32);
        *(f32*)f->channel = ((Interpolate)interpolate)(dl_word((void*)f->channel,0x14), frame,
            *(f32*)(f->channel+4), *(f32*)(f->channel+8));
        dl_animation_notify((u32*)(f->brk+0x20));
    }
}
static void dl_aura_frame_restore(DLAuraFrame* f) {
    if (dl_word((void*)f->brk,0x10) != f->animation || dl_word((void*)f->brk,0x20) != f->channel) return;
    *(f32*)f->animation = f->frame; *(f32*)(f->brk+4) = f->control;
    *(f32*)f->channel = f->value;
    dl_animation_notify((u32*)(f->brk+0x20));
}
/* Black additive light is invisible. Change this shell's material blend to
 * zero / inverse-source-alpha, preserving its authored alpha/depth.
 * FMAT+20 relative render state: flags, polygon, depth, alpha, ref, color,
 * target, blend, constant RGBA. BfresLibrary WiiU RenderState/BlendControl. */
static void dl_aura_restore(void) {
    for (u32 i=0;i<dl_aura_blend_count;++i) {
        DLAuraBlend* b=&dl_aura_blends[i];
        if (!dl_game_span(b->model,0xb0) ||
            !dl_game_span(b->material,4) ||
            *(u32*)b->material!=b->resource || dl_relative(b->resource+0x20)!=b->state) continue;
        u32 array=dl_word((void*)b->model,0x34), count=*(u16*)(b->model+0x2a);
        if (!count || count>16 || !dl_game_span(array,count*0x3c) ||
            b->material<array || (b->material-array)%0x3c || (b->material-array)/0x3c>=count) continue;
        const u32 offsets[3]={0,0x14,0x1c};
        for (u32 k=0;k<3;++k) {
            u32* word=(u32*)(b->state+offsets[k]);
            const u32 masks[3]={0x33,0x100,0x3fff1fff};
            if ((*word&masks[k])==(b->ours[k]&masks[k]))
                *word=(*word&~masks[k])|(b->before[k]&masks[k]);
        }
    }
    dl_aura_blend_count=0;
}
static void dl_aura_blend(daPy_lk_c* link, u32 index) {
    dl_aura_restore();
    u32 model=dl_word(link,0x5700);
    if (!dl_game_span(model,0xb0)) return;
    u32 count=*(u16*)(model+0x2a), materials=dl_word((void*)model,0x34);
    if (!count || count>16 || index>=count || !dl_game_span(materials,count*0x3c)) return;
    for (u32 i=index;i<index+1;++i) {
        u32 material=materials+i*0x3c, res=*(u32*)material;
        if (!dl_game_span(res,0x24) || *(u32*)res!=0x464d4154) continue;
        u32 state=dl_relative(res+0x20);
        if (!dl_game_span(state,0x30)) continue;
        DLAuraBlend* b=&dl_aura_blends[dl_aura_blend_count++];
        *b=(DLAuraBlend){.model=model,.material=material,.resource=res,.state=state};
        const u32 offsets[3]={0,0x14,0x1c};
        for (u32 k=0;k<3;++k) b->before[k]=*(u32*)(state+offsets[k]);
        b->ours[0]=(b->before[0]&~0x33u)|0x13u; /* translucent, color blending */
        b->ours[1]=b->before[1]|0x100u;
        /* Ignore shader RGB entirely. Animated BRK/custom shaders can still
         * output violet after TEV overrides: 0*src + (1-srcAlpha)*dst is
         * exactly a black veil with the native animated coverage. */
        b->ours[2]=(b->before[2]&~0x1fffu)|(5u<<8);
        /* Without separate alpha, changing RGB factors also changes the
         * destination alpha. Retain the original alpha equation explicitly. */
        if(!(b->before[2]&(1u<<29)))
            b->ours[2]=(b->ours[2]&~0x1fff0000u)|((b->before[2]&0x1fff)<<16)|(1u<<29);
        for (u32 k=0;k<3;++k) *(u32*)(state+offsets[k])=b->ours[k];
    }
}
WWHD_HOOK(0x0240B7F4, dl_aura_update_before, (daPy_lk_c* link)) {
    dl_aura_update_owner=0;
    if (!dl_aura_active(link)) return;
    /* A real Magic Armor/Tingle effect keeps its own activation and fading. */
    if ((dl_word(link,0x3bc)&1) || link->mTinkleShieldTimer) return;
    if (!dl_aura_frame((u32)link+0x5614,&dl_aura_update_frames[0]) ||
        !dl_aura_frame((u32)link+0x5704,&dl_aura_update_frames[1])) return;
    dl_aura_update_owner=(u32)link;
    for (u32 i=0;i<2;++i) {
        DLAuraFrame* f=&dl_aura_update_frames[i];
        /* Native inactive path subtracts one then updates all matrices/BTK.
         * Only its animation input is borrowed, never a protection flag. */
        *(f32*)f->animation=f->end+0.999f;
    }
}
WWHD_HOOK_RETURN(0x0240B7F4, dl_aura_update_after, (daPy_lk_c* link)) {
    if (dl_aura_update_owner!=(u32)link) return;
    dl_aura_update_owner=0;
    for (u32 i=0;i<2;++i) {
        DLAuraFrame* f=&dl_aura_update_frames[i];
        f32 baseline=f->frame>1 ? f->frame-1 : 0;
        dl_aura_set(f,baseline); /* leave native fade at its natural next frame */
    }
}
WWHD_HOOK(WWHD_ADDR_daPy_Draw, dl_aura_draw_before, (daPy_lk_c* link)) {
    dl_aura_draw_owner=0;
    if (!dl_aura_active(link) || !dl_aura_frame((u32)link+0x5614,&dl_aura_draw_frames[0]) ||
        !dl_aura_frame((u32)link+0x5704,&dl_aura_draw_frames[1])) return;
    dl_aura_draw_owner=(u32)link;
    for (u32 i=0;i<2;++i) {
        DLAuraFrame* f=&dl_aura_draw_frames[i];
        /* Only Ymgcs00 (index 1) is the black shell. Sample the middle of
         * its native BRK fade instead of the fully visible end. This keeps
         * authored coverage/soft edges and the independent BTK animation.
         * Half the fade time is not necessarily half the shader alpha.
         * The surrounding aura and the real armor's saved frame stay intact. */
        dl_aura_set(f, i == 1 ? f->end * 0.5f : f->end - 0.001f);
    }
}
WWHD_HOOK_RETURN(WWHD_ADDR_daPy_Draw, dl_aura_draw_after, (daPy_lk_c* link)) {
    if (dl_aura_draw_owner!=(u32)link) return;
    dl_aura_draw_owner=0;
    for (u32 i=0;i<2;++i) dl_aura_frame_restore(&dl_aura_draw_frames[i]);
}
