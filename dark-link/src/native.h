/* WWHD USA-v0 identities; WWHD_GAME_FUNC/HOOK relocate these for EU at install.
 * Layout evidence: public ZeldaWWHDDecomp/wwhd (CC0), d_cc_uty.h, d_cc_d.h,
 * JSystem/J3DGraph.h and d_a_player_main_01.h. No game data is included. */
#pragma once
#include "wwhd_guest.h"
#include "wwhd/functions.h"
#include "wwhd/actor.h"
#include "wwhd/link.h"
#include "wwhd/save.h"

#define DL_AT_POWER 0x02518DB0
#define DL_CC_CHECK 0x025192A8
#define DL_MODEL_ENTRY 0x025E2E5C
#define DL_MODEL_UPDATE 0x025E2DE0
#define DL_MODEL_VIEW 0x027F55FC
#define DL_JOINT_ENTRY 0x027F583C

WWHD_GAME_FUNC(0x027F9F0C, f32*, dl_material_parameter, (void*, u32));

WWHD_GAME_FUNC(0x027DF9B0, s32, dl_material_name_index, (void*, const char*));
WWHD_GAME_FUNC(0x025986BC, int, dl_menu_open, (void));
WWHD_GAME_FUNC(WWHD_ADDR_dScnPly_isPause, int, dl_paused, (void));

typedef struct {
    u32 object, actor;
    u8 damage, dead, type, pad;
    s16 angle[3];
    u16 cut;
    u32 particle_position, hit_sound;
} DLAttack;
_Static_assert(sizeof(DLAttack) == 0x1c, "CcAtInfo size");
_Static_assert(__builtin_offsetof(DLAttack, damage) == 8, "CcAtInfo damage");

static u32 dl_word(const void* p, u32 offset) { return *(const u32*)((const u8*)p + offset); }
static u32 dl_actor_id(const void* p) { return dl_word(p, 4); }
static daPy_lk_c* dl_player(void) {
    u8* play = wwhd_play_get();
    return play ? (daPy_lk_c*)dl_word(play, WWHD_PLAY_PLAYER_OFFSET) : 0;
}
static int dl_sword(const DLAttack* hit, const daPy_lk_c* link) {
    if (!hit || !link || !hit->object || hit->actor != (u32)link || hit->type != 1) return 0;
    /* Require one of Link's actual three sword capsules, not a carried weapon,
     * hammer, bomb, arrow, enemy sword, or merely RESULT_SWORD classification. */
    u32 first = (u32)link + 0x7b1c;
    return (hit->object == first || hit->object == first + 0x138 ||
            hit->object == first + 2 * 0x138) &&
           (dl_word((void*)hit->object, 0x10) & 2);
}

/* Exact current-player model identities. PlayerModelState begins at Link+0xd90;
 * +0xec scabbard, +0xf0 selected shield, +0x36b0 held item (sword only).
 * Evidence: setSwordModel, initModel, draw, setEquipItem models in main_01/04. */
#define DL_MODEL_PARTS 8
enum { DL_SURFACE_BODY, DL_SURFACE_EYE, DL_SURFACE_AURA, DL_SURFACE_BODY_BLACK };
static int dl_model_part(const daPy_lk_c* link, u32 model) {
    if (!link || !model) return -1;
    if (model == link->mpCLModel) return 0;
    if (model == link->mpHandsModel) return 1;
    if (model == link->mpKatsuraModel) return 2;
    if (model == link->mpEquippedSwordModel) return 3;
    if (model == dl_word(link, 0xe80)) return 4;
    if (model == dl_word(link, 0xe7c)) return 5;
    if (link->mEquipItem == 0x103 && model == dl_word(link, 0x4440)) return 6;
    if (model == dl_word(link, 0x5700)) return 7; /* native Magic Armor shell */
    return -1;
}
