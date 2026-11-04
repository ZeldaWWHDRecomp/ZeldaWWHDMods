/* SPDX-License-Identifier: MPL-2.0; shared by Enhanced Controls and Dark Link. */
#pragma once
static wwhd_hud_element element;
static const char* const names[]={"Jump","Slash","Spin","Dodge","Dark Link"};
static void editor_button(u32 list,int i);
static void box(u32 list,EcRect r,const char* label,u32 color) {
    element=(wwhd_hud_element){.kind=WWHD_HUD_RECT,.anchor=WWHD_HUD_CENTER,
        .x=640+r.x-r.w/2,.y=360-r.y-r.h/2,.w=r.w,.h=r.h,
        .thickness=1,.u1=1,.v1=1,.rgba=color};
    wwhd_hud_emit(list,&element);
    unsigned n=0; while(label[n]) ++n;
    element.kind=WWHD_HUD_TEXT; element.x+=10; element.y+=r.h/2-9;
    element.size=18; element.rgba=0xffffffff; element.text=label; element.text_bytes=n;
    wwhd_hud_emit(list,&element);
}
/* The host validates positive thickness for every primitive, including text
 * and images. Zero-initialized omitted fields invalidate the entire HUD list. */
static void label(u32 list,float x,float y,const char* text,float font,u32 color) {
    unsigned n=0; while(text[n]) ++n;
    element=(wwhd_hud_element){.kind=WWHD_HUD_TEXT,.anchor=WWHD_HUD_CENTER,
        .x=640+x,.y=360-y,.size=font,.thickness=1,.rgba=color,.text=text,.text_bytes=n};
    wwhd_hud_emit(list,&element);
}
static void outline(u32 list,EcRect r,u32 color) {
    element=(wwhd_hud_element){.kind=WWHD_HUD_RECT_OUTLINE,.anchor=WWHD_HUD_CENTER,
        .x=640+r.x-r.w/2,.y=360-r.y-r.h/2,.w=r.w,.h=r.h,.thickness=2,.rgba=color};
    wwhd_hud_emit(list,&element);
}
static void line(u32 list,float x,float y,float xx,float yy,u32 color) {
    element=(wwhd_hud_element){.kind=WWHD_HUD_LINE,.anchor=WWHD_HUD_CENTER,
        .x=640+x,.y=360-y,.w=xx-x,.h=y-yy,.thickness=1,.rgba=color};
    wwhd_hud_emit(list,&element);
}
static void ee_draw_button(u32 list,EcPlacement p,const char* name,u32 image,int editing,int selected,int active) {
    EcRect r=ee_rect(p); float side=r.w<r.h?r.w:r.h;
    u32 alpha=(u32)(255*p.opacity/100);
    if(editing && alpha<128) alpha=128; /* editable even if hidden or faint */
    if(editing && !p.visible) alpha=96;
    u32 tint=(active?0xB0D8FF00u:0xFFFFFF00u)|alpha;
    element=(wwhd_hud_element){.kind=p.shape?(p.filled?WWHD_HUD_RECT:WWHD_HUD_RECT_OUTLINE):
        (p.filled?WWHD_HUD_CIRCLE:WWHD_HUD_CIRCLE_OUTLINE),.anchor=WWHD_HUD_CENTER,
        .x=640+r.x-(p.shape?r.w/2:0),.y=360-r.y-(p.shape?r.h/2:0),
        .w=r.w,.h=r.h,.size=side/2,.thickness=2,.rgba=p.filled?(0x14222C00u|alpha):tint};
    wwhd_hud_emit(list,&element);
    if(image && p.icons) {
        element=(wwhd_hud_element){.kind=WWHD_HUD_IMAGE,.anchor=WWHD_HUD_CENTER,
            .x=640+r.x-side/2,.y=360-r.y-side/2,.w=side,.h=side,
            .thickness=1,.u1=1,.v1=1,.rgba=tint,.image=image};
        wwhd_hud_emit(list,&element);
    } else label(list,r.x-r.w/2+8,r.y+9,name,18*p.size/100.f,tint);
    if(editing && selected) outline(list,r,0xFFDB70FF);
}
static char editor_text[12][64];
static void editor_value(char* out,const char* prefix,int value,const char* suffix) {
    unsigned n=0; while(*prefix) out[n++]=*prefix++;
    if(value<0) { out[n++]='-'; value=-value; }
    if(value>=100) out[n++]='0'+value/100;
    if(value>=10) out[n++]='0'+(value/10)%10;
    out[n++]='0'+value%10;
    while(*suffix) out[n++]=*suffix++;
    out[n]=0;
}
static void editor_hud(u32 list) {
    box(list,(EcRect){0,245,1260,210},"",0x102332F0);
    if(editor.draft.grid) {
        for(int x=-620;x<=620;x+=20) line(list,x,-320,x,150,0xD0DCEC28);
        for(int y=-320;y<=140;y+=20) line(list,-620,y,620,y,0xD0DCEC28);
    }
    for(int i=0;i<5;++i) if((ee_mask(&editor)&(1u<<i)) && i!=editor.selected) editor_button(list,i);
    editor_button(list,editor.selected);
    for(int i=0;i<5;++i) if(ee_mask(&editor)&(1u<<i)) box(list,ee_tab(i),names[i],editor.selected==i?0x436B8FFF:0x223C50FF);
    EcPlacement p=editor.draft.button[editor.selected];
    editor_value(editor_text[0],"Smaller ",p.size,"%");
    editor_value(editor_text[1],"Larger ",p.size,"%");
    editor_value(editor_text[2],"Opacity ",p.opacity,"%");
    const char* captions[EE_ACTIONS]={editor_text[0],editor_text[1],editor_text[2],
        p.shape?"Shape: square":"Shape: circle",p.filled?"Fill: on":"Fill: off",
        p.icons?"Style: icon":"Style: text",p.visible?"Visible: yes":"Visible: no",
        editor.draft.grid?"Grid: on":"Grid: off","Reset all","Cancel","Save / Done"};
    for(int i=0;i<EE_ACTIONS;++i) box(list,ee_action_rect(i),captions[i],i==EE_SAVE?0x26745AFF:0x29475FFF);
    label(list,-590,345,"TOUCH EDITOR - select a button, then drag it",18,0xFFFFFFFF);
    box(list,(EcRect){0,-342,1280,36},"",0x102332F0);
    label(list,-590,-334,editor.error?"Save failed - retry Save or Cancel":
        "Edit installed mod buttons. The game keeps running; use a safe place.",16,editor.error?0xFF8888FF:0xFFFFFFFF);
}
