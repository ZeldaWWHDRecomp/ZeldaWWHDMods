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
    assert(hdm_room_on_floor(0x80, 0x81, "MapKami1F_03_2F_04^t", "2F"));
    assert(!hdm_room_on_floor(0x80, 0x82, "MapKami1F_03_2F_04^t", "3F"));
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
    assert(hdm_blocked_buttons(0, 1, 0) == (0x0800 | 0x0400));            /* the boat in the Tower of the Gods */
    assert(hdm_blocked_buttons(1, 0, 1) == HDM_ALL_BUTTONS);            /* an event or a stage change */
    assert(!hdm_pressed(0x0800 & ~hdm_blocked_buttons(0, 1, 1), 0, 0x0800));
    assert(hdm_pressed(0x0100 & ~hdm_blocked_buttons(0, 1, 1), 0, 0x0100));
    assert(hdm_initial_marine_view(0, 1, 2) == 0);
    assert(hdm_initial_marine_view(0, 0, 0) == 1);
    assert(hdm_initial_marine_view(1, 1, 2) == 2);
    assert(hdm_initial_marine_view(1, 0, 0) == 0);
    hdm_choice v = {0, 0, 1};
    hdm_cycle_dungeon(&v); assert(!v.hidden && v.expanded && v.zoom);
    hdm_cycle_dungeon(&v); assert(v.hidden && v.expanded && v.zoom);
    hdm_cycle_dungeon(&v); assert(!v.hidden && !v.expanded && v.zoom);
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
    assert(hdm_dpad_icons(1, 0, 1, 0, 0, 0, 1).down == HDM_ICON_UNKNOWN);
    assert(hdm_dpad_icons(1, 0, 1, 0, 0, 1, 0).down == HDM_ICON_NONE);   /* out at sea: the compasses */
    d = hdm_dpad_icons(0, 1, 1, 0, 0, 0, 0);                       /* the boat in the Tower of the Gods */
    assert(d.left == HDM_ICON_NONE && d.right == HDM_ICON_NONE && d.down == HDM_ICON_VIEW);
    d = hdm_dpad_icons(0, 0, 0, 0, 0, 0, 0);                       /* no minimap here */
    assert(d.left == HDM_ICON_NONE && d.right == HDM_ICON_NONE && d.down == HDM_ICON_NONE);
}

static void turning(void) {
    assert(hdm_turn_heading(0, 0x1000, 1, 0x4000) == 0x1000);      /* turned with Link */
    assert(hdm_turn_heading(1, 0x1000, 1, 0x4000) == 0x4000);      /* with the camera */
    assert(hdm_turn_heading(1, 0x1000, 0, 0x4000) == 0x1000);      /* no camera: Link's heading */
    assert(hdm_heading_on_turned(0x1234, 0x1234) == (short)0x8000);   /* the heading turned up is drawn up */
    assert(hdm_heading_on_turned(0x4000, 0) == (short)0xC000);
    assert(hdm_heading_on_turned(0x7000, -0x7000) == 0x6000);         /* wraps around a turn */
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
    /* the whole plan: the root is the panel's centre whatever the floor, and the frame's corner lands on the
     * padding; Forsaken Fortress's 1F (rooms centred 43.5 units up) stays above the centre, as on the GamePad */
    v = hdm_plan_view(200, .04f);
    hdm_to_panel(&v, 200, 0, 0, &x, &y);
    assert(fabsf(x - 100) < 1e-4f && fabsf(y - 100) < 1e-4f);
    hdm_to_panel(&v, 200, -HDM_PLAN_FRAME / 2, HDM_PLAN_FRAME / 2, &x, &y);
    assert(fabsf(x - 8) < 1e-3f && fabsf(y - 8) < 1e-3f);
    hdm_to_panel(&v, 200, -0.6f, 43.5f, &x, &y);
    assert(y < 100 - 15);
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

static void forsaken_outside(void) {
    /* the outdoor plan on foot on the fortress's island in sector 1; the island aboard, off it or elsewhere */
    assert(hdm_forsaken_outside(1, 0, 1));
    assert(!hdm_forsaken_outside(1, 1, 1) && !hdm_forsaken_outside(1, 0, 0) && !hdm_forsaken_outside(2, 0, 1));
    float x, y, cx = HDM_OUTDOOR_CENTRE_X - 24, cy = HDM_OUTDOOR_CENTRE_Y - 15;  /* N_OutdoorCenter_00 at (-24, -15) */
    hdm_turned_place(cx / HDM_OUTDOOR_SCALE, -cy / HDM_OUTDOOR_SCALE, HDM_OUTDOOR_SCALE, cx, cy, &x, &y);
    assert(fabsf(x - cx) < 1e-3f && fabsf(y - cy) < 1e-3f);  /* the centre stays */
    /* the middle of room 1's map box (its 2DMA: -308452 / -307806 to -293546 / -292900) is its origin and lands
     * near the middle of P_Outdoor_00 (0 / 0, 507 x 507); the box's 14906 units span about its width */
    hdm_turned_place((-308452.0f - 293546.0f) / 2 - HDM_OUTDOOR_ORIGIN_X, (-307806.0f - 292900.0f) / 2 - HDM_OUTDOOR_ORIGIN_Z,
                     HDM_OUTDOOR_SCALE, cx, cy, &x, &y);
    assert(fabsf(x) < 40 && fabsf(y) < 40);
    assert(fabsf(14906.0f * HDM_OUTDOOR_SCALE - 460.5f) < 1);
    /* the same turn as the rooms': forsaken_place is turned_place at the first visit's scale */
    float fx, fy;
    hdm_forsaken_place(1000, 2000, 3, 4, &fx, &fy);
    hdm_turned_place(1000, 2000, HDM_FORSAKEN_SCALE, 3, 4, &x, &y);
    assert(fx == x && fy == y);
    /* a searchlight stops with its switch (101..103 in room 1) or with the event bit for all three */
    assert(!hdm_searchlight_stopped(0, 0) && hdm_searchlight_stopped(0, 1) && hdm_searchlight_stopped(1, 0));
    assert(HDM_SEARCHLIGHT_SWITCH + HDM_SEARCHLIGHTS - 1 == 103);
}

static void contexts(void) {
    /* Every Fortress foot/marine handoff: a different context cannot overwrite
     * the hidden/compact/expanded dungeon choice or its orientation. */
    for (int foot_state = 0; foot_state < 3; ++foot_state)
        for (int marine = 0; marine < 3; ++marine)
            for (int zoom = 0; zoom < 2; ++zoom) {
                hdm_choice foot = {0, 0, 0}, dungeon = {foot_state == 0, foot_state == 2, zoom};
                hdm_choice before = dungeon;
                hdm_choice now = hdm_change_context(&foot, &dungeon, 1, 0, before);
                now.hidden = marine == 0; /* independent island choice */
                now = hdm_change_context(&foot, &dungeon, 0, 1, now);
                assert(now.hidden == before.hidden && now.expanded == before.expanded && now.zoom == before.zoom);
                /* Boat/foot within the same dungeon preserves everything. */
                now = hdm_change_context(&foot, &dungeon, 1, 1, now);
                assert(now.hidden == before.hidden && now.expanded == before.expanded && now.zoom == zoom);
            }
    /* Link, Medli and Makar use the same altitude policy, including glide.
     * Oscillation inside the +/-32 margin cannot change the accepted floor. */
    for (int role = 0; role < 3; ++role) {
        int floor = 0x80;
        for (int i = 0; i < 120; ++i) {
            floor = hdm_stable_floor(floor, i & 1 ? 0x80 : 0x81, 0x80, 0x81);
            assert(floor == 0x80);
        }
        floor = hdm_stable_floor(floor, 0x81, 0x81, 0x81);
        assert(floor == 0x81);
    }
}

static void captions(void) {
    assert(hdm_known_build("USA") && hdm_known_build("EU"));
    assert(!hdm_known_build(0) && !hdm_known_build("") && !hdm_known_build("JP") && !hdm_known_build("USA2"));
    char out[64];
    const unsigned short plain[] = {'T','r','e','a','s','u','r','e',' ', 'C','h','a','r','t',' ', '1',0};
    assert(hdm_message_name(plain, sizeof plain / 2, out, sizeof out) == 16);
    assert(!strcmp(out, "Treasure Chart 1"));
    const unsigned short tagged[] = {0xe,0,3,2,0,'C','a','r','t','e',' ',0xe9,0xd83d,0xddfa,0,'x'};
    assert(hdm_message_name(tagged, sizeof tagged / 2, out, sizeof out) > 0);
    assert(!strcmp(out, "Carte \xc3\xa9\xf0\x9f\x97\xba"));
    const unsigned short bad[] = {'A',0xe,0,1,0xffff};
    assert(hdm_message_name(bad, sizeof bad / 2, out, sizeof out) == 1 && !strcmp(out, "A"));
    const unsigned short truncated[] = {0xe,0};
    assert(hdm_message_name(truncated, 2, out, sizeof out) == 0 && !out[0]);
    const unsigned short lines[] = {'A','\n','B',0};
    assert(hdm_message_name(lines, 4, out, sizeof out) == 1 && !strcmp(out,"A"));
    const unsigned short accent[] = {0xe9,0};
    assert(hdm_message_name(accent, 2, out, 2) == 0 && !out[0]);
    assert(hdm_message_name(accent, 2, out, 3) == 2 && !strcmp(out,"\xc3\xa9"));
    assert(hdm_message_name(plain, 1, out, 0) == 0);
}

static void animation(void) {
    assert(hdm_ease(-1) == 0 && hdm_ease(2) == 1 && hdm_ease(.5f) == .5f);
    for (int fps = 30; fps <= 120; fps *= 2) {
        hdm_tween t = {0};
        hdm_tween_target(&t, 0, HDM_EXIT_SECONDS); /* hidden to hidden */
        hdm_tween_step(&t, 1.0f / fps); assert(t.value == 0);
        hdm_tween_target(&t, 1, HDM_ENTER_SECONDS);
        for (int i = 0; i < fps / 10; ++i) hdm_tween_step(&t, 1.0f / fps);
        assert(fabsf(t.value - .5f) < 1e-5f);
        float before = t.value;
        hdm_tween_target(&t, 0, HDM_EXIT_SECONDS);
        assert(t.value == before); /* reversal starts at actual fraction */
        hdm_tween_step(&t, .125f); assert(fabsf(t.value - before * .5f) < 1e-5f);
        hdm_tween_step(&t, .125f); assert(t.value == 0);
        hdm_tween_step(&t, NAN); assert(t.value == 0);
    }
    hdm_tween size = {300, 300, 300, 0, 0};
    hdm_tween_target(&size, 190, HDM_ENTER_SECONDS);
    hdm_tween_step(&size, .1f); assert(size.value == 245);
    hdm_tween_target(&size, 300, HDM_ENTER_SECONDS); assert(size.value == 245);
    hdm_tween_step(&size, .2f); assert(size.value == 300);
}

static void sdk_integration(void){
    hdm_chart_sync s={-1,-1};int cancel;
    assert(hdm_chart_observe(&s,3,&cancel)==3&&!cancel);
    s.pending=4;assert(hdm_chart_observe(&s,3,&cancel)==-1&&!cancel&&s.pending==4);
    assert(hdm_chart_observe(&s,4,&cancel)==4&&!cancel&&s.pending==-1);
    s.pending=6;assert(hdm_chart_observe(&s,5,&cancel)==5&&cancel&&s.pending==-1);
    s.pending=8;assert(hdm_chart_observe(&s,-2,&cancel)==-1&&cancel&&s.pending==-1);
    assert(hdm_chart_observe(&s,7,&cancel)==7&&!cancel);
    s.pending=8;assert(hdm_chart_cancel(&s)&&s.observed==7);assert(!hdm_chart_cancel(&s));
    assert(hdm_chart_observe(&s,50,&cancel)==-1&&!cancel);
    assert(hdm_compass_wanted(1,1,.1f,0,1)); // first entry and finishing exit
    assert(hdm_compass_wanted(1,1,0,.1f,1)); // independent treasure tail
    assert(!hdm_compass_wanted(1,1,0,0,1)); // release exactly once both finish
    assert(!hdm_compass_wanted(0,1,1,1,1)); // disembark
    assert(!hdm_compass_wanted(1,0,1,1,1)); // visibility revoked
    assert(!hdm_compass_wanted(1,1,1,1,0));
    assert(!hdm_map_requested(0,1,1,1,0,0)); // hidden boat, including unknown hint
    assert(hdm_map_requested(0,1,1,1,1,1));
    assert(!hdm_map_requested(0,1,0,1,1,0)); // coverage loss, keep choice elsewhere
    assert(!hdm_map_requested(1,0,0,0,1,1));
    assert(hdm_map_requested(1,0,0,0,0,0));
    // Renew through the 250ms panel/chart exit; the host owns the next 200ms.
    for(int hz=30;hz<=120;hz*=2){
        hdm_tween panel={1,1,1,0,0},chart=panel;
        hdm_tween_target(&panel,0,HDM_EXIT_SECONDS);hdm_tween_target(&chart,0,HDM_EXIT_SECONDS);
        float dt=1.f/hz,elapsed=0;int held=1,releases=0;
        for(int i=0;i<hz;i++){
            hdm_tween_step(&panel,dt);hdm_tween_step(&chart,dt);elapsed+=dt;
            int wanted=hdm_compass_wanted(1,1,panel.value,chart.value,1);
            if(held&&!wanted){releases++;assert(elapsed>=.249f&&elapsed<.251f+dt);}
            held=wanted;
        }
        assert(releases==1);
    }
}

static void chart_mark(void) {
    /* the art stays centred; only the X moves by (spot - centre) * side / world, y down like the sea's z */
    float dx, dy;
    const short left_below[4] = {1000, -2000, -24000, 25000};
    assert(hdm_chart_mark(left_below, 470, 100000, &dx, &dy));
    assert(fabsf(dx - (-25000 * 470 / 100000.0f)) < 1e-3f && dx < 0);
    assert(fabsf(dy - (27000 * 470 / 100000.0f)) < 1e-3f && dy > 0);
    const short centred[4] = {-3000, 4000, -3000, 4000};
    assert(hdm_chart_mark(centred, 470, 100000, &dx, &dy) && dx == 0 && dy == 0);
    /* an untrusted world size leaves the X in the centre */
    assert(!hdm_chart_mark(left_below, 470, 0, &dx, &dy) && dx == 0 && dy == 0);
    assert(!hdm_chart_mark(left_below, 470, 2e6f, &dx, &dy) && dx == 0 && dy == 0);
    assert(!hdm_chart_mark(left_below, 0, 100000, &dx, &dy) && dx == 0 && dy == 0);
}

static void hud_entry(void) {
    /* the D-pad's chain after a save load (log of 2026-11-02): State, InOut, N_All_00 (waiting at 0), parents */
    const unsigned char waiting_global[] = {255, 255, 0, 255, 255, 255, 255, 255};
    const unsigned char shown[] = {3, 3, 0x13, 1, 3, 1, 1, 1};
    assert(hdm_hud_alpha(waiting_global, shown, 8) == 0);
    const unsigned char coming_global[] = {0x1B, 0x1B, 0x1B, 255, 255, 255, 255, 255};
    assert(fabsf(hdm_hud_alpha(coming_global, shown, 8) - 0x1B / 255.0f) < 1e-6f);
    const unsigned char in_global[] = {255, 255, 255, 255, 255, 255, 255, 255};
    assert(hdm_hud_alpha(in_global, shown, 8) == 1);
    const unsigned char hidden[] = {3, 3, 3, 0, 3, 1, 1, 1};   /* a hidden parent */
    assert(hdm_hud_alpha(in_global, hidden, 8) == 0);
    assert(hdm_hud_alpha(in_global, shown, 0) == 0);
    /* the panel's entrance waits while the HUD is found and not in; never held without the HUD */
    assert(hdm_hud_waiting(1, 1, 0));
    assert(!hdm_hud_waiting(1, 1, 0x1B / 255.0f));
    assert(!hdm_hud_waiting(1, 0, 0));
    assert(!hdm_hud_waiting(0, 1, 0));   /* once in, the HUD's later fades don't hold the map */
    /* a D-pad that stays hidden (Medli or Makar controlled) holds the map only for a bounded wait */
    int waiting = 1, steps = 0;
    while (waiting && steps < 1000) { waiting = hdm_hud_waiting(waiting, 1, 0); ++steps; }
    assert(!waiting && steps == HDM_HUD_WAIT_STEPS);
    /* the HUD coming in on the last waiting step still ends the wait */
    assert(hdm_hud_waiting(HDM_HUD_WAIT_STEPS - 1, 1, 0) == HDM_HUD_WAIT_STEPS);
    assert(!hdm_hud_waiting(HDM_HUD_WAIT_STEPS - 1, 1, .5f));
}
static void sailing(void) {
    float left, low;
    /* replacing the compasses: the on-foot corner, whatever the compass rectangle does */
    hdm_panel_corner(1, 1, 24, 30, &left, &low);
    assert(left == 24 && low == 30);
    /* aboard without the lease: above the native compasses; on foot: the corner */
    hdm_panel_corner(0, 1, 24, 30, &left, &low);
    assert(left == 24 && low == 140);
    hdm_panel_corner(0, 0, 24, 30, &left, &low);
    assert(left == 24 && low == 30);
    /* the chart's title: centred over the chart, just above it, with the native proportions */
    hdm_chart_title t = hdm_chart_title_box(100, 200, 200);
    assert(fabsf(t.w - 188) < 1e-3f && fabsf(t.h - 188 * 47.0f / 312.0f) < 1e-3f);
    assert(fabsf(t.x + t.w / 2 - 200) < 1e-3f);
    assert(fabsf(t.y + t.h - (200 - 6)) < 1e-3f);
    assert(t.shadow_y > t.y && t.shadow_y < t.y + t.h && fabsf(t.shadow_h / t.h - 25.0f / 47.0f) < 1e-5f);
    /* a short name keeps 0.75 x the ribbon's height; a long one shrinks to fit the 296/312 box */
    assert(fabsf(hdm_chart_title_size(&t, 2) - t.h * .75f) < 1e-4f);
    float long_size = hdm_chart_title_size(&t, 60);
    assert(long_size < t.h * .75f && fabsf(60 * long_size * HDM_TITLE_ADVANCE - t.w * 296.0f / 312.0f) < 1e-2f);
    assert(fabsf(hdm_chart_title_size(&t, 0) - t.h * .75f) < 1e-4f);
}
static void stairs(void) {
    char key[64];
    hdm_stair_pane_key(key, 1, "2F", 1, 0);
    assert(!strcmp(key, "layout-pane:DungeonMapFloor_01/N_2FUp_00"));
    hdm_stair_pane_key(key, 4, "B1", 0, 1);
    assert(!strcmp(key, "layout-pane:DungeonMapFloor_04/N_B1Down_01"));
    assert(strlen(key) < 64);
    /* the floor each marker leads to: the next one up or down; Forsaken Fortress's up leads outside */
    int chars;
    const char* label;
    label = hdm_stair_label(4, 0x80, 0, &chars);
    assert(label && chars == 2 && !strcmp(label, "B1"));
    label = hdm_stair_label(4, 0x7f, 1, &chars);
    assert(label && chars == 2 && !strcmp(label, "1F"));
    label = hdm_stair_label(4, 0x7f, 0, &chars);
    assert(label && chars == 2 && !strcmp(label, "B2"));
    label = hdm_stair_label(1, 0x81, 1, &chars);
    assert(label && chars == 2 && !strcmp(label, "3F"));
    label = hdm_stair_label(0, 0x80, 1, &chars);
    assert(label && chars == 4 && !strcmp(label, "Ext."));
    /* past the last floor there is no label */
    assert(!hdm_stair_label(4, 0x88, 1, &chars) && chars == 0);
    assert(!hdm_stair_label(4, 0x78, 0, &chars) && chars == 0);
    /* the constant labels are the floor tags of the art names, on every floor */
    for (int floor = 0x77; floor <= 0x89; ++floor)
        for (int up = 0; up < 2; ++up) {
            char tag[3];
            int has = hdm_floor_tag(floor + (up ? 1 : -1), tag);
            label = hdm_stair_label(4, floor, up, &chars);
            assert(has ? label && chars == 2 && !strcmp(label, tag) : !label && !chars);
        }
}
static void retries(void) {
    /* a failed picture is tried again a second later, a few times, then left alone until reset */
    hdm_retry r = {0};
    assert(hdm_retry_due(&r, 0));
    unsigned long long step = 100;
    int loads = 0;
    for (int frame = 0; frame < 1000; ++frame, ++step)
        if (hdm_retry_due(&r, step)) { ++loads; hdm_retry_failed(&r, step); }
    assert(loads == HDM_RETRY_TRIES);
    /* spaced out: not on the very next step */
    hdm_retry s = {0};
    hdm_retry_failed(&s, 10);
    assert(!hdm_retry_due(&s, 11) && !hdm_retry_due(&s, 10 + HDM_RETRY_STEPS - 1) && hdm_retry_due(&s, 10 + HDM_RETRY_STEPS));
    /* a reset (new chart, released images, state load) starts over */
    r = (hdm_retry){0};
    assert(hdm_retry_due(&r, step));
}
static void hud_follow(void) {
    /* the HUD's hearts decide: aiming with the HUD up keeps the map; a hidden HUD hides it */
    assert(hdm_pane_shown(1, 255) && !hdm_pane_shown(0, 255) && !hdm_pane_shown(1, 0) && hdm_pane_shown(3, 1));
    assert(!hdm_hud_hides_map(1, 1) && !hdm_hud_hides_map(1, 0));
    assert(hdm_hud_hides_map(0, 0) && hdm_hud_hides_map(0, 1));
    /* without a readable HUD, Link's aiming / first-person / telescope flags */
    assert(hdm_hud_hides_map(-1, 1) && !hdm_hud_hides_map(-1, 0));
    /* aiming blocks the map's buttons */
    assert(hdm_blocked_buttons(1, 0, 0) == HDM_ALL_BUTTONS);
}
static void hit_stop(void) {
    /* shown: Link's step ran this step or the one before */
    assert(!hdm_presentation_hidden(1, 0, 0, 10, 10));
    assert(!hdm_presentation_hidden(1, 0, 0, 11, 10));
    /* a menu or a missed step hides it */
    assert(hdm_presentation_hidden(1, 1, 0, 10, 10));
    assert(hdm_presentation_hidden(1, 0, 0, 12, 10));
    assert(hdm_presentation_hidden(0, 0, 0, 10, 10));
    /* hit-stop: paused, Link's step missed, but the last frame stays */
    assert(!hdm_presentation_hidden(1, 1, 1, 14, 10));
    /* nothing to keep when there was no frame */
    assert(hdm_presentation_hidden(0, 1, 1, 14, 10));
}

int main(void) {
    chart_mark();
    hud_entry();
    hit_stop();
    hud_follow();
    retries();
    stairs();
    sailing();
    captions();
    animation();
    contexts();
    assert(hdm_stable_floor(0x80, 0x81, 0x80, 0x81) == 0x80);
    assert(hdm_stable_floor(0x80, 0x81, 0x81, 0x81) == 0x81);
    assert(hdm_stable_floor(0x81, 0x80, 0x80, 0x81) == 0x81);
    assert(hdm_stable_floor(0x81, 0x80, 0x80, 0x80) == 0x80);
    assert(hdm_stable_floor(-1, 0x80, 0x7f, 0x80) == 0x80);
    assert(hdm_stable_floor(0x80, 10, 10, 10) == 0x80);
    assert(hdm_chart_message(-1) == 0 && hdm_chart_message(49) == 0);
    assert(hdm_chart_message(0) == 0x31d8 && hdm_chart_message(2) == 0x31da);
    assert(hdm_chart_message(3) == 0x31e0 && hdm_chart_message(35) == 0x3200);
    assert(hdm_chart_message(36) == 0x3715 && hdm_chart_message(43) == 0x371c);
    assert(hdm_chart_message(44) == 0x3729 && hdm_chart_message(48) == 0x372d);
    sdk_integration();
    plan_rooms();
    forsaken();
    forsaken_outside();
    sectors();
    location_table();
    names();
    fading();
    controls();
    dpad_icons();
    turning();
    fitting();
    puts("hd-minimap math: ok");
    return 0;
}
