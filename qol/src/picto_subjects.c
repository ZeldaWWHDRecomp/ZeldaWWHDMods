/* Picto subjects: in the Picto Box album, a label over the picto shown says whether it is good for a Nintendo
 * Gallery figurine, and if not, why (black and white, turned away or out of view, partly hidden, no gallery
 * subject). Option "picto_subjects" (Picto Box), off by default.
 *
 * The HD album (not on the GameCube): the HD save object (*dComIfG_save_info_pointer + 0x12C0) keeps four blocks of
 * 0x3C0340 bytes from +0x700: one per save file and the working album of the file in play (02720144: +0xB409C0).
 * A block: 12 slots of 0x50040 (a slot's header: +5 the shot's flaws, 1 turned away / out of view, 2 partly
 * hidden; +6 used; +7 1 a colour picto; +8 the subject (dSnap), a figurine from 0x49 on), the order s8[12] at +0x3C0300
 * (-1 empty). The album viewer (the in-game UI's texture cache, *(sIngameUi + 0x1D8)) keeps its counter's number
 * (1..count) at +0x880 and the count at +0x881; the picto shown is the order's entry before the number. The album
 * is up while ZR's action is "Picto Box" (0x48).
 * The figure is Carlov's (qol_figure_of: changePhotoNo, then the gallery's figure); a figurine subject's label names
 * it ("Good for a figurine: Sturgeon") in the game's language: the name is the first line of the figurine's message in
 * the Nintendo Gallery (daObjFigure_c::getMsg: message 0x37DD + figure), read as the game reads a numbered message
 * (hdTextUnit's putMessage 025FD6xx: a message object 025F65F4, MsgResMgr::getMessage 025F50E8 with the label
 * "%05d" in the "message*" sets, the text of its label index 0273A2E8), with the player's name for its name tag
 * (hdSaveHolders_c::getCurrentName 02720170; the game's default name when it is empty). No name found: the label
 * without one.
 * Idea from the GreenNaugahyde Android fork (MPL-2.0), mods/picto_pages.cpp picto_subject; reimplemented as a
 * guest mod. */
#define PICTO_SAVE_OBJECT 0x12C0
#define PICTO_WORKING (0x700 + 0xB409C0)
#define PICTO_SLOT 0x50040
#define PICTO_ORDER 0x3C0300
#define PICTO_SLOTS 12
#define PICTO_UI_TEXTURES 0x1D8
#define PICTO_ALBUM_ACTION 0x48
#define PICTO_INGAME_UI 0x101F8344  /* sIngameUi (hdIngameUi_c*, 0 outside gameplay) */

#define PICTO_MSG_MANAGER 0x101F4AE8  /* the message resource manager (MsgResMgr*) */
#define PICTO_SAFE_STRING 0x100E0F90  /* a sead::SafeString's vtable */
#define PICTO_DEFAULT_NAME 0x100E1300 /* the player's default name (UTF-16) */
#define PICTO_MSG_OBJECT 0x924        /* the message object (HdMsgText): +8 its set, +0x118 its label index */
#define PICTO_FIGURE_MSG 0x37DD

WWHD_GAME_FUNC(0x025F65F4, u32, picto_msg_construct, (void* message));
WWHD_GAME_FUNC(0x025F50E8, void, picto_msg_load, (u32 manager, void* message, void* set_name, void* label));
WWHD_GAME_FUNC(0x0273A2E8, u32, picto_msg_text, (u32 data, u32 index));
WWHD_GAME_FUNC(0x02720170, u32, picto_current_name, (u32 holders));

static int picto_on;
static wwhd_hud_element picto_box, picto_text;
static char picto_label[160];
static u32 picto_label_bytes, picto_label_chars;
static u8* picto_msg;               /* the message object (this mod's heap) */
static u32 picto_named = 0xFFFF;    /* the figure picto_name is of (0xFFFF: none yet) */
static unsigned long long picto_named_epoch;
static char picto_name[96];

static u32 picto_slot, picto_figure;  /* the picto shown on the last logic step and its figure */

static void picto_option_changed(void) { picto_on = wwhd_config_bool("picto_subjects", 0); }
static u32 picto_shown(void);

static u32 picto_working(void) {
    u32 info = qol_save();
    return info ? info + PICTO_SAVE_OBJECT + PICTO_WORKING : 0;
}

/* the UTF-16 text of the game's message `number` (0: none) */
static u32 picto_message(u32 number) {
    const u32 manager = WWHD_GAME_DATA(PICTO_MSG_MANAGER, u32);
    if (!qol_guest_ptr(manager)) return 0;
    if (!picto_msg && !(picto_msg = (u8*)wwhd_malloc(PICTO_MSG_OBJECT))) return 0;
    static char label[8];
    static u32 set_name[2], label_string[2];
    for (int i = 4; i >= 0; i--) label[i] = (char)('0' + number % 10), number /= 10;
    label[5] = 0;
    set_name[0] = (u32)"", set_name[1] = (u32)&WWHD_GAME_DATA(PICTO_SAFE_STRING, u32);
    label_string[0] = (u32)label, label_string[1] = set_name[1];
    picto_msg_construct(picto_msg);
    picto_msg_load(manager, picto_msg, set_name, label_string);
    const u32 set = qol_ld32((u32)picto_msg + 8), index = qol_ld32((u32)picto_msg + 0x118);
    if (!qol_guest_ptr(set) || index >= qol_ld32(set + 4)) return 0;
    const u32 text = picto_msg_text(qol_ld32(set), index);
    return qol_guest_ptr(text) ? text : 0;
}

/* the figure's name (UTF-8) into picto_name; "" when none */
static void picto_find_name(u32 figure) {
    unsigned short text[QOL_NAME_SCAN], player[32];
    unsigned n = 0, pn = 0;
    picto_name[0] = 0;
    const u32 t = picto_message(PICTO_FIGURE_MSG + figure);
    if (!t) return;
    for (; n < QOL_NAME_SCAN; n++) text[n] = *(volatile u16*)(t + n * 2);  /* (tags hold zeros: the name rule ends it) */
    const u32 save = qol_save();
    const u32 name = save ? picto_current_name(save + QOL_HD_SAVE) : 0;
    const u32 chars = qol_guest_ptr(name) ? qol_ld32(name) : 0;
    if (qol_guest_ptr(chars))
        while (pn < 31 && (player[pn] = *(volatile u16*)(chars + pn * 2))) pn++;
    if (!pn) {  /* the game's default name */
        const u32 d = (u32)&WWHD_GAME_DATA(PICTO_DEFAULT_NAME, u16);
        while (pn < 31 && (player[pn] = *(volatile u16*)(d + pn * 2))) pn++;
    }
    qol_figure_name(text, n, player, pn, picto_name, sizeof picto_name);
}

/* the picto shown, its figure and name, on the logic step (the game's functions), the label drawn from them */
static void picto_step(void) {
    if (!picto_on) return;
    const unsigned long long epoch = wwhd_hud_epoch();
    if (epoch != picto_named_epoch) picto_named_epoch = epoch, picto_named = 0xFFFF;  /* (a state loaded) */
    const u32 s = picto_shown();
    const u32 figure = s ? qol_figure_of(qol_ld32(s + 8) & 0xFF) : QOL_NO_FIGURE;
    picto_slot = s, picto_figure = figure;
    if (figure == QOL_NO_FIGURE || figure == picto_named) return;
    picto_named = figure;
    picto_find_name(figure);
    wwhd_log(picto_name[0] ? picto_name : "picto subjects: no name for the figure");
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

/* the label: a verdict and, for a figurine subject whose name is known, the name in it */
static void picto_add(const char* text) {
    while (*text && picto_label_bytes + 1 < sizeof picto_label) {
        picto_label_chars += (*text & 0xC0) != 0x80;
        picto_label[picto_label_bytes++] = *text++;
    }
}
static void picto_draw(u32 list) {
    if (!picto_on) return;
    u32 s = picto_shown();
    if (!s || s != picto_slot) return;
    const u32 figure = picto_figure;
    const u8 flaws = *(volatile u8*)(s + 5), colour = *(volatile u8*)(s + 7) == 1;
    const char* name = figure != QOL_NO_FIGURE && figure == picto_named && picto_name[0] ? picto_name : 0;
    u32 rgba = 0xFFFFFFFF;
    picto_label_bytes = picto_label_chars = 0;
    if (figure == QOL_NO_FIGURE) picto_add("No figurine subject");
    else {
        const char* why = !colour ? "black and white" : flaws == 1 ? "turned away or out of view" : flaws == 2 ? "partly hidden" : 0;
        if (!why) picto_add(name ? "Good for a figurine: " : "Good for a figurine"), rgba = 0xFFD54AFF;
        else picto_add(name ? "Figurine subject (" : "Figurine subject");
        if (name) picto_add(name);
        if (why) picto_add(name ? "), but " : ", but "), picto_add(why);
    }
    picto_label[picto_label_bytes] = 0;
    /* the box fits the text (about 0.44 of the text's height per character), centred where it is wider than 660 */
    f32 w = 44.0f + 10.5f * (f32)picto_label_chars;
    if (w < 660.0f) w = 660.0f;
    if (w > 1260.0f) w = 1260.0f;
    const f32 x = w > 660.0f ? (1280.0f - w) * 0.5f : 310.0f;
    picto_box = (wwhd_hud_element){.kind = WWHD_HUD_RECT, .anchor = WWHD_HUD_TOP_LEFT, .x = x, .y = 70, .w = w, .h = 44,
                                   .thickness = 1, .u1 = 1, .v1 = 1, .rgba = 0x101820C0};
    wwhd_hud_emit(list, &picto_box);
    picto_text = (wwhd_hud_element){.kind = WWHD_HUD_TEXT, .anchor = WWHD_HUD_TOP_LEFT, .x = x + 20, .y = 80, .size = 24,
                                    .thickness = 1, .rgba = rgba, .text = picto_label, .text_bytes = picto_label_bytes};
    wwhd_hud_emit(list, &picto_text);
}
