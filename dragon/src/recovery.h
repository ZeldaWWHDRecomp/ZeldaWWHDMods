/* Recovery decisions shared by the real guest lifecycle and native tests. */
#pragma once
#include "hud.h"
#define DRAGON_PRIVATE_TAG 0x44520000u
#define DRAGON_PRIVATE_MASK 0xffff0000u
static inline int dragon_request_owned(unsigned actor_id,unsigned parameter,
                                       unsigned request_id,unsigned request_parameter) {
    return request_id!=~0u && request_parameter && actor_id==request_id && parameter==request_parameter;
}
static inline int dragon_wait_cancel(enum dragon_phase phase,float ticks,
                                    int same_player,int sea,int event,int cancel) {
    if(phase!=DRAGON_LOADING && phase!=DRAGON_APPROACH)return 0;
    return !same_player || !sea || event || cancel || ticks>300;
}
static inline int dragon_rope_ready(unsigned rope,int initialized) {
    return rope!=0 && initialized!=0;
}
