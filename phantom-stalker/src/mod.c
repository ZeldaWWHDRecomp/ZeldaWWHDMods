/* Phantom stalker: now and then, at night on an island of the Great Sea, fog closes in and Phantom Ganon comes down
 * behind Link and hunts him around the island for two minutes. Getting back in the boat, leaving the island or
 * driving him off ends it. Windfall and Outset stay safe.
 *
 * Runs once per logic step, after the actors' execute (a return hook on fpcEx_Handler). Phantom Ganon is the game's
 * own (daFganon, the kind that waits high up and comes down when Link is near), made with fopAcM_create in Link's
 * room and process layer and left to fight with his own AI; the mod only keeps his home point on Link. Offsets are
 * the HD game's (USA and EU). */
#include "wwhd_guest.h"
#include "wwhd/functions.h"
#include "wwhd/data.h"
#include "wwhd/actor.h"
#include "wwhd/link.h"
#include "wwhd/objects.h"
#include "wwhd/save.h"

/* ---- the game ---- */
WWHD_GAME_FUNC(0x025D5834, u32, ps_create, (u32 proc, u32 param, cXyz* pos, s32 room, csXyz* angle, cXyz* scale, s32 subtype, u32 create_func));
WWHD_GAME_FUNC(0x025DE50C, u8*, ps_search, (u32 id));          /* fpcEx_SearchByID: the process while it exists */
WWHD_GAME_FUNC(0x025D57E4, s32, ps_delete, (u32 id));          /* fopAcM_delete by id */
WWHD_GAME_FUNC(0x025DED64, u32, ps_current_layer, (void));      /* fpcLy_CurrentLayer */
WWHD_GAME_FUNC(0x025DEAB4, void, ps_set_layer, (u32 layer));     /* fpcLy_SetCurrentLayer */
WWHD_GAME_FUNC(0x0255671C, void, ps_fog, (f32 start, f32 end, f32 ratio));  /* dKy_fog_startendz_set */
/* his particle bank (below) */
WWHD_GAME_FUNC(0x026124B0, u8*, ps_archive_file, (u32 archive, u32 archive_name, u32 file_name, u32 zero));  /* getArchiveFile */
WWHD_GAME_FUNC(0x027DFA24, u8*, ps_embedded_file, (u8* dictionary, const char* name));
WWHD_GAME_FUNC(0x025E3528, u32, ps_solid_heap, (u32 size, u32 parent));  /* mDoExt_createSolidHeapFromGame */
WWHD_GAME_FUNC(0x0273B050, u32, ps_new, (u32 size, u32 heap, s32 align));  /* operator new on a heap */
WWHD_GAME_FUNC(0x02827000, void, ps_resource_manager, (u32 self, u8* data, u32 heap, u32 user));  /* JPAResourceManager ctor */
WWHD_GAME_FUNC(0x025E3678, void, ps_adjust_heap, (u32 heap));   /* mDoExt_adjustSolidHeap */
WWHD_GAME_FUNC(0x025E3868, void, ps_destroy_heap, (u32 heap));  /* mDoExt_destroySolidHeap */
WWHD_GAME_FUNC(0x028221EC, void, ps_clear_bank, (u32 manager, u32 slot));  /* JPAEmitterManager::clearResourceManager */
WWHD_GAME_FUNC(0x02826EF8, u32, ps_bank_has, (u32 resources, u32 id));      /* the bank's emitter resource by id */
WWHD_GAME_ORIGINAL(0x02821448, u32, ps_emitter_original, (u32 manager, u32 pos, u32 id, u32 group, u32 bank, u32 callback, u32 callback2));
WWHD_GAME_ORIGINAL(0x028249B0, void, ps_rt_matrix_original, (u32 a, u32 b, u32 c));

#define NONE 0xFFFFFFFFu
#define PI 3.14159265358979323846
#define ITEM_WIND_WAKER 0x22
#define ITEM_SAIL 0x78
#define PLAY_LINK 0x5B34    /* dComIfG_play_c: Link */
/* Phantom Ganon (daFganon): type 3, the one that waits 10000 up, unseen, until Link is within 2550, then comes down */
#define FGANON_PROC 0xF1
#define FGANON_PARAM 0x00FFFF03u
#define FG_ACTION 0x5BA     /* s16: 6 after a volley */
#define FG_MODE 0x5BC       /* s16: 3 while he waits for his energy ball to die */
#define FG_BALL 0x63C       /* u8: his energy ball's state; 35 ends it, as when it flies too far */
/* sea rooms that stay safe: the towns. The sea is a 7x7 grid of 100000-unit squares, room 1 at the north-west; the
 * room is found from Link's place (his room byte is not yet his while a stage loads) */
#define ROOM_WINDFALL 11
#define ROOM_OUTSET 44

static u32 word(u8* p, u32 o) { return *(volatile u32*)(p + o); }
static cXyz pos_of(u8* a) { return ((fopAc_ac_c*)a)->current.pos; }
static u32 bits(f32 f) { union { f32 f; u32 u; } v = {f}; return v.u; }

/* ---- freestanding math and randomness (no libm in guest mods) ---- */
static double wrap(double x) {
    while (x > PI) x -= 2 * PI;
    while (x < -PI) x += 2 * PI;
    return x;
}
static double ps_sin(double x) {
    x = wrap(x);
    if (x > PI / 2) x = PI - x;
    if (x < -PI / 2) x = -PI - x;
    const double q = x * x;
    return x * (1 + q * (-1.0 / 6 + q * (1.0 / 120 + q * (-1.0 / 5040 + q * (1.0 / 362880)))));
}
static double ps_cos(double x) { return ps_sin(x + PI / 2); }
static s32 round_(double x) { return x < 0 ? -(s32)(-x + 0.5) : (s32)(x + 0.5); }  /* (|x| < 2^31 here) */
static s32 sea_room(cXyz at) { return (round_(at.z / 1e5) + 3) * 7 + round_(at.x / 1e5) + 3 + 1; }

static u32 rng;
static u32 rnd(void) {
    rng ^= rng << 13, rng ^= rng >> 17, rng ^= rng << 5;
    return rng;
}
static double rnd01(void) { return (rnd() >> 8) / 16777216.0; }

/* ---- options ---- */
/* How often: the chance each minute ashore at night (x0.4, x1, x3), or Always (on islands day and night) */
static int often = 1;
static const double kScale[] = {0.4, 1, 3};
static int choice(const char* id, const char* const* names, int count, int fallback) {
    char value[32];
    if (!wwhd_config_string(id, value, sizeof value)) return fallback;
    for (int i = 0; i < count; i++) {
        const char* a = value;
        const char* b = names[i];
        while (*a && *a == *b) a++, b++;
        if (!*a && !*b) return i;
    }
    return fallback;
}
static int always(void) { return often == 3; }

static int has_item(u8 item) {
    const u8* inventory = (const u8*)(WWHD_GAME_DATA(WWHD_ADDR_save_info_pointer, u32) + WWHD_OFFSET_save_inventory);
    for (int i = 0; i < 21; i++)
        if (inventory[i] == item) return 1;
    return 0;
}

/* ---- his particle bank ---- */
/* Phantom Ganon's effects (his energy and dark balls, dark shots, smoke, light-arrow hits) are in Ganon's Tower's scene
 * particle bank, Pscene080. The sea's rooms load their own scene bank into the emitter manager's slot 1, which lacks
 * them; his energy ball then sets the matrix of an emitter that was never made and the game halts. So Pscene080 goes
 * into slot 2 (the manager has 8, the game uses 0 and 1) the way dPa_control_c::createRoomScene makes a bank: a solid
 * heap from the game heap, a JPAResourceManager on it over the bank, an embedded file of Particle.bfres. A scene effect
 * slot 1 lacks is made from slot 2 (createSimpleEmitterID, replaced below), and the bank goes with the stage. */
#define BANK 2
#define MANAGER_BANKS 0x30          /* JPAEmitterManager: its resource managers by slot */
static u32 bank_heap;
static u32 manager(void) { return WWHD_GAME_DATA(0x1047B2D4, u32); }
static int in_mem2(u32 a) { return a >= 0x10000000 && a < 0x50000000; }
/* the archive's and the file's names as the game passes them: string objects (the text, then the class's vtable) */
static char names[0x30] = "Particle\0\0\0\0\0\0\0\0Particle.bfres\0\0Pscene080.jpc";
static u32 name_objects[4];
static void load_bank(u8* play) {
    const u32 mgr = manager(), control = word(play, 0x5AB0);  /* dPa_control_c */
    if (bank_heap || !in_mem2(mgr) || !in_mem2(control) || word((u8*)mgr, MANAGER_BANKS + 4 * BANK)) return;
    const u32 vtable = (u32)&WWHD_GAME_DATA(0x100534B4, u32);
    name_objects[0] = (u32)names, name_objects[1] = vtable;
    name_objects[2] = (u32)names + 0x10, name_objects[3] = vtable;
    u8* bfres = ps_archive_file(WWHD_GAME_DATA(0x101F4F7C, u32), (u32)&name_objects[0], (u32)&name_objects[2], 0);
    if (!bfres || !word(bfres, 0x4C)) return;
    u8* file = ps_embedded_file(bfres + 0x4C + word(bfres, 0x4C), names + 0x20);
    if (!file || !word(file, 0)) return;
    const u32 heap = ps_solid_heap(0, 0);  /* all that is free, then back down to what the bank took */
    if (!heap) return;
    const u32 res = ps_new(0xC, heap, 4);
    if (res) ps_resource_manager(res, file + word(file, 0), heap, word((u8*)control, 0x415C));
    ps_adjust_heap(heap);
    if (!res) {
        ps_destroy_heap(heap);
        return;
    }
    *(volatile u32*)(mgr + MANAGER_BANKS + 4 * BANK) = res;
    bank_heap = heap;
    wwhd_log("phantom-stalker: Phantom Ganon's particles loaded");
}
static int bank_has(u32 bank, u32 id) { return bank && ps_bank_has(word((u8*)bank, 4), id & 0xFFFF); }
/* a scene effect (bank 1) the stage's bank lacks is made from ours when ours has it; everything else as it was */
WWHD_REPLACE(0x02821448, u32, emitter, (u32 mgr, u32 pos, u32 id, u32 group, u32 bank, u32 callback, u32 callback2)) {
    if (bank_heap && bank == 1 && in_mem2(mgr)) {
        const u32 stage = word((u8*)mgr, MANAGER_BANKS + 4), ours = word((u8*)mgr, MANAGER_BANKS + 4 * BANK);
        if (!bank_has(stage, id) && bank_has(ours, id)) bank = BANK;
    }
    return ps_emitter_original(mgr, pos, id, group, bank, callback, callback2);
}
/* JPABaseEmitter::setGlobalRTMatrix: his energy ball calls it on its hit effect's emitters without checking them
 * (daFgmahou_Execute); with the bank loaded they exist, and without it (it could not be loaded, or went with the
 * stage) there is nothing to set */
WWHD_REPLACE(0x028249B0, void, rt_matrix, (u32 a, u32 b, u32 c)) {
    if (a < 0x1000 || b < 0x1000) return;
    ps_rt_matrix_original(a, b, c);
}
/* dPa_control_c::removeScene, as the play scene ends: the bank goes with the stage's own */
WWHD_HOOK(WWHD_ADDR_dPa_removeScene_025A809C, scene_removed, (void* control)) {
    (void)control;
    if (!bank_heap) return;
    ps_clear_bank(manager(), BANK);
    ps_destroy_heap(bank_heap);
    bank_heap = 0;
    wwhd_log("phantom-stalker: Phantom Ganon's particles freed with the stage");
}

/* ---- Phantom Ganon ---- */
static double now;                  /* game seconds */
static u32 phantom = NONE;          /* his process id */
static double next;                 /* no new one before this */
static double waiting;              /* since when he has waited on his energy ball */

/* behind Link, 1500-2000 out, at his height, facing him; made in Link's room and in his process layer (base_process_class
 * +0x2C), as an actor's own spawns are: after the executor the current layer is the root one */
static cXyz spawn_pos;
static csXyz spawn_angle;
static void spawn(u8* link) {
    const double heading = ((fopAc_ac_c*)link)->shape_angle.y * PI / 32768;
    const double a = heading + PI + (rnd01() - 0.5) * 1.6, dist = 1500 + rnd() % 500;
    const cXyz at = pos_of(link);
    spawn_pos.x = (f32)(at.x + ps_sin(a) * dist), spawn_pos.y = at.y, spawn_pos.z = (f32)(at.z + ps_cos(a) * dist);
    spawn_angle.x = 0, spawn_angle.y = (s16)(s32)(wrap(a + PI) * 32768 / PI), spawn_angle.z = 0;
    const u32 layer = ps_current_layer();
    ps_set_layer(word(link, 0x2C));
    phantom = ps_create(FGANON_PROC, FGANON_PARAM, &spawn_pos, ((fopAc_ac_c*)link)->current.roomNo, &spawn_angle, 0, -1, 0);
    ps_set_layer(layer);
    next = now + 20;
    waiting = now;
    wwhd_log("phantom-stalker: Phantom Ganon comes");
}

/* every step: he fights around his home point (fopAc_ac_c::home), so it is wherever Link is; his room is Link's, so the
 * room he was made in can unload without taking him; his own execute eases his scale, held at 1 */
static void keep(u8* pg, u8* link) {
    fopAc_ac_c* a = (fopAc_ac_c*)pg;
    const fopAc_ac_c* l = (const fopAc_ac_c*)link;
    a->home.pos.x = l->current.pos.x, a->home.pos.z = l->current.pos.z;
    a->current.roomNo = l->current.roomNo;
    a->scale.x = a->scale.y = a->scale.z = 1;
    /* after a volley he waits (action 6, mode 3) for his energy ball to die, which with nothing for a missed ball to hit
     * can take a long while: after 8 s it is ended, and he flies on to his next attack */
    if (*(volatile s16*)(pg + FG_ACTION) != 6 || *(volatile s16*)(pg + FG_MODE) != 3) waiting = now;
    else if (now - waiting > 8) pg[FG_BALL] = 35, waiting = now;
}

/* ---- the spell ---- */
static double until;                /* its end (0: none) */
static double cool;                 /* none before this */
static double file_at = -1, last_roll, wet;
#define LENGTH 120.0                /* seconds a spell lasts */
#define QUIET 720.0                 /* seconds of quiet after one */

static void finish(const char* why) {
    wwhd_log(why);
    until = 0;
    if (!always()) cool = now + QUIET;
    if (phantom != NONE && ps_search(phantom)) ps_delete(phantom);
    phantom = NONE;
}

/* ---- the step ---- */
static u8* last_link;
static double settle;
static f32 fog;
static int fogged;

WWHD_HOOK_RETURN(WWHD_ADDR_fpcEx_Handler_025DE788, phantom_stalker_step, (u32 queue)) {
    (void)queue;
    const double dt = wwhd_logic_dt();
    now += dt > 0 && dt < 0.2 ? dt : 1.0 / 30;
    u8* play = wwhd_play_get();
    if (!play) return;
    u8* link = (u8*)word(play, PLAY_LINK);
    const char* stage = (const char*)play + WWHD_PLAY_START_STAGE_NAME_OFFSET;
    const int title = !stage[0] || (stage[0] == 's' && stage[1] == 'e' && stage[2] == 'a' && stage[3] == '_' && stage[4] == 'T') ||
                      (stage[0] == 'N' && stage[1] == 'a' && stage[2] == 'm' && stage[3] == 'e' && !stage[4]) ||
                      (stage[0] == 'E' && stage[1] == 'N' && stage[2] == 'D' && stage[3] == 'u' && stage[4] == 'm' && stage[5] == 'i');
    if (!link || title) {
        file_at = -1;
        return;
    }
    if (!rng) {  /* seeded once, from the step count and where Link stands */
        const cXyz at = pos_of(link);
        rng = (u32)wwhd_logic_step() * 2654435761u ^ bits(at.x) ^ (bits(at.z) << 7);
        if (!rng) rng = 0x9E3779B9u;
        static const char* const kOften[] = {"Rare", "Now and then", "Often", "Always"};
        often = choice("how_often", kOften, 4, 1);
    }
    if (link != last_link) {  /* a file loaded, a stage entered: Phantom Ganon went with the old one */
        last_link = link;
        settle = now + 2;
        phantom = NONE, next = 0, until = 0;
    }
    if (file_at < 0) file_at = now, cool = cool > now + 240 ? cool : now + 240;  /* none in a file's first 4 minutes */
    /* nor while a stage settles or the next one is asked for, nor in a cutscene */
    const int settled = now >= settle && !play[WWHD_PLAY_ENABLE_NEXT_STAGE_OFFSET];
    const int cutscene = play[WWHD_PLAY_EVENT_RUNNING_OFFSET] != 0;

    const daPy_lk_c* player = (const daPy_lk_c*)link;
    const s32 proc = player->mCurProc;  /* riding the boat: SHIP_READY..SHIP_RESTART, not SHIP_GET_OFF */
    const int riding = proc >= 0x86 && proc <= 0x91 && proc != 0x90, swimming = (player->mModeFlg & 0x40000) != 0;
    if (riding || swimming) wet = now;
    const s32 room = sea_room(pos_of(link));
    const int at_sea = stage[0] == 's' && stage[1] == 'e' && stage[2] == 'a' && !stage[3];
    const int island = at_sea && room != ROOM_WINDFALL && room != ROOM_OUTSET && !riding;
    const int ashore = island && now - wet > 3;  /* (knocked into the water he is not off the island) */
    const f32 time = *(volatile f32*)(WWHD_GAME_DATA(WWHD_ADDR_save_info_pointer, u32) + WWHD_OFFSET_save_info + 0x24);
    const double hour = time / 15;  /* degrees, 15 an hour from midnight */
    const int night = hour >= 19 || hour < 5;

    if (until > 0) {
        if (!island) finish("phantom-stalker: ends, Link left the island");
        else if (now >= until) finish("phantom-stalker: ends");
        else if (phantom != NONE && now >= next && !ps_search(phantom)) finish("phantom-stalker: ends, Phantom Ganon is gone");
    }
    if (!always() && until == 0 && settled && !cutscene && now - last_roll >= 60) {  /* one roll a minute */
        last_roll = now;
        if (ashore && night && now >= cool && has_item(ITEM_SAIL) && has_item(ITEM_WIND_WAKER) && rnd01() < 0.03 * kScale[often]) {
            until = now + LENGTH;
            wwhd_log("phantom-stalker: begins");
        }
    }

    /* the fog closes in over 30 s and lifts after: the world fades from 100 to 2500 around the camera, blended with the
     * game's own fog by the ratio; once more at 0 to hand the game its fog back */
    const int on = always() ? island : until > 0;
    const f32 to = on ? 1.f : 0.f, ease = (f32)(dt > 0 && dt < 0.2 ? dt : 1.0 / 30) / 30;
    fog = to > fog ? (fog + ease < to ? fog + ease : to) : (fog - ease > to ? fog - ease : to);
    if (fog > 0 || fogged) {
        ps_fog(100, 2500, fog);
        fogged = fog > 0;
    }

    if (!on) {
        if (phantom != NONE && ps_search(phantom)) ps_delete(phantom);  /* (Always: Link left the island) */
        phantom = NONE;
        return;
    }
    if (!settled) return;
    load_bank(play);
    u8* pg = phantom != NONE ? ps_search(phantom) : 0;
    if (!pg && now >= next) spawn(link);
    else if (pg) keep(pg, link);
}
