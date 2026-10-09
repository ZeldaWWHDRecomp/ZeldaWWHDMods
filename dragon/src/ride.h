#pragma once
#include "game.h"
#include "hud.h"
typedef struct {
    enum dragon_phase phase;
    daPy_lk_c* player;
    fopAc_ac_c* actor;
    u32 id,buttons,old_leaf,rope,hook_model;
    float ticks;
    unsigned blocked_ticks;
    cXyz pos,grab_offset,audio_pos,launch_link,hook_pos,hand_offset;
    int grab_valid,song_boat,song_wind;
    float lift_start,lift_target,yaw,pitch,bank;
} dragon_ride_state;
extern dragon_ride_state dragon_ride;
extern int dragon_valoo_resources;
int dragon_tagged(void* actor);
int dragon_carried(void);
void dragon_release(daPy_lk_c* player);
void dragon_place_link(daPy_lk_c* player);
