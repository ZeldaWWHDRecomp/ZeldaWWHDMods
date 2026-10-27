/* HD minimap: a corner map on the TV drawn with the HD game's own GamePad map art and rules.
 * The map art (island maps, dungeon floor plans, treasure charts, markers) is read by the port from the
 * player's installed game at run time (WWHD_HUD_GAME); this mod only names layouts, textures and panes.
 * The frame and the wind arrow are Minimap HD artwork by Chironeyes (MIT, see NOTICE.md).
 * Read-only: no writes to game memory. */
#include "wwhd_guest.h"
#include "wwhd/actor.h"
#include "wwhd/functions.h"
#include "wwhd/data.h"
#include "hd_minimap.h"

/* game functions (USA addresses; the port translates them for EU) */
WWHD_GAME_FUNC(WWHD_ADDR_dScnPly_isPause, s32, game_paused, (void));
WWHD_GAME_FUNC(0x025B8568, s32, sector_discovered, (u32 player_map, s32 cell));        /* dSv_player_map_c::isSaveArriveGrid */
WWHD_GAME_FUNC(WWHD_ADDR_dSv_player_map_c_isGetMap, s32, chart_owned, (u32 player_map, s32 index));
WWHD_GAME_FUNC(WWHD_ADDR_dSv_player_map_c_isOpenMap, s32, chart_open, (u32 player_map, s32 index));
WWHD_GAME_FUNC(WWHD_ADDR_dSv_memBit_c_isDungeonItem, s32, dungeon_item, (u32 membit, s32 item));
WWHD_GAME_FUNC(WWHD_ADDR_dSv_memBit_c_isTbox, s32, chest_open, (u32 membit, s32 chest));
WWHD_GAME_FUNC(WWHD_ADDR_dSv_memBit_c_isVisitedRoom, s32, room_visited, (u32 membit, s32 room));
WWHD_GAME_FUNC(WWHD_ADDR_dComIfGs_isStageTbox, s32, stage_chest_open, (s32 stage, s32 chest));
WWHD_GAME_FUNC(WWHD_ADDR_dComIfGs_isStageMap, s32, stage_map, (s32 stage));
WWHD_GAME_FUNC(WWHD_ADDR_dComIfGs_isStageCompass, s32, stage_compass, (s32 stage));
WWHD_GAME_FUNC(0x0258DC48, u8, stage_floor, (void* stage, f32 y));                     /* dMap_GetFloorNo */
WWHD_GAME_FUNC(0x0258DBF4, u8, room_floor, (s32 room, f32 y));                         /* dMap_GetFloorNo_WithRoom */
WWHD_GAME_FUNC(0x0257DACC, u32, wind_angle, (void));                                   /* dKyw_get_wind_angle */
WWHD_GAME_FUNC(0x025C11DC, u8*, room_data, (u32 control, s32 room));
WWHD_GAME_FUNC(0x025C4734, u8*, stage_treasures, (void));
WWHD_GAME_FUNC(0x025C4740, u8*, stage_doors, (void));

/* play (dComIfGp) and save offsets used here */
#define PLAY_STAGE 0x5150           /* dStage_stageDt_c: +0x48 stage info, +0x74 DMAP */
#define PLAY_PARTNER 0x5B38         /* the second player slot: Medli or Makar in their temples */
#define PLAY_STATUS0 0x5CD8
#define PLAY_STATUS1 0x5CDC
#define SAVE_PLAYER_MAP 0xE4
#define SAVE_MEMBIT 0x798
#define SAVE_CHART_SET 0x1C1
#define CHART_TABLE 0x1047A878      /* the loaded chart table (cmapdat): count, then 0x26-byte entries */
#define LOCATION_TABLE 0x1047A874   /* the loaded location table (FmapDat): count, then 8-byte entries */
#define STTS0_HIDE (0x1000u | 0x2000u | 0x4000u | 0x20000u | 0x80000u | 0x200000u) /* aiming, first person, telescope */
#define STTS0_SHIP 0x10000u
#define STTS1_CONDUCT 0x1u

enum { MODE_NONE, MODE_SEA, MODE_DUNGEON };
#define MAX_ROOMS 40
#define MAX_POINTS 48
#define MAX_CHARTS 64

static struct {
    float x, side_compact, side_expanded, bottom, opacity, faded, bg_opacity;
    int fade, background, wind, dungeons, debug, zoom_turns, parchment, smooth;
    u32 key_toggle, key_size, key_view, key_chart;
} opt;

static struct {
    int mode, visible, sector, discovered, covered, on_ship, dungeon, floor, room, has_map, has_compass, companion;
    float x, y, z, cx, cz, island_dx, island_dz;
    s16 yaw, cyaw;
    u32 wind;
    int chart_set, chart_count;
    struct { s8 sea, save, number; s16 rect[4]; } charts[MAX_CHARTS];
    float origin_x, origin_z, size, map_width;
    unsigned long long visited;
    int door_count, boss_visible;
    struct { float x, z; s16 heading; } doors[MAX_POINTS];
    float boss_x, boss_z, boss_dx, boss_dy;
    int dmap, turned;  /* turned: Forsaken Fortress after the first visit */
    int point_count;
    struct { float x, z; } points[MAX_POINTS];
    char stage[9];
} snap;

/* player choices */
static struct { int hidden, expanded, zoom, sea_view, chart, feedback; float feedback_time; } ui = {0, 0, 0, 1, 0, 0, 0};
enum { FEEDBACK_NONE, FEEDBACK_HIDDEN, FEEDBACK_EXPANDED, FEEDBACK_COMPACT };
static u32 img_feedback[4];
static const char* const feedback_art[4] = {0, "assets/hide.png", "assets/zoom.png", "assets/reduce.png"};
static u32 previous_buttons;
static hdm_fade fade = {1, 1};
static int initialized;

/* images */
static unsigned long long image_epoch;
static u32 img_frame, img_wind, img_unknown, img_face, img_arrow, img_chest, img_medli, img_makar, img_ship, img_base, img_door, img_boss;
static u32 img_island, img_chart, img_ribbon, img_chart_base, img_mark, img_mark_fill;
static int island_room = -1, island_full = -1, chart_key = -1;
static struct { int pane, room; float x, y, w, h; u32 image; } rooms[MAX_ROOMS];
/* the floor screens' room tables (DungeonMapFloor_00..05): (plan room, floor, room) per plan room; Forsaken
 * Fortress's corridors are in none */
static const unsigned int* plan_table(int dungeon, int* count) {
    static const int counts[6] = {10, 21, 27, 22, 22, 18};
    if (dungeon < 0 || dungeon > 5) return 0;
    *count = counts[dungeon];
    switch (dungeon) {
        case 0: return (const unsigned int*)&WWHD_GAME_DATA(0x100F10B0, u8);
        case 1: return (const unsigned int*)&WWHD_GAME_DATA(0x100F1594, u8);
        case 2: return (const unsigned int*)&WWHD_GAME_DATA(0x100F1AB4, u8);
        case 3: return (const unsigned int*)&WWHD_GAME_DATA(0x100F2064, u8);
        case 4: return (const unsigned int*)&WWHD_GAME_DATA(0x100F2594, u8);
        default: return (const unsigned int*)&WWHD_GAME_DATA(0x100F2ABC, u8);
    }
}
static int room_count, rooms_dungeon = -1, rooms_floor = -1;
static float floor_min_x, floor_min_y, floor_max_x, floor_max_y;
/* the point Forsaken Fortress's later markers turn about (read with its plan) */
static float forsaken_cx = HDM_FORSAKEN_CENTRE_X, forsaken_cy = HDM_FORSAKEN_CENTRE_Y;

static wwhd_hud_element element;
static char name[96];
static const float pi = 3.14159265358979323846f;

static float bounded(double v, float lo, float hi, float fallback) { return v >= lo && v <= hi ? (float)v : fallback; }
static u32 key_option(const char* id, const char* fallback) {
    static char value[16];
    value[0] = 0;
    if (!wwhd_config_string(id, value, sizeof value)) { int i = 0; for (; fallback[i] && i < 15; ++i) value[i] = fallback[i]; value[i] = 0; }
    return hdm_button_bit(value);
}
static void configure(void) {
    opt.x = bounded(wwhd_config_float("x", 12), 0, 1200, 12);
    opt.bottom = bounded(wwhd_config_float("y", 20), 0, 650, 20);
    opt.side_compact = bounded(wwhd_config_float("size", 190), 96, 400, 190);
    opt.side_expanded = bounded(wwhd_config_float("size_expanded", 300), 96, 600, 300);
    opt.opacity = bounded(wwhd_config_float("opacity", 1), 0, 1, 1);
    opt.faded = bounded(wwhd_config_float("faded_opacity", .7), 0, 1, .7f);
    opt.bg_opacity = bounded(wwhd_config_float("background_opacity", .4), 0, 1, .4f);
    opt.fade = wwhd_config_bool("fade", 1);
    opt.background = wwhd_config_bool("background", 1);
    opt.wind = wwhd_config_bool("wind", 1);
    opt.dungeons = wwhd_config_bool("dungeons", 1);
    opt.debug = wwhd_config_bool("debug", 0);
    opt.zoom_turns = wwhd_config_bool("zoom_turns", 1);
    opt.parchment = wwhd_config_bool("parchment", 0);
    opt.smooth = wwhd_config_bool("smooth", 1);
    opt.key_toggle = key_option("key_toggle", "left");
    opt.key_size = key_option("key_size", "right");
    opt.key_view = key_option("key_view", "down");
    opt.key_chart = key_option("key_chart", "l3");
    ui.expanded = wwhd_config_bool("start_expanded", 0);
    initialized = 1;
}

/* ---- images ---- */
static u32 game_image(const char* spec) { return wwhd_hud_texture(WWHD_HUD_GAME, spec); }
static void drop(u32* h) { if (*h) wwhd_hud_release(*h); *h = 0; }
static void forget_rooms(void) {
    for (int i = 0; i < room_count; ++i) drop(&rooms[i].image);
    room_count = 0; rooms_dungeon = rooms_floor = -1;
}
static void forget_images(void) {
    /* after a state load the handles are gone: just forget them */
    img_frame = img_wind = img_unknown = img_face = img_arrow = img_chest = img_medli = img_makar = img_ship = img_base = img_door = img_boss = 0;
    img_island = img_chart = img_ribbon = img_chart_base = img_mark = img_mark_fill = 0; island_room = island_full = chart_key = -1;
    for (int i = 0; i < 4; ++i) img_feedback[i] = 0;
    for (int i = 0; i < room_count; ++i) rooms[i].image = 0;
    room_count = 0; rooms_dungeon = rooms_floor = -1;
}
static void static_images(void) {
    if (!img_frame) img_frame = wwhd_hud_texture(WWHD_HUD_PACKAGE, "assets/frame.png");
    if (!img_face) img_face = game_image("DungeonMapLink_00/MapLinkS_00^l");
    if (!img_arrow) img_arrow = game_image("DungeonMapLink_00/MapDirectionArrow_00^t#P_Direction_00");
}
static int texture_exists(const char* spec) {
    static char key[80];
    static wwhd_game_pane info;
    int i = hdm_put(key, 0, "layout-texture:");
    for (int k = 0; spec[k] && spec[k] != '#' && i < 78; ++k) key[i++] = spec[k];
    key[i] = 0;
    return wwhd_game_pane_get(key, &info);
}
static void island_image(int room) {
    if (island_room == room) return;
    drop(&img_island);
    island_room = room;
    hdm_sector_texture(name, room, 1);
    island_full = texture_exists(name);
    if (!island_full) hdm_sector_texture(name, room, 0);
    if (island_full || texture_exists(name)) {
        /* "Smooth island maps": the port enlarges the 100 x 100 art 3 times (Lanczos-3) on the player's
         * machine; the original art when the port can't */
        if (opt.smooth) {
            int i = 0;
            while (name[i]) ++i;
            name[i] = '@'; name[i + 1] = '3'; name[i + 2] = 0;
            img_island = game_image(name);
            name[i] = 0;
        }
        if (!img_island) img_island = game_image(name);
    }
}
static void chart_image(int sea) {
    int key = sea * 4 + snap.chart_set;
    if (chart_key == key) return;
    drop(&img_chart);
    chart_key = key;
    hdm_chart_texture(name, sea, snap.chart_set);
    img_chart = game_image(name);
}
/* the floor plan: the room panes of the dungeon's layout whose art is on the floor */
static void floor_rooms(int dungeon, int floor) {
    static wwhd_game_pane pane;
    static char key[64];
    char tag[3];
    if (rooms_dungeon == dungeon && rooms_floor == floor) return;
    forget_rooms();
    rooms_dungeon = dungeon; rooms_floor = floor;
    if (!hdm_floor_tag(floor, tag)) return;
    if (dungeon == 0 && wwhd_game_pane_get("layout-pane:DungeonMapFloor_00/N_FloorCenter_00", &pane)) {
        forsaken_cx = HDM_FORSAKEN_CENTRE_X + pane.x;
        forsaken_cy = HDM_FORSAKEN_CENTRE_Y + pane.y;
    }
    floor_min_x = floor_min_y = 1e9f; floor_max_x = floor_max_y = -1e9f;
    for (int p = 0; p < MAX_ROOMS; ++p) {
        hdm_room_pane_key(key, dungeon, p);
        if (!wwhd_game_pane_get(key, &pane) || !(pane.flags & 2)) continue;
        pane.texture[31] = 0;
        int table_floor = 0, table_room = -1, count = 0;
        const unsigned int* table = plan_table(dungeon, &count);
        if (hdm_plan_room(table, count, p, &table_floor, &table_room)) {
            if (table_floor != floor) continue;
        } else if (!hdm_texture_on_floor(pane.texture, tag)) {
            continue;
        }
        rooms[room_count].pane = p;
        rooms[room_count].room = table_room;
        rooms[room_count].x = pane.x; rooms[room_count].y = pane.y;
        rooms[room_count].w = pane.w; rooms[room_count].h = pane.h;
        /* every room with the plan's room colours (room 1's material: the entrance pane has a dark one) */
        hdm_room_texture(name, dungeon, pane.texture, 1);
        rooms[room_count].image = game_image(name);
        if (pane.x - pane.w / 2 < floor_min_x) floor_min_x = pane.x - pane.w / 2;
        if (pane.x + pane.w / 2 > floor_max_x) floor_max_x = pane.x + pane.w / 2;
        if (pane.y - pane.h / 2 < floor_min_y) floor_min_y = pane.y - pane.h / 2;
        if (pane.y + pane.h / 2 > floor_max_y) floor_max_y = pane.y + pane.h / 2;
        if (++room_count == MAX_ROOMS) break;
    }
}
/* world X/Z -> the floor layout's root coordinates, as the game places its map markers: relative to the
 * stage's DMAP origin, scaled by the game's map width ratio over the DMAP scale entry (hdDungeonMap_c
 * updateMapBounds); +y up. Forsaken Fortress after the first visit: the first visit's scale, turned with its
 * plan (hd_minimap.h) */
static void world_to_layout(float x, float z, float* lx, float* ly) {
    if (snap.turned) { hdm_forsaken_place(x - snap.origin_x, z - snap.origin_z, forsaken_cx, forsaken_cy, lx, ly); return; }
    float k = snap.size > 0 ? snap.map_width / snap.size : 0;
    *lx = (x - snap.origin_x) * k;
    *ly = -(z - snap.origin_z) * k;
}
/* the floor's room pane under a layout point (the current room on the plan), or -1 */
static int room_at(float lx, float ly) {
    int best = -1;
    float best_area = 1e30f;
    for (int i = 0; i < room_count; ++i) {
        float dx = lx - rooms[i].x, dy = ly - rooms[i].y;
        if (dx < -rooms[i].w / 2 || dx > rooms[i].w / 2 || dy < -rooms[i].h / 2 || dy > rooms[i].h / 2) continue;
        if (rooms[i].w * rooms[i].h < best_area) { best_area = rooms[i].w * rooms[i].h; best = rooms[i].pane; }
    }
    return best;
}

/* ---- drawing ---- */
static void image(u32 list, u32 handle, float x, float y, float w, float h, float rotation, u32 rgb, float alpha) {
    if (!handle || alpha <= 0) return;
    element = (wwhd_hud_element){0};
    element.kind = WWHD_HUD_IMAGE; element.anchor = WWHD_HUD_TOP_LEFT;
    element.x = x; element.y = y; element.w = w; element.h = h; element.rotation = rotation;
    element.thickness = 1; element.u1 = element.v1 = 1; element.image = handle;
    element.rgba = (rgb << 8) | (u32)(alpha * 255.0f + .5f);
    wwhd_hud_emit(list, &element);
}
static void rect(u32 list, float x, float y, float w, float h, u32 rgb, float alpha) {
    if (alpha <= 0) return;
    element = (wwhd_hud_element){0};
    element.kind = WWHD_HUD_RECT; element.anchor = WWHD_HUD_TOP_LEFT;
    element.x = x; element.y = y; element.w = w; element.h = h; element.thickness = 1; element.u1 = element.v1 = 1;
    element.rgba = (rgb << 8) | (u32)(alpha * 255.0f + .5f);
    wwhd_hud_emit(list, &element);
}
static void clip(u32 list, int push, float x, float y, float w, float h) {
    element = (wwhd_hud_element){0};
    element.kind = push ? WWHD_HUD_CLIP_PUSH : WWHD_HUD_CLIP_POP;
    element.anchor = WWHD_HUD_TOP_LEFT;
    element.x = x; element.y = y; element.w = w; element.h = h; element.thickness = 1;
    wwhd_hud_clip(list, &element);
}
static float yaw_rotation(s16 yaw) { return pi - (float)yaw * (2 * pi / 65536.0f); } /* art points up, yaw 0 faces +Z (down) */
static void marker(u32 list, u32 handle, float px, float py, float size, float rotation, float alpha) {
    image(list, handle, px - size / 2, py - size / 2, size, size, rotation, 0xffffff, alpha);
}
/* the game's marker (DungeonMapLink_00): the face (64 x 64 at 0.7) and a 34 x 22 heading arrow 29 units
 * from its centre, turned with Link; sizes in units of the face's 44.8 */
static float sin_approx(float x) {
    while (x > pi) x -= 2 * pi;
    while (x < -pi) x += 2 * pi;
    float y = x * (1.27323954f - 0.405284735f * (x < 0 ? -x : x));
    return y * (0.775f + 0.225f * (y < 0 ? -y : y));
}
static void heading_marker(u32 list, u32 icon, float icon_size, float px, float py, float side, s16 yaw, float alpha) {
    float unit = side * .12f / 44.8f, r = yaw_rotation(yaw), d = 29 * unit;
    float ax = px + d * sin_approx(r), ay = py - d * sin_approx(r + pi / 2);
    image(list, img_arrow, ax - 17 * unit, ay - 11 * unit, 34 * unit, 22 * unit, r, 0xffffff, alpha);
    marker(list, icon, px, py, icon_size > 0 ? icon_size : 44.8f * unit, 0, alpha);
}
static void link_marker(u32 list, float px, float py, float side, float alpha) {
    heading_marker(list, img_face, 0, px, py, side, snap.yaw, alpha);
}
static void frame(u32 list, float x, float y, float side, float a) {
    image(list, img_frame, x, y, side, side, 0, 0xffffff, a);
}

static void draw_sea(u32 list, float x, float y, float side, float a) {
    float inset = side * 6.0f / 256.0f, inner = side - 2 * inset;
    if (!snap.discovered || !snap.covered) {
        if (snap.discovered) return;  /* outside the island art: the game's compasses only */
        /* undiscovered sector: the hint only, like the game's "?" */
        if (!img_unknown) img_unknown = wwhd_hud_texture(WWHD_HUD_PACKAGE, "assets/unknown.png");
        float s = side * .32f;
        image(list, img_unknown, x + inset, y + side - s - inset, s, s, 0, 0xffffff, a * .6f);
        return;
    }
    island_image(snap.sector);
    if (!img_island) return;
    if (opt.parchment && !img_base) img_base = game_image("BtnMapBalloon_00/MapArea_00^t#P_MapBase_00");
    if (opt.background) rect(list, x + inset, y + inset, inner, inner, 0x000000, opt.bg_opacity * a / (opt.opacity > 0 ? opt.opacity : 1));
    clip(list, 1, x + inset, y + inset, inner, inner);
    /* see-through by default: the island art sits on the lightly darkened game picture; the game's
     * parchment card behind it is an option */
    if (opt.parchment)
        image(list, img_base, x + inset - inner * .1f, y + inset - inner * .1f, inner * 1.2f, inner * 1.2f, 0, 0xffffff, a);
    image(list, img_island, x + inset, y + inset, inner, inner, 0, 0xffffff, a);
    float u, v;
    hdm_island_uv(snap.sector, snap.island_dx, snap.island_dz, snap.x, snap.z, &u, &v);
    if (snap.on_ship) {
        if (!img_ship) img_ship = game_image("Map_00/MapShip_00^t");
        heading_marker(list, img_ship, side * .13f, x + inset + u * inner, y + inset + v * inner, side, snap.yaw, a);
    } else {
        link_marker(list, x + inset + u * inner, y + inset + v * inner, side, a);
    }
    clip(list, 0, 0, 0, 0, 0);
    frame(list, x, y, side, a);
    if (opt.wind) {
        if (!img_wind) img_wind = wwhd_hud_texture(WWHD_HUD_PACKAGE, "assets/wind.png");
        /* the game's angle points where the wind comes from; the arrow shows where it blows */
        s16 toward = (s16)(u16)(snap.wind + 0x8000u);
        float s = side * .2f;
        image(list, img_wind, x + side - s - inset * .5f, y + inset * .5f, s, s, yaw_rotation(toward), 0xffffff, a);
    }
}
/* a piece of the chart paper exactly where the paper under it is drawn (same texels, so an overlay of
 * rising opacity blends the art into the paper without seams) */
static float paper_x, paper_y, paper_size;
static void rect_image(u32 list, u32 handle, float x, float y, float w, float h, float alpha) {
    if (!handle || alpha <= 0 || paper_size <= 0) return;
    element = (wwhd_hud_element){0};
    element.kind = WWHD_HUD_IMAGE; element.anchor = WWHD_HUD_TOP_LEFT;
    element.x = x; element.y = y; element.w = w; element.h = h; element.thickness = 1;
    element.u0 = (x - paper_x) / paper_size; element.u1 = (x + w - paper_x) / paper_size;
    element.v0 = (y - paper_y) / paper_size; element.v1 = (y + h - paper_y) / paper_size;
    if (element.u0 < 0) element.u0 = 0;
    if (element.v0 < 0) element.v0 = 0;
    if (element.u1 > 1) element.u1 = 1;
    if (element.v1 > 1) element.v1 = 1;
    if (element.u1 < element.u0 || element.v1 < element.v0) return;
    element.image = handle;
    element.rgba = 0xffffff00u | (u32)(alpha * 255.0f + .5f);
    wwhd_hud_emit(list, &element);
}
static void draw_chart(u32 list, float x, float y, float side, float a) {
    if (!snap.chart_count) return;
    if (ui.chart >= snap.chart_count) ui.chart = 0;
    chart_image(snap.charts[ui.chart].sea);
    float inset = side * 6.0f / 256.0f, inner = side - 2 * inset;
    /* the chart's own blue paper (TreMap_00's base) under the sector's chart art */
    if (!img_chart_base) img_chart_base = game_image("TreMap_00/MapArea_00^t#P_TreMapBase_00");
    /* as the game's treasure chart screen does it (hdTreMapScreen_c::selectChart): the chart is centred on
     * the treasure's spot, where the X is, and the sector's chart art is moved by the art centre's
     * distance from it; the chart spans the game's cell world size */
    const s16* r = snap.charts[ui.chart].rect;
    f32 world = WWHD_GAME_DATA(0x1010855C, f32);
    float k = hdm_finite(world) && world > 1000 && world < 1e6f ? inner / world : 0;
    float ax = (float)(r[2] - r[0]) * k, ay = (float)(r[3] - r[1]) * k;
    clip(list, 1, x + inset, y + inset, inner, inner);
    paper_x = x + inset - inner * .1f; paper_y = y + inset - inner * .1f; paper_size = inner * 1.2f;
    image(list, img_chart_base, paper_x, paper_y, paper_size, paper_size, 0, 0xffffff, a);
    image(list, img_chart, x + inset + ax, y + inset + ay, inner, inner, 0, 0xffffff, a);
    /* where the moved art ends inside the chart, it fades into the chart paper (paper strips of rising
     * opacity over its last eighth), so no cut edge shows */
    {
        float left = x + inset, top = y + inset, f = inner * .14f;
        float art_l = left + ax, art_t = top + ay, art_r = art_l + inner, art_b = art_t + inner;
        /* nested strips, each from its step to the art's end: after n of N layers the paper covers n / N
         * (layer n with opacity 1 / (N - n + 1)), with no seams between the strips */
        const int steps = 16;
        float w = f / steps;
        for (int n = 1; n <= steps; ++n) {
            float o = a / (float)(steps - n + 1 + 1), d = (n - 1) * w;
            if (art_r < left + inner) rect_image(list, img_chart_base, art_r - f + d, top, f - d, inner, o);
            if (art_l > left) rect_image(list, img_chart_base, art_l, top, f - d, inner, o);
            if (art_b < top + inner) rect_image(list, img_chart_base, left, art_b - f + d, inner, f - d, o);
            if (art_t > top) rect_image(list, img_chart_base, left, art_t, inner, f - d, o);
        }
    }
    if (!img_mark) img_mark = game_image("TreMap_00/MapTreasureMark_00^t#P_TreasureMark_00");
    if (!img_mark_fill) img_mark_fill = game_image("TreMap_00/MapTreasureMark_00^t#P_TreasureMarkColor_00");
    float m = inner * .15f, cx = x + side / 2, cy = y + side / 2;
    image(list, img_mark, cx - m / 2, cy - m / 2, m, m, 0, 0xffffff, a);
    image(list, img_mark_fill, cx - m / 3, cy - m / 3, m * 2 / 3, m * 2 / 3, 0, 0xffffff, a);
    clip(list, 0, 0, 0, 0, 0);
    frame(list, x, y, side, a);
    /* the game's title ribbon with the chart's number (language-neutral; the chart names are game text) */
    if (!img_ribbon) img_ribbon = game_image("MenuMap_00/TitleBase_00^t#P_TitleTreBase_00");
    /* the ribbon is drawn from two halves, the right one turned (the game mirrors it) */
    float rw = side * .62f, rh = side * .16f, rx = x + (side - rw) / 2, ry = y - rh * .45f;
    image(list, img_ribbon, rx, ry, rw / 2, rh, 0, 0xffffff, a);
    image(list, img_ribbon, rx + rw / 2, ry, rw / 2, rh, pi, 0xffffff, a);
    static char number[4];
    int n = snap.charts[ui.chart].number;
    number[0] = (char)('0' + n / 10 % 10); number[1] = (char)('0' + n % 10); number[2] = 0;
    if (n < 10) { number[0] = number[1]; number[1] = 0; }
    element = (wwhd_hud_element){0};
    element.kind = WWHD_HUD_TEXT; element.anchor = WWHD_HUD_TOP_LEFT;
    element.x = rx + rw / 2 - rh * (n < 10 ? .18f : .34f); element.y = ry + rh * .12f; element.size = rh * .7f;
    element.thickness = 1; element.u1 = element.v1 = 1;
    element.rgba = 0x1c4f8a00u | (u32)(a * 255.0f);
    element.text = number; element.text_bytes = n < 10 ? 1 : 2;
    wwhd_hud_emit(list, &element);
}
/* layout point -> panel point; zoomed, the plan turns about Link so that he faces up, as the GamePad's
 * zoomed map does (hdDungeonMap_c::updateMapRotation follows Link's heading) */
static hdm_view dview;
static float dinner, dturn, dlink_px, dlink_py;
static void place(float lx, float ly, float* px, float* py) {
    hdm_to_panel(&dview, dinner, lx, ly, px, py);
    if (dturn == 0) return;
    float c = sin_approx(dturn + pi / 2), s = sin_approx(dturn), dx = *px - dlink_px, dy = *py - dlink_py;
    *px = dlink_px + dx * c - dy * s;
    *py = dlink_py + dx * s + dy * c;
}
static void draw_dungeon(u32 list, float x, float y, float side, float a) {
    floor_rooms(snap.dungeon, snap.floor);
    if (!room_count) return;
    float inset = side * 6.0f / 256.0f, inner = side - 2 * inset;
    float link_x, link_y;
    world_to_layout(snap.x, snap.z, &link_x, &link_y);
    int here = room_at(link_x, link_y);
    if (ui.zoom) {
        float span = (floor_max_x - floor_min_x > floor_max_y - floor_min_y ? floor_max_x - floor_min_x : floor_max_y - floor_min_y) * .45f;
        dview = hdm_fit(link_x - span / 2, link_y - span / 2, link_x + span / 2, link_y + span / 2, inner, 0);
    } else {
        dview = hdm_fit(floor_min_x, floor_min_y, floor_max_x, floor_max_y, inner, .04f);
    }
    dinner = inner; dturn = 0;
    hdm_to_panel(&dview, inner, link_x, link_y, &dlink_px, &dlink_py);
    s16 heading = hdm_plan_heading(snap.yaw, snap.turned);
    if (ui.zoom && opt.zoom_turns) dturn = -yaw_rotation(heading);
    float ox = x + inset, oy = y + inset;
    if (opt.background) rect(list, ox, oy, inner, inner, 0x000000, opt.bg_opacity * a / (opt.opacity > 0 ? opt.opacity : 1));
    clip(list, 1, ox, oy, inner, inner);
    for (int i = 0; i < room_count; ++i) {
        int p = rooms[i].pane;
        int room = rooms[i].room;
        int current = room >= 0 ? room == snap.room : p == here;
        int visited = room >= 0 && room < 64 && ((snap.visited >> room) & 1);
        u32 rgb = current ? 0xffffff : visited ? 0xc8c8c8 : 0x707070;
        float px, py, w = rooms[i].w * dview.scale, h = rooms[i].h * dview.scale;
        place(rooms[i].x, rooms[i].y, &px, &py);
        image(list, rooms[i].image, ox + px - w / 2, oy + py - h / 2, w, h, dturn, rgb, current || visited ? a : a * .7f);
    }
    /* marker sizes follow the GamePad's: a chest 24 x 18, a door 32 x 10, the boss icon 35 x 29 (layout units
     * of a 520-unit plan), scaled to the panel */
    float unit = side / 330.0f;
    if (!img_door) img_door = game_image("DungeonMapDoor_00/MapDoor_00^t#P_Door_00");
    for (int i = 0; i < snap.door_count; ++i) {
        float lx, ly, px, py;
        world_to_layout(snap.doors[i].x, snap.doors[i].z, &lx, &ly);
        place(lx, ly, &px, &py);
        /* turned as the game turns its door pane: rotZ is the heading plus the plan's turn, counterclockwise
         * (USA 0268B438), and the HUD turns clockwise */
        image(list, img_door, ox + px - 16 * unit, oy + py - 5 * unit, 32 * unit, 10 * unit,
              dturn - (float)hdm_plan_heading(snap.doors[i].heading, snap.turned) * (2 * pi / 65536.0f), 0xffffff, a);
    }
    if (snap.has_compass) {
        if (!img_chest) img_chest = game_image("DungeonMapTreasure_00/MapTreasure_00^t#P_Treasure_00");
        for (int i = 0; i < snap.point_count; ++i) {
            float lx, ly, px, py;
            world_to_layout(snap.points[i].x, snap.points[i].z, &lx, &ly);
            place(lx, ly, &px, &py);
            image(list, img_chest, ox + px - 12 * unit, oy + py - 9 * unit, 24 * unit, 18 * unit, 0, 0xffffff, a);
        }
        if (snap.boss_visible) {
            if (!img_boss) img_boss = game_image("DungeonMapBoss_00/MapBossIcon_00^q");
            float lx, ly, px, py;
            world_to_layout(snap.boss_x, snap.boss_z, &lx, &ly);
            place(lx, ly, &px, &py);
            /* beside the boss door by the game's per-dungeon offset (l_bossPositions, +y up) */
            px += snap.boss_dx * unit; py -= snap.boss_dy * unit;
            image(list, img_boss, ox + px - 17.5f * unit, oy + py - 14.7f * unit, 35 * unit, 29.4f * unit, 0, 0xffffff, a);
        }
    }
    if (snap.companion) {
        u32* img = snap.dungeon == 4 ? &img_medli : &img_makar;
        if (!*img) *img = game_image(snap.dungeon == 4 ? "DungeonMapMedley_00/MapMedleyS_00^q" : "DungeonMapMakore_00/MapMakoreS_00^q");
        float lx, ly, px, py;
        world_to_layout(snap.cx, snap.cz, &lx, &ly);
        place(lx, ly, &px, &py);
        marker(list, *img, ox + px, oy + py, side * .1f, 0, a);
    }
    if (dturn != 0) heading = (s16)0x8000;  /* turned with Link: he always faces up */
    heading_marker(list, img_face, 0, ox + dlink_px, oy + dlink_py, side, heading, a);
    clip(list, 0, 0, 0, 0, 0);
    frame(list, x, y, side, a);
}

/* the shortcut's picture for a moment after a press (Minimap HD's icons), drawn last so that it stays on
 * top of the map, at the panel's top-right corner */
static void feedback(u32 list, float side) {
    if (!ui.feedback || ui.feedback_time <= 0) return;
    if (!img_feedback[ui.feedback]) img_feedback[ui.feedback] = wwhd_hud_texture(WWHD_HUD_PACKAGE, feedback_art[ui.feedback]);
    float t = ui.feedback_time > .3f ? 1 : ui.feedback_time / .3f, size = 56;
    float x = opt.x + side - size * .75f, y = 720 - opt.bottom - side - size * .25f;
    image(list, img_feedback[ui.feedback], x, y, size, size, 0, 0xffffff, t * (opt.opacity > .3f ? opt.opacity : .3f));
}
static unsigned long long captured_step = ~0ull;
static void draw(u32 list) {
    unsigned long long epoch = wwhd_hud_epoch();
    if (epoch != image_epoch) { image_epoch = epoch; forget_images(); }
    /* hidden while the game is paused or a menu is open: Link's step did not run for this logic step */
    if (opt.debug && (wwhd_logic_step() % 60) == 0) {
        wwhd_log_int("draw.visible", snap.visible); wwhd_log_int("draw.mode", snap.mode);
        wwhd_log_int("draw.sector", snap.sector); wwhd_log_int("draw.discovered", snap.discovered);
        wwhd_log_hex("draw.step", (u32)wwhd_logic_step()); wwhd_log_hex("draw.captured", (u32)captured_step);
        wwhd_log_int("draw.paused", game_paused());
        wwhd_log_float("sea.x", snap.x); wwhd_log_float("sea.z", snap.z);
        wwhd_log_float("sea.island_dx", snap.island_dx); wwhd_log_float("sea.island_dz", snap.island_dz);
        wwhd_log_int("sea.covered", snap.covered); wwhd_log_int("sea.ship", snap.on_ship); wwhd_log_int("sea.charts", snap.chart_count);
        wwhd_log_int("sea.chart_set", snap.chart_set);
        wwhd_log_float("chart.world", WWHD_GAME_DATA(0x1010855C, f32));
        {
            u8* play = wwhd_play_get();
            fopAc_ac_c* ship = play ? *(fopAc_ac_c**)(play + 0x5B3C) : 0;
            if (ship) { wwhd_log_float("ship.x", ship->current.pos.x); wwhd_log_float("ship.y", ship->current.pos.y); wwhd_log_float("ship.z", ship->current.pos.z); wwhd_log_int("ship.yaw", ship->shape_angle.y); }
        }
    }
    /* Link's step runs in the logic step before this callback; in menus or while paused it doesn't */
    if (!snap.visible || game_paused() || wwhd_logic_step() > captured_step + 1) return;
    if (ui.hidden) { feedback(list, opt.side_compact); return; }
    float rest = opt.opacity, a = opt.fade ? hdm_fade_opacity(&fade, rest, opt.faded) : rest;
    if (a <= 0) return;
    static_images();
    if (snap.mode == MODE_SEA) {
        float side = snap.on_ship ? opt.side_compact : (ui.expanded ? opt.side_expanded : opt.side_compact);
        if (snap.on_ship && ui.sea_view == 0) return;
        /* aboard, above the game's compasses (bottom left) */
        float x = opt.x, y = 720 - side - opt.bottom - (snap.on_ship ? 110 : 0);
        draw_sea(list, x, y, side, a);
        if (snap.on_ship && ui.sea_view == 2 && snap.discovered) draw_chart(list, x + side + 8, y, side, a);
    } else if (snap.mode == MODE_DUNGEON) {
        float side = ui.expanded ? opt.side_expanded : opt.side_compact;
        draw_dungeon(list, opt.x, 720 - side - opt.bottom, side, a);
    }
    feedback(list, ui.expanded && !(snap.mode == MODE_SEA && snap.on_ship) ? opt.side_expanded : opt.side_compact);
}

/* ---- per-step capture (Link's own step, so it doesn't run in menus or while paused) ---- */
static void capture_chests(u32 membit, const u8* records, s32 count, int filter_room, int stage_kind) {
    if (!records || count < 0 || count > 128) return;
    for (s32 i = 0; i < count && snap.point_count < MAX_POINTS; ++i) {
        const u8* r = records + 32 * i;
        u32 params = ((u32)r[8] << 24) | ((u32)r[9] << 16) | ((u32)r[10] << 8) | r[11];
        float x = *(const f32*)(r + 12), y = *(const f32*)(r + 16), z = *(const f32*)(r + 20);
        u32 rooms = ((u32)r[24] << 8) | r[25];
        u32 number = (params >> 7) & 31, type = params & 127;
        if (number == 15 || number == 16) continue;
        if (filter_room && (int)(rooms & 63) != snap.room) continue;
        int opened = (type == 7 || type == 8) ? stage_chest_open(stage_kind, (s32)number) : chest_open(membit, (s32)number);
        if (opened || room_floor((s32)(rooms & 63), y) != snap.floor) continue;
        snap.points[snap.point_count].x = x; snap.points[snap.point_count].z = z; ++snap.point_count;
    }
}
/* the stage's doors on Link's floor (all shown: the plan needs the Map), and the boss door, which carries
 * the boss icon with the Compass until the boss is defeated (hdDungeonMap_c::initDoorsFromStage, isPlayerDoor) */
static int name_is(const u8* name, const char* want) {
    for (int i = 0; i < 8; ++i) { if (name[i] != (u8)want[i]) return 0; if (!want[i]) return 1; }
    return 1;
}
static void capture_doors(u32 membit) {
    snap.door_count = 0; snap.boss_visible = 0;
    u8* list = stage_doors();
    if (!list) return;
    s32 count = *(s32*)list;
    if (count < 0 || count > 128) return;
    for (s32 i = 0; i < count; ++i) {
        const u8* r = list + 4 + 36 * i;
        float x = *(const f32*)(r + 12), y = *(const f32*)(r + 16), z = *(const f32*)(r + 20);
        s16 heading = *(const s16*)(r + 26);
        u32 kind = (*(const u16*)(r + 28) >> 8) & 0xFF;
        int boss = name_is(r, "door20") || (name_is(r, "door12") && (kind == 9 || kind == 12));
        if (stage_floor(0, y) != snap.floor) continue;
        if (boss && snap.has_compass && !dungeon_item(membit, 3)) {
            const f32* offsets = (const f32*)&WWHD_GAME_DATA(0x104926EC, u8);
            snap.boss_visible = 1; snap.boss_x = x; snap.boss_z = z;
            snap.boss_dx = offsets[2 * snap.dungeon]; snap.boss_dy = offsets[2 * snap.dungeon + 1];
        }
        if (snap.door_count < MAX_POINTS) {
            snap.doors[snap.door_count].x = x; snap.doors[snap.door_count].z = z; snap.doors[snap.door_count].heading = heading;
            ++snap.door_count;
        }
    }
}
static void read_stage(u8* play) {
    u8* stage = play + PLAY_STAGE;
    u32 dmap = *(u32*)(stage + 0x74);
    snap.dmap = 0;
    if (dmap) {
        u32 entries = *(u32*)(dmap + 4);
        if (entries) {
            snap.origin_x = *(f32*)entries; snap.origin_z = *(f32*)(entries + 4); snap.size = *(f32*)(entries + 8);
            snap.dmap = hdm_finite(snap.size) && snap.size > 0;
        }
    }
}
static float floor_offset(u8* play) {
    u32 info = *(u32*)(play + PLAY_STAGE + 0x48);
    u32 dmap = *(u32*)(play + PLAY_STAGE + 0x74);
    if (!info || !dmap || !*(u32*)(dmap + 4)) return 0;
    u32 kind = (*(u32*)(info + 0xC) >> 16) & 7;
    return (kind == 3 || kind == 6) ? *(f32*)(*(u32*)(dmap + 4) + 12) : 0;
}
static void read_charts(u32 save) {
    snap.chart_count = 0;
    u32 table = *(u32*)&WWHD_GAME_DATA(CHART_TABLE, u8);
    if (!table) return;
    s32 count = *(s32*)table;
    if (count <= 0 || count > 64) return;
    for (s32 i = 0; i < count && snap.chart_count < MAX_CHARTS; ++i) {
        const u8* e = (const u8*)(table + 4 + 0x26 * i);
        s8 sea = (s8)e[0], save_no = (s8)e[1];
        u8 type = e[3];
        if (sea < 1 || sea > 49 || save_no < 1) continue;          /* sector charts only: no overview charts */
        if (!chart_owned(save + SAVE_PLAYER_MAP, save_no - 1)) continue;
        if (type == 0 && !chart_open(save + SAVE_PLAYER_MAP, save_no - 1)) continue;  /* Triforce charts once deciphered */
        snap.charts[snap.chart_count].sea = sea; snap.charts[snap.chart_count].save = save_no;
        snap.charts[snap.chart_count].number = (s8)e[2];
        /* the chart set's rectangle: the treasure's spot (the chart's centre) and the art's centre, sector-local */
        for (int k = 0; k < 4; ++k) {
            const u8* r = e + 6 + 2 * (4 * snap.chart_set + k);
            snap.charts[snap.chart_count].rect[k] = (s16)(((u16)r[0] << 8) | r[1]);
        }
        ++snap.chart_count;
    }
}
static void handle_input(void) {
    wwhd_input_state in;
    wwhd_input_read(&in);
    u32 b = in.buttons;
    if (ui.feedback_time > 0) ui.feedback_time -= (float)wwhd_logic_dt();
    if (hdm_pressed(b, previous_buttons, opt.key_toggle)) {
        ui.hidden = !ui.hidden;
        ui.feedback = ui.hidden ? FEEDBACK_HIDDEN : (ui.expanded ? FEEDBACK_EXPANDED : FEEDBACK_COMPACT);
        ui.feedback_time = ui.hidden ? 1.5f : 0;
    }
    if (!ui.hidden) {
        if (hdm_pressed(b, previous_buttons, opt.key_size) && !(snap.mode == MODE_SEA && snap.on_ship)) {
            ui.expanded = !ui.expanded;
            ui.feedback = ui.expanded ? FEEDBACK_EXPANDED : FEEDBACK_COMPACT;
            ui.feedback_time = 1.0f;
        }
        if (hdm_pressed(b, previous_buttons, opt.key_view)) {
            if (snap.mode == MODE_DUNGEON) ui.zoom = !ui.zoom;
            else if (snap.mode == MODE_SEA && snap.on_ship) ui.sea_view = hdm_next_sea_view(ui.sea_view, snap.chart_count);
        }
        if (hdm_pressed(b, previous_buttons, opt.key_chart) && snap.mode == MODE_SEA && snap.on_ship && ui.sea_view == 2 && snap.chart_count)
            ui.chart = (ui.chart + 1) % snap.chart_count;
    }
    previous_buttons = b;
    hdm_fade_step(&fade, hdm_moving(in.lx, in.ly, in.rx, in.ry), (float)wwhd_logic_dt());
}

WWHD_HOOK_RETURN(WWHD_ADDR_daPy_Execute, capture, (fopAc_ac_c* actor)) {
    if (!initialized) configure();
    wwhd_hud_register(draw, WWHD_HUD_TV);
    u8* play = wwhd_play_get();
    if (!play || !actor || (u32)actor != *(u32*)(play + WWHD_PLAY_PLAYER_OFFSET)) return;
    captured_step = wwhd_logic_step();
    snap.visible = 0;
    if (play[WWHD_PLAY_ENABLE_NEXT_STAGE_OFFSET] || play[WWHD_PLAY_EVENT_RUNNING_OFFSET]) return;
    u32 status0 = *(u32*)(play + PLAY_STATUS0), status1 = *(u32*)(play + PLAY_STATUS1);
    handle_input();
    if ((status0 & STTS0_HIDE) || (status1 & STTS1_CONDUCT)) return;
    for (int i = 0, ended = 0; i < 8; ++i) {
        char ch = ended ? 0 : (char)play[WWHD_PLAY_START_STAGE_NAME_OFFSET + i];
        if (!ch) ended = 1;
        snap.stage[i] = ch;
    }
    snap.stage[8] = 0;
    u32 save = *(u32*)&WWHD_GAME_DATA(WWHD_ADDR_dComIfG_save_info_pointer, u8);
    if (!save) return;
    snap.x = actor->current.pos.x; snap.y = actor->current.pos.y; snap.z = actor->current.pos.z;
    snap.yaw = actor->shape_angle.y;
    snap.room = actor->current.roomNo;
    snap.on_ship = (status0 & STTS0_SHIP) != 0;
    if (hdm_same(snap.stage, "sea")) {
        snap.mode = MODE_SEA;
        snap.sector = hdm_sea_room(snap.x, snap.z);
        if (!snap.sector) return;
        snap.discovered = sector_discovered(save + SAVE_PLAYER_MAP, snap.sector - 1) != 0;
        const u8* location = hdm_location(*(const u8**)&WWHD_GAME_DATA(LOCATION_TABLE, u8), snap.sector);
        snap.covered = 0;
        if (location) {
            float u, v;
            snap.island_dx = hdm_s16(location + 2); snap.island_dz = hdm_s16(location + 4);
            hdm_island_uv(snap.sector, snap.island_dx, snap.island_dz, snap.x, snap.z, &u, &v);
            snap.covered = hdm_inside(u, v);
        }
        snap.wind = wind_angle();
        snap.chart_set = *(u8*)(save + SAVE_CHART_SET) & 3;
        read_charts(save);
        snap.visible = 1;
        return;
    }
    snap.dungeon = hdm_dungeon_of(snap.stage);
    if (snap.dungeon < 0 || !opt.dungeons || snap.room < 0 || snap.room >= 64) { snap.mode = MODE_NONE; return; }
    u32 membit = save + SAVE_MEMBIT;
    int forsaken = snap.dungeon == 0;
    snap.has_map = forsaken ? stage_map(2) != 0 : dungeon_item(membit, 0) != 0;
    if (opt.debug && (wwhd_logic_step() % 60) == 0) { wwhd_log_int("dungeon", snap.dungeon); wwhd_log_int("dungeon.map", snap.has_map); }
    if (!snap.has_map) { snap.mode = MODE_NONE; return; }
    snap.has_compass = forsaken ? stage_compass(2) != 0 : dungeon_item(membit, 1) != 0;
    snap.turned = hdm_forsaken_turned(snap.stage);
    read_stage(play);
    /* the game's map width ratio (the GamePad map pane over its reference width): from the game's own
     * dungeon map when it has one, else the value it has in every dungeon */
    snap.map_width = 1.68919f;
    {
        u32 ui_scene = *(u32*)&WWHD_GAME_DATA(0x101F8344, u8);
        u32 coord = ui_scene ? *(u32*)(ui_scene + 0x218) : 0;
        u32 dmap_obj = coord ? *(u32*)(coord + 0x2C) : 0;
        if (dmap_obj) { f32 w = *(f32*)(dmap_obj + 0x3AC); if (hdm_finite(w) && w > .1f && w < 10) snap.map_width = w; }
    }
    if (!snap.dmap) { snap.mode = MODE_NONE; return; }
    snap.floor = stage_floor(0, snap.y + floor_offset(play));
    snap.visited = 0;
    {
        int count = 0;
        const unsigned int* table = plan_table(snap.dungeon, &count);
        for (int i = 0; table && i < count; ++i) {
            int room = (int)table[3 * i + 2];
            if (room >= 0 && room < 64 && room_visited(membit, room)) snap.visited |= 1ull << room;
        }
    }
    capture_doors(membit);
    snap.point_count = 0;
    if (snap.has_compass) {
        u8* table = stage_treasures();
        if (table) capture_chests(membit, table + 4, *(s32*)table, 0, 1);  /* the whole floor, as the plan shows it */
        u8* room = room_data((u32)play + 0x51cc, snap.room);
        if (room) { u8* t = *(u8**)(room + 0x40); if (t) capture_chests(membit, *(u8**)(t + 4), *(s32*)t, 0, 1); }
    }
    snap.companion = 0;
    if (snap.dungeon == 4 || snap.dungeon == 5) {
        fopAc_ac_c* partner = *(fopAc_ac_c**)(play + PLAY_PARTNER);
        if (partner && partner != actor) {
            snap.cx = partner->current.pos.x; snap.cz = partner->current.pos.z; snap.cyaw = partner->shape_angle.y;
            snap.companion = room_floor(partner->current.roomNo, partner->current.pos.y + floor_offset(play)) == snap.floor;
        }
    }
    if (opt.debug && (wwhd_logic_step() % 30) == 0) {
        float lx, ly;
        world_to_layout(snap.x, snap.z, &lx, &ly);
        wwhd_log_int("room", snap.room); wwhd_log_int("floor", snap.floor);
        wwhd_log_hex("partner.5B30", *(u32*)(play + 0x5B30)); wwhd_log_hex("partner.5B34", *(u32*)(play + 0x5B34));
        wwhd_log_hex("partner.5B38", *(u32*)(play + 0x5B38)); wwhd_log_hex("partner.5B3C", *(u32*)(play + 0x5B3C));
        wwhd_log_hex("link", (u32)actor); wwhd_log_int("companion", snap.companion); wwhd_log_int("chests", snap.point_count);
        wwhd_log_int("compass", snap.has_compass);
        {
            int count = 0;
            const unsigned int* table = plan_table(snap.dungeon, &count);
            for (int i = 0; table && i < count; ++i) { wwhd_log_int("plan.marker", (int)table[3*i]); wwhd_log_hex("plan.floor", table[3*i+1]); wwhd_log_int("plan.room", (int)table[3*i+2]); }
            wwhd_log_hex("visited", (u32)snap.visited);
            const f32* boss = (const f32*)&WWHD_GAME_DATA(0x104926EC, u8);
            for (int i = 0; i < 6; ++i) { wwhd_log_float("boss.x", boss[2*i]); wwhd_log_float("boss.y", boss[2*i+1]); }
            u8* doors = stage_doors();
            if (doors) { wwhd_log_int("doors", *(s32*)doors); }
        }
        wwhd_log_float("x", snap.x); wwhd_log_float("z", snap.z);
        wwhd_log_float("dmap.x", snap.origin_x); wwhd_log_float("dmap.z", snap.origin_z); wwhd_log_float("dmap.size", snap.size);
        wwhd_log_float("layout.x", lx); wwhd_log_float("layout.y", ly);
        /* the game's own dungeon map object, for calibration only (read, never written) */
        u32 ui_scene = *(u32*)&WWHD_GAME_DATA(0x101F8344, u8);
        u32 coord = ui_scene ? *(u32*)(ui_scene + 0x218) : 0;
        u32 dmap_obj = coord ? *(u32*)(coord + 0x2C) : 0;
        if (dmap_obj) {
            wwhd_log_float("game.origin.x", *(f32*)(dmap_obj + 0x2FC)); wwhd_log_float("game.origin.y", *(f32*)(dmap_obj + 0x300));
            wwhd_log_float("game.size", *(f32*)(dmap_obj + 0x304)); wwhd_log_float("game.rotation", *(f32*)(dmap_obj + 0x308));
            wwhd_log_float("game.scale", *(f32*)(dmap_obj + 0x30C)); wwhd_log_float("game.width", *(f32*)(dmap_obj + 0x3AC));
            wwhd_log_float("game.pane.x", *(f32*)(dmap_obj + 0x3B0)); wwhd_log_float("game.pane.y", *(f32*)(dmap_obj + 0x3B4));
            wwhd_log_int("game.floor", *(u8*)(dmap_obj + 0x3D0));
            u32 map = *(u32*)(dmap_obj + 0x920);
            if (map) {
                wwhd_log_float("map.origin.x", *(f32*)(map + 0x4E0)); wwhd_log_float("map.origin.y", *(f32*)(map + 0x4E4));
                wwhd_log_float("map.scale", *(f32*)(map + 0x4EC));
                wwhd_log_float("map.current.x", *(f32*)(map + 0x4F0)); wwhd_log_float("map.current.y", *(f32*)(map + 0x4F4));
                wwhd_log_float("map.target.x", *(f32*)(map + 0x4F8)); wwhd_log_float("map.target.y", *(f32*)(map + 0x4FC));
            }
        }
        for (int i = 0; i < room_count; ++i) if (rooms[i].pane == snap.room) {
            wwhd_log_float("pane.x", rooms[i].x); wwhd_log_float("pane.y", rooms[i].y);
            wwhd_log_float("pane.w", rooms[i].w); wwhd_log_float("pane.h", rooms[i].h);
        }
    }
    snap.mode = MODE_DUNGEON;
    snap.visible = 1;
}
