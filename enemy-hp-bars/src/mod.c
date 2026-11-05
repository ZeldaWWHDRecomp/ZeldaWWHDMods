/* SPDX-License-Identifier: MPL-2.0
 * Observational entry/return hooks only. No game memory writes, replacements,
 * saved actor pointers, assets, heap allocation, or save-file changes.
 */
#include "wwhd/actor.h"
#include "wwhd/data.h"
#include "core.h"
static EhpState state;
static EhpVec player;
static int active, registered, bars=1, numbers=1, after_hit, values;
static float scale=1, range=2000;
static int room=-999;
static u32 link_id;
static char stage[8];
static unsigned long long step=~0ull, epoch=~0ull, revisions[7];
static wwhd_hud_element element;
static float world[3], projected[2];
static char labels[EHP_BARS+EHP_POPUPS][24];
static int label_count;
static void draw(u32 list);
static int valid(u32 p, u32 bytes) { return !(p&3) && p>=0x10000000u && p<=0x50000000u-bytes; }
static int finite(float f) { return f==f && f>-10000000 && f<10000000; }
static int vector_ok(EhpVec p) { return finite(p.x) && finite(p.y) && finite(p.z); }
static EhpKey key(fopAc_ac_c *a) { return (EhpKey){(u32)a, *(u32 *)((u8 *)a+4), *(u16 *)((u8 *)a+0xE)}; }
static int equal8(const char *a, const char *b) {
    for (int i=0;i<8;++i) if (a[i]!=b[i]) return 0;
    return 1;
}
static float bounded(double n, float low, float high, float fallback) {
    if (!(n>=low && n<=high)) return fallback;
    return (float)n;
}
static void options(void) {
    static const char *ids[]={"hp_bars","damage_numbers","display_mode","show_values","scale","distance","enabled"};
    int changed=0;
    for(int i=0;i<7;++i) {
        unsigned long long r=wwhd_config_changed(ids[i]);
        if (!registered || r!=revisions[i]) { changed=1; revisions[i]=r; }
    }
    if (!changed) return;
    bars=wwhd_config_bool("hp_bars",1);
    numbers=wwhd_config_bool("damage_numbers",1);
    values=wwhd_config_bool("show_values",0);
    char mode[32]={0};
    wwhd_config_string("display_mode",mode,sizeof(mode));
    after_hit=mode[0]=='A' && mode[1]=='f';
    scale=bounded(wwhd_config_float("scale",100),50,200,100)/100;
    range=bounded(wwhd_config_float("distance",2000),300,6000,2000);
    if (!wwhd_config_bool("enabled",1)) bars=numbers=0;
    ehp_clear(&state);
}
static void prepare(void) {
    unsigned long long now=wwhd_logic_step(), current_epoch=wwhd_hud_epoch();
    if (now==step && current_epoch==epoch) return;
    int reset=current_epoch!=epoch || now<step;
    step=now; epoch=current_epoch;
    options();
    if (!registered || reset) registered=wwhd_hud_register(draw,WWHD_HUD_TV)!=0;
    u8 *play=wwhd_play_get();
    if (!valid((u32)play,0x5B30)) { active=0; ehp_clear(&state); return; }
    u32 p=*(u32 *)(play+WWHD_PLAY_PLAYER_OFFSET);
    if (!valid(p,sizeof(fopAc_ac_c)) || *(u16 *)(p+0xE)!=0xA8) {
        active=0; ehp_clear(&state); return;
    }
    fopAc_ac_c *link=(fopAc_ac_c *)p;
    const char *name=(const char *)play+WWHD_PLAY_START_STAGE_NAME_OFFSET;
    if (!equal8(stage,name) || room!=link->current.roomNo || link_id!=key(link).id) reset=1;
    for(int i=0;i<8;++i) stage[i]=name[i];
    room=link->current.roomNo; link_id=key(link).id;
    player=(EhpVec){link->current.pos.x,link->current.pos.y,link->current.pos.z};
    int allowed=(bars || numbers) && vector_ok(player) &&
        !play[WWHD_PLAY_ENABLE_NEXT_STAGE_OFFSET] && !play[WWHD_PLAY_EVENT_RUNNING_OFFSET] &&
        !play[0x5BB2] && !play[0x5BB3];
    if (reset || !allowed || !active) ehp_clear(&state);
    active=allowed;
    if (active) ehp_tick(&state,(float)wwhd_logic_dt());
}
static void observe(fopAc_ac_c *a, unsigned profile, float height) {
    prepare();
    if (!active || !valid((u32)a,sizeof(*a))) return;
    EhpKey k=key(a);
    if (k.profile!=profile || a->current.roomNo!=room) return;
    /* Octorok rocks / Baba Buds / Wizzrobe spells share the enemy class. */
    unsigned type=a->mParameters&255;
    if ((profile==0xE3 && type!=0) || (profile==0xD6 && type!=0) ||
        (profile==0xD0 && type>=10)) return;
    EhpVec pos={a->current.pos.x,a->current.pos.y,a->current.pos.z};
    if (!vector_ok(pos) || ehp_distance2(player,pos)>range*range) return;
    EhpVec eye={a->eyePos.x,a->eyePos.y,a->eyePos.z};
    if (vector_ok(eye) && ehp_distance2(eye,pos)<600*600 && eye.y>=pos.y+10) pos=eye;
    else pos.y+=height;
    pos.y+=25;
    ehp_observe(&state,k,pos,a->health,a->max_health,numbers);
}
static void forget(fopAc_ac_c *a) {
    if (valid((u32)a,sizeof(*a))) ehp_forget(&state,key(a));
}
/* Entry initializes before a first lethal hit. Return reads FINAL native HP,
 * after e.g. Moblin restores HP for a boomerang stun. No cc_at_check hook. */
#define ENEMY(name,profile,execute,destroy,height) \
    WWHD_HOOK(execute, before_##name, (fopAc_ac_c *a)) { observe(a,profile,height); } \
    WWHD_HOOK_RETURN(execute, after_##name, (fopAc_ac_c *a)) { observe(a,profile,height); } \
    WWHD_HOOK(destroy, delete_##name, (fopAc_ac_c *a)) { forget(a); }
#include "enemies.h"
#undef ENEMY
WWHD_HOOK(0x0240EBB0, link_step, (void *link)) { (void)link; prepare(); }
WWHD_HOOK(0x02721908, loaded_save, (u32 self,u32 selected,u32 reset)) {
    (void)self; (void)selected; (void)reset;
    ehp_clear(&state); active=0; step=~0ull;
}
static int project(EhpVec p,float *x,float *y) {
    world[0]=p.x; world[1]=p.y; world[2]=p.z;
    if (!wwhd_screen_point(world,projected) || !finite(projected[0]) || !finite(projected[1])) return 0;
    *x=640+projected[0]; *y=360-projected[1];
    /* Renderer clips horizontal extent, including widescreen. */
    return *y>=0 && *y<=720 && *x>-4096 && *x<5376;
}
static void rect(u32 list,float x,float y,float w,float h,u32 rgba) {
    element=(wwhd_hud_element){.kind=WWHD_HUD_RECT,.anchor=WWHD_HUD_CENTER,
        .x=x,.y=y,.w=w,.h=h,.thickness=1,.u1=1,.v1=1,.rgba=rgba};
    wwhd_hud_emit(list,&element);
}
static char *integer(char *out,unsigned n) {
    char buf[12]; int count=0;
    do { buf[count++]=(char)('0'+n%10); n/=10; } while(n);
    while(count) *out++=buf[--count];
    *out=0; return out;
}
static void text(u32 list,float x,float y,float size,u32 color,int hp,int maximum) {
    if(label_count>=EHP_BARS+EHP_POPUPS) return;
    char *s=labels[label_count++], *end=integer(s,(unsigned)hp);
    if (maximum) { *end++='/'; end=integer(end,(unsigned)maximum); }
    unsigned n=(unsigned)(end-s);
    element=(wwhd_hud_element){.kind=WWHD_HUD_TEXT,.anchor=WWHD_HUD_CENTER,
        .x=x-(float)n*size*.28f,.y=y,.size=size,.thickness=1,.u1=1,.v1=1,
        .rgba=(color&255),.text=s,.text_bytes=n};
    element.x+=scale; element.y+=scale; wwhd_hud_emit(list,&element);
    element.x-=scale; element.y-=scale; element.rgba=color; wwhd_hud_emit(list,&element);
}
static void draw(u32 list) {
    label_count=0;
    /* interp.cpp::hook_025DE788 publishes guestmods::frame AFTER actor Execute,
     * then invokes draw_frame. Actor snapshots normally carry the previous
     * published counter. Accept that completed pass (and same-step callbacks),
     * but reject stale snapshots, a rewound counter or a different state epoch.
     * Do not prepare/tick here: drawing must not age or clear the new hit data. */
    unsigned long long drawn_step=wwhd_logic_step();
    if (!active || drawn_step<step || drawn_step-step>1 || epoch!=wwhd_hud_epoch()) return;
    if (bars) {
        int indices[EHP_BARS], count=ehp_nearest(&state,player,range,after_hit,indices);
        for(int i=0;i<count;++i) {
            const EhpEnemy *e=&state.enemies[indices[i]];
            float x,y; if(!project(e->position,&x,&y)) continue;
            float w=70*scale,h=7*scale,ratio=(float)e->hp/e->max_hp;
            u32 color=ratio>.5f?0x70D593FF:ratio>.25f?0xF2CB6BFF:0xED7165FF;
            rect(list,x-w/2-scale,y-scale,w+2*scale,h+2*scale,0x101C24E8);
            rect(list,x-w/2,y,w,h,0x45505AE8);
            rect(list,x-w/2,y,w*ratio,h,color);
            if(values) text(list,x,y+h+3*scale,13*scale,0xFFFFFFFF,e->hp,e->max_hp);
        }
    }
    if(numbers) for(int i=0;i<EHP_POPUPS;++i) {
        const EhpPopup *p=&state.popups[i];
        if(!p->damage || ehp_distance2(p->position,player)>range*range) continue;
        float x,y; if(!project(p->position,&x,&y)) continue;
        float alpha=p->age<.55f?1:(1-p->age)/.45f;
        u32 color=0xFFE1A000u|(u32)(255*alpha);
        x+=((int)(p->serial%3)-1)*12*scale;
        text(list,x,y-(18+40*p->age)*scale,20*scale,color,p->damage,0);
    }
}


/* Catalogue builds a single translation unit. */
#include "core.c"
