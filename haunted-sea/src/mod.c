/* Haunted sea: now and then, at night far out on the Great Sea, the wind dies away and fog rolls in for 30 seconds,
 * then the sea turns on Link for two to four minutes: Gyorgs, Seahats, Kargarocs, cannon boats and Poes come for him,
 * and the Ghost Ship sails in the fog. Sailing away from the signs, or reaching an island, ends it.
 *
 * Runs once per logic step, after the actors' execute (a return hook on fpcEx_Handler), so the game's own rules
 * (health, damage, deletion) still apply. Monsters are the game's own, made with fopAcM_create in Link's room and
 * process layer, out of his view. Offsets are the HD game's (USA and EU). */
#include "wwhd_guest.h"
#include "wwhd/functions.h"
#include "wwhd/data.h"
#include "wwhd/actor.h"
#include "wwhd/link.h"
#include "wwhd/objects.h"
#include "wwhd/save.h"

/* ---- the game ---- */
WWHD_GAME_FUNC(0x025D5834, u32, hs_create, (u32 proc, u32 param, cXyz* pos, s32 room, csXyz* angle, cXyz* scale, s32 subtype, u32 create_func));
WWHD_GAME_FUNC(0x025D5A20, u32, hs_create_child, (u32 proc, u32 parent, u32 param, cXyz* pos, s32 room, csXyz* angle, cXyz* scale, s32 subtype, u32 create_func));
WWHD_GAME_FUNC(0x025DE50C, u8*, hs_search, (u32 id));          /* fpcEx_SearchByID: the process while it exists */
WWHD_GAME_FUNC(0x025D57E4, s32, hs_delete, (u32 id));          /* fopAcM_delete by id */
WWHD_GAME_FUNC(0x025DED64, u32, hs_current_layer, (void));      /* fpcLy_CurrentLayer */
WWHD_GAME_FUNC(0x025DEAB4, void, hs_set_layer, (u32 layer));     /* fpcLy_SetCurrentLayer */
WWHD_GAME_FUNC(0x0257E594, void, hs_windpower, (f32 power));     /* dKyw_custom_windpower: above 0 overrides the wind, 0 hands it back */
WWHD_GAME_FUNC(0x0255671C, void, hs_fog, (f32 start, f32 end, f32 ratio));  /* dKy_fog_startendz_set */
WWHD_GAME_FUNC(0x02560828, s32, hs_moon, (void));                /* dKy_moon_type_chk: tonight's moon phase */
WWHD_GAME_FUNC(0x02169B48, void, hs_gyorg_hunt, (u8* gyorg));    /* daGy_c::modeAttackPlayerInit */
WWHD_GAME_FUNC(0x02169B68, void, hs_gyorg_ram, (u8* gyorg));     /* daGy_c::modeAttackInit */
WWHD_GAME_ORIGINAL(0x02520C0C, u8, hs_check_item_original, (u32 item));

#define NONE 0xFFFFFFFFu
#define PI 3.14159265358979323846
#define ITEM_BOOMERANG 0x2D
#define ITEM_WIND_WAKER 0x22
#define ITEM_SAIL 0x78
/* dComIfG_play_c: Link and the King of Red Lions; the ship-ride status the Seahats read (bit 0x10000) */
#define PLAY_LINK 0x5B34
#define PLAY_SHIP_STATUS 0x5CD8
/* base_process_class: process id +4, process name +8 */
#define PROC_NAME 8

static u32 word(u8* p, u32 o) { return *(volatile u32*)(p + o); }
static void set_word(u8* p, u32 o, u32 v) { *(volatile u32*)(p + o) = v; }
static u16 proc_of(u8* a) { return *(volatile u16*)(a + PROC_NAME); }
static cXyz pos_of(u8* a) { return ((fopAc_ac_c*)a)->current.pos; }
static u32 bits(f32 f) { union { f32 f; u32 u; } v = {f}; return v.u; }
static int in_mem2(u32 a) { return a >= 0x10000000 && a < 0x50000000; }

/* ---- freestanding math and randomness (no libm in guest mods) ---- */
static double wrap(double x) {
    while (x > PI) x -= 2 * PI;
    while (x < -PI) x += 2 * PI;
    return x;
}
static double hs_sin(double x) {
    x = wrap(x);
    if (x > PI / 2) x = PI - x;
    if (x < -PI / 2) x = -PI - x;
    const double q = x * x;
    return x * (1 + q * (-1.0 / 6 + q * (1.0 / 120 + q * (-1.0 / 5040 + q * (1.0 / 362880)))));
}
static double hs_cos(double x) { return hs_sin(x + PI / 2); }
static double hs_sqrt(double x) {
    if (x <= 0) return 0;
    double r = x > 1 ? x / 2 : 1;
    for (int i = 0; i < 30; i++) r = (r + x / r) / 2;
    return r;
}
static double dist2d(cXyz a, cXyz b) { return hs_sqrt((double)(a.x - b.x) * (a.x - b.x) + (double)(a.z - b.z) * (a.z - b.z)); }
static double fmin_(double a, double b) { return a < b ? a : b; }
static double fmax_(double a, double b) { return a > b ? a : b; }
static double round_(double x) { return x < 0 ? -(double)(s32)(-x + 0.5) : (double)(s32)(x + 0.5); }  /* (|x| < 2^31 here) */

static u32 rng;
static u32 rnd(void) {
    rng ^= rng << 13, rng ^= rng >> 17, rng ^= rng << 5;
    return rng;
}
static double rnd01(void) { return (rnd() >> 8) / 16777216.0; }
static int chance(double p) { return rnd01() < p; }

/* ---- options ---- */
/* How often: the chance of a spell each minute out at sea at night (x0.4, x1, x3), or Always (on at sea day and night,
 * without signs). Monsters: seconds between spawns and how many are out at once. */
static int often = 1, level = 0;
static const double kScale[] = {0.4, 1, 3};
static const double kEvery[] = {25, 12, 5};
static const u32 kMax[] = {2, 4, 7};
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

/* ---- monsters ---- */
/* from the game's object name table: process name, subtype, parameter (0xFFFFFFFF: the defaults), angle.x (some read
 * a second parameter there) */
typedef struct { u16 proc; s8 subtype; u32 param; s16 angle_x; } Monster;
static const Monster kGyCtrl = {0xE5, -1, 0x7F00FF01, 0};  /* type 1, no Gyorgs of its own, waits on switch 0x7F (never set at sea) */
static const Monster kGyorg = {0xE4, -1, NONE, 0};
static const Monster kSeahat = {0xD1, -1, 0xFFFFFF01, 0};
static const Monster kKargaroc = {0xB5, -1, 0, 0};
static const Monster kPoe = {0xD4, -1, NONE, 0};
/* a cannon boat (daOship): no path, no Triforce / salvage / switch conditions; angle.x 0xFFFF the plain kind */
static const Monster kOship = {0xB3, -1, 0x00FFFF00, (s16)0xFFFF};
/* the Ghost Ship (daGhostship): no path; tonight's moon phase in the low byte, or the game hides it */
static const Monster kGhostShip = {0xB2, -1, 0x00FF0000, 0};
#define FLAG_PROC 0xAE  /* daMajuu_Flag, a cannon boat's flag */

#define MAX_IDS 16
typedef struct { u32 ids[MAX_IDS]; double born[MAX_IDS]; u32 count; double next; } Spawner;
static Spawner sea;                  /* the spell's monsters */
static Spawner gyorgs;               /* the Gyorgs' controller and Gyorgs: their Boomerang check passes */
static u32 gyctrl = NONE, ghost = NONE;
static double ghost_next;

static int listed(const u32* ids, u32 count, u32 id) {
    for (u32 i = 0; i < count; i++)
        if (ids[i] == id) return 1;
    return 0;
}
static double now;  /* game seconds */
static void add(Spawner* s, u32 id) {
    if (id != NONE && s->count < MAX_IDS) s->born[s->count] = now, s->ids[s->count++] = id;
}
static double ram_at[MAX_IDS];
/* the ones gone; a process asked for is found only once the game has made it, so the last 3 s are kept */
static void prune(Spawner* s, double* also) {
    u32 n = 0;
    for (u32 i = 0; i < s->count; i++)
        if (now - s->born[i] < 3 || hs_search(s->ids[i])) {
            s->ids[n] = s->ids[i], s->born[n] = s->born[i];
            if (also) also[n] = also[i];
            n++;
        }
    s->count = n;
}

/* made in Link's room and in his process layer (base_process_class +0x2C), as an actor's own spawns are: after the
 * executor the current layer is the root one, and an actor made there is drawn outside the play scene's buffers */
static cXyz spawn_pos;
static csXyz spawn_angle;
static u32 create_at(Monster m, u8* link, double x, double y, double z, s16 yaw, u32 parent) {
    spawn_pos.x = (f32)x, spawn_pos.y = (f32)y, spawn_pos.z = (f32)z;
    spawn_angle.x = m.angle_x, spawn_angle.y = yaw, spawn_angle.z = 0;
    const u32 layer = hs_current_layer();
    hs_set_layer(word(link, 0x2C));
    const s32 room = ((fopAc_ac_c*)link)->current.roomNo;
    const u32 id = parent != NONE ? hs_create_child(m.proc, parent, m.param, &spawn_pos, room, &spawn_angle, 0, m.subtype, 0)
                                  : hs_create(m.proc, m.param, &spawn_pos, room, &spawn_angle, 0, m.subtype, 0);
    hs_set_layer(layer);
    return id;
}
/* `dist` from Link, `dy` above the sea, facing him; `behind`: behind him (out of the camera's view while he sails),
 * else anywhere around him */
static u32 spawn(Spawner* s, Monster m, u8* link, double dist, int behind, double dy, u32 parent) {
    const double heading = ((fopAc_ac_c*)link)->shape_angle.y * PI / 32768;
    const double a = behind ? heading + PI + (rnd01() - 0.5) * 1.6 : rnd01() * 2 * PI;
    const cXyz at = pos_of(link);
    const u32 id = create_at(m, link, at.x + hs_sin(a) * dist, dy, at.z + hs_cos(a) * dist, (s16)(s32)(wrap(a + PI) * 32768 / PI), parent);
    if (s) add(s, id);
    return id;
}
/* one of ours deleted; a cannon boat's flag goes with it (it reads the boat's matrix every step, and left behind it
 * read freed memory and flew off to 1e36, halting the game) */
static void delete_actor(u32 id) {
    u8* a = hs_search(id);
    if (a && proc_of(a) == kOship.proc)
        for (u32 o = 0xD00; o < 0xF00; o += 4) {
            u8* f = hs_search(word(a, o));
            if (f && f != a && proc_of(f) == FLAG_PROC) {
                hs_delete(word(a, o));
                break;
            }
        }
    hs_delete(id);
}

/* ---- the spell ---- */
static double until;                /* the spell's end (0: none) */
static double cool;                 /* no spell before this */
static double out_since, file_at = -1, last_roll, wet;
static int warning;                 /* the signs are showing */
static double warn_end, warn_far, warn_x, warn_z;
#define WARNING 30.0                /* seconds of signs */
#define QUIET 720.0                 /* seconds of quiet after a spell */

static void finish(u8* link, const char* why) {
    wwhd_log(why);
    until = 0;
    if (!always()) cool = now + QUIET;
    const cXyz at = pos_of(link);
    for (u32 i = 0; i < sea.count; i++) {  /* those out in the fog (cannon boats) go */
        u8* a = hs_search(sea.ids[i]);
        if (a && dist2d(pos_of(a), at) > 2000) delete_actor(sea.ids[i]);
    }
    if (ghost != NONE && hs_search(ghost)) hs_delete(ghost);
}
/* the signs' place: 7000 ahead of Link, or as far as he sails in 25 s (the King of Red Lions' speedF +0x370 a step) */
static void warn(u8* play, u8* link) {
    u8* ship = (u8*)word(play, PLAY_LINK + 8);
    double speed = 0;
    if (ship) {
        const f32 s = *(volatile f32*)(ship + 0x370);
        speed = (s < 0 ? -s : s) * 30;
    }
    const double heading = ((fopAc_ac_c*)link)->shape_angle.y * PI / 32768;
    warning = 1, warn_end = now + WARNING, warn_far = fmax_(7000, speed * (WARNING - 5));
    const cXyz at = pos_of(link);
    warn_x = at.x + hs_sin(heading) * warn_far, warn_z = at.z + hs_cos(heading) * warn_far;
    wwhd_log("haunted-sea: the signs show");
}

static int has_item(u8 item) {
    const u8* inventory = (const u8*)(WWHD_GAME_DATA(WWHD_ADDR_save_info_pointer, u32) + WWHD_OFFSET_save_inventory);
    for (int i = 0; i < 21; i++)
        if (inventory[i] == item) return 1;
    return 0;
}

/* once a step: signs, start and end */
static void direct(u8* play, u8* link, int night, int water, int at_sea, int settled) {
    const cXyz at = pos_of(link);
    /* the sea is a 7x7 grid of 100000-unit squares, each island near its square's centre */
    const double isle = hs_sqrt((at.x - round_(at.x / 1e5) * 1e5) * (at.x - round_(at.x / 1e5) * 1e5) +
                                (at.z - round_(at.z / 1e5) * 1e5) * (at.z - round_(at.z / 1e5) * 1e5));
    if (water) wet = now;  /* knocked off the boat he is neither riding nor swimming for a moment */
    const int ashore = !at_sea || now - wet > 3, cutscene = play[WWHD_PLAY_EVENT_RUNNING_OFFSET] != 0;
    if (ashore) out_since = now;
    if (file_at < 0) file_at = now, cool = fmax_(cool, now + 240);  /* a file loaded: no spell in its first 4 minutes */

    if (until > 0) {
        if (ashore || isle < 3500) finish(link, ashore ? "haunted-sea: ends, Link is off the water" : "haunted-sea: ends near an island");
        else if (now >= until) finish(link, "haunted-sea: ends");
    }
    if (warning) {
        const double d = hs_sqrt((at.x - warn_x) * (at.x - warn_x) + (at.z - warn_z) * (at.z - warn_z));
        const int fled = d > warn_far + 2500, landed = ashore || isle < 3500;
        if (fled || landed) {
            wwhd_log(landed ? "haunted-sea: the signs fade, Link made land" : "haunted-sea: the signs fade, Link sailed away");
            if (fled) cool = fmax_(cool, now + QUIET / 3);
            warning = 0, hs_windpower(0);
        } else if (now >= warn_end || d < 2000) {  /* (or he sailed right up to it) */
            warning = 0, hs_windpower(0);
            until = now + 120 + rnd() % 120;
            wwhd_log("haunted-sea: begins");
        } else {  /* the wind dies away */
            static double next;
            if (now >= next) {
                next = now + 2;
                hs_windpower((f32)fmax_(0.02, 0.3 * (1 - (now - (warn_end - WARNING)) / WARNING)));
            }
        }
    }
    if (always() || now - last_roll < 60 || !settled || cutscene) return;
    last_roll = now;
    if (!has_item(ITEM_SAIL) || !has_item(ITEM_WIND_WAKER)) return;
    if (now < cool || warning || until > now || ashore || isle < 6000 || !night) return;
    const double more = fmin_(3, 1 + (now - out_since) / 600);  /* the longer he stays out, up to x3 */
    if (chance(0.12 * more * kScale[often])) warn(play, link);
}

/* ---- the step ---- */
static u8* last_link;
static double settle;
static f32 fog;
static int fogged;

static void reset(void) {
    sea.count = 0, sea.next = 0;
    gyorgs.count = 0, gyctrl = NONE, ghost = NONE, ghost_next = 0;
    for (u32 i = 0; i < MAX_IDS; i++) ram_at[i] = 0;
}

WWHD_HOOK_RETURN(WWHD_ADDR_fpcEx_Handler_025DE788, haunted_sea_step, (u32 queue)) {
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
        static const char* const kLevel[] = {"Sometimes", "Often", "Relentless"};
        often = choice("how_often", kOften, 4, 1);
        level = choice("monsters", kLevel, 3, 0);
    }
    if (link != last_link) {  /* a file loaded, a stage entered: start over, and no spawns until it has settled */
        last_link = link;
        reset();
        settle = now + 2;
        if (warning) hs_windpower(0), warning = 0;
        until = 0;
    }
    /* nor while the next stage is asked for */
    const int settled = now >= settle && !play[WWHD_PLAY_ENABLE_NEXT_STAGE_OFFSET];

    const daPy_lk_c* player = (const daPy_lk_c*)link;
    const s32 proc = player->mCurProc;  /* riding the boat: SHIP_READY..SHIP_RESTART, not SHIP_GET_OFF */
    const int riding = proc >= 0x86 && proc <= 0x91 && proc != 0x90, swimming = (player->mModeFlg & 0x40000) != 0;
    const int at_sea = stage[0] == 's' && stage[1] == 'e' && stage[2] == 'a' && !stage[3];
    const int water = riding || swimming, on_sea = at_sea && water;
    const f32 time = *(volatile f32*)(WWHD_GAME_DATA(WWHD_ADDR_save_info_pointer, u32) + WWHD_OFFSET_save_info + 0x24);
    const double hour = time / 15;  /* degrees, 15 an hour from midnight */
    const int night = hour >= 19 || hour < 5;
    direct(play, link, night, water, at_sea, settled);

    /* the fog rolls in with the signs and out after the spell, over 30 s: the world fades from 100 to 2500 around the
     * camera, blended with the game's own fog by the ratio; once more at 0 to hand the game its fog back */
    const int spell = warning || (until > now && !always());
    const f32 to = spell ? 1.f : 0.f, ease = (f32)(dt > 0 && dt < 0.2 ? dt : 1.0 / 30) / 30;
    fog = to > fog ? (fog + ease < to ? fog + ease : to) : (fog - ease > to ? fog - ease : to);
    if (fog > 0 || fogged) {
        hs_fog(100, 2500, fog);
        fogged = fog > 0;
    }

    if (!(always() || until > now) || !on_sea || !settled) return;
    const cXyz at = pos_of(link);
    prune(&sea, 0);
    prune(&gyorgs, ram_at);  /* (their ram timers with them) */
    if (now >= sea.next && sea.count < kMax[level] + (night ? 2 : 0)) {
        if (gyctrl == NONE || !hs_search(gyctrl)) {  /* the Gyorgs' controller first, Gyorgs once it exists */
            gyctrl = spawn(0, kGyCtrl, link, 0, 0, 0, NONE);
            add(&gyorgs, gyctrl);
            sea.next = now + 1;  /* (it is no monster: the first one comes right after) */
            return;
        } else if (riding && rnd() % 4 == 0) {  /* a cannon boat, bombarding from its range */
            spawn(&sea, kOship, link, 3000 + rnd() % 2500, 0, 0, NONE);
        } else if (rnd() % 2 == 0) {  /* from under the water, where they are not seen coming */
            if (gyorgs.count < MAX_IDS) ram_at[gyorgs.count] = 0;
            add(&gyorgs, spawn(&sea, kGyorg, link, 900 + rnd() % 600, 0, -40, gyctrl));
        } else {  /* Seahats, Kargarocs and at night Poes over the water, behind him */
            static const Monster* const kDay[] = {&kSeahat, &kSeahat, &kKargaroc};
            static const Monster* const kNight[] = {&kSeahat, &kKargaroc, &kPoe, &kPoe};
            const Monster m = night ? *kNight[rnd() % 4] : *kDay[rnd() % 3];
            const int poe = m.proc == kPoe.proc;
            spawn(&sea, m, link, poe ? 500 + rnd() % 400 : 900 + rnd() % 600, 1, poe ? 120 : 300, NONE);
        }
        sea.next = now + kEvery[level] * (night ? 0.6 : 1) * (0.6 + rnd01() * 0.8);
    }
    /* the Ghost Ship: one, at night, far off ahead of him; it fades as he comes close */
    if (night && riding && (ghost == NONE || !hs_search(ghost)) && now >= ghost_next) {
        Monster m = kGhostShip;
        m.param |= (u32)hs_moon() & 0xFF;
        ghost = spawn(0, m, link, 4500 + rnd() % 2000, 0, 0, NONE);
        ghost_next = now + 5;
    }
    /* Seahats keep to the anchor they were made at (+0x448/+0x450): theirs is wherever Link is now */
    for (u32 i = 0; i < sea.count; i++) {
        u8* a = hs_search(sea.ids[i]);
        if (a && proc_of(a) == kSeahat.proc) set_word(a, 0x448, bits(at.x)), set_word(a, 0x450, bits(at.z));
    }
    /* the Gyorgs hunt Link: without their controller's path they would only dive and circle (daGy_c mode +0x3CC: 0 dive,
     * 1 circle, 6 circle with others), so they attack him (mode 3), which gives up while he rides the boat: then they
     * circle it (their controller's clear-line flag, +0x42E + the child index +0x3C8) and each breaches to ram it
     * every 5-9 s (mode 2), kept within 2000 of him so a sailing boat does not leave them behind */
    for (u32 i = 0; i < gyorgs.count; i++) {
        if (gyorgs.ids[i] == gyctrl) continue;
        u8* gy = hs_search(gyorgs.ids[i]);
        if (!gy) continue;
        const u32 mode = word(gy, 0x3CC);
        const int idle = mode == 0 || mode == 1 || mode == 6;
        if (!riding) {
            if (idle) hs_gyorg_hunt(gy);
            continue;
        }
        u8* ctrl = (u8*)word(gy, 0x3D4);
        const u32 child = word(gy, 0x3C8);
        if (in_mem2((u32)ctrl) && child < 8) ctrl[0x42E + child] = 1;
        if (idle && now >= ram_at[i]) {
            if (ram_at[i] > 0) hs_gyorg_ram(gy);
            ram_at[i] = now + 5 + rnd() % 5;
        }
        const cXyz g = pos_of(gy);
        const double far = dist2d(g, at);
        if (far > 2000) {
            fopAc_ac_c* actor = (fopAc_ac_c*)gy;
            actor->current.pos.x = (f32)(at.x + (g.x - at.x) * 2000 / far);
            actor->current.pos.z = (f32)(at.z + (g.z - at.z) * 2000 / far);
        }
    }
}

/* ---- the game's checks, for our monsters only ---- */
/* The game makes Gyorgs (daGy_c::_create) and their controller (daGyCtrl_Create) only once Link has the Boomerang;
 * ours come before that too: while one of ours is created, the Boomerang check passes. */
static int creating_ours;
static int ours(u8* process) { return process && listed(gyorgs.ids, gyorgs.count, word(process, WWHD_OFFSET_actor_process_id)); }
WWHD_HOOK(WWHD_ADDR_daGy_c___create_02167A00, gyorg_create, (void* self)) { creating_ours = ours((u8*)self); }
WWHD_HOOK_RETURN(WWHD_ADDR_daGy_c___create_02167A00, gyorg_created, (void* self)) { (void)self, creating_ours = 0; }
WWHD_HOOK(WWHD_ADDR_daGyCtrl_Create_0216BA8C, gyctrl_create, (void* self)) { creating_ours = ours((u8*)self); }
WWHD_HOOK_RETURN(WWHD_ADDR_daGyCtrl_Create_0216BA8C, gyctrl_created, (void* self)) { (void)self, creating_ours = 0; }
WWHD_REPLACE(WWHD_ADDR_dComIfGs_checkGetItem_02520C0C, u8, check_item, (u32 item)) {
    if (creating_ours && item == ITEM_BOOMERANG) return 1;
    return hs_check_item_original(item);
}

/* Seahats (ph_class Execute) swoop only while Link has the ship-ride status: ours come for him swimming too. */
static u32 ship_status, ship_status_set;
WWHD_HOOK(WWHD_ADDR_daPH_Execute_023CC070, seahat_execute, (void* self)) {
    u8* play = wwhd_play_get();
    ship_status_set = 0;
    if (!play || !listed(sea.ids, sea.count, word((u8*)self, WWHD_OFFSET_actor_process_id))) return;
    ship_status = word(play, PLAY_SHIP_STATUS);
    set_word(play, PLAY_SHIP_STATUS, ship_status | 0x10000);
    ship_status_set = 1;
}
WWHD_HOOK_RETURN(WWHD_ADDR_daPH_Execute_023CC070, seahat_executed, (void* self)) {
    (void)self;
    u8* play = wwhd_play_get();
    if (play && ship_status_set) set_word(play, PLAY_SHIP_STATUS, ship_status);
    ship_status_set = 0;
}
