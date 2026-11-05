/* Read-only observations of the existing gauge selection. No native loading,
 * game writes, resource exports or diagnostic files. Shown only on request. */
#pragma once
#define DL_GAUGE_ROWS 48
typedef struct {
    char name[24];
    const char* reason;
    u32 maps,progress,alpha,width,height;
} DLGaugeRow;
typedef struct {
    DLGaugeRow rows[DL_GAUGE_ROWS];
    u32 count,overflow,pictures,windows,uv_skips,animations,black,white;
    const char* stage;
    u32 frame_attempt,frame_flags,frame_part,frame_maps,frame_tw,frame_th;
    u32 frame_authored[4],frame_used[4];
    char frame_material[29],frame_texture[64];
} DLGaugeReport;
static DLGaugeReport dl_gauge_reports[2];
static u32 dl_gauge_variant;
static u32 dl_gauge_dimension(f32 v) { return v>=0 && v<=1000000?(u32)(v+0.5f):0; }
static u32 dl_gauge_row(const char* name,const char* reason) {
    DLGaugeReport* r=&dl_gauge_reports[dl_gauge_variant];
    if(r->count==DL_GAUGE_ROWS){++r->overflow;return DL_GAUGE_ROWS;}
    u32 i=r->count++;DLGaugeRow* row=&r->rows[i];
    memset(row,0,sizeof *row);dl_copy_string(row->name,24,name,24);row->reason=reason;
    return i;
}
static void dl_gauge_reason(u32 i,const char* reason) {
    if(i<DL_GAUGE_ROWS)dl_gauge_reports[dl_gauge_variant].rows[i].reason=reason;
}
static void dl_gauge_picture_report(dl_magic_picture* pic,u32 alpha,const wwhd_game_pane* pane) {
    if(pic->probe>=DL_GAUGE_ROWS)return;
    DLGaugeRow* row=&dl_gauge_reports[dl_gauge_variant].rows[pic->probe];
    row->maps=pic->maps;row->progress=pic->progress;row->alpha=alpha;
    row->width=dl_gauge_dimension(pane?pane->w:pic->pane[6]*pic->pane[8]);
    row->height=dl_gauge_dimension(pane?pane->h:pic->pane[7]*pic->pane[9]);
}
static void dl_gauge_probe_hud(u32 list) {
    u32 save=WWHD_GAME_DATA(WWHD_ADDR_save_info_pointer,u32),v=save && *((u8*)save+0x33)>16;
    /* HUD text must belong to the mod region. svc_hud_record rejects guest
     * stack buffers and drops the ENTIRE list (including gauge and buttons).
     * The service copies each string synchronously, so one static line suffices. */
    static char line[192];
    const DLGaugeReport* r=&dl_gauge_reports[v];u32 n=0;
#define GP_S(s) n=dl_probe_append(line,n,s)
#define GP_N(x) n=dl_probe_number(line,n,x)
    u32 lines=r->count>24?24:r->count;
    dl_rect(list,20,240,1240,58+15*lines+(r->frame_attempt?30:0),0x101018ef);
    GP_S("Gauge ");GP_N(v);GP_S(" stage=");GP_S(r->stage?r->stage:"not loaded");
    GP_S(" pic/wnd=");GP_N(r->pictures);GP_S("/");GP_N(r->windows);
    GP_S(" UVskip=");GP_N(r->uv_skips);GP_S(" overflow=");GP_N(r->overflow);
    GP_S(" anim-mask=");n=dl_probe_hex(line,n,r->animations);
    dl_text(list,28,246,line,n,0xffffffff,14);n=0;
    GP_S("Fill material black/white=");n=dl_probe_hex(line,n,r->black);GP_S("/");n=dl_probe_hex(line,n,r->white);
    GP_S(" | rows: name WxH textures/progress/alpha -> selection result");
    dl_text(list,28,265,line,n,0xffffffff,13);
    if(r->frame_attempt) {
        n=0;GP_S("Frame flags=");n=dl_probe_hex(line,n,r->frame_flags);
        GP_S(" part=");GP_N(r->frame_part);GP_S(" maps=");GP_N(r->frame_maps);
        GP_S(" tex=");GP_N(r->frame_tw);GP_S("x");GP_N(r->frame_th);
        GP_S(" material=");GP_S(r->frame_material);
        dl_text(list,28,285,line,n,0xffffffff,12);
        n=0;GP_S("Slices authored/used L,R,T,B=");
        for(u32 k=0;k<4;++k) {if(k)GP_S(",");GP_N(r->frame_authored[k]);GP_S("/");GP_N(r->frame_used[k]);}
        GP_S(" file=");GP_S(r->frame_texture);
        dl_text(list,28,300,line,n,0xffffffff,12);
    }
    for(u32 i=0;i<r->count;++i) {
        const DLGaugeRow* row=&r->rows[i];n=0;
        GP_S(row->name);GP_S(" ");GP_N(row->width);GP_S("x");GP_N(row->height);
        GP_S(" ");GP_N(row->maps);GP_S("/");GP_N(row->progress);GP_S("/");GP_N(row->alpha);
        GP_S(" ");GP_S(row->reason?row->reason:"?");
        dl_text(list,28+(i/24)*618,288+(r->frame_attempt?30:0)+(i%24)*15,line,n,0xffffffff,12);
    }
#undef GP_S
#undef GP_N
}
