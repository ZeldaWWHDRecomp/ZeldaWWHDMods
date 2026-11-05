/* SPDX-License-Identifier: MPL-2.0
 * Generic straight blades and motion marks, authored as HUD primitives.
 * No game artwork, character emblems or distinctive sword design is bundled. */
#pragma once
static void ec_icon_line(u32 list,EcPlacement p,float x,float y,float xx,float yy,u32 color,float width) {
    float scale=ee_rect(p).h/60;
    element=(wwhd_hud_element){.kind=WWHD_HUD_LINE,.anchor=WWHD_HUD_CENTER,
        .x=640+p.x+x*scale,.y=360-p.y+y*scale,.w=(xx-x)*scale,.h=(yy-y)*scale,
        .thickness=width*scale,.rgba=color};
    wwhd_hud_emit(list,&element);
}
static void ec_combat_icon(u32 list,EcPlacement p,int action,int editing,int active) {
    u32 alpha=255*p.opacity/100;
    if(editing && alpha<128) alpha=128;
    if(editing && !p.visible) alpha=96;
    u32 silver=(active?0xb0d8ff00u:0xe2edf200u)|alpha, gold=0xe7b54d00u|alpha;
    float tip=action==EC_SLASH?20:-20, hilt=-tip*.45f;
    ec_icon_line(list,p,0,tip,0,hilt,silver,5);
    ec_icon_line(list,p,0,tip,-3,tip*.72f,silver,2);
    ec_icon_line(list,p,0,tip,3,tip*.72f,silver,2);
    ec_icon_line(list,p,-8,hilt,8,hilt,gold,3);
    ec_icon_line(list,p,0,hilt,0,hilt-tip*.4f,0x96704d00u|alpha,4);
    if(action==EC_SPIN) {
        /* Octagonal sweep plus arrowhead, deliberately independent of the blade. */
        static const signed char points[8][2]={{-18,-10},{-22,0},{-18,13},{0,23},{18,13},{22,0},{18,-13},{7,-21}};
        for(int i=0;i<7;++i) ec_icon_line(list,p,points[i][0],points[i][1],points[i+1][0],points[i+1][1],gold,2);
        ec_icon_line(list,p,7,-21,8,-12,gold,2);ec_icon_line(list,p,7,-21,16,-21,gold,2);
    } else {
        float dir=action==EC_JUMP?-1:1;
        for(int side=-1;side<=1;side+=2) {
            float x=side*17;
            ec_icon_line(list,p,x,-dir*13,x,dir*13,gold,2);
            ec_icon_line(list,p,x,dir*13,x-4,dir*8,gold,2);
            ec_icon_line(list,p,x,dir*13,x+4,dir*8,gold,2);
        }
    }
}
