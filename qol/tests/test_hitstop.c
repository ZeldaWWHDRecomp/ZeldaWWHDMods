#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t u8;
typedef uint32_t u32;
typedef struct { u8 group; unsigned health, cut_type, parry_state; } fopAc_ac_c;
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
    fopAc_ac_c enemy={2,9,8,7}, unchanged=enemy;
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
    puts("hitstop: actual observer guards, native combat-field isolation and 30/true60 duration contracts passed");
}
