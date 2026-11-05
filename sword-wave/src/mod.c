/* SPDX-License-Identifier: MPL-2.0 */
#include "native.h"

/* Dawnlight's normal Great Spin projectile: one active wave, 40 units per
 * 30-Hz tick, 2400 range, a 150-half-width / 55-radius sword capsule. */
#define SPEED 1200.0f
#define RANGE 2400.0f
#define WIDTH 150.0f
#define RADIUS 55.0f
#define POINTS 11
static struct {
    daPy_lk_c* owner;
    u32 owner_id, manager;
    unsigned long long step, epoch;
    int active, attempted, armed, executing, initialized;
    s8 room;
    s16 heading;
    u8 stage[8];
    cXyz pos, dir, right, points[POINTS+1];
    f32 distance, collision_start;
    Capsule capsule;
    Status status;
    LineCheck line;
    SpinCallback effects[3];
    u32 colors[3][2];
    u32 colors_ready, seeded;
} wave;
static const u16 effect_ids[3] = {0x25, 0x26, 0x28};
static daPy_lk_c* player(u8* play) { return play ? (daPy_lk_c*)word(play, WWHD_PLAY_PLAYER_OFFSET) : 0; }
static int spin(const daPy_lk_c* link) { return link->mCurProc == 0x55 || link->mCurProc == 0x56; }
static volatile dSv_player_status_a_view* health(void) {
    u32 save = WWHD_GAME_DATA(WWHD_ADDR_save_info_pointer, u32);
    return save ? (volatile dSv_player_status_a_view*)(save + WWHD_OFFSET_save_info) : 0;
}
static int alive(void) {
    volatile dSv_player_status_a_view* h = health();
    return h && h->max_life >= 12 && h->max_life <= 80 && h->life > 0;
}
static int full(void) {
    volatile dSv_player_status_a_view* h = health();
    return alive() && h->life == h->max_life;
}
/* Native end immediately detaches the callback before its storage is reused.
 * 'discard' is used only after the particle world has already been destroyed. */
static void stop(int discard) {
    for (int i=0; i<3; ++i) {
        if (!discard && wave.effects[i].emitter) particle_end(&wave.effects[i]);
        wave.effects[i] = (SpinCallback){0};
    }
    put_word(&wave.capsule, 0, 0);
    wave.active = 0; wave.seeded = 0;
}
static void reset(int discard) {
    stop(discard);
    wave.owner = 0; wave.armed = wave.attempted = wave.executing = 0;
    wave.colors_ready = 0;
}
static int same_world(u8* play, daPy_lk_c* link) {
    if (link != wave.owner || word(link,4) != wave.owner_id ||
        word(play,0x5ab0) != wave.manager || ((fopAc_ac_c*)link)->current.roomNo != wave.room) return 0;
    for (int i=0;i<8;++i) if (play[WWHD_PLAY_START_STAGE_NAME_OFFSET+i]!=wave.stage[i]) return 0;
    return 1;
}
static void remember(u8* play, daPy_lk_c* link) {
    wave.owner=link; wave.owner_id=word(link,4); wave.manager=word(play,0x5ab0);
    wave.room=((fopAc_ac_c*)link)->current.roomNo;
    for (int i=0;i<8;++i) wave.stage[i]=play[WWHD_PLAY_START_STAGE_NAME_OFFSET+i];
    wave.step=wwhd_logic_step()-1;
}
/* Reuse only the native query's vtable identities. Every mutable field and both
 * embedded self-pointers belong to our query, never to Link's arrow query. */
static void initialize(daPy_lk_c* link) {
    if (!wave.initialized) {
        capsule_ctor(&wave.capsule); status_ctor(&wave.status);
        wave.initialized=1;
    }
    line_ctor(&wave.line);
    u8* p=wave.line.bytes;
    for(int i=0;i<7;++i) p[0x5c+i]=0;
    put_word(p,0x68,1); put_word(p,0,(u32)(p+0x58)); put_word(p,4,(u32)(p+0x64));
    const u32 offsets[4]={0x10,0x20,0x58,0x64};
    for(int i=0;i<4;++i) put_word(p,offsets[i],word(link->bytes+0xbb4,offsets[i]));
    status_init(&wave.status,120,0xff,link);
    put_word(&wave.status,0x28,0); put_word(&wave.status,0x2c,0);
    put_word(&wave.status,0x30,0); put_word(&wave.status,0x34,0);
    /* Attack-only native sword; sample the actual spin's damage/material. */
    u8* c=wave.capsule.bytes; const u8* source=link->bytes+SW_CYL;
    put_word(c,0,word(source,0)); put_word(c,0x10,word(source,0x10));
    c[0x14]=source[0x14]; put_word(c,0x18,0); put_word(c,0x2c,0);
    put_word(c,0x44,(u32)&wave.status); put_word(c,0x50,1); /* AtNoConHit */
    put_word(c,0x58,0); /* no hit callback borrowed from Link */
    for(int i=0;i<4;++i) c[0x6c+i]=source[0x6c+i];
    reset_hit(c);
}
static f32 root(f32 x) {
    if (!(x>0.000001f) || x>1.0e12f) return 0;
    f32 y=x>1?x:1;
    for(int i=0;i<24;++i) y=0.5f*(y+x/y);
    return y;
}
static void direction(daPy_lk_c* link) {
    wave.dir=(cXyz){angle_sin(wave.heading),0,angle_cos(wave.heading)};
    /* Lock-on aims at the native target's eye; otherwise shoot forwards from
     * the pre-spin heading, independently of the spinning body animation. */
    fopAc_ac_c* target=(fopAc_ac_c*)word(link,0x65b0);
    if(target) {
        cXyz d={target->eyePos.x-wave.pos.x,target->eyePos.y-wave.pos.y,target->eyePos.z-wave.pos.z};
        f32 n=root(d.x*d.x+d.y*d.y+d.z*d.z);
        if(n>0.001f) wave.dir=(cXyz){d.x/n,d.y/n,d.z/n};
    }
    f32 h=root(wave.dir.x*wave.dir.x+wave.dir.z*wave.dir.z);
    wave.right=h>0.001f ? (cXyz){wave.dir.z/h,0,-wave.dir.x/h} :
        (cXyz){angle_cos(wave.heading),0,-angle_sin(wave.heading)};
}
static void effects(void) {
    /* Keep the confirmed front curve. Index zero is the native callback's
     * emitter anchor; it emits indices count-1 through 1, not index zero. */
    for(int j=0;j<POINTS;++j) {
        s16 a=(s16)(-0x4000+j*0x8000/(POINTS-1));
        f32 side=WIDTH*angle_sin(a), forward=RADIUS*angle_cos(a);
        wave.points[j+1]=(cXyz){wave.pos.x+wave.right.x*side+wave.dir.x*forward,
            wave.pos.y+wave.dir.y*forward,wave.pos.z+wave.right.z*side+wave.dir.z*forward};
    }
    wave.points[0]=wave.points[1];
    for(int i=0;i<3;++i) {
        wave.effects[i].points=wave.points;
        wave.effects[i].count=wave.effects[i].emitter && !(wave.seeded&(1u<<i))?POINTS+1:0;
    }
}
/* Full states restore the guest arena and emitter pool together. Detect their
 * new host epoch before renewing particles, even if Link has not executed yet.
 * Only detach emitters in the restored manager with our exact callback. */
static void state_epoch(void) {
    unsigned long long epoch=wwhd_hud_epoch();
    if (wave.epoch==epoch) return;
    u8* play=wwhd_play_get();
    int same=play && word(play,0x5ab0)==wave.manager;
    for(int i=0;i<3;++i) {
        void* e=wave.effects[i].emitter;
        if(!same || !e || word(e,0x1e4)!=(u32)&wave.effects[i])
            wave.effects[i].emitter=0;
    }
    reset(0); wave.epoch=epoch;
}
/* The native stripe renderer connects the entire emitter particle list. Never
 * append another arc (or an automatic anchor particle) to that list: it creates
 * an end-to-start chord and a trail across the back. Seed once, then move those
 * same particles. Native allocation, shader setup and deferred deletion remain
 * in charge; no particle/list/GPU objects are copied, freed or relinked here. */
static int owned_emitter(void* emitter) {
    state_epoch();
    if(!wave.active || !emitter) return -1;
    for(int i=0;i<3;++i) if(wave.effects[i].emitter==emitter &&
        word(emitter,0x1e4)==(u32)&wave.effects[i]) return i;
    return -1;
}
static void place_arc(void* emitter, int renew) {
    u32 count=word(emitter,0x1b4);
    if(count<2 || count>POINTS) return;
    void* node=(void*)word(emitter,0x1ac);
    for(u32 j=0;j<count && node;++j) {
        void* particle=(void*)word(node,0);
        if(!particle || word(particle,0xd4)!=(u32)emitter) return;
        /* Do not revive particles already in native deferred teardown. */
        if(!((u8*)particle)[0x10c] && !((u8*)particle)[0x120]) {
            cXyz p=wave.points[1+j*(POINTS-1)/(count-1)];
            *(cXyz*)((u8*)particle+0x10)=p;
            *(cXyz*)((u8*)particle+0x1c)=(cXyz){0,0,0};
            *(cXyz*)((u8*)particle+0x28)=p;
            /* The stripe width must lie in the arc's plane. Native drawing
             * transports this axis between frames, so restore its normal after
             * physics and clear rotation around the tangent for our particles. */
            *(cXyz*)((u8*)particle+0x8c)=(cXyz){
                wave.dir.y*wave.right.z,
                wave.dir.z*wave.right.x-wave.dir.x*wave.right.z,
                -wave.dir.y*wave.right.x};
            *(u16*)((u8*)particle+0xc0)=0; /* JPADrawParams::mRotateAngle */
            if(renew) {
                put_real(particle,0x78,-1); /* native first-frame phase after aging */
                put_real(particle,0x7c,3); /* expires normally after detach */
            }
        }
        node=(void*)word(node,0xc);
    }
}
/* This runs AFTER cutTurn::executeAfter has seeded the arc, but BEFORE native
 * aging. Re-anchor after native physics as well, before the stripe is drawn. */
WWHD_HOOK(0x0281FBD8, before_particles, (void* emitter)) {
    int index=owned_emitter(emitter);
    if(index<0) return;
    /* Rolling finite lease: without this mod the native emitter and the
     * unrenewed particles expire on their own. Never set Immortal. */
    put_word(emitter,0x5c,(u32)real(emitter,0x194)+8);
    put_word(emitter,0x254,word(emitter,0x254)&~0x40u);
    wave.seeded|=1u<<index;
    wave.effects[index].count=0;
    place_arc(emitter,1);
}
WWHD_HOOK_RETURN(0x0281FBD8, after_particles, (void* emitter)) {
    if(owned_emitter(emitter)>=0) place_arc(emitter,0);
}
/* dirTypePrevPtcl uses the emitter origin at the list head. The cut-turn
 * callback updates that origin only when emitting: it is stale once our arc
 * travels, and initially coincides with the endpoint (zero tangent). Continue
 * the first segment backwards instead, without adding a rendered particle or
 * changing shared renderer state. The remaining native tangents are valid. */
WWHD_HOOK_RETURN(0x028304F0, stripe_direction, (void* particle, void* emitter, cXyz* out)) {
    if(owned_emitter(emitter)<0 || !particle || !out ||
        word(particle,0xd4)!=(u32)emitter ||
        (u32)particle!=word(emitter,0x1ac) || word(particle,8)) return;
    void* next=(void*)word(particle,0xc);
    if(!next) return;
    void* neighbor=(void*)word(next,0);
    if(!neighbor || word(neighbor,0xd4)!=(u32)emitter) return;
    const cXyz* p=(const cXyz*)((u8*)particle+0x28);
    const cXyz* q=(const cXyz*)((u8*)neighbor+0x28);
    *out=(cXyz){p->x-q->x,p->y-q->y,p->z-q->z};
}
static void launch(daPy_lk_c* link) {
    stop(0);
    wave.pos=((fopAc_ac_c*)link)->current.pos; wave.pos.y+=60;
    direction(link); initialize(link);
    wave.distance=0; wave.collision_start=link->m35A4+RADIUS;
    wave.active=1;
    csXyz rotation={0,wave.heading,0};
    for(int i=0;i<3;++i) {
        SpinCallback* original=(SpinCallback*)(link->bytes+SW_CALLBACK+i*0x10);
        wave.effects[i].vtable=original->vtable;
        void* emitter=particle_set((void*)wave.manager,1,effect_ids[i],&wave.pos,&rotation,0,255,
            &wave.effects[i],wave.room,&wave.colors[i][0],&wave.colors[i][1],0);
        if(emitter) {
            /* Finite native fallback even before the first particle pass. */
            put_word(emitter,0x5c,(u32)real(emitter,0x194)+8);
            put_word(emitter,0x254,word(emitter,0x254)&~0x40u);
            /* Stop only automatic emission; native cut-turn manual creation
             * still seeds the open arc with the real spin materials. */
            put_word(emitter,0x254,word(emitter,0x254)|1);
            if(original->emitter) put_real(emitter,0x238,real(original->emitter,0x238));
        }
    }
    effects();
}
static void update(u8* play, daPy_lk_c* link) {
    if(!wave.active) return;
    if(word(&wave.capsule,0x54)&1) { stop(0); return; } /* native shield hit */
    f32 dt=(f32)wwhd_logic_dt();
    if(!(dt>0)) return;
    if(dt>1.0f/15) dt=1.0f/15;
    f32 travel=SPEED*dt;
    if(wave.distance+travel>RANGE+0.01f) { stop(0); return; }
    cXyz next={wave.pos.x+wave.dir.x*travel,wave.pos.y+wave.dir.y*travel,wave.pos.z+wave.dir.z*travel};
    /* Sweep the center and both outer edges: a wide wave cannot clip a wall
     * just because its center happens to fit through an opening. */
    for(int i=-1;i<=1;++i) {
        cXyz a={wave.pos.x+wave.right.x*WIDTH*i,wave.pos.y,wave.pos.z+wave.right.z*WIDTH*i};
        cXyz b={next.x+wave.right.x*WIDTH*i,next.y,next.z+wave.right.z*WIDTH*i};
        line_set(&wave.line,&a,&b,link);
        if(line_cross(play+0x12a0,&wave.line)) { stop(0); return; }
    }
    wave.pos=next; wave.distance+=travel;
    effects();
    status_move(wave.status.bytes+0x1c);
    if(wave.distance<wave.collision_start) return;
    cXyz a={wave.pos.x-wave.right.x*WIDTH,wave.pos.y,wave.pos.z-wave.right.z*WIDTH};
    cXyz b={wave.pos.x+wave.right.x*WIDTH,wave.pos.y,wave.pos.z+wave.right.z*WIDTH};
    *(cXyz*)(wave.capsule.bytes+0x118)=a;
    *(cXyz*)(wave.capsule.bytes+0x124)=b;
    put_real(&wave.capsule,0x134,RADIUS);
    *(cXyz*)(wave.capsule.bytes+0x7c)=(cXyz){wave.dir.x*SPEED/30,wave.dir.y*SPEED/30,wave.dir.z*SPEED/30};
    collision_set(play+0x26a4,&wave.capsule);
}
static void begin_spin(daPy_lk_c* link) {
    state_epoch();
    u8* play=wwhd_play_get();
    if(link!=player(play)) return;
    if(!same_world(play,link)) { reset(word(play,0x5ab0)!=wave.manager); remember(play,link); }
    wave.heading=((fopAc_ac_c*)link)->shape_angle.y;
    wave.armed=1; wave.attempted=0; wave.colors_ready=0;
}
WWHD_HOOK(SW_SPIN, spin_begin, (daPy_lk_c* link)) { begin_spin(link); }
WWHD_HOOK(SW_ROLL, roll_begin, (daPy_lk_c* link)) { begin_spin(link); }
/* Observe the native spin's colors. Do not substitute or mutate its emitter. */
WWHD_HOOK(SW_PARTICLE, capture_colors, (void* manager,u8 group,u16 id,const cXyz* pos,
    const csXyz* rot,const cXyz* scale,u8 alpha,SpinCallback* cb,s8 room,
    const u32* primary,const u32* environment,const cXyz* scale2)) {
    (void)pos;(void)rot;(void)scale;(void)alpha;(void)room;(void)scale2;
    if(!wave.executing || !wave.owner || !wave.armed || group!=1 ||
       (u32)manager!=wave.manager || !primary || !environment) return;
    for(int i=0;i<3;++i) if(id==effect_ids[i] && cb==(SpinCallback*)(wave.owner->bytes+SW_CALLBACK+i*0x10)) {
        wave.colors[i][0]=*primary; wave.colors[i][1]=*environment;
        wave.colors_ready|=1u<<i;
    }
}
WWHD_HOOK(SW_LINK, before_execute, (daPy_lk_c* link)) {
    state_epoch();
    u8* play=wwhd_play_get();
    if(link!=player(play)) return;
    if(!same_world(play,link)) { reset(word(play,0x5ab0)!=wave.manager); remember(play,link); }
    wave.executing=1;
    if(play[WWHD_PLAY_ENABLE_NEXT_STAGE_OFFSET] || !alive()) { reset(0); return; }
}
WWHD_HOOK_RETURN(SW_LINK, after_execute, (daPy_lk_c* link)) {
    wave.executing=0;
    u8* play=wwhd_play_get();
    if(link!=player(play) || !same_world(play,link)) return;
    if(!alive() || play[WWHD_PLAY_ENABLE_NEXT_STAGE_OFFSET] || !wwhd_config_bool("enabled",1)) {
        stop(0); wave.armed=0; return;
    }
    unsigned long long step=wwhd_logic_step();
    if(step==wave.step) return;
    wave.step=step;
    if(game_paused() || menu_open()) {
        for(int i=0;i<3;++i) wave.effects[i].count=0;
        return;
    }
    if(play[WWHD_PLAY_EVENT_RUNNING_OFFSET]) { stop(0); wave.armed=0; return; }
    update(play,link);
    if(!spin(link)) { wave.armed=0; return; }
    if(!wave.armed || wave.attempted || link->mEquipItem!=0x103) return;
    /* Native cylinder OnAtSetBit is the actual damaging window, not input or
     * the charging animation. Never turn healing midway through a spin into
     * a delayed launch. Missing effect setup fails closed for this attack. */
    if(word(link,SW_CYL)&1) {
        wave.attempted=1;
        if(full() && wave.colors_ready==7 && wave.manager) launch(link);
    }
}
WWHD_HOOK(SW_DELETE, delete_link, (daPy_lk_c* link)) { state_epoch(); if(link==wave.owner) reset(0); }
/* Save deserialization may replace all actors/particle pools. Drop handles
 * before it runs; Link deletion also handles ordinary stage teardown. */
WWHD_HOOK(SW_LOAD, load_save, (void)) { state_epoch(); reset(0); }
