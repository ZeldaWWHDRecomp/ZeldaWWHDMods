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
#include "rules.h"

/* the game's state (dComIfGp: the accessor's object; offsets in the port's play_context.h) */
#define QOL_EVENT_MODE 0x5292     /* dEvt_control_c mode: 0 no event */
#define QOL_PLAYER 0x5B2C         /* mPlayerInfo[0].mpPlayer: Link while he plays */
#define QOL_ZR_ACTION 0x5BB5      /* mRStatus; +3 its forced variant */
#define QOL_IN_PHOTO 0x52B2       /* a talk about a picto shown (mbInPhoto) */
#define QOL_PIC_SUBJECT 0x5BE6    /* the picto shown: its dSnap subject, flaws (1 face, 2 body), colour */
#define QOL_PIC_FLAWS 0x5BE7
#define QOL_PIC_COLOUR 0x5BEA

static inline u8* qol_play(void) { return wwhd_play_get(); }
static inline int qol_event(void) { u8* p = qol_play(); return p && p[QOL_EVENT_MODE] != 0; }
static inline u8 qol_zr_action(void) {
    u8* p = qol_play();
    if (!p) return 0;
    return p[QOL_ZR_ACTION + 3] ? p[QOL_ZR_ACTION + 3] : p[QOL_ZR_ACTION];
}
static inline u32 qol_ld32(u32 a) { return *(volatile u32*)a; }
static inline int qol_guest_ptr(u32 a) { return a >= 0x10000000u && a < 0x50000000u; }

/* the save in play (dSv_info_c, *dComIfG_save_info_pointer) and its HD object (+0x12C0: hdSaveHolders_c) */
#define QOL_HD_SAVE 0x12C0
static inline u32 qol_save(void) {
    u32 info = WWHD_GAME_DATA(WWHD_ADDR_dComIfG_save_info_pointer, u32);
    return qol_guest_ptr(info) ? info : 0;
}
static inline u32 qol_events(void) { u32 s = qol_save(); return s ? s + 0x644 : 0; }  /* mSavedata (+0x20) .mEvent (+0x624) */

/* Carlov's figurine orders (daNpcMt_c; his helpers work on the save, not on his actor: called with 0). The orders are
 * the HD save object's current card events (+0x8C, hdSaveInventory_c: +4 the figures u8[12], +0x10 their count). A
 * picto's subject becomes a figure as he does it: changePhotoNo, then 0x49.. 0xCE are figures 0.. 133. */
WWHD_GAME_FUNC(0x022A1F1C, u32, qol_mt_change_photo_no, (u32 photo));
WWHD_GAME_FUNC(0x025BD600, u32, qol_mt_photo_figure, (u32 photo));
WWHD_GAME_FUNC(0x0229FE28, u32, qol_mt_is_made, (void* carlov, u32 figure));
WWHD_GAME_FUNC(0x022A1F64, u32, qol_mt_has_order, (void* carlov, u32 figure));
WWHD_GAME_FUNC(0x022A037C, u32, qol_mt_order_count, (void* carlov));
WWHD_GAME_FUNC(0x022A0898, u32, qol_mt_order, (void* carlov, u32 index));
WWHD_GAME_FUNC(0x022A2008, void, qol_mt_submit, (void* carlov, u32 figure));
WWHD_GAME_FUNC(0x0229FF54, void, qol_mt_clear_orders, (void* carlov));
WWHD_GAME_FUNC(0x025B8B94, u32, qol_ev_is, (u32 events, u32 bit));
WWHD_GAME_FUNC(0x025B8B68, void, qol_ev_on, (u32 events, u32 bit));
WWHD_GAME_FUNC(0x025B8B7C, void, qol_ev_off, (u32 events, u32 bit));
#define QOL_FIGURES 0x86
#define QOL_NO_FIGURE 0xFFu
static inline u32 qol_figure_of(u32 subject) {
    u32 photo = qol_mt_change_photo_no(subject & 0xFF);
    return photo - 0x49 < QOL_FIGURES ? qol_mt_photo_figure(photo) & 0xFF : QOL_NO_FIGURE;
}

/* the features' option state, read again when its revision changes (mod.c) */
typedef struct {
    const char* option;
    void (*changed)(void);   /* the option's value changed (also once at start) */
    unsigned long long seen; /* the revision read last */
} qol_feature;
