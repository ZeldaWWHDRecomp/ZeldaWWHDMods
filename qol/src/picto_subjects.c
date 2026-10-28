/* Picto subjects: in the Picto Box album, a label over the picto shown says whether it is good for a Nintendo
 * Gallery figurine, and if not, why (black and white, turned away or out of view, partly hidden, no gallery
 * subject). Option "picto_subjects" (Picto Box), off by default.
 *
 * The HD album (not on the GameCube): the HD save object (*dComIfG_save_info_pointer + 0x12C0) keeps four blocks of
 * 0x3C0340 bytes from +0x700: one per save file and the working album of the file in play (02720144: +0xB409C0).
 * A block: 12 slots of 0x50040 (a slot's header: +5 the shot's flaws, 1 turned away / out of view, 2 partly
 * hidden; +6 used; +7 1 a colour picto; +8 the subject, a figurine from 0x49 on), the order s8[12] at +0x3C0300
 * (-1 empty). The album viewer (the in-game UI's texture cache, *(sIngameUi + 0x1D8)) keeps its counter's number
 * (1..count) at +0x880 and the count at +0x881; the picto shown is the order's entry before the number. The album
 * is up while ZR's action is "Picto Box" (0x48).
 * Idea from the GreenNaugahyde Android fork (MPL-2.0), mods/picto_pages.cpp picto_subject; reimplemented as a
 * guest mod, with English words instead of the figurines' names. */
#define PICTO_SAVE_OBJECT 0x12C0
#define PICTO_WORKING (0x700 + 0xB409C0)
#define PICTO_SLOT 0x50040
#define PICTO_ORDER 0x3C0300
#define PICTO_SLOTS 12
#define PICTO_UI_TEXTURES 0x1D8
#define PICTO_ALBUM_ACTION 0x48
#define PICTO_FIRST_FIGURINE 0x49
#define PICTO_INGAME_UI 0x101F8344  /* sIngameUi (hdIngameUi_c*, 0 outside gameplay) */

static int picto_on;
static wwhd_hud_element picto_box, picto_text;
static const char* picto_label;
static u32 picto_label_bytes;

static void picto_option_changed(void) { picto_on = wwhd_config_bool("picto_subjects", 0); }

static u32 picto_working(void) {
    u32 info = WWHD_GAME_DATA(WWHD_ADDR_dComIfG_save_info_pointer, u32);
    return qol_guest_ptr(info) ? info + PICTO_SAVE_OBJECT + PICTO_WORKING : 0;
}

/* the header of the picto the album shows, 0 none */
static u32 picto_shown(void) {
    if (qol_zr_action() != PICTO_ALBUM_ACTION) return 0;
    u32 ui = WWHD_GAME_DATA(PICTO_INGAME_UI, u32);
    u32 textures = qol_guest_ptr(ui) ? qol_ld32(ui + PICTO_UI_TEXTURES) : 0;
    u32 album = picto_working();
    if (!qol_guest_ptr(textures) || !album) return 0;
    int count = *(volatile u8*)(textures + 0x881), pos = (int)*(volatile u8*)(textures + 0x880) - 1;
    if (count <= 0 || count > PICTO_SLOTS) return 0;
    if (pos < 0) pos += count;
    if (pos >= count) return 0;
    int slot = *(volatile s8*)(album + PICTO_ORDER + pos);
    if (slot < 0 || slot >= PICTO_SLOTS) return 0;
    u32 s = album + (u32)slot * PICTO_SLOT;
    return *(volatile u8*)(s + 6) ? s : 0;
}

#define PICTO_SET(text) (picto_label = (text), picto_label_bytes = sizeof(text) - 1)
static void picto_draw(u32 list) {
    if (!picto_on) return;
    u32 s = picto_shown();
    if (!s) return;
    const u32 subject = qol_ld32(s + 8) & 0xFF;
    const u8 flaws = *(volatile u8*)(s + 5), colour = *(volatile u8*)(s + 7) == 1;
    u32 rgba = 0xFFFFFFFF;
    if (subject < PICTO_FIRST_FIGURINE) PICTO_SET("No figurine subject");
    else if (!colour) PICTO_SET("Figurine subject, but black and white");
    else if (flaws == 1) PICTO_SET("Figurine subject, but turned away or out of view");
    else if (flaws == 2) PICTO_SET("Figurine subject, but partly hidden");
    else { PICTO_SET("Good for a figurine"); rgba = 0xFFD54AFF; }
    picto_box = (wwhd_hud_element){.kind = WWHD_HUD_RECT, .anchor = WWHD_HUD_TOP_LEFT, .x = 310, .y = 70, .w = 660, .h = 44,
                                   .thickness = 1, .u1 = 1, .v1 = 1, .rgba = 0x101820C0};
    wwhd_hud_emit(list, &picto_box);
    picto_text = (wwhd_hud_element){.kind = WWHD_HUD_TEXT, .anchor = WWHD_HUD_TOP_LEFT, .x = 330, .y = 80, .size = 24,
                                    .thickness = 1, .rgba = rgba, .text = picto_label, .text_bytes = picto_label_bytes};
    wwhd_hud_emit(list, &picto_text);
}
