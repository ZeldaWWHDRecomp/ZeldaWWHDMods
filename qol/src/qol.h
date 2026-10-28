/* QoL package: shared declarations (one translation unit: mod.c includes every feature file).
 * Each feature has an option in manifest.json (all off by default, "live": the player switches it while playing),
 * one entry in the feature table in mod.c, and its own file with <name>_option_changed() and its hooks; a hook
 * returns at once while its feature is off. */
#pragma once
#include "wwhd_guest.h"
#include "wwhd/bindings.h"
#include "wwhd/data.h"
#include "wwhd/actor.h"
#include "wwhd/link.h"

/* the game's state (dComIfGp: the accessor's object; offsets in the port's play_context.h) */
#define QOL_EVENT_MODE 0x5292     /* dEvt_control_c mode: 0 no event */
#define QOL_PLAYER 0x5B2C         /* mPlayerInfo[0].mpPlayer: Link while he plays */
#define QOL_ZR_ACTION 0x5BB5      /* mRStatus; +3 its forced variant */

static inline u8* qol_play(void) { return wwhd_play_get(); }
static inline int qol_event(void) { u8* p = qol_play(); return p && p[QOL_EVENT_MODE] != 0; }
static inline u8 qol_zr_action(void) {
    u8* p = qol_play();
    if (!p) return 0;
    return p[QOL_ZR_ACTION + 3] ? p[QOL_ZR_ACTION + 3] : p[QOL_ZR_ACTION];
}
static inline u32 qol_ld32(u32 a) { return *(volatile u32*)a; }
static inline int qol_guest_ptr(u32 a) { return a >= 0x10000000u && a < 0x50000000u; }

/* the features' option state, read again when its revision changes (mod.c) */
typedef struct {
    const char* option;
    void (*changed)(void);   /* the option's value changed (also once at start) */
    unsigned long long seen; /* the revision read last */
} qol_feature;
