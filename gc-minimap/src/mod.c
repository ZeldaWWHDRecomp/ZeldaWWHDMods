/* GameCube-style minimap guest mod. No writes to game memory or embedded game data. */
#include "wwhd_guest.h"
#include "wwhd/actor.h"
#include "wwhd/functions.h"
#include "wwhd/data.h"
#include "minimap_math.h"

static struct { float x, z; s16 yaw; int room, visible; } snapshot;
static struct { float x, bottom, side, opacity; int original; } options;
static unsigned long long image_epoch;
static int cached_room, initialized;
static u32 chart, frame, marker, arrow;
static float bounds[4];
static unsigned char bounds_bytes[17];
static char path[32], style[16];
static wwhd_hud_element element;
static const float pi = 3.14159265358979323846f;

static float bounded(double value, float low, float high, float fallback) {
    if (!(value >= low && value <= high)) return fallback;
    return (float)value;
}
static void configure(void) {
    options.x = bounded(wwhd_config_float("x", 6), 0, 1200, 6);
    options.bottom = bounded(wwhd_config_float("y", 18), 0, 650, 18);
    options.side = bounded(wwhd_config_float("size", 225), 48, 600, 225);
    options.opacity = bounded(wwhd_config_float("opacity", .8), 0, 1, .8);
    if (options.x + options.side > 1280) options.x = 1280 - options.side;
    if (options.bottom + options.side > 720) options.bottom = 720 - options.side;
    wwhd_config_string("style", style, sizeof style);
    options.original = style[0]=='o' && style[1]=='r' && style[2]=='i' && style[3]=='g' &&
                       style[4]=='i' && style[5]=='n' && style[6]=='a' && style[7]=='l' && !style[8];
    initialized = 1;
}
static void room_path(int room, const char* suffix) {
    const char prefix[] = "room-";
    int i;
    for (i = 0; i < 5; ++i) path[i] = prefix[i];
    path[i++] = (char)('0' + room / 10);
    path[i++] = (char)('0' + room % 10);
    for (int j = 0; suffix[j]; ++j) path[i++] = suffix[j];
    path[i] = 0;
}
static void forget_images(void) {
    chart = frame = marker = arrow = 0;
    cached_room = 0;
}
static void prepare_chart(int room) {
    if (cached_room == room) return;
    if (chart) wwhd_hud_release(chart);
    chart = 0;
    cached_room = room; /* Missing/unverified data stays hidden until restart/epoch/sector change. */
    room_path(room, ".bounds");
    s32 count = wwhd_file_read(path, bounds_bytes, sizeof bounds_bytes);
    if (count < 0 || !minimap_decode_bounds(bounds_bytes, (u32)count, bounds)) return;
    room_path(room, options.original ? "-original.png" : "-vector.png");
    chart = wwhd_hud_texture(WWHD_HUD_DATA, path);
    if (!chart) {
        room_path(room, options.original ? "-vector.png" : "-original.png");
        chart = wwhd_hud_texture(WWHD_HUD_DATA, path);
    }
    if (!chart) return;
    if (!frame) frame = wwhd_hud_texture(WWHD_HUD_PACKAGE, "assets/frame.png");
    if (!marker) marker = wwhd_hud_texture(WWHD_HUD_PACKAGE, "assets/marker.png");
    if (!arrow) arrow = wwhd_hud_texture(WWHD_HUD_PACKAGE, "assets/arrow.png");
}
static void image(u32 list, u32 handle, float x, float y, float w, float h,
                  float rotation, float opacity) {
    if (!handle || opacity <= 0) return;
    element = (wwhd_hud_element){0};
    element.kind = WWHD_HUD_IMAGE;
    element.anchor = WWHD_HUD_TOP_LEFT;
    element.x=x; element.y=y; element.w=w; element.h=h;
    element.rotation=rotation;
    element.thickness=1;
    element.u1=element.v1=1;
    element.image=handle;
    element.rgba=0xffffff00u | (u32)(opacity * 255.0f + .5f);
    wwhd_hud_emit(list, &element);
}
static void draw(u32 list) {
    unsigned long long epoch = wwhd_hud_epoch();
    if (epoch != image_epoch) { image_epoch=epoch; forget_images(); }
    if (!snapshot.visible || options.opacity <= 0) return;
    prepare_chart(snapshot.room);
    if (!chart) return;
    minimap_point point = minimap_project(snapshot.x, snapshot.z, bounds);
    if (!point.valid) return;
    float s=options.side, x=options.x, y=720-s-options.bottom, a=options.opacity;
    image(list, chart, x+.1f*s, y+.1f*s, .8f*s, .8f*s, 0, a);
    float overlay = a < .8f ? a / .8f : 1;
    image(list, frame, x+.1f*s, y+.1f*s, .8f*s, .8f*s, 0, overlay);
    /* Game yaw zero faces +Z (map down); original marker art points up. */
    float extent=.8f*s*.078f;
    image(list, marker, x+s*(.1f+.8f*point.x)-extent*.5f,
          y+s*(.1f+.8f*point.z)-extent*.5f, extent, extent,
          pi-(float)snapshot.yaw*(2*pi/65536.0f), overlay);
    image(list, arrow, x+.45f*s, y+.02f*s, .1f*s, .05f*s, pi, overlay);
    image(list, arrow, x-.005f*s, y+.475f*s, .1f*s, .05f*s, pi*.5f, overlay);
    image(list, arrow, x+.905f*s, y+.475f*s, .1f*s, .05f*s, -pi*.5f, overlay);
}
static int is_sea(const u8* stage) {
    return stage[0]=='s' && stage[1]=='e' && stage[2]=='a' &&
           (!stage[3] || (stage[3]=='_' && (stage[4]=='T' || stage[4]=='E') && !stage[5]));
}
WWHD_HOOK_RETURN(WWHD_ADDR_daPy_Execute, capture_minimap, (fopAc_ac_c* actor)) {
    if (!initialized) configure();
    wwhd_hud_register(draw, WWHD_HUD_TV);
    snapshot.visible=0;
    if (options.opacity <= 0) return;
    u8* play=wwhd_play_get();
    if (!play || !actor || (u32)actor != *(u32*)(play+WWHD_PLAY_PLAYER_OFFSET) ||
        !is_sea(play+WWHD_PLAY_START_STAGE_NAME_OFFSET) ||
        play[WWHD_PLAY_ENABLE_NEXT_STAGE_OFFSET] || play[WWHD_PLAY_EVENT_RUNNING_OFFSET]) return;
    snapshot.x=actor->current.pos.x;
    snapshot.z=actor->current.pos.z;
    snapshot.yaw=actor->shape_angle.y;
    snapshot.room=minimap_sea_room(snapshot.x, snapshot.z);
    snapshot.visible=snapshot.room!=0;
}
