/* SPDX-License-Identifier: MPL-2.0
 * Cooperative touch-layout wire v1, shared verbatim by both mods.
 * The SDK has no IPC export. A synchronous guest hook rendezvous uses the
 * verified read-only hdHudMainStateNumber_c::getNumber (0267598C), whose entire
 * native body returns this->mNumber. Ordinary game objects are never handled:
 * requests must be in the loader's reserved mod arena and match this header.
 * No fixed scratch address, retained peer pointer, cross-owner host handle or
 * native-object writes. Responses are value copies into the caller's buffer.
 * Source: ZeldaWWHDDecomp/wwhd b2811a3, d/hd_screen_hud_main.cpp.
 */
#pragma once
#include "editor.h"
typedef struct {
    u32 magic,version,bytes,target,answered;
    unsigned long long epoch;
    u32 ready,open,selected;
    EcPlacement placement;
} EeExchange;
WWHD_GAME_FUNC(0x0267598C,u32,ee_rendezvous,(EeExchange*));
static EeExchange ee_query;
static inline int ee_packet(EeExchange* q,u32 role) {
    u32 p=(u32)q;
    return !(p&7) && p>=0x7f000000 && p<=0x80000000u-sizeof(*q) &&
        q->magic==0x45455443u && q->version==1 && q->bytes==sizeof(*q) &&
        q->target==role && !q->answered && q->epoch==wwhd_hud_epoch();
}
static inline int ee_ask(u32 role) {
    ee_query=(EeExchange){.magic=0x45455443u,.version=1,.bytes=sizeof(ee_query),
        .target=role,.epoch=wwhd_hud_epoch()};
    ee_rendezvous(&ee_query);
    EcLayout validation=ee_defaults(); validation.button[4]=ee_query.placement;
    return ee_query.answered==0x54434f4bu && ee_query.ready<=1 && ee_query.open<=1 &&
        ee_query.selected<5 && ee_valid(&validation);
}
