#pragma once
#include "wwhd_guest.h"
#include "wwhd/actor.h"
#include "wwhd/link.h"
#include "wwhd/objects.h"
#include "wwhd/data.h"
#include "wwhd/audio.h"
#include "wwhd/bindings.h"
static inline u32* dragon_word(void* object,u32 offset) {return (u32*)((u8*)object+offset);}
static inline int dragon_sea(void) {
    u8* play=wwhd_play_get();if(!play)return 0;
    u8* stage=play+WWHD_PLAY_START_STAGE_NAME_OFFSET;
    return stage[0]=='s' && stage[1]=='e' && stage[2]=='a' && !stage[3] &&
        !play[WWHD_PLAY_ENABLE_NEXT_STAGE_OFFSET];
}
static inline int dragon_event(void) {
    u8* play=wwhd_play_get();return !play || play[WWHD_PLAY_EVENT_RUNNING_OFFSET];
}
static inline fopAc_ac_c* dragon_actor(void* actor) {return (fopAc_ac_c*)actor;}
static inline float dragon_abs(float x) {return x<0?-x:x;}
static inline float dragon_distance_squared(cXyz a,cXyz b) {
    float x=a.x-b.x,y=a.y-b.y,z=a.z-b.z;return x*x+y*y+z*z;
}
static inline int dragon_roost(daPy_lk_c* player) {
    return dragon_sea() && dragon_actor(player)->current.roomNo==13;
}
static inline int dragon_profile(u32 create) {
    for(u32 i=0;i<0x200;++i) {
        u32 profile=wwhd_get_025E10E0(i);
        if(profile<0x10000000u || profile>=0x11000000u)continue;
        u32 methods=*(u32*)(profile+WWHD_OFFSET_actor_profile_methods);
        if(methods>=0x10000000u && methods<0x11000000u && *(u32*)methods==create)return (int)i;
    }
    return -1;
}
