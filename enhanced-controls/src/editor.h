/* SPDX-License-Identifier: MPL-2.0
 * Macro-only editor policy. Integer, versioned layout records are portable
 * between USA/EU guest modules; validated alternating files survive short writes. */
#ifndef ENHANCED_EDITOR_H
#define ENHANCED_EDITOR_H
#include "editor_geometry.h"
typedef struct { int x,y,size,opacity,shape,filled,icons,visible; } EcPlacement;
typedef struct { EcPlacement button[5]; int grid; } EcLayout;
typedef struct { unsigned magic,version,sequence,checksum; EcLayout layout; } EcLayoutFile;
typedef struct {
    EcLayout saved, draft;
    unsigned sequence;
    unsigned available;
    int open,selected,drag,action,ignore_up,error;
    float dx,dy,down_x,down_y;
} EcEditor;
enum { EE_SMALLER,EE_LARGER,EE_OPACITY,EE_SHAPE,EE_FILL,EE_ICONS,EE_VISIBLE,
       EE_GRID,EE_RESET,EE_CANCEL,EE_SAVE,EE_ACTIONS };
static inline EcLayout ee_defaults(void) {
    EcLayout l={0};
    for(int i=0;i<5;++i) l.button[i]=(EcPlacement){-150+i*100,-180,100,100,0,0,1,1};
    l.button[4]=(EcPlacement){348,-222,100,100,0,0,1,1};
    return l;
}
static inline EcRect ee_rect(EcPlacement p) { return (EcRect){p.x,p.y,88*p.size/100.f,60*p.size/100.f}; }
static inline int ee_limit(int v,int a,int b) { return v<a?a:v>b?b:v; }
static inline int ee_round(float x) { return (int)(x<0?x-.5f:x+.5f); }
static inline void ee_fit(EcPlacement* p) {
    p->size=ee_limit(p->size,60,180);
    int halfw=(88*p->size+199)/200, halfh=(60*p->size+199)/200;
    p->x=ee_limit(p->x,-620+halfw,620-halfw);
    p->y=ee_limit(p->y,-316+halfh,150-halfh);
}
static inline int ee_valid(const EcLayout* l) {
    if(l->grid<0 || l->grid>1) return 0;
    for(int i=0;i<5;++i) {
        EcPlacement p=l->button[i], fit=p; ee_fit(&fit);
        if(p.x!=fit.x || p.y!=fit.y || p.size!=fit.size || p.opacity<20 || p.opacity>100 ||
           p.shape<0 || p.shape>1 || p.filled<0 || p.filled>1 ||
           p.icons<0 || p.icons>1 || p.visible<0 || p.visible>1) return 0;
    }
    return 1;
}
static inline unsigned ee_hash(const EcLayoutFile* file) {
    /* Hash words, not host-endian bytes; includes generation and format. */
    unsigned h=2166136261u;
#define EE_HASH(v) h=(h^(unsigned)(v))*16777619u
    EE_HASH(file->magic); EE_HASH(file->version); EE_HASH(file->sequence);
    for(int i=0;i<5;++i) {
        const EcPlacement* p=&file->layout.button[i];
        EE_HASH(p->x); EE_HASH(p->y); EE_HASH(p->size); EE_HASH(p->opacity);
        EE_HASH(p->shape); EE_HASH(p->filled); EE_HASH(p->icons); EE_HASH(p->visible);
    }
    EE_HASH(file->layout.grid);
#undef EE_HASH
    return h;
}
static inline int ee_file_valid(const EcLayoutFile* f) {
    return f->magic==0x45434C59u && f->version==2 && ee_valid(&f->layout) && f->checksum==ee_hash(f);
}
static inline unsigned ee_mask(const EcEditor* e) { return e->available?e->available:15u; }
typedef struct { unsigned magic,version,sequence,checksum; EcPlacement button[4]; int grid; } EcLayoutFileV1;
static inline int ee_decode(EcLayoutFile* f,int bytes) {
    if(bytes==(int)sizeof(*f)) return ee_file_valid(f);
    if(bytes!=(int)sizeof(EcLayoutFileV1)) return 0;
    EcLayoutFileV1 old;
    unsigned char* dst=(unsigned char*)&old; const unsigned char* src=(const unsigned char*)f;
    for(unsigned i=0;i<sizeof(old);++i) dst[i]=src[i];
    if(old.magic!=0x45434c59u || old.version!=1) return 0;
    unsigned h=2166136261u;
    h=(h^old.magic)*16777619u; h=(h^old.version)*16777619u; h=(h^old.sequence)*16777619u;
    for(int i=0;i<4;++i) {
        EcPlacement p=old.button[i];
#define OLD_HASH(v) h=(h^(unsigned)(v))*16777619u
        OLD_HASH(p.x); OLD_HASH(p.y); OLD_HASH(p.size); OLD_HASH(p.opacity);
        OLD_HASH(p.shape); OLD_HASH(p.filled); OLD_HASH(p.icons); OLD_HASH(p.visible);
#undef OLD_HASH
    }
    h=(h^(unsigned)old.grid)*16777619u;
    if(h!=old.checksum) return 0;
    *f=(EcLayoutFile){old.magic,2,old.sequence,0,ee_defaults()};
    for(int i=0;i<4;++i) f->layout.button[i]=old.button[i];
    f->layout.grid=old.grid; f->checksum=ee_hash(f);
    return ee_file_valid(f);
}
static inline void ee_begin(EcEditor* e) {
    e->draft=e->saved; e->open=1; e->selected=0; e->drag=-1; e->action=-1;
    e->ignore_up=1; e->error=0;
    while(e->selected<4 && !(ee_mask(e)&(1u<<e->selected))) ++e->selected;
}
static inline void ee_close(EcEditor* e) { e->open=0; e->drag=e->action=-1; e->error=0; }
static inline EcRect ee_tab(int i) { return (EcRect){-400+200*i,305,184,46}; }
static inline EcRect ee_action_rect(int i) {
    if(i<7) return (EcRect){-480+160*i,245,150,46};
    return (EcRect){-300+200*(i-7),185,184,46};
}
static inline void ee_apply(EcEditor* e,int action) {
    EcPlacement* p=&e->draft.button[e->selected];
    switch(action) {
        case EE_SMALLER: p->size-=5; ee_fit(p); break;
        case EE_LARGER: p->size+=5; ee_fit(p); break;
        case EE_OPACITY: p->opacity=p->opacity<=20?100:p->opacity-20; break;
        case EE_SHAPE: p->shape^=1; break;
        case EE_FILL: p->filled^=1; break;
        case EE_ICONS: p->icons^=1; break;
        case EE_VISIBLE: p->visible^=1; break;
        case EE_GRID: e->draft.grid^=1; break;
        case EE_RESET: e->draft=ee_defaults(); break;
    }
    e->error=0;
}
/* Returns an action only on matching DOWN/UP without a swipe. */
static inline int ee_touch(EcEditor* e,unsigned kind,float x,float y) {
    if(kind==3 || kind==4 || kind==5) { e->drag=e->action=-1; e->ignore_up=0; return -1; }
    if(kind==0) {
        e->ignore_up=0; e->drag=e->action=-1; e->down_x=x; e->down_y=y;
        for(int i=0;i<5;++i) if((ee_mask(e)&(1u<<i)) && ec_inside(ee_tab(i),x,y)) { e->selected=i; return -1; }
        for(int i=0;i<EE_ACTIONS;++i) if(ec_inside(ee_action_rect(i),x,y)) { e->action=i; return -1; }
        /* Selected button wins overlap, then the last drawn button. */
        int hit=ec_inside(ee_rect(e->draft.button[e->selected]),x,y)?e->selected:-1;
        if(hit<0) for(int i=0;i<5;++i) if((ee_mask(e)&(1u<<i)) && ec_inside(ee_rect(e->draft.button[i]),x,y)) hit=i;
        if(hit>=0) {
            e->selected=e->drag=hit;
            e->dx=x-e->draft.button[hit].x; e->dy=y-e->draft.button[hit].y;
        }
    } else if(kind==1 || kind==2) {
        if(e->ignore_up) { if(kind==2) e->ignore_up=0; return -1; }
        if(e->drag>=0) {
            EcPlacement* p=&e->draft.button[e->drag];
            p->x=ee_round(x-e->dx); p->y=ee_round(y-e->dy);
            if(e->draft.grid) { p->x=20*ee_round(p->x/20.f); p->y=20*ee_round(p->y/20.f); }
            ee_fit(p); e->error=0;
        }
        if(ec_d2(x,y,e->down_x,e->down_y)>24*24) e->action=-1;
        if(kind==2) {
            int a=e->action; e->action=e->drag=-1;
            return a>=0 && ec_inside(ee_action_rect(a),x,y)?a:-1;
        }
    }
    return -1;
}
#endif
