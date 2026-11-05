/* Reserve the Dark Link row in Main_00's calculation input. Resolve the
 * placement through the game's virtual FindPane(name, recursive) operation,
 * exactly as hdHudMain_c::init (02676174) resolves its own named panes.
 * Source: ZeldaWWHDDecomp/wwhd b2811a3, hd_screen_hud_main.cpp and
 * hd_parts_lyt.h (pane vtable +0x5c). No private whole-HUD traversal/limit.
 * The calculated output remains available to embedded and standalone draws.
 */
#pragma once
static u32 dl_shortcut_owner,dl_shortcut_depth,dl_shortcut_pane;
static u32 dl_shortcut_original[2],dl_shortcut_written[2];
/* UI allocations may use either native heap. Keep this local to UI: it does
 * not relax the existing model/texture guards. runtime/src/runtime.h defines
 * MEM1 [f4000000,f6000000), MEM2 [10000000,50000000). */
static int dl_shortcut_span(u32 p,u32 size) {
    if(p&3u) return 0;
    return (p>=0x10000000u && p<0x50000000u && size<=0x50000000u-p) ||
           (p>=0xf4000000u && p<0xf6000000u && size<=0xf6000000u-p);
}
static int dl_shortcut_find(void* parts,u32* result) {
    u32 layout=dl_word(parts,0x44);
    if(!dl_shortcut_span(layout,8)) return 0;
    u32 accessor=dl_word((void*)layout,4);
    if(!dl_shortcut_span(accessor,0x10)) return 0;
    u32 root=dl_word((void*)accessor,0xc);
    if(!dl_shortcut_span(root,0xa4)) return 0;
    u32 table=dl_word((void*)root,8);
    if(!dl_shortcut_span(table,0x60)) return 0;
    u32 find=dl_word((void*)table,0x5c);
    if((find&3u) || find<0x02000000u || find>=0x04000000u)
        return 0;
    typedef u32 (*FindPane)(u32,const char*,u32);
    u32 pane=((FindPane)find)(root,"L_Shortcut_00",1);
    if(!pane) return 0;
    if(!dl_shortcut_span(pane,0xa4) ||
       !dl_str_equal((const char*)(pane+0x80),"L_Shortcut_00",24))
        return 0;
    *result=pane;return 1;
}
WWHD_HOOK(0x026F8CD0,dl_shortcut_calc_enter,(void* parts)) {
    if(dl_shortcut_owner) { ++dl_shortcut_depth; return; }
    if(!dl_shortcut_span((u32)parts,0x50) ||
       dl_word(parts,4)!=(u32)&WWHD_GAME_DATA(0x100F7A48,u8)) return;
    if(!dl_show_hud || !dl_owner || !dl_allowed(dl_owner) ||
       wwhd_hud_epoch()!=dl_magic_epoch) return;
    u32 save=WWHD_GAME_DATA(WWHD_ADDR_save_info_pointer,u32);
    u32 capacity=save?*((u8*)save+0x33):0;
    wwhd_hud_element gauge;
    u32 v=capacity>16;
    if(!dl_magic_place(v,&dl_magic_outer[v],1,dl_magic_images[v][0],&gauge) || !(gauge.rgba&255)) return;
    f32 row=(gauge.h+8)*(capacity?1:2);
    if(!(row>8 && row<320)) return;
    u32 p;
    if(!dl_shortcut_find(parts,&p)) return;
    u32 parent=dl_word((void*)p,0xc);
    if(parent==p || !dl_shortcut_span(parent,0xa4)) { return; }
    /* Convert a downward layout-space vector through the parent's inverse 2D
     * basis. Scaling/rotation must not alter the reserved screen-space height. */
    f32 a=*(f32*)(parent+0x48),b=*(f32*)(parent+0x4c);
    f32 c=*(f32*)(parent+0x58),d=*(f32*)(parent+0x5c),det=a*d-b*c;
    f32 x=*(f32*)(p+0x1c),y=*(f32*)(p+0x20);
    if(!(dl_abs(det)>0.0001f && dl_abs(det)<10000 &&
         x>-10000 && x<10000 && y>-10000 && y<10000)) {
        return;
    }
    f32 nx=x+b*row/det,ny=y-a*row/det;
    if(!(nx>-10000 && nx<10000 && ny>-10000 && ny<10000)) {
        return;
    }
    dl_shortcut_owner=(u32)parts;dl_shortcut_depth=1;dl_shortcut_pane=p;
    union {f32 value;u32 bits;} encoded[2]={{.value=nx},{.value=ny}};
    for(u32 i=0;i<2;++i) {
        dl_shortcut_original[i]=dl_word((void*)p,0x1c+i*4);
        dl_shortcut_written[i]=encoded[i].bits;
        *(u32*)(p+0x1c+i*4)=encoded[i].bits;
    }
    *((u8*)p+0x44)|=0x10; /* native calculation must consume the new input */
}
WWHD_HOOK_RETURN(0x026F8CD0,dl_shortcut_calc_leave,(void* parts)) {
    if(!dl_shortcut_owner) return;
    if(dl_shortcut_depth>1) { --dl_shortcut_depth;return; }
    if((u32)parts!=dl_shortcut_owner) return;
    u32 p=dl_shortcut_pane;
    for(u32 i=0;i<2;++i) if(dl_word((void*)p,0x1c+i*4)==dl_shortcut_written[i])
        *(u32*)(p+0x1c+i*4)=dl_shortcut_original[i];
    /* Keep the calculated result for drawing. Dirty again so disabling the
     * gauge recomputes native geometry even when there is no animation. */
    *((u8*)p+0x44)|=0x10;
    dl_shortcut_owner=dl_shortcut_depth=dl_shortcut_pane=0;
}
