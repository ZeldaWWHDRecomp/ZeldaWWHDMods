#pragma once
#include "wwhd_guest.h"
enum dragon_phase { DRAGON_IDLE, DRAGON_LOADING, DRAGON_APPROACH, DRAGON_LAUNCH,
                    DRAGON_TAKEOFF, DRAGON_RIDING, DRAGON_LEAVING, DRAGON_SONG };
typedef struct {
    int visible, quest_visible, phase, conducting;
    unsigned notes;
    float altitude, minimum;
} dragon_hud_state;
extern dragon_hud_state dragon_hud;
void dragon_hud_register(void);
