/* HD minimap: pure helpers shared by the guest code and the host tests (tests/test_math.c).
 * No game data: names of the game's layout panes/textures are composed here, the art itself is read
 * by the port from the player's installed game at run time. */
#ifndef HD_MINIMAP_H
#define HD_MINIMAP_H

static inline int hdm_finite(float v) {
    union { float f; unsigned int bits; } u = { v };
    return (u.bits & 0x7f800000u) != 0x7f800000u;
}

/* ---- the Great Sea: 7 x 7 sectors of 100000 units, D4's centre at (0, 0); room = row * 7 + col + 1 ---- */
static inline int hdm_sea_room(float x, float z) {
    if (!hdm_finite(x) || !hdm_finite(z) || x < -350000.0f || x >= 350000.0f || z < -350000.0f || z >= 350000.0f) return 0;
    float fx = x / 100000.0f + 3.5f, fz = z / 100000.0f + 3.5f;
    int col = (int)fx, row = (int)fz;
    if (col < 0 || col > 6 || row < 0 || row > 6) return 0;
    return row * 7 + col + 1;
}
/* position inside a sector's island art (0..1, north up): the art spans 20000 units around the island's
 * centre, the sector centre plus the island's offset from the game's location table (as the GamePad's
 * island balloons place their markers) */
#define HDM_ISLAND_SPAN 20000.0f
static inline void hdm_island_uv(int room, float dx, float dz, float x, float z, float* u, float* v) {
    int col = (room - 1) % 7, row = (room - 1) / 7;
    *u = (x - ((col - 3) * 100000.0f + dx)) / HDM_ISLAND_SPAN + 0.5f;
    *v = (z - ((row - 3) * 100000.0f + dz)) / HDM_ISLAND_SPAN + 0.5f;
}
static inline int hdm_inside(float u, float v) { return u >= 0 && u <= 1 && v >= 0 && v <= 1; }
/* the game's location table: count, then 8 bytes per sector (s8 column, s8 row, s16 x and z offsets,
 * u8 kind, pad); the entry of a sector, or 0 */
static inline const unsigned char* hdm_location(const unsigned char* table, int room) {
    if (!table || room < 1 || room > 49) return 0;
    unsigned int n = ((unsigned int)table[0] << 24) | ((unsigned int)table[1] << 16) | ((unsigned int)table[2] << 8) | table[3];
    if (n != 49) return 0;
    const unsigned char* e = table + 4 + 8 * (room - 1);
    int col = (signed char)e[0] + 3, row = (signed char)e[1] + 3;
    return row * 7 + col + 1 == room ? e : 0;
}
static inline float hdm_s16(const unsigned char* p) { return (float)(short)(((unsigned int)p[0] << 8) | p[1]); }

/* ---- names ---- */
static inline char hdm_hex(int v) { return (char)(v < 10 ? '0' + v : 'a' + v - 10); }
static inline int hdm_put(char* out, int i, const char* s) { while (*s) out[i++] = *s++; out[i] = 0; return i; }
static inline int hdm_put2(char* out, int i, int v) { out[i++] = (char)('0' + v / 10 % 10); out[i++] = (char)('0' + v % 10); out[i] = 0; return i; }
/* "BtnMapBalloon_00/r2c_b^t#P_DummyMapBig_00": a sector's island art (full: the detailed variant) */
static inline void hdm_sector_texture(char* out, int room, int full) {
    int i = hdm_put(out, 0, "BtnMapBalloon_00/r");
    out[i++] = hdm_hex(room >> 4 & 15); out[i++] = hdm_hex(room & 15);
    hdm_put(out, i, full ? "_Full^t#P_DummyMapFull_00" : "_b^t#P_DummyMapBig_00");
}
/* "TreMap_00/r2c_col_0^t#P_DummyTreMap_00": a treasure chart of a sector, in the save's chart set 0..3 */
static inline void hdm_chart_texture(char* out, int room, int set) {
    int i = hdm_put(out, 0, "TreMap_00/r");
    out[i++] = hdm_hex(room >> 4 & 15); out[i++] = hdm_hex(room & 15);
    i = hdm_put(out, i, "_col_");
    out[i++] = (char)('0' + (set & 3));
    hdm_put(out, i, "^t#P_DummyTreMap_00");
}
/* the dungeon layouts: DungeonMapFloor_00..05 */
static inline void hdm_floor_layout(char* out, int dungeon) {
    int i = hdm_put(out, 0, "DungeonMapFloor_");
    hdm_put2(out, i, dungeon);
}
/* "layout-pane:DungeonMapFloor_05/P_Room_07" */
static inline void hdm_room_pane_key(char* out, int dungeon, int pane) {
    int i = hdm_put(out, 0, "layout-pane:DungeonMapFloor_");
    i = hdm_put2(out, i, dungeon);
    i = hdm_put(out, i, "/P_Room_");
    hdm_put2(out, i, pane);
}
/* "DungeonMapFloor_05/MapKaze1F_05^t#P_Room_07": a room's art with its own material */
static inline void hdm_room_texture(char* out, int dungeon, const char* texture, int pane) {
    int i = hdm_put(out, 0, "DungeonMapFloor_");
    i = hdm_put2(out, i, dungeon);
    out[i++] = '/';
    for (int k = 0; texture[k] && k < 31; ++k) out[i++] = texture[k];
    out[i] = 0;
    i = hdm_put(out, i, "#P_Room_");
    hdm_put2(out, i, pane);
}
/* the game's floor numbers are 0x80-based (0x80 1F, 0x81 2F, 0x7F B1); tag "1F" / "B1" as in the art names */
static inline int hdm_floor_tag(int floor, char* tag) {
    if (floor >= 0x80 && floor <= 0x88) { tag[0] = (char)('1' + floor - 0x80); tag[1] = 'F'; tag[2] = 0; return 1; }
    if (floor >= 0x78 && floor < 0x80) { tag[0] = 'B'; tag[1] = (char)('0' + 0x80 - floor); tag[2] = 0; return 1; }
    tag[0] = 0;
    return 0;
}
/* Does a room texture ("MapKaze1F_05^t", "MapKami1F_03_2F_04^t") belong to the floor tag? */
static inline int hdm_texture_on_floor(const char* texture, const char* tag) {
    for (int i = 0; texture[i] && texture[i + 1]; ++i) {
        if (texture[i] != tag[0] || texture[i + 1] != tag[1]) continue;
        char before = i ? texture[i - 1] : '_', after = texture[i + 2];
        int letter_before = (before >= 'a' && before <= 'z') || (before >= 'A' && before <= 'Z');
        /* "...Kaze1F_" (a name, then the tag) or "..._2F_" (a second floor) */
        if (after == '_' && (letter_before || before == '_')) return 1;
    }
    return 0;
}

/* A plan pane may span several floors (notably Tower of the Gods).
 * The room-colour table's primary floor must not suppress its other artwork. */
static inline int hdm_room_on_floor(int primary, int floor, const char* texture, const char* tag) {
    return primary == floor || hdm_texture_on_floor(texture, tag);
}

/* stage name -> dungeon layout (0 Forsaken Fortress, 1 Dragon Roost Cavern, 2 Forbidden Woods,
 * 3 Tower of the Gods, 4 Earth Temple, 5 Wind Temple), -1 none */
static inline int hdm_same(const char* a, const char* b) {
    int i = 0;
    for (; a[i] && b[i]; ++i) if (a[i] != b[i]) return 0;
    return a[i] == b[i];
}
static inline int hdm_dungeon_of(const char* stage) {
    if (hdm_same(stage, "majroom") || hdm_same(stage, "ma2room") || hdm_same(stage, "ma3room") || hdm_same(stage, "MajyuE")) return 0;
    if (hdm_same(stage, "M_NewD2")) return 1;
    if (hdm_same(stage, "kindan")) return 2;
    if (hdm_same(stage, "Siren")) return 3;
    if (hdm_same(stage, "M_Dai")) return 4;
    if (hdm_same(stage, "kaze")) return 5;
    return -1;
}

/* ---- Forsaken Fortress after the first visit (ma2room, ma3room: the game's map kinds 6 and 7). Their stages
 * are turned from the first visit's, whose plan the GamePad draws, so the game places their markers with the
 * first visit's scale, turned 64.995 degrees clockwise about the plan's centre: 30.8 / 24.8 from the pane
 * N_FloorCenter_00 (USA 0268BCFC and 026893C8; the map kinds by stage name in 0268698C) ---- */
#define HDM_FORSAKEN_SCALE 0.026728f       /* layout units per world unit (USA 0x100F9BC4) */
#define HDM_FORSAKEN_CENTRE_X 30.8f
#define HDM_FORSAKEN_CENTRE_Y 24.8f
#define HDM_FORSAKEN_TURN 0x2E38           /* the plan's turn in game angle units: 64.995 degrees */
static inline int hdm_forsaken_turned(const char* stage) {
    return hdm_same(stage, "ma2room") || hdm_same(stage, "ma3room");
}
/* a point (dx, dz) from an origin -> the plan (+y up) at a scale, turned with Forsaken Fortress's plan about
 * the centre (cx, cy) */
static inline void hdm_turned_place(float dx, float dz, float scale, float cx, float cy, float* x, float* y) {
    const float c = 0.422697f, s = -0.906271f;  /* cos and sin of -64.995 degrees (USA 0x100F9BD0) */
    float px = dx * scale - cx, py = -dz * scale - cy;
    *x = cx + px * c - py * s;
    *y = cy + px * s + py * c;
}
/* a point (dx, dz) from the stage's DMAP origin -> the plan (+y up), turned about the centre (cx, cy) */
static inline void hdm_forsaken_place(float dx, float dz, float cx, float cy, float* x, float* y) {
    hdm_turned_place(dx, dz, HDM_FORSAKEN_SCALE, cx, cy, x, y);
}

/* ---- Forsaken Fortress's outside after the first visit: the Great Sea's room 1 (sector A1). On foot on the
 * fortress's island art (the square the GamePad tests, USA 02687398), the GamePad shows the plan's outdoor
 * picture (DungeonMapFloor_00, P_Outdoor_00) instead of the island; aboard, the island (USA 02687674). Its
 * markers: from room 1's map origin (its 2DMA), at a fixed scale, turned with the plan about 25.6 / 46.8 from
 * the pane N_OutdoorCenter_00 (USA 0268B9A4, 0268BBCC; the origin in 02657FE8) ---- */
#define HDM_OUTDOOR_SECTOR 1
#define HDM_OUTDOOR_SCALE 0.0308909f       /* layout units per world unit (USA 0x100F9BC8) */
#define HDM_OUTDOOR_CENTRE_X 25.6f
#define HDM_OUTDOOR_CENTRE_Y 46.8f
#define HDM_OUTDOOR_ORIGIN_X -300999.0f    /* sea_Room1's 2DMA */
#define HDM_OUTDOOR_ORIGIN_Z -300353.0f
static inline int hdm_forsaken_outside(int sector, int on_ship, int covered) {
    return sector == HDM_OUTDOOR_SECTOR && !on_ship && covered;
}
/* its three searchlights (L_Searchlight_00..02): searching until the event bit or the light's switch in room 1
 * is on; then the plan shows them stopped, MapSearchlight_01 at half opacity instead of MapSearchlight_00
 * (USA 0268A098, 02661574; the animation Searchlight_00_Valid) */
#define HDM_SEARCHLIGHTS 3
#define HDM_SEARCHLIGHTS_EVENT 0x1820
#define HDM_SEARCHLIGHT_SWITCH 101          /* + the light's number (USA 0x100F98E0) */
static inline int hdm_searchlight_stopped(int event_on, int switch_on) { return event_on || switch_on; }
/* a game angle (Link's heading, a door's) as seen on the plan: turned with it */
static inline short hdm_plan_heading(int heading, int turned) {
    return (short)(turned ? heading - HDM_FORSAKEN_TURN : heading);
}

/* ---- the game's room table of a dungeon plan: per plan room (marker, floor, room), three words each
 * (the floor screens' marker mappings that colour a plan room for a visited or current room) ---- */
static inline int hdm_plan_room(const unsigned int* table, int count, int pane, int* floor, int* room) {
    if (!table || count <= 0 || count > 40) return 0;
    for (int i = 0; i < count; ++i) {
        const unsigned int* e = table + 3 * i;
        if ((int)e[0] != pane) continue;
        if (e[1] < 0x78 || e[1] > 0x88 || e[2] > 63) return 0;
        *floor = (int)e[1]; *room = (int)e[2];
        return 1;
    }
    return 0;
}

/* ---- fading: 60 ms out on input, back over 200 ms after 100 ms without input ---- */
typedef struct { float level, idle; } hdm_fade;   /* level 1 = resting opacity, 0 = faded opacity */
static inline void hdm_fade_step(hdm_fade* f, int active, float dt) {
    if (!(dt > 0) || dt > 0.25f) dt = 1.0f / 30.0f;
    if (active) { f->idle = 0; f->level -= dt / 0.060f; if (f->level < 0) f->level = 0; return; }
    f->idle += dt;
    if (f->idle >= 0.100f) { f->level += dt / 0.200f; if (f->level > 1) f->level = 1; }
}
static inline float hdm_fade_opacity(const hdm_fade* f, float rest, float faded) {
    if (faded > rest) faded = rest;
    return faded + (rest - faded) * f->level;
}
/* stick or camera input above the dead zone */
static inline int hdm_moving(float lx, float ly, float rx, float ry) {
    const float dz = 0.2f;
    return lx * lx + ly * ly > dz * dz || rx * rx + ry * ry > dz * dz;
}

/* ---- buttons: a press is a rising edge of the chosen VPAD bit ---- */
static inline int hdm_pressed(unsigned int now, unsigned int before, unsigned int bit) {
    return bit && (now & bit) && !(before & bit);
}
/* "left" "right" "up" "down" "l3" "r3" "none" -> VPAD bit */
static inline unsigned int hdm_button_bit(const char* s) {
    if (hdm_same(s, "left")) return 0x0800;
    if (hdm_same(s, "right")) return 0x0400;
    if (hdm_same(s, "up")) return 0x0200;
    if (hdm_same(s, "down")) return 0x0100;
    if (hdm_same(s, "l3")) return 0x40000;
    if (hdm_same(s, "r3")) return 0x20000;
    return 0;
}
/* buttons left to the game: all of them during an event or a stage change, and aboard outside the
 * Great Sea (the Tower of the Gods); aboard at sea, D-pad left and right (the cannon and the grappling hook) */
#define HDM_ALL_BUTTONS 0xffffffffu
static inline unsigned int hdm_blocked_buttons(int busy, int on_ship, int at_sea) {
    (void)at_sea;
    if (busy) return HDM_ALL_BUTTONS;
    return on_ship ? 0x0800u | 0x0400u : 0;
}

/* ---- sailing views: 0 compasses only (no minimap), 1 minimap, 2 minimap with a treasure chart ---- */
static inline int hdm_next_sea_view(int view, int charts) {
    view = (view + 1) % 3;
    if (view == 2 && !charts) view = 0;
    return view;
}
/* View preferences are independent of temporary eligibility and riding state.
 * Behaviour adapted from Minimap HD 1.3, Copyright (c) 2026 Chironeyes, MIT; see NOTICE.md. */
typedef struct { int hidden, expanded, zoom; } hdm_choice;
static inline hdm_choice hdm_change_context(hdm_choice* foot, hdm_choice* dungeon,
                                           int previous, int next, hdm_choice displayed) {
    *(previous ? dungeon : foot) = displayed;
    return *(next ? dungeon : foot);
}
static inline void hdm_cycle_dungeon(hdm_choice* v) {
    if (v->hidden) { v->hidden = 0; v->expanded = 0; }
    else if (!v->expanded) v->expanded = 1;
    else v->hidden = 1;
}
static inline int hdm_floor_valid(int floor) { return floor >= 0x78 && floor <= 0x88; }
/* Probe at height +/-32: only cross a boundary once both probes agree. Applies
 * throughout flight/glide; never reads the native menu's landing-held floor. */
static inline int hdm_stable_floor(int previous, int candidate, int below, int above) {
    if (!hdm_floor_valid(candidate)) return hdm_floor_valid(previous) ? previous : candidate;
    if (!hdm_floor_valid(previous) || (below == candidate && above == candidate)) return candidate;
    return previous;
}
/* Native MenuMap::setTreTitle message ranges, adapted from Minimap HD 1.3 (MIT).
 * Input is the zero-based native chart index, not the displayed chart number. */
static inline unsigned int hdm_chart_message(int index) {
    if (index < 0 || index >= 49) return 0;
    return (unsigned int)index + (index < 3 ? 0x31D8 : index < 36 ? 0x31DD : index < 44 ? 0x36F1 : 0x36FD);
}

/* ---- the map's icons beside the HUD's D-pad (Minimap HD's shortcut icons): left shows and hides the
 * minimap, right is compact / expanded, down the dungeon zoom (the compass) or, sailing, the view ---- */
enum { HDM_ICON_NONE, HDM_ICON_MAP, HDM_ICON_MAP_UNKNOWN, HDM_ICON_EXPAND, HDM_ICON_REDUCE, HDM_ICON_COMPASS,
       HDM_ICON_VIEW, HDM_ICON_UNKNOWN, HDM_ICON_COUNT };
typedef struct { int left, right, down; } hdm_icons;
/* sea / dungeon: where the minimap is; discovered: the sector's map is known; covered: Link is on its
 * island art. Aboard at sea, left and right are the boat's; aboard elsewhere (the Tower of the Gods) no icon. */
static inline hdm_icons hdm_dpad_icons(int sea, int dungeon, int on_ship, int hidden, int expanded, int discovered, int covered) {
    hdm_icons d = {HDM_ICON_NONE, HDM_ICON_NONE, HDM_ICON_NONE};
    if (dungeon && on_ship) { d.down = HDM_ICON_VIEW; return d; }
    if (sea && on_ship) {
        d.down = !covered ? HDM_ICON_NONE : !discovered ? HDM_ICON_UNKNOWN : HDM_ICON_VIEW;
        return d;
    }
    if ((!sea && !dungeon) || on_ship) return d;
    d.left = sea && !discovered ? HDM_ICON_MAP_UNKNOWN : HDM_ICON_MAP;
    if (hidden) return d;
    d.right = expanded ? HDM_ICON_REDUCE : HDM_ICON_EXPAND;
    if (dungeon) d.down = HDM_ICON_COMPASS;
    return d;
}

/* ---- the dungeon zoom's turn: the plan turns about Link so that a heading (game angle, 0x10000 a turn)
 * faces up: Link's, or with the camera option the camera's when the game has one ---- */
static inline int hdm_turn_heading(int with_camera, int link, int has_camera, int camera) {
    return with_camera && has_camera ? camera : link;
}
/* Link's heading as drawn on the plan turned so that `up` faces up (0x8000 is up on the panel) */
static inline short hdm_heading_on_turned(int heading, int up) { return (short)(heading - up + 0x8000); }

/* Presentation timings adapted from Minimap HD 1.3 (Chironeyes, MIT).
 * Retarget from displayed geometry, so reversals never jump. */
#define HDM_ENTER_SECONDS .200f
#define HDM_EXIT_SECONDS .250f
#define HDM_COMPASS_DELAY .200f
typedef struct { float value, from, target, elapsed, duration; } hdm_tween;
static inline float hdm_ease(float t) {
    if (t <= 0) return 0;
    if (t >= 1) return 1;
    return t * t * (3 - 2 * t);
}
static inline void hdm_tween_target(hdm_tween* t, float target, float duration) {
    if (t->target == target) return;
    t->from = t->value; t->target = target; t->elapsed = 0; t->duration = duration;
}
static inline void hdm_tween_step(hdm_tween* t, float dt) {
    if (!hdm_finite(dt) || dt <= 0) return;
    t->elapsed += dt;
    float f = t->duration > 0 ? hdm_ease(t->elapsed / t->duration) : 1;
    t->value = t->from + (t->target - t->from) * f;
}

/* ---- rectangles: fit a box (cx, cy, w, h in layout units, +y up) into a square panel ---- */
typedef struct { float scale, cx, cy; } hdm_view;
static inline hdm_view hdm_fit(float min_x, float min_y, float max_x, float max_y, float side, float padding) {
    hdm_view v;
    float w = max_x - min_x, h = max_y - min_y, inner = side * (1 - 2 * padding);
    if (!(w > 0)) w = 1;
    if (!(h > 0)) h = 1;
    v.scale = inner / (w > h ? w : h);
    v.cx = (min_x + max_x) * 0.5f;
    v.cy = (min_y + max_y) * 0.5f;
    return v;
}
/* the GamePad's whole plan: a fixed 500-unit frame about the floor screen's root (Minimap HD's native frame).
 * A dungeon's floors share that root and are offset from each other in it, so every floor keeps its place
 * instead of being centred on its own rooms */
#define HDM_PLAN_FRAME 500.0f
static inline hdm_view hdm_plan_view(float side, float padding) {
    return hdm_fit(-HDM_PLAN_FRAME / 2, -HDM_PLAN_FRAME / 2, HDM_PLAN_FRAME / 2, HDM_PLAN_FRAME / 2, side, padding);
}
/* layout point -> panel pixel (relative to the panel's top-left) */
static inline void hdm_to_panel(const hdm_view* v, float side, float x, float y, float* px, float* py) {
    *px = side * 0.5f + (x - v->cx) * v->scale;
    *py = side * 0.5f - (y - v->cy) * v->scale;
}
#endif
