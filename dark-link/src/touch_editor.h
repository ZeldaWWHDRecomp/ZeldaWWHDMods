/* Standalone Dark Link touch editor; no game-function IPC. */
#pragma once
#include "editor.h"
static EcEditor editor;
static EcLayoutFile dl_layout_files[2];
static const char* const dl_layout_names[]={"touch-layout-a.bin","touch-layout-b.bin"};
static int dl_editor_open,dl_editor_selected,dl_editor_release;
static unsigned long long dl_editor_epoch=~0ull;
static EcPlacement dl_placement;
static u32 dl_icon;
static int dl_icon_attempted;
static EcRect dl_claims[3];
static unsigned dl_claim_mask;
static const EcRect dl_edit_rect={120,260,96,48};
static void dl_editor_load(void) {
    editor.saved=ee_defaults(); editor.available=16; editor.sequence=0; ee_close(&editor);
    editor.saved.button[4].x=dl_touch_x-640+48;
    editor.saved.button[4].y=360-dl_touch_y-22;
    ee_fit(&editor.saved.button[4]);
    for(int i=0;i<2;++i) {
        EcLayoutFile* f=&dl_layout_files[i];
        if(ee_decode(f,wwhd_file_read(dl_layout_names[i],f,sizeof(*f))) &&
            (!editor.sequence || (s32)(f->sequence-editor.sequence)>0)) {
            editor.saved=f->layout; editor.sequence=f->sequence;
        }
    }
}
static int dl_editor_save(void) {
    unsigned seq=editor.sequence+1; if(!seq) seq=1;
    EcLayoutFile* f=&dl_layout_files[seq&1];
    *f=(EcLayoutFile){0x45434c59u,2,seq,0,editor.draft}; f->checksum=ee_hash(f);
    if(wwhd_file_write(dl_layout_names[seq&1],f,sizeof(*f))!=(s32)sizeof(*f)) {
        editor.error=1; return 0;
    }
    editor.saved=editor.draft; editor.sequence=seq; return 1;
}
static void dl_editor_refresh(void) {
    unsigned long long epoch=wwhd_hud_epoch();
    if(epoch!=dl_editor_epoch) {
        dl_editor_load(); dl_editor_epoch=epoch; dl_icon=0; dl_icon_attempted=0;
        dl_claim_mask=0; dl_editor_release=0;
    }
    int allowed=dl_touch_enabled && dl_owner && dl_allowed(dl_owner);
    if(!allowed) ee_close(&editor);
    dl_editor_open=editor.open;
    dl_editor_selected=editor.selected;
    dl_placement=(editor.open?editor.draft:editor.saved).button[4];
    if(dl_editor_open) { dl_request=0; dl_editor_release=1; }
    EcRect rects[3]={ee_rect(dl_placement),dl_edit_rect,{0,0,1280,720}};
    unsigned wanted=allowed?(dl_editor_open?4u:
        ((dl_placement.visible?1u:0u)|2u)):0u;
    for(int i=0;i<3;++i) {
        EcRect a=rects[i],b=dl_claims[i]; unsigned bit=1u<<i;
        if((wanted&bit)!=(dl_claim_mask&bit) || ((wanted&bit) &&
            (a.x!=b.x || a.y!=b.y || a.w!=b.w || a.h!=b.h))) {
            if(wanted&bit) wwhd_touch_claim(i,a.x-a.w/2,a.y-a.h/2,a.x+a.w/2,a.y+a.h/2);
            else wwhd_touch_claim(i,0,0,0,0);
        }
        dl_claims[i]=a;
    }
    dl_claim_mask=wanted;
}
static void dl_editor_touch(const wwhd_touch_event* event) {
    dl_editor_refresh(); if(!dl_touch_enabled || !dl_owner || !dl_allowed(dl_owner)) return;
    if(editor.open) {
        int action=ee_touch(&editor,event->kind,event->x,event->y);
        if(action==EE_CANCEL) ee_close(&editor);
        else if(action==EE_SAVE) { if(dl_editor_save()) ee_close(&editor); }
        else if(action>=0) ee_apply(&editor,action);
        dl_editor_refresh(); return;
    }
    if(event->kind!=WWHD_TOUCH_DOWN) return;
    if(ec_inside(dl_edit_rect,event->x,event->y)) {
        ee_begin(&editor); dl_request=0; dl_editor_release=1; dl_editor_refresh();
    } else if(dl_placement.visible && ec_inside(ee_rect(dl_placement),event->x,event->y)) dl_request=1;
}
static int dl_editor_filter(wwhd_input_state* pad) {
    dl_editor_refresh();
    if(!dl_editor_open && !dl_editor_release) return 0;
    int neutral=!pad->buttons && pad->lx>=-.15f && pad->lx<=.15f && pad->ly>=-.15f && pad->ly<=.15f &&
        pad->rx>=-.15f && pad->rx<=.15f && pad->ry>=-.15f && pad->ry<=.15f;
    if(!dl_editor_open && neutral) dl_editor_release=0;
    pad->buttons=0; pad->lx=pad->ly=pad->rx=pad->ry=0; dl_request=0; return 1;
}
#include "editor_view.h"
static void editor_button(u32 list,int i) {
    if(i!=4) return;
    ee_draw_button(list,dl_placement,names[4],dl_icon,dl_editor_open,dl_editor_selected==4,dl_state.active);
}
static void dl_editor_hud(u32 list) {
    dl_editor_refresh(); if(!dl_touch_enabled) return;
    int needed=dl_placement.icons && (dl_placement.visible || dl_editor_open);
    if(!needed) { if(dl_icon) wwhd_hud_release(dl_icon); dl_icon=0; dl_icon_attempted=0; }
    else if(!dl_icon_attempted) {
        dl_icon=wwhd_hud_texture(WWHD_HUD_PACKAGE,"assets/dark_link.png"); dl_icon_attempted=1;
    }
    if(editor.open) { editor_hud(list); return; }
    if(dl_placement.visible || dl_editor_open) editor_button(list,4);
    box(list,dl_edit_rect,"DL Edit",0x23445cd0);
}
