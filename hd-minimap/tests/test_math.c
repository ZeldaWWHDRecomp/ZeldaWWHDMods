/* Host tests of the HD minimap's pure rules (src/hd_minimap.h): cc -I src tests/test_math.c && ./a.out */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "hd_minimap.h"

static void sectors(void) {
    assert(hdm_sea_room(0, 0) == 25);                  /* D4, the centre */
    assert(hdm_sea_room(-200000, 300000) == 44);       /* Outset: B7 */
    assert(hdm_sea_room(-349999, -349999) == 1);
    assert(hdm_sea_room(349999, 349999) == 49);
    assert(hdm_sea_room(350000, 0) == 0 && hdm_sea_room(0, -350001) == 0);
    assert(hdm_sea_room(NAN, 0) == 0);
    float u, v;
    /* Outset's island art: centred 105 / 19713 units from the sector centre, 20000 units wide */
    hdm_island_uv(44, 105, 19713, -200000 + 105, 300000 + 19713, &u, &v);
    assert(fabsf(u - .5f) < 1e-4f && fabsf(v - .5f) < 1e-4f && hdm_inside(u, v));
    hdm_island_uv(44, 105, 19713, -194845, 314243, &u, &v);   /* the pier north of Link's house */
    assert(u > .7f && u < .8f && v > .2f && v < .25f);
    hdm_island_uv(44, 105, 19713, -200000, 300000, &u, &v);   /* the sector's centre is out at sea */
    assert(!hdm_inside(u, v));
}

static void location_table(void) {
    unsigned char table[4 + 49 * 8];
    memset(table, 0, sizeof table);
    table[3] = 49;
    for (int room = 1; room <= 49; ++room) {
        unsigned char* e = table + 4 + 8 * (room - 1);
        e[0] = (unsigned char)(signed char)((room - 1) % 7 - 3);
        e[1] = (unsigned char)(signed char)((room - 1) / 7 - 3);
    }
    unsigned char* outset = table + 4 + 8 * 43;
    outset[2] = 0x00; outset[3] = 0x69; outset[4] = 0x4d; outset[5] = 0x01;
    const unsigned char* e = hdm_location(table, 44);
    assert(e && hdm_s16(e + 2) == 105 && hdm_s16(e + 4) == 19713);
    assert(!hdm_location(table, 0) && !hdm_location(table, 50) && !hdm_location(0, 1));
    table[3] = 48;
    assert(!hdm_location(table, 44));          /* not the table we know: no map rather than a wrong one */
    table[3] = 49;
    outset[0] = 0;                             /* an entry for another sector */
    assert(!hdm_location(table, 44));
}

static void names(void) {
    char s[96];
    hdm_sector_texture(s, 44, 1);
    assert(!strcmp(s, "BtnMapBalloon_00/r2c_Full^t#P_DummyMapFull_00"));
    hdm_sector_texture(s, 1, 0);
    assert(!strcmp(s, "BtnMapBalloon_00/r01_b^t#P_DummyMapBig_00"));
    hdm_chart_texture(s, 44, 2);
    assert(!strcmp(s, "TreMap_00/r2c_col_2^t#P_DummyTreMap_00"));
    hdm_room_pane_key(s, 5, 7);
    assert(!strcmp(s, "layout-pane:DungeonMapFloor_05/P_Room_07"));
    hdm_room_texture(s, 5, "MapKaze1F_05^t", 5);
    assert(!strcmp(s, "DungeonMapFloor_05/MapKaze1F_05^t#P_Room_05"));
    char tag[3];
    assert(hdm_floor_tag(0x80, tag) && !strcmp(tag, "1F"));
    assert(hdm_floor_tag(0x83, tag) && !strcmp(tag, "4F"));
    assert(hdm_floor_tag(0x7F, tag) && !strcmp(tag, "B1"));
    assert(hdm_floor_tag(0x7E, tag) && !strcmp(tag, "B2"));
    assert(!hdm_floor_tag(10, tag) && !hdm_floor_tag(0x85 + 10, tag));
    assert(hdm_texture_on_floor("MapKaze1F_05^t", "1F"));
    assert(!hdm_texture_on_floor("MapKaze1F_05^t", "2F"));
    assert(hdm_texture_on_floor("MapKazeB1_03^t", "B1"));
    assert(hdm_texture_on_floor("MapKami1F_03_2F_04^t", "1F") && hdm_texture_on_floor("MapKami1F_03_2F_04^t", "2F"));
    assert(!hdm_texture_on_floor("MapKami1F_03_2F_04^t", "3F"));
    assert(!hdm_texture_on_floor("MapMajyuOD_00^t", "1F"));
    assert(hdm_dungeon_of("kaze") == 5 && hdm_dungeon_of("M_Dai") == 4 && hdm_dungeon_of("Siren") == 3);
    assert(hdm_dungeon_of("kindan") == 2 && hdm_dungeon_of("M_NewD2") == 1 && hdm_dungeon_of("majroom") == 0);
    assert(hdm_dungeon_of("sea") == -1 && hdm_dungeon_of("kaz") == -1 && hdm_dungeon_of("kazeB") == -1);
}

static void fading(void) {
    hdm_fade f = {1, 1};
    float dt = 1.0f / 30;
    hdm_fade_step(&f, 1, dt);                  /* 33 ms of input: half way out (60 ms) */
    assert(f.level > .4f && f.level < .5f);
    hdm_fade_step(&f, 1, dt);
    assert(f.level == 0);
    assert(fabsf(hdm_fade_opacity(&f, 1, .7f) - .7f) < 1e-6f);
    hdm_fade_step(&f, 0, dt); hdm_fade_step(&f, 0, dt); hdm_fade_step(&f, 0, dt);   /* 100 ms idle: still faded */
    assert(f.level < .2f);
    for (int i = 0; i < 6; ++i) hdm_fade_step(&f, 0, dt);                          /* back over 200 ms */
    assert(f.level == 1);
    assert(fabsf(hdm_fade_opacity(&f, .5f, .9f) - .5f) < 1e-6f);  /* faded is capped at the resting opacity */
    assert(hdm_moving(0, .5f, 0, 0) && hdm_moving(0, 0, -.3f, 0) && !hdm_moving(.1f, .1f, .1f, .1f));
}

static void controls(void) {
    assert(hdm_button_bit("left") == 0x0800 && hdm_button_bit("down") == 0x0100 && hdm_button_bit("l3") == 0x40000);
    assert(hdm_button_bit("none") == 0 && hdm_button_bit("") == 0);
    assert(hdm_pressed(0x0800, 0, 0x0800) && !hdm_pressed(0x0800, 0x0800, 0x0800) && !hdm_pressed(0x0800, 0, 0));
    assert(hdm_next_sea_view(0, 3) == 1 && hdm_next_sea_view(1, 3) == 2 && hdm_next_sea_view(2, 3) == 0);
    assert(hdm_next_sea_view(1, 0) == 0);      /* no chart: never the chart view */
    assert(hdm_blocked_buttons(0, 0, 1) == 0);                          /* on foot */
    assert(hdm_blocked_buttons(0, 1, 1) == (0x0800 | 0x0400));          /* sailing: the cannon and the hook */
    assert(hdm_blocked_buttons(0, 1, 0) == HDM_ALL_BUTTONS);            /* the boat in the Tower of the Gods */
    assert(hdm_blocked_buttons(1, 0, 1) == HDM_ALL_BUTTONS);            /* an event or a stage change */
    assert(!hdm_pressed(0x0800 & ~hdm_blocked_buttons(0, 1, 1), 0, 0x0800));
    assert(hdm_pressed(0x0100 & ~hdm_blocked_buttons(0, 1, 1), 0, 0x0100));
    assert(hdm_boarding_view(1, 2) == 0);      /* hidden on foot: the compasses alone */
    assert(hdm_boarding_view(0, 0) == 1 && hdm_boarding_view(0, 2) == 2);
}

static void dpad_icons(void) {
    /* sea, dungeon, on_ship, hidden, expanded, discovered, covered */
    hdm_icons d = hdm_dpad_icons(1, 0, 0, 0, 0, 1, 1);            /* on foot, island: map, expand */
    assert(d.left == HDM_ICON_MAP && d.right == HDM_ICON_EXPAND && d.down == HDM_ICON_NONE);
    d = hdm_dpad_icons(1, 0, 0, 1, 0, 1, 1);                       /* hidden: only the map at left */
    assert(d.left == HDM_ICON_MAP && d.right == HDM_ICON_NONE && d.down == HDM_ICON_NONE);
    d = hdm_dpad_icons(0, 1, 0, 0, 1, 0, 0);                       /* dungeon, expanded: reduce, the compass */
    assert(d.left == HDM_ICON_MAP && d.right == HDM_ICON_REDUCE && d.down == HDM_ICON_COMPASS);
    d = hdm_dpad_icons(1, 0, 0, 0, 0, 0, 0);                       /* undiscovered on foot */
    assert(d.left == HDM_ICON_MAP_UNKNOWN);
    d = hdm_dpad_icons(1, 0, 1, 1, 1, 1, 1);                       /* sailing: only the view, even hidden on foot */
    assert(d.left == HDM_ICON_NONE && d.right == HDM_ICON_NONE && d.down == HDM_ICON_VIEW);
    assert(hdm_dpad_icons(1, 0, 1, 0, 0, 0, 0).down == HDM_ICON_UNKNOWN);
    assert(hdm_dpad_icons(1, 0, 1, 0, 0, 1, 0).down == HDM_ICON_NONE);   /* out at sea: the compasses */
    d = hdm_dpad_icons(0, 1, 1, 0, 0, 0, 0);                       /* the boat in the Tower of the Gods */
    assert(d.left == HDM_ICON_NONE && d.right == HDM_ICON_NONE && d.down == HDM_ICON_NONE);
    d = hdm_dpad_icons(0, 0, 0, 0, 0, 0, 0);                       /* no minimap here */
    assert(d.left == HDM_ICON_NONE && d.right == HDM_ICON_NONE && d.down == HDM_ICON_NONE);
}

static void fitting(void) {
    hdm_view v = hdm_fit(-100, -50, 100, 50, 200, 0);
    float x, y;
    hdm_to_panel(&v, 200, -100, 0, &x, &y);
    assert(fabsf(x) < 1e-4f && fabsf(y - 100) < 1e-4f);
    hdm_to_panel(&v, 200, 0, 50, &x, &y);       /* +y up in the layout is up on the panel */
    assert(fabsf(x - 100) < 1e-4f && fabsf(y - 50) < 1e-4f);
    v = hdm_fit(0, 0, 100, 100, 200, .04f);
    hdm_to_panel(&v, 200, 0, 100, &x, &y);
    assert(fabsf(x - 8) < 1e-3f && fabsf(y - 8) < 1e-3f);
}

static void plan_rooms(void) {
    /* a made-up plan: room 15 on plan pane 0 (1F), room 2 on panes 1 (1F) and 2 (B1) */
    const unsigned int table[] = {0, 0x80, 15, 1, 0x80, 2, 2, 0x7F, 2};
    int floor, room;
    assert(hdm_plan_room(table, 3, 0, &floor, &room) && floor == 0x80 && room == 15);
    assert(hdm_plan_room(table, 3, 2, &floor, &room) && floor == 0x7F && room == 2);
    assert(!hdm_plan_room(table, 3, 3, &floor, &room));
    assert(!hdm_plan_room(0, 3, 0, &floor, &room) && !hdm_plan_room(table, 0, 0, &floor, &room));
    const unsigned int bad[] = {0, 0x20, 1};   /* not a floor number: not the table we know */
    assert(!hdm_plan_room(bad, 1, 0, &floor, &room));
}

static void forsaken(void) {
    assert(hdm_forsaken_turned("ma2room") && hdm_forsaken_turned("ma3room"));
    assert(!hdm_forsaken_turned("majroom") && !hdm_forsaken_turned("MajyuE") && !hdm_forsaken_turned("ma2roo"));
    float x, y, cx = HDM_FORSAKEN_CENTRE_X - 34, cy = HDM_FORSAKEN_CENTRE_Y + 4;  /* N_FloorCenter_00 at (-34, 4) */
    hdm_forsaken_place(cx / HDM_FORSAKEN_SCALE, -cy / HDM_FORSAKEN_SCALE, cx, cy, &x, &y);
    assert(fabsf(x - cx) < 1e-3f && fabsf(y - cy) < 1e-3f);  /* the centre stays */
    hdm_forsaken_place((cx + 10) / HDM_FORSAKEN_SCALE, -cy / HDM_FORSAKEN_SCALE, cx, cy, &x, &y);
    assert(fabsf(x - (cx + 4.227f)) < 1e-2f && fabsf(y - (cy - 9.063f)) < 1e-2f);  /* east turns clockwise */
    /* ma2room's 1F door between the ring's rooms 1 and 2 (world 2215 / -6117.7, DMAP origin -149 / -311.5) lands
     * where the panes P_Room_01 (95.55 / -55.25, 117 x 162.5) and P_Room_02 (108.55 / 91.65, 130 x 156) meet */
    hdm_forsaken_place(2215.0f + 149.0f, -6117.7f + 311.5f, cx, cy, &x, &y);
    assert(fabsf(x - 95.55f) <= 58.5f && fabsf(y + 55.25f) <= 81.25f);
    assert(fabsf(x - 108.55f) <= 65.0f && fabsf(y - 91.65f) <= 78.0f);
    assert(hdm_plan_heading(0x8000, 0) == (short)0x8000 && hdm_plan_heading(0x4000, 1) == 0x4000 - 0x2E38);
    assert(hdm_plan_heading(-0x7000, 1) == (short)(-0x7000 - 0x2E38 + 0x10000));  /* wraps */
}

int main(void) {
    plan_rooms();
    forsaken();
    sectors();
    location_table();
    names();
    fading();
    controls();
    dpad_icons();
    fitting();
    puts("hd-minimap math: ok");
    return 0;
}
