/* SPDX-License-Identifier: MPL-2.0
 * USA-v0 function identities, relocated to EU by the guest translator.
 * Layouts verified against ZeldaWWHDDecomp/wwhd a874e1a9 (CC0).
 * See README.md for the source/ABI map. */
#pragma once
#include "wwhd_guest.h"
#include "wwhd/actor.h"
#include "wwhd/link.h"
#include "wwhd/save.h"
#include "wwhd/functions.h"
#define SW_LINK 0x0240EBB0
#define SW_DELETE 0x0240EE28
#define SW_SPIN 0x023E6380
#define SW_ROLL 0x02441A44
#define SW_PARTICLE 0x025A847C
#define SW_LOAD 0x02721908
#define SW_CYL 0x78bc
#define SW_CALLBACK 0x6730

typedef struct { u32 vtable; u8 alpha, ending; s16 count; cXyz* points; void* emitter; } SpinCallback;
_Static_assert(sizeof(SpinCallback) == 0x10, "native spin callback");
typedef union { u32 align; u8 bytes[0x138]; } Capsule;
typedef union { u32 align; u8 bytes[0x3c]; } Status;
typedef union { u32 align; u8 bytes[0x6c]; } LineCheck;
WWHD_GAME_FUNC(0x025A847C, void*, particle_set, (void*, u8, u16, const cXyz*, const csXyz*, const cXyz*, u8, SpinCallback*, s8, const u32*, const u32*, const cXyz*));
WWHD_GAME_FUNC(0x025A9D64, void, particle_end, (SpinCallback*));
WWHD_GAME_FUNC(0x024445B0, void*, capsule_ctor, (void*));
WWHD_GAME_FUNC(0x02375AB8, void*, status_ctor, (void*));
WWHD_GAME_FUNC(0x02515F14, void, status_init, (void*, s32, s32, void*));
WWHD_GAME_FUNC(0x02515E50, void, status_move, (void*));
WWHD_GAME_FUNC(0x02516138, void, reset_hit, (void*));
WWHD_GAME_FUNC(0x0200E240, void, collision_set, (void*, void*));
WWHD_GAME_FUNC(0x02008FEC, void*, line_ctor, (void*));
WWHD_GAME_FUNC(0x024F1AFC, void, line_set, (void*, const cXyz*, const cXyz*, void*));
WWHD_GAME_FUNC(0x02008860, s32, line_cross, (void*, void*));
WWHD_GAME_FUNC(0x025AF2A4, s32, game_paused, (void));
WWHD_GAME_FUNC(0x025986BC, s32, menu_open, (void));
static u32 word(const void* p, u32 off) { return *(const u32*)((const u8*)p + off); }
static void put_word(void* p, u32 off, u32 value) { *(u32*)((u8*)p + off) = value; }
static f32 real(const void* p, u32 off) { return *(const f32*)((const u8*)p + off); }
static void put_real(void* p, u32 off, f32 value) { *(f32*)((u8*)p + off) = value; }
static f32 angle_sin(s16 a) { return ((volatile f32*)&WWHD_GAME_DATA(0x104A44F8,u8))[((u16)a >> 3)*2]; }
static f32 angle_cos(s16 a) { return ((volatile f32*)&WWHD_GAME_DATA(0x104A44F8,u8))[((u16)a >> 3)*2+1]; }
