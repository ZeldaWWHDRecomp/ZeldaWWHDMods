#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t u8;
typedef uint32_t u32;
typedef int8_t s8;
/* fixture actor: the fields hitstop.c reads (group, health) and sub_method at +0xF0 (a fake guest pointer) */
typedef struct { u8 group; s8 health; unsigned cut_type, parry_state; u8 pad[0xF0 - 12]; u32 sub_method; } fopAc_ac_c;
_Static_assert(__builtin_offsetof(fopAc_ac_c, sub_method) == 0xF0, "fixture sub_method");
static u32 fake_execute; /* what the fake sub_method's +8 holds */
static int qol_guest_ptr(u32 a) { return a == 0x10000000u; }
static u32 qol_ld32(u32 a) { assert(a == 0x10000008u); return fake_execute; }
static int event_mode;
static int qol_event(void) { return event_mode; }
static int kinds_on[5] = {1, 1, 1, 1, 0};
static const char* const kind_names[5] = {"hit_freeze_hits", "hit_freeze_strong", "hit_freeze_defeats", "hit_freeze_bosses", "hit_freeze_signs"};
static int wwhd_config_bool(const char* id, int fallback) {
    for (int i = 0; i < 5; i++) if (!strcmp(id, kind_names[i])) return kinds_on[i];
    assert(0); return fallback;
}
#define WWHD_ADDR_fopAc_Execute 0x025D475C
static _Alignas(4) u8 play[0x6000];
static u8 request_pending, active;
static int have_play=1;
static const char* option="Original";
#define QOL_PLAYER 0x5B2C
#define QOL_EVENT_MODE 0x5292
#define WWHD_HOOK_RETURN(address,name,params) static void name params
#define WWHD_GAME_DATA(address,type) request_pending
static u8* qol_play(void) { return have_play?play:0; }
static int wwhd_config_string(const char* id,char* out,unsigned n) {
    assert(!strcmp(id,"enemy_hit_freeze"));snprintf(out,n,"%s",option);return 1;
}
#include "../src/hitstop.c"
_Static_assert(sizeof(qol_hit_info)==12,"Native CcAtInfo prefix remains 12 bytes");
static void choose(const char* value) {option=value;hitstop_option_changed();}
/* Native scene pause contract: transfer the request_pending signed-byte request, then
 * decrement the active timer on subsequent logical ticks. True60 skips the
 * unconverted scene on half passes; this fixture models that scheduling only. */
static unsigned stalled_frames(unsigned hz) {
    unsigned stalled=0;
    for(unsigned frame=0;frame<600;++frame) {
        if(hz==60 && frame%2) {if(active)++stalled;continue;}
        if(request_pending) {active=request_pending;request_pending=0;}
        else if(active)--active;
        if(!active)return stalled;
        ++stalled;
    }
    assert(0);return 0;
}
int main(void) {
    *(u32*)(play+QOL_PLAYER)=0x12345678;
    fopAc_ac_c enemy={2,9,8,7,{0},0}, unchanged=enemy;
    qol_hit_info hit={0x1000,0x12345678,2,0,1,0};
    for(unsigned duration=1;duration<=127;++duration)for(int mode=0;mode<3;++mode) {
        choose(mode==0?"Original":mode==1?"Half":"Off");
        request_pending=duration;active=55;
        qol_hit_info before=hit;
        qol_enemy_hit_freeze(&enemy,&hit);
        assert(!memcmp(&hit,&before,sizeof hit));
        unsigned expected=mode==0?duration:mode==1?(duration+1)/2:0;
        assert(request_pending==expected && active==55);
        assert(!memcmp(&enemy,&unchanged,sizeof enemy));
        for(unsigned hz=30;hz<=60;hz+=30) {
            request_pending=expected;active=0;
            assert(stalled_frames(hz)==expected*(hz/30));
        }
    }
    choose("Off");
    /* A null object can leave stale sword/actor bytes; a valid object with no
     * actor is the other native early exit. Neither owns an existing request. */
    for(int scenario=0;scenario<8;++scenario) {
        qol_hit_info h=hit;fopAc_ac_c a=enemy;request_pending=6;active=4;
        if(scenario==0)h.object=0;
        if(scenario==1)h.actor=0;
        if(scenario==2)h.actor=0x87654321;
        if(scenario==3)h.result=9; // hammer, including its critical path
        if(scenario==4)a.group=0;
        if(scenario==5)play[QOL_EVENT_MODE]=1;
        if(scenario==6)have_play=0;
        if(scenario==7)h.result=10; // returning boomerang
        qol_enemy_hit_freeze(&a,&h);
        assert(request_pending==6 && active==4);
        play[QOL_EVENT_MODE]=0;have_play=1;
    }
    request_pending=7;qol_enemy_hit_freeze(0,&hit);qol_enemy_hit_freeze(&enemy,0);assert(request_pending==7);
    for(unsigned value=128;value<256;++value) {request_pending=value;qol_enemy_hit_freeze(&enemy,&hit);assert(request_pending==value);}
    choose("unrecognized");request_pending=6;qol_enemy_hit_freeze(&enemy,&hit);assert(request_pending==6);
    /* kinds: today's set (hits, strong, defeats, bosses) by default; each switched off keeps its pauses */
    choose("Off");
    hitstop_kinds_changed();
    struct { s8 health; u8 duration; u32 execute; int kind; } cases[] = {
        {5, 1, 0, 0}, {5, 2, 0, 0}, {5, 4, 0, 1}, {5, 6, 0, 1}, {0, 6, 0, 2}, {-1, 8, 0, 2}, {5, 3, 0x020F1494, 3}, {0, 1, 0x020F1494, 3}};
    for (unsigned c = 0; c < sizeof cases / sizeof *cases; c++)
        for (int off = -1; off < 5; off++) {
            for (int k = 0; k < 5; k++) kinds_on[k] = k == off ? 0 : k == 4 ? 0 : 1;
            hitstop_kinds_changed();
            fopAc_ac_c a = enemy; a.health = cases[c].health;
            a.sub_method = cases[c].execute ? 0x10000000u : 0; fake_execute = cases[c].execute;
            request_pending = cases[c].duration;
            qol_enemy_hit_freeze(&a, &hit);
            const int changed = off != cases[c].kind;
            assert(request_pending == (changed ? 0 : cases[c].duration));
        }
    for (int k = 0; k < 5; k++) kinds_on[k] = k != 4;
    hitstop_kinds_changed();
    /* a boss's or a signpost's own request in its step (not a sword hit): changed only when its kind is chosen */
    for (int mode = 0; mode < 3; mode++)
        for (int signs = 0; signs < 2; signs++)
            for (int who = 0; who < 3; who++) {  /* 0 another actor, 1 a boss, 2 a signpost */
                choose(mode == 0 ? "Original" : mode == 1 ? "Half" : "Off");
                kinds_on[4] = signs; hitstop_kinds_changed();
                fopAc_ac_c a = enemy; a.group = who == 2 ? 4 : 2;
                fake_execute = who == 1 ? 0x02066A38 : who == 2 ? 0x0218B234 : 0x02200000;
                a.sub_method = 0x10000000u;
                request_pending = 2;  /* a pause asked for earlier in this frame: Off keeps it */
                hitstop_actor_before(&a);
                request_pending = who == 2 ? 1 : 8;  /* the step's own request */
                qol_hit_freeze_actor_step(&a);
                const int chosen = who == 1 || (who == 2 && signs);
                const u8 asked = who == 2 ? 1 : 8;
                const u8 expected = !chosen || mode == 0 ? asked : mode == 1 ? (u8)((asked + 1) / 2) : 2;
                assert(request_pending == expected);
            }
    /* no change asked in the step, an event under way, or the sword path already handled it: left alone */
    choose("Off"); kinds_on[4] = 1; hitstop_kinds_changed();
    fopAc_ac_c boss = enemy; boss.sub_method = 0x10000000u; fake_execute = 0x02066A38;
    request_pending = 3; hitstop_actor_before(&boss); qol_hit_freeze_actor_step(&boss); assert(request_pending == 3);
    event_mode = 1; hitstop_actor_before(&boss); request_pending = 8; qol_hit_freeze_actor_step(&boss); assert(request_pending == 8);
    event_mode = 0;
    request_pending = 0; hitstop_actor_before(&boss); request_pending = 6; qol_enemy_hit_freeze(&boss, &hit);
    assert(request_pending == 0); request_pending = 5; qol_hit_freeze_actor_step(&boss); assert(request_pending == 5);
    puts("hitstop: kinds (hits, strong, defeats, bosses, signs) and the bosses' and signposts' own requests passed");
    puts("hitstop: actual observer guards, native combat-field isolation and 30/true60 duration contracts passed");
}
