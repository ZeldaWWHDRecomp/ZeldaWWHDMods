/* A copy of MagicBar_00's authored picture art, loaded from the player's game.
 * No extracted artwork is packaged. Native HD magic controller: 02675414,
 * panes +58 N_All_00, +5c P_MagicBase_00, +60 P_MagicBase_01.
 * SARC/BFLYT layouts: tools/story_mods/lyt and runtime/src/mods/game_layouts. */
#pragma once
WWHD_GAME_FUNC(0x026FBB24, u32, dl_layout_archive, (u32, const u32*));
static int dl_game_span(u32, u32);
#include "magic_art.h"
#define DL_MAGIC_PICTURES 32
typedef struct { char name[24], material[29]; u32 fill, progress, maps, probe, wrap[2], black, white, vertex[4]; f32 uv[4], tex_srt[5], pane[10], original[10]; } dl_magic_picture;
static dl_magic_picture dl_magic_pictures[DL_MAGIC_PICTURES];
static dl_magic_picture* dl_magic_material_target;
static u32 dl_magic_picture_count, dl_magic_loaded, dl_magic_attempted;
static unsigned long long dl_magic_epoch;
static wwhd_game_pane dl_magic_query, dl_magic_base[2], dl_magic_fill[2], dl_magic_outer[2];
static u32 dl_magic_images[2][2], dl_magic_archive;
static f32 dl_magic_position[4];
static f32 dl_magic_authored[2];
static u32 dl_magic_authored_valid;
static int dl_magic_position_valid;
static char dl_magic_key[64], dl_magic_path[96];
static const char dl_magic_name[]="MagicBar_00";
static u32 dl_magic_string[2];
static int dl_str_equal(const char* a,const char* b,u32 maximum) {
    for (u32 i=0;i<maximum;++i) { if (a[i]!=b[i]) return 0; if (!a[i]) return 1; }
    return 0;
}
static int dl_copy_string(char* dst,u32 capacity,const char* src,u32 maximum) {
    for (u32 i=0;i<maximum && i<capacity;++i) { dst[i]=src[i]; if (!src[i]) return 1; }
    dst[0]=0; return 0;
}
/* Material identifiers may be padded with spaces rather than a terminator. */
static int dl_material_string(char out[29],const char* source) {
    u32 n=0; while(n<28 && source[n]) { out[n]=source[n]; ++n; }
    while(n && out[n-1]==' ') --n;
    out[n]=0; return n!=0;
}
static u32 dl_append(char* dst,u32 at,u32 capacity,const char* src) {
    while (*src && at+1<capacity) dst[at++]=*src++;
    dst[at]=0; return at;
}
static int dl_green(u32 c) {
    u32 r=c>>24,g=(c>>16)&255,b=(c>>8)&255;
    return g>r+16 && g>b+8;
}
static f32 dl_abs(f32 x) { return x<0 ? -x:x; }
static f32 dl_gauge_fraction(void) {
    f32 v=dl_config.free_transform ? 1 : dl_state.active ? dl_state.remaining/dl_state.total : dl_state.charge/dl_config.capacity;
    return v<0 ? 0 : v>1 ? 1 : v;
}
/* All archive offsets are range checked before dereference, including strings. */
static int dl_blob_span(u32 base,u32 size,u32 at,u32 bytes) {
    return at>=base && at-base<=size && bytes<=size-(at-base);
}
static u32 dl_magic_file(u32 arc,const char* wanted,u32* bytes) {
    u32 length=0;while(wanted[length] && length<95) ++length;
    if(length==95) return 0;
    if (!dl_game_span(arc,0x20) || *(u32*)arc!=0x53415243) return 0;
    u32 size=dl_word((void*)arc,8), data=dl_word((void*)arc,0xc);
    if (size>16*1024*1024 || !dl_game_span(arc,size) || data>=size) return 0;
    u32 fat=arc+*(u16*)(arc+4);
    if (!dl_blob_span(arc,size,fat,12) || *(u32*)fat!=0x53464154) return 0;
    u32 n=*(u16*)(fat+6), nodes=fat+*(u16*)(fat+4);
    if (!n || n>4096 || !dl_blob_span(arc,size,nodes,n*16+8)) return 0;
    u32 names=nodes+n*16;
    if (*(u32*)names!=0x53464e54) return 0;
    names+=*(u16*)(names+4);
    for(u32 i=0;i<n;++i) {
        u32 node=nodes+16*i, attr=dl_word((void*)node,4);
        if (!(attr>>24)) continue;
        u32 name=names+(attr&0xffffff)*4;
        if (!dl_blob_span(arc,size,name,length+1) || !dl_str_equal((char*)name,wanted,length+1)) continue;
        u32 start=dl_word((void*)node,8), end=dl_word((void*)node,12);
        if (end<=start || end>size-data) return 0;
        *bytes=end-start; return arc+data+start;
    }
    return 0;
}
#include "gauge_probe.h"
#include "magic_style.h"
static int dl_magic_parse(u32 f,u32 size) {
    DLGaugeReport* report=&dl_gauge_reports[dl_gauge_variant];
    memset(report,0,sizeof *report);report->stage="parse";
    if (size<20 || *(u32*)f!=0x464c5954 || *(u16*)(f+4)!=0xfeff) return 0;
    u32 at=f+*(u16*)(f+6), mat=0, mat_bytes=0, count=*(u16*)(f+16);
    if (count>1024) return 0;
    dl_magic_picture_count=0;
    /* Material table can occur before or after pictures. */
    for(u32 i=0;i<count;++i) {
        if (!dl_blob_span(f,size,at,8)) return 0;
        u32 n=dl_word((void*)at,4);
        if (n<8 || (n&3) || !dl_blob_span(f,size,at,n)) return 0;
        if (*(u32*)at==0x6d617431) { mat=at; mat_bytes=n; }
        at+=n;
    }
    if (!mat || mat_bytes<12) return 0;
    u32 mats=*(u16*)(mat+8);
    if (!mats || mats>256 || mat_bytes<12+4*mats) return 0;
    at=f+*(u16*)(f+6);
    for(u32 i=0;i<count;++i) {
        u32 n=dl_word((void*)at,4);
        if (*(u32*)at==0x70696331) ++report->pictures;
        if (*(u32*)at==0x70696331 && dl_magic_picture_count==DL_MAGIC_PICTURES) ++report->overflow;
        if (*(u32*)at==0x70696331 && n>=0x80 && dl_magic_picture_count<DL_MAGIC_PICTURES) {
            u32 index=*(u16*)(at+0x5c+8); /* pan1 grows eight bytes in HD */
            if(index>=mats) return 0;
            u32 m=mat+dl_word((void*)mat,12+index*4);
            if(!dl_blob_span(mat,mat_bytes,m,40)) return 0;
            u32 p=dl_magic_picture_count;
            if(!dl_copy_string(dl_magic_pictures[p].name,24,(char*)(at+12),24) ||
               !dl_material_string(dl_magic_pictures[p].material,(char*)m)) return 0;
            dl_magic_pictures[p].probe=dl_gauge_row(dl_magic_pictures[p].name,"not-selected");
            u32 green=dl_green(dl_word((void*)m,28)) || dl_green(dl_word((void*)m,32));
            for(u32 v=0;v<4;++v) green|=dl_green(dl_word((void*)at,0x54+v*4));
            dl_magic_pictures[p].fill=green;dl_magic_pictures[p].progress=0;
            dl_magic_pictures[p].black=dl_word((void*)m,28);
            dl_magic_pictures[p].white=dl_word((void*)m,32);
            for(u32 k=0;k<4;++k) dl_magic_pictures[p].vertex[k]=dl_word((void*)at,0x54+k*4);
            for(u32 k=0;k<10;++k) dl_magic_pictures[p].pane[k]=dl_magic_pictures[p].original[k]=*(f32*)(at+0x2c+k*4);
            for(u32 k=0;k<5;++k) dl_magic_pictures[p].tex_srt[k]=k>=3?1:0;
            u32 flags=dl_word((void*)m,36), maps=flags&3, srts=(flags>>2)&3;
            dl_magic_pictures[p].maps=maps;
            dl_magic_pictures[p].wrap[0]=dl_magic_pictures[p].wrap[1]=0;
            if(maps) {
                if(!dl_blob_span(mat,mat_bytes,m+40,maps*4)) return 0;
                dl_magic_pictures[p].wrap[0]=*((u8*)m+42)&3;
                dl_magic_pictures[p].wrap[1]=*((u8*)m+43)&3;
            }
            if(srts) {
                u32 srt=m+40+maps*4;
                if(!dl_blob_span(mat,mat_bytes,srt,20)) return 0;
                for(u32 k=0;k<5;++k) dl_magic_pictures[p].tex_srt[k]=*(f32*)(srt+k*4);
            }
            /* First UV set: TL,TR,BL,BR. A single unrotated image is sufficient
             * for these authored gauge pictures; reject unsupported UV shapes. */
            f32* uv=(f32*)(at+0x68);
            if (n>=0x88 && uv[1]==uv[3] && uv[0]==uv[4] && uv[6]==uv[2] && uv[7]==uv[5]) {
                dl_magic_pictures[p].uv[0]=uv[0]; dl_magic_pictures[p].uv[1]=uv[1];
                dl_magic_pictures[p].uv[2]=uv[2]; dl_magic_pictures[p].uv[3]=uv[5];
                ++dl_magic_picture_count;
            } else { ++report->uv_skips;dl_gauge_reason(dl_magic_pictures[p].probe,"UV-shape"); }
        }
        at+=n;
    }
    /* Visibility metadata selects optional decoration; failure must not
     * suppress the independently owned base/fill. Picture bounds above still
     * have to validate. */
    if(!dl_magic_style_parse(f,size)) dl_magic_style_count=0;
    return dl_magic_picture_count!=0;
}
#include "magic_animation.h"
static int dl_magic_pane(const char* name,wwhd_game_pane* out) {
    u32 n=dl_append(dl_magic_key,0,sizeof dl_magic_key,"layout-pane:MagicBar_00/");
    dl_append(dl_magic_key,n,sizeof dl_magic_key,name);
    if(!wwhd_game_pane_get(dl_magic_key,out) || out->version!=1 || !(out->flags&2) || !(out->w>0 && out->h>0)) return 0;
    for(u32 i=0;i<dl_magic_picture_count;++i) if(dl_str_equal(name,dl_magic_pictures[i].name,24)) {
        f32 *a=dl_magic_pictures[i].original,*b=dl_magic_pictures[i].pane;
        f32 ow=a[6]*a[8],oh=a[7]*a[9];
        if(ow>0 && oh>0) {
            out->x+=(b[0]-a[0])*out->w/ow;out->y+=(b[1]-a[1])*out->h/oh;
            out->w*=b[6]*b[8]/ow;out->h*=b[7]*b[9]/oh;
        }
        break;
    }
    return out->w>0 && out->h>0;
}
/* MagicBase is the inner background, not the outer frame rectangle. Read the
 * matching native window's authored bounds even when its flash artwork is
 * hidden. This only supplies geometry; it never enables warning content. */
static void dl_magic_outer_bounds(u32 v,int separate_fill) {
    dl_magic_outer[v]=dl_magic_base[v];
    if(!separate_fill) return;
    const char* name=v?"W_MagicBase_01":"W_MagicBase_00";
    u32 k=dl_append(dl_magic_key,0,sizeof dl_magic_key,"layout-pane:MagicBar_00/");
    dl_append(dl_magic_key,k,sizeof dl_magic_key,name);
    if(!wwhd_game_pane_get(dl_magic_key,&dl_magic_query) || dl_magic_query.version!=1) return;
    const wwhd_game_pane *p=&dl_magic_query,*b=&dl_magic_base[v];
    f32 dw=p->w-b->w,dh=p->h-b->h;
    if(p->rotation==0 && dw>=0 && dw<=64 && dh>=0 && dh<=64 &&
       dl_abs(p->x-b->x)<=dw/2 && dl_abs(p->y-b->y)<=dh/2)
        dl_magic_outer[v]=*p;
}
static int dl_magic_pixels(wwhd_game_pane* pane,u32 picture,u32 fill,u32 empty) {
    /* The raw BC4 texture can be a luminance mask with fully opaque alpha.
     * Apply the actual material, including alpha, before making the red copy. */
    u32 n=dl_append(dl_magic_path,0,sizeof dl_magic_path,"timg/");
    n=dl_append(dl_magic_path,n,sizeof dl_magic_path,pane->texture);
    dl_append(dl_magic_path,n,sizeof dl_magic_path,".bflim");
    u32 bytes=0,texture=dl_magic_file(dl_magic_archive,dl_magic_path,&bytes);
    int srgb=0;
    if(!texture || !dl_art_decode((const u8*)texture,bytes,&srgb)) return 0;
    u32 black=dl_magic_pictures[picture].black,white=dl_magic_pictures[picture].white;
    dl_art_material(black,white,srgb);
    dl_magic_picture* pic=&dl_magic_pictures[picture];
    u32 width=dl_art_width,height=dl_art_height;
    /* Repeated detail needs a canvas at pane resolution; baking many repeats
     * back into a tiny source mask aliases all segments to the same sample. */
    if(pic->wrap[0] || pic->wrap[1]) {
        if(!(pane->w>0 && pane->w<=1024 && pane->h>0 && pane->h<=256)) return 0;
        if(pane->w>width) width=(u32)(pane->w+0.5f);
        if(pane->h>height) height=(u32)(pane->h+0.5f);
    }
    if(!dl_art_bake_sampler(pic->uv,pic->tex_srt,pic->wrap[0],pic->wrap[1],width,height)) return 0;
    dl_art_vertices(pic->vertex,fill,empty);
    /* The mod owns base/fill visibility. Native pane/ancestor fades can be
     * zero before magic is unlocked (or while the native HUD is hidden).
     * Preserve artwork/material/vertex coverage, but never bake those native
     * visibility gates into the cached Dark Link images. */
    return 1;
}
/* Files survive savestate loads while HUD handles do not. Reuse identical
 * generated bytes on disk; never trust a restored in-memory written flag.
 * Pixel scratch is no longer needed after PNG encoding and bounds the read. */
static int dl_magic_cache_write(const char* path,u32 bytes) {
    if (!bytes || bytes+1>sizeof dl_art_pixels) return 0;
    s32 old=wwhd_file_read(path,dl_art_pixels,bytes+1);
    if(old==(s32)bytes) {
        u32 i=0; while(i<bytes && dl_art_pixels[i]==dl_art_png[i]) ++i;
        if(i==bytes) return 1;
    }
    return wwhd_file_write(path,dl_art_png,bytes)==(s32)bytes;
}
static u32 dl_magic_image(wwhd_game_pane* pane,u32 picture,u32 variant,u32 fill,u32 empty) {
    if(!dl_magic_pixels(pane,picture,fill,empty)) return 0;
    u32 size=dl_art_encode();
    if(!size) return 0;
    static const char* const files[2][2]={{"magic-0-frame.png","magic-0-fill.png"},{"magic-1-frame.png","magic-1-fill.png"}};
    const char* filename=files[variant][fill];
    if(!dl_magic_cache_write(filename,size)) return 0;
    return wwhd_hud_texture(WWHD_HUD_DATA,filename);
}
#include "magic_frame.h"
#include "magic_layers.h"
static void dl_magic_load(void) {
    u32 mgr=WWHD_GAME_DATA(0x101F7274,u32);
    if (!mgr) return;
    dl_magic_string[0]=(u32)dl_magic_name;
    dl_magic_string[1]=(u32)&WWHD_GAME_DATA(0x1010A3FC,u32);
    dl_magic_archive=dl_layout_archive(mgr,dl_magic_string);
    u32 size=0,f=dl_magic_file(dl_magic_archive,"blyt/MagicBar_00.bflyt",&size);
    if(!f) return; /* Native layout not resident yet; no IO was attempted. */
    dl_magic_attempted=1; /* A decode/write/texture failure is terminal this epoch. */
    for(u32 v=0;v<2;++v) {
        if(dl_magic_images[v][0] && dl_magic_images[v][1]) continue;
        dl_gauge_variant=v;
        if(!dl_magic_parse(f,size)) return;
        DLGaugeReport* report=&dl_gauge_reports[v];report->stage="select";
        dl_magic_full_animation(v);
        for(u32 a=0;a<5;++a)if(dl_magic_animations[a].file)report->animations|=1u<<a;
        for(u32 i=0;i<dl_magic_picture_count;++i) {
            u32 green=dl_green(dl_magic_pictures[i].black)||dl_green(dl_magic_pictures[i].white);
            for(u32 k=0;k<4;++k) green|=dl_green(dl_magic_pictures[i].vertex[k]);
            dl_magic_pictures[i].fill=green;
            dl_gauge_picture_report(&dl_magic_pictures[i],dl_magic_style_alpha(dl_magic_pictures[i].name),0);
        }
        for(u32 k=0;k<2;++k) if(dl_magic_images[v][k]) {
            wwhd_hud_release(dl_magic_images[v][k]); dl_magic_images[v][k]=0;
        }
        const char* base=v ? "P_MagicBase_01":"P_MagicBase_00";
        const char* native_fill=v ? "P_Magic_01":"P_Magic_00";
        if(!dl_magic_pane(base,&dl_magic_base[v])) {report->stage="base-geometry";continue;}
        s32 frame=-1,fill=-1,authored_fill=-1; f32 score=1e9f;u32 animated=0;
        for(u32 i=0;i<dl_magic_picture_count;++i) {
            if(dl_str_equal(dl_magic_pictures[i].name,base,24)) frame=(s32)i;
            if(dl_str_equal(dl_magic_pictures[i].name,native_fill,24)) authored_fill=(s32)i;
            if(!dl_magic_pictures[i].fill || !dl_magic_pane(dl_magic_pictures[i].name,&dl_magic_query)) continue;
            /* Choose the authored green picture nearest the selected capacity's
             * base size/centre. Its native texture supplies the rounded ends. */
            f32 d=dl_abs(dl_magic_query.w-dl_magic_base[v].w)+
                2*dl_abs(dl_magic_query.x-dl_magic_base[v].x)+2*dl_abs(dl_magic_query.y-dl_magic_base[v].y);
            if((dl_magic_pictures[i].progress && !animated) ||
               (dl_magic_pictures[i].progress==animated && d<score)) {
                score=d;fill=(s32)i;animated=dl_magic_pictures[i].progress;dl_magic_fill[v]=dl_magic_query;
            }
        }
        /* Without a distinct progress-animated fill, some layouts put the
         * entire coloured gauge in MagicBase. Retain that fallback rather
         * than selecting a smaller static green highlight. */
        if(frame>=0 && dl_magic_pictures[frame].fill && !animated) { fill=frame;dl_magic_fill[v]=dl_magic_base[v]; }
        /* The real MagicBar layout names its two capacity fills explicitly.
         * Their progress can live in another texture stage, so a missing FLPA
         * width/first-SRT track must not select the green background instead.
         * Native visibility also does not select our independent capacity. */
        if(authored_fill>=0) {
            if(!dl_magic_pane(native_fill,&dl_magic_fill[v])) {report->stage="fill-geometry";continue;}
            fill=authored_fill;
        }
        if(frame<0 || fill<0) {report->stage="base/fill-missing";continue;}
        dl_magic_outer_bounds(v,frame!=fill);
        report->black=dl_magic_pictures[fill].black;report->white=dl_magic_pictures[fill].white;
        /* A separate MagicBase is the native background/frame artwork. Its
         * green shading is intentional; neutralise only a shared base+fill. */
        dl_magic_images[v][0]=dl_magic_image(&dl_magic_base[v],(u32)frame,v,0,frame==fill && dl_magic_pictures[frame].fill);
        dl_magic_images[v][1]=dl_magic_image(&dl_magic_fill[v],(u32)fill,v,1,0);
        dl_magic_load_border(f,size,v);
        dl_magic_load_details(v,(u32)frame,(u32)fill);
        dl_gauge_reason(dl_magic_pictures[frame].probe,dl_magic_images[v][0]?"BASE":"base-image-failed");
        dl_gauge_reason(dl_magic_pictures[fill].probe,dl_magic_images[v][1]?(frame==fill?"BASE+FILL":"FILL"):"fill-image-failed");
        report->stage="cached";

    }
    dl_magic_loaded=dl_magic_images[0][0] && dl_magic_images[0][1] &&
        dl_magic_images[1][0] && dl_magic_images[1][1];
    /* Before magic is unlocked its hidden native panes may never calculate a
     * matrix. Use Main_00's authored placement, not a guessed screen position. */
    dl_copy_string(dl_magic_key,sizeof dl_magic_key,"layout-pane:Main_00/L_MagicBar_00",40);
    if(wwhd_game_pane_get(dl_magic_key,&dl_magic_query)) {
        dl_magic_authored[0]=dl_magic_query.x; dl_magic_authored[1]=dl_magic_query.y;
        dl_magic_authored_valid=1;
    }
}
WWHD_HOOK_RETURN(0x02675414, dl_magic_track, (void* magic)) {
    u32 save=WWHD_GAME_DATA(WWHD_ADDR_save_info_pointer,u32);
    u32 v=save && *((u8*)save+0x33)>16;
    if(!dl_game_span((u32)magic,0x8c)) return;
    u32 pane=dl_word(magic,0x5c+v*4);
    if(!dl_game_span(pane,0xa4)) return;
    f32 w=dl_abs(*(f32*)(pane+0x48) * *(f32*)(pane+0x3c));
    f32 h=dl_abs(*(f32*)(pane+0x5c) * *(f32*)(pane+0x40));
    f32 x=*(f32*)(pane+0x54),y=*(f32*)(pane+0x64);
    if(!(w>8 && w<600 && h>2 && h<150 && x>-2000 && x<2000 && y>-1000 && y<1000)) return;
    u32 origin=*((u8*)pane+0x47), bh=origin&3, bv=(origin>>2)&3;
    dl_magic_position[0]=640+x+(bh==1 ? 0 : bh==2 ? -w : -w/2);
    dl_magic_position[1]=360-y+(bv==1 ? h : bv==2 ? 0 : h/2)+8;
    dl_magic_position[2]=w; dl_magic_position[3]=h; dl_magic_position_valid=1;
}
/* Shared geometry/visibility check: the native shortcut reserves space only
 * when the same gauge frame can actually be submitted by dl_magic_hud. */
static int dl_magic_place(u32 v,const wwhd_game_pane* p,f32 fraction,u32 image,wwhd_hud_element* out) {
    if(!dl_magic_images[v][0] || !dl_magic_images[v][1]) return 0;
    u32 anchor=WWHD_HUD_CENTER;
    if(!dl_magic_position_valid) {
        if(!dl_magic_authored_valid) return 0;
        dl_magic_position[0]=640+dl_magic_authored[0]+dl_magic_base[v].x-dl_magic_base[v].w/2;
        dl_magic_position[1]=360-dl_magic_authored[1]-dl_magic_base[v].y+dl_magic_base[v].h/2+8;
        dl_magic_position[2]=dl_magic_base[v].w; dl_magic_position[3]=dl_magic_base[v].h;
        anchor=WWHD_HUD_TOP_LEFT;
    }
    f32 scale=dl_magic_position[2]/dl_magic_base[v].w;
    f32 scale_y=dl_magic_position[3]/dl_magic_base[v].h;
    f32 w=p->w*scale,h=p->h*scale_y;
    f32 x=dl_magic_position[0]+(p->x-dl_magic_base[v].x)*scale+(dl_magic_position[2]-w)/2;
    f32 y=dl_magic_position[1]-(p->y-dl_magic_base[v].y)*scale_y+(dl_magic_position[3]-h)/2;
    /* position[1] follows the native inner background. Reserve the outer
     * bottom margin of the native bar plus the top margin of our new row.
     * Their sum is the outer/inner height difference, also for off-centre
     * windows. Do not mutate position: repeated HUD reads must not drift. */
    y+=(dl_magic_outer[v].h-dl_magic_base[v].h)*scale_y;
    /* One rejected command invalidates the whole owner list, including
     * DARK. Unsupported/malformed native geometry must stay local here. */
    if(!(x>=-65536 && x<=65536 && y>=-65536 && y<=65536 &&
         w>0 && w<=65536 && h>0 && h<=65536 && fraction>0 && fraction<=1)) return 0;
    *out=(wwhd_hud_element){.kind=WWHD_HUD_IMAGE,.anchor=anchor,
        .x=x,.y=y,.w=w*fraction,.h=h,.rgba=0xffffffff,
        .image=image,.thickness=1,
        .u0=0,.v0=0,.u1=fraction,.v1=1};
    return 1;
}
static int dl_magic_element(u32 v,u32 layer,wwhd_hud_element* out) {
    return dl_magic_place(v,layer?&dl_magic_fill[v]:&dl_magic_base[v],
        layer?dl_gauge_fraction():1,dl_magic_images[v][layer],out);
}
static void dl_magic_hud(u32 list) {
    unsigned long long epoch=wwhd_hud_epoch();
    if(epoch!=dl_magic_epoch) {
        memset(dl_gauge_reports,0,sizeof dl_gauge_reports);
        dl_magic_epoch=epoch; dl_magic_loaded=dl_magic_attempted=0;
        dl_magic_position_valid=0; dl_magic_authored_valid=0; memset(dl_magic_images,0,sizeof dl_magic_images);
        memset(dl_magic_border,0,sizeof dl_magic_border);
        memset(dl_magic_detail_count,0,sizeof dl_magic_detail_count);
    }
    if(!dl_magic_loaded && !dl_magic_attempted) dl_magic_load();
    u32 save=WWHD_GAME_DATA(WWHD_ADDR_save_info_pointer,u32);
    u32 v=save && *((u8*)save+0x33)>16;
    if(!dl_magic_images[v][0] || !dl_magic_images[v][1]) return;
    /* Native frame pixels can be opaque inside the gauge. Submit the outer
     * frame first so the inner background and charge fill remain on top. */
    if(dl_magic_border[v] && dl_magic_place(v,&dl_magic_border_pane[v],1,dl_magic_border[v],&dl_element))
        wwhd_hud_emit(list,&dl_element);
    for(u32 layer=0;layer<2;++layer)
        if(dl_magic_element(v,layer,&dl_element)) wwhd_hud_emit(list,&dl_element);
    for(u32 i=0;i<dl_magic_detail_count[v];++i) {
        wwhd_game_pane* p=&dl_magic_details[v][i].pane;
        f32 fraction=1;
        if(dl_magic_details[v][i].progress) {
            f32 right=dl_magic_fill[v].x-dl_magic_fill[v].w/2+dl_magic_fill[v].w*dl_gauge_fraction();
            fraction=(right-(p->x-p->w/2))/p->w;
            if(!(fraction>0)) continue;if(fraction>1)fraction=1;
        }
        if(dl_magic_place(v,p,fraction,dl_magic_details[v][i].image,&dl_element))
            wwhd_hud_emit(list,&dl_element);
    }
}
