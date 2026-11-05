/* Native pic1 outlines and progress detail are separate from MagicBase.
 * Keep their authored textures/geometry; do not synthesize a replacement skin. */
#pragma once
#define DL_MAGIC_DETAILS 8
static struct { u32 image,progress; wwhd_game_pane pane; } dl_magic_details[2][DL_MAGIC_DETAILS];
static u32 dl_magic_detail_count[2];
static u32 dl_magic_detail_alpha(const char* name,const char* base) {
    s32 p=dl_magic_style_find(name),root=dl_magic_style_find(base);u32 alpha=255;
    if(p<0 || root<0) return 0;
    /* Common container fades belong to native magic visibility, not this mod.
     * Separate effect branches still retain their local alpha/visibility. */
    for(u32 n=0;n<DL_MAGIC_STYLES && (u32)p<dl_magic_style_count;++n) {
        for(s32 q=(s32)dl_magic_styles[root].parent;(u32)q<dl_magic_style_count;q=(s32)dl_magic_styles[q].parent)
            if(p==q) return alpha;
        if(!dl_magic_styles[p].visible || dl_magic_styles[p].flash) return 0;
        alpha=alpha*dl_magic_styles[p].alpha/255;p=(s32)dl_magic_styles[p].parent;
    }
    return alpha;
}
static void dl_magic_load_details(u32 v,u32 base,u32 fill) {
    for(u32 i=0;i<dl_magic_detail_count[v];++i) wwhd_hud_release(dl_magic_details[v][i].image);
    dl_magic_detail_count[v]=0;
    for(u32 i=0;i<dl_magic_picture_count && dl_magic_detail_count[v]<DL_MAGIC_DETAILS;++i) {
        dl_magic_picture* pic=&dl_magic_pictures[i];
        if(i==base || i==fill || dl_str_equal(pic->name,"P_MagicBase_00",24) ||
           dl_str_equal(pic->name,"P_MagicBase_01",24) ||
           dl_str_equal(pic->name,"P_Magic_00",24) || dl_str_equal(pic->name,"P_Magic_01",24)) continue;
        u32 alpha=dl_magic_detail_alpha(pic->name,dl_magic_pictures[base].name);
        dl_gauge_picture_report(pic,alpha,0);
        if(!alpha) {dl_gauge_reason(pic->probe,"hidden/flash");continue;}
        if(!dl_magic_pane(pic->name,&dl_magic_query)) {dl_gauge_reason(pic->probe,"pane-query");continue;}
        dl_gauge_picture_report(pic,alpha,&dl_magic_query);
        wwhd_game_pane p=dl_magic_query,*b=&dl_magic_base[v];
        f32 dx=dl_abs(p.x-b->x),dy=dl_abs(p.y-b->y);
        if(p.rotation!=0 || !(p.w>0 && p.h>0)) {dl_gauge_reason(pic->probe,"transform");continue;}
        int progress=pic->progress!=0;
        if(progress) {
            /* Only progress artwork inside this capacity's gauge. */
            if(dx+p.w/2>b->w/2+1 || dy+p.h/2>b->h/2+1) {dl_gauge_reason(pic->probe,"outside-fill");continue;}
        } else {
            /* Static outlines must enclose it, not belong to the short variant
             * sitting inside the doubled bar or an unrelated large overlay. */
            if(p.w<b->w || p.w>b->w+32 || p.h<b->h || p.h>b->h+32 ||
               dx>(p.w-b->w)/2+0.5f || dy>(p.h-b->h)/2+0.5f) {dl_gauge_reason(pic->probe,"outline-bounds");continue;}
        }
        if(!dl_magic_pixels(&p,i,pic->fill,0)) {dl_gauge_reason(pic->probe,"pixels-failed");continue;}
        if(!progress) {
            /* The native texture itself must leave the gauge's inner area
             * clear. Pane alpha alone cannot turn a white panel into a frame. */
            int clear=1;
            for(u32 y=1;y<4;++y) for(u32 x=1;x<4;++x) {
                u32 px=(u32)((0.5f+(b->x-p.x+(x/4.0f-0.5f)*b->w)/p.w)*dl_art_width);
                u32 py=(u32)((0.5f-(b->y-p.y+(y/4.0f-0.5f)*b->h)/p.h)*dl_art_height);
                if(px>=dl_art_width || py>=dl_art_height || dl_art_pixels[(py*dl_art_width+px)*4+3]>8) clear=0;
            }
            if(!clear) {dl_gauge_reason(pic->probe,"opaque-centre");continue;}
        }
        for(u32 k=0;k<dl_art_width*dl_art_height;++k) dl_art_pixels[k*4+3]=dl_art_pixels[k*4+3]*alpha/255;
        u32 bytes=dl_art_encode();if(!bytes) {dl_gauge_reason(pic->probe,"PNG-failed");continue;}
        u32 n=dl_magic_detail_count[v];char path[]="magic-0-detail-00.png";
        path[6]='0'+v;path[14]='0'+n/10;path[15]='0'+n%10;
        if(!dl_magic_cache_write(path,bytes)) {dl_gauge_reason(pic->probe,"write-failed");continue;}
        u32 image=wwhd_hud_texture(WWHD_HUD_DATA,path);if(!image) {dl_gauge_reason(pic->probe,"texture-failed");continue;}
        dl_gauge_reason(pic->probe,"DETAIL");
        dl_magic_details[v][n].image=image;dl_magic_details[v][n].progress=progress;
        dl_magic_details[v][n].pane=p;++dl_magic_detail_count[v];
    }
}
