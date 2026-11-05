/* Dark Link: original code using the public WWHD guest SDK. */
#include "native.h"
#include "state.h"

static DLState dl_state;
static DLConfig dl_config = {100, 5, 20, 200, 0};
static daPy_lk_c* dl_owner;
static u32 dl_owner_id;
static int dl_enabled = 1, dl_touch_enabled = 1, dl_show_hud = 1;
static int dl_request, dl_combo_held, dl_auto, dl_gameplay;
static int dl_touch_x = 940, dl_touch_y = 560;
static unsigned long long dl_step;
static unsigned long long dl_revision[9];
static wwhd_hud_element dl_element;
#include "appearance_probe.h"
static void dl_texture_restore_all(void);
static void dl_aura_restore(void);

static int dl_limit(int v, int low, int high) { return v < low ? low : v > high ? high : v; }
static void dl_options(void) {
    static const char* const keys[] = {"enabled", "capacity", "hit_gain", "duration", "damage_percent",
        "free_transform", "auto_transform", "show_hud", "touch_button"};
    int changed = !dl_owner;
    for (u32 i = 0; i < sizeof keys / sizeof keys[0]; ++i) {
        unsigned long long r = wwhd_config_changed(keys[i]);
        if (r != dl_revision[i]) { dl_revision[i] = r; changed = 1; }
    }
    if (changed) {
        dl_enabled = wwhd_config_bool("enabled", 1);
        dl_config.capacity = dl_limit(wwhd_config_int("capacity", 100), 1, 1000);
        dl_config.gain = dl_limit(wwhd_config_int("hit_gain", 5), 1, 100);
        dl_config.duration = dl_limit(wwhd_config_int("duration", 20), 1, 300);
        dl_config.damage_percent = dl_limit(wwhd_config_int("damage_percent", 200), 100, 500);
        dl_config.free_transform = wwhd_config_bool("free_transform", 0);
        dl_auto = wwhd_config_bool("auto_transform", 0);
        dl_show_hud = wwhd_config_bool("show_hud", 1);
        dl_touch_enabled = wwhd_config_bool("touch_button", 1);
    }
    dl_touch_x = dl_limit(wwhd_config_int("touch_x", 940), 0, 1184);
    dl_touch_y = dl_limit(wwhd_config_int("touch_y", 560), 0, 676);
    int diagnostics = wwhd_config_bool("appearance_diagnostics", 0);
    if (diagnostics != dl_diagnostics) dl_probe = (DLAppearanceProbe){.reason=DL_PROBE_NO_DRAW};
    dl_diagnostics = diagnostics;
    if (!dl_enabled) { dl_texture_restore_all(); dl_reset(&dl_state); }
}
static int dl_alive(daPy_lk_c* link) {
    return link && link == dl_player() && WWHD_GAME_DATA(WWHD_ADDR_save_info_pointer, u32) &&
           dComIfGs_player_status_a.max_life && dComIfGs_player_status_a.life;
}
static int dl_allowed(daPy_lk_c* link) {
    u8* play = wwhd_play_get();
    return dl_enabled && dl_alive(link) && play &&
        !play[WWHD_PLAY_EVENT_RUNNING_OFFSET] && !play[WWHD_PLAY_ENABLE_NEXT_STAGE_OFFSET] &&
        !dl_menu_open() && !dl_paused();
}
#include "touch_editor.h"
static void dl_touch(const wwhd_touch_event* event) { dl_editor_touch(event); }
static void dl_filter(wwhd_input_state* input) {
    const u32 combo=0x00060000;
    int held=(input->buttons&combo)==combo;
    if(dl_editor_filter(input)) { dl_combo_held=held; return; }
    int allowed=dl_owner && dl_allowed(dl_owner);
    if(held && !dl_combo_held && allowed) dl_request=1;
    dl_combo_held=held;
    if(held && allowed) input->buttons&=~combo;
    if(!allowed) dl_request=0;
}
static void dl_rect(u32 list, f32 x, f32 y, f32 width, f32 height, u32 color) {
    dl_element = (wwhd_hud_element){.kind=WWHD_HUD_RECT, .anchor=WWHD_HUD_CENTER,
        .x=x, .y=y, .w=width, .h=height, .rgba=color, .thickness=1, .u1=1, .v1=1};
    wwhd_hud_emit(list, &dl_element);
}
static void dl_text(u32 list, f32 x, f32 y, const char* text, u32 length, u32 color, f32 size) {
    dl_element = (wwhd_hud_element){.kind=WWHD_HUD_TEXT, .anchor=WWHD_HUD_CENTER,
        .x=x, .y=y, .size=size, .rgba=color, .text=text, .text_bytes=length, .thickness=1};
    wwhd_hud_emit(list, &dl_element);
}
#include "magic_hud.h"
#include "shortcut_hud.h"
static void dl_hud(u32 list) {
    if (!dl_owner || !dl_allowed(dl_owner)) return;
    if (dl_show_hud) dl_magic_hud(list);
    if (dl_diagnostics) dl_diag_hud(list);
    dl_editor_hud(list);
}
static void dl_clear(void) {
    dl_texture_restore_all();
    dl_reset(&dl_state);
    dl_owner = 0; dl_owner_id = 0; dl_request = 0; dl_gameplay = 0;
    ee_close(&editor);
    for(u32 i=0;i<3;++i) wwhd_touch_claim(i,0,0,0,0);
    dl_claim_mask=0;
}
WWHD_HOOK(WWHD_ADDR_daPy_Execute, dl_execute, (daPy_lk_c* link)) {
    /* Register idempotently: native HUD/input handles are renewed after state loads. */
    wwhd_hud_register(dl_hud, WWHD_HUD_BOTH);
    wwhd_touch_register(dl_touch);
    wwhd_input_filter_register(dl_filter);
    dl_options();
    dl_editor_refresh();
    if (link != dl_player()) return;
    if (link != dl_owner || dl_actor_id(link) != dl_owner_id) {
        dl_texture_restore_all(); /* Discard stale owners without dereferencing their slots. */
        dl_reset(&dl_state); dl_request = 0;
        dl_owner = link; dl_owner_id = dl_actor_id(link);
        dl_step = wwhd_logic_step() - 1;
    }
    u8* play = wwhd_play_get();
    if (!dl_alive(link) || play[WWHD_PLAY_ENABLE_NEXT_STAGE_OFFSET]) {
        dl_texture_restore_all();
        dl_reset(&dl_state); dl_request = 0; dl_gameplay = 0;
        wwhd_touch_claim(0, 0, 0, 0, 0);
        return;
    }
    unsigned long long step = wwhd_logic_step();
    if (step == dl_step) return;
    dl_step = step;
    dl_gameplay = dl_allowed(link);
    int request = dl_request || (dl_auto && !dl_config.free_transform && !dl_state.active && dl_ready(&dl_state, &dl_config));
    dl_request = 0;
    /* Same maximum-magic byte and threshold used by the native gauge. Do
     * not use current magic, HUD scale, or charge capacity for this bonus. */
    u32 save=WWHD_GAME_DATA(WWHD_ADDR_save_info_pointer,u32);
    dl_sync_magic_capacity(&dl_state,*((u8*)save+0x33));
    int was_active = dl_state.active;
    dl_tick(&dl_state, &dl_config, (f32)wwhd_logic_dt(), request, dl_gameplay);
    if (!was_active && dl_state.active) dl_probe = (DLAppearanceProbe){.reason=DL_PROBE_NO_DRAW};
    if (!dl_state.active) dl_texture_restore_all();
}
WWHD_HOOK(WWHD_ADDR_daPy_Delete, dl_delete, (daPy_lk_c* link)) {
    if (link == dl_owner) dl_clear();
}

/* Scale the freshly computed damage, never the persistent collider power. This
 * preserves native hit processing, invulnerability, drops and death sequencing. */
WWHD_HOOK_RETURN(DL_AT_POWER, dl_attack_power, (DLAttack* hit)) {
    if (dl_enabled && dl_gameplay && dl_state.active && dl_owner == dl_player() && dl_sword(hit, dl_owner)) {
        hit->damage = (u8)dl_damage(hit->damage, dl_config.damage_percent);
        /* Native cc_at_check subtracts twice for this spin state. Keep the sum
         * within signed-byte range so even extreme settings cannot wrap HP. */
        u8 cut = dl_owner->bytes[0x3ac];
        if ((cut == 8 || cut == 9) && dl_owner->bytes[0x69e8] && hit->damage < 128 && hit->damage > 63)
            hit->damage = 63;
    }
}
typedef struct { fopAc_ac_c* target; DLAttack* hit; s8 health; int eligible; } DLHitSave;
static DLHitSave dl_hits[8];
static u32 dl_hit_depth;
WWHD_HOOK(DL_CC_CHECK, dl_hit_before, (fopAc_ac_c* target, DLAttack* hit)) {
    u32 depth = dl_hit_depth++;
    if (depth >= 8) return;
    dl_hits[depth] = (DLHitSave){target, hit, target ? target->health : 0,
        dl_enabled && dl_gameplay && !dl_state.active && target && target->group == 2};
}
WWHD_HOOK_RETURN(DL_CC_CHECK, dl_hit_after, (fopAc_ac_c* target, DLAttack* hit)) {
    if (!dl_hit_depth) return;
    u32 depth = --dl_hit_depth;
    if (depth >= 8) return;
    DLHitSave* before = &dl_hits[depth];
    if (before->eligible && before->target == target && before->hit == hit && before->health > 0 &&
        target->health < before->health && dl_owner == dl_player() && dl_sword(hit, dl_owner))
        dl_hit(&dl_state, &dl_config, 1);
}

#include "visuals.h"
#include "textures.h"
#include "aura.h"

#include "draw_scope.h"
