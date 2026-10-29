/* Album pages: the Picto Box album gets more pages of 12. Option "album_pages" (Picto Box), off by default.
 * In the album, L turns to the next page and ZL back (the album has no use for them); on a touch screen or with the
 * mouse, the arrows at the top of the album. Past the last page a new, empty page starts (only after a page with
 * pictos), and an empty page left at the end goes again. A picto taken while the album is full goes to a new page.
 *
 * Page 1 is always the game's own album, the only one the game saves: the other pages are files of this package
 * (wwhd_file_*, its own data folder), never part of the game's save. While another page is shown, the game's album in
 * memory holds that page; whenever the game saves (SaveMgr::syncSlot 02721B34, which copies the album in play to the
 * save file's) page 1 is put back for the save and the page shown again after, so the game's save is the same with
 * and without this package. Loading a save file (SaveMgr::sync 02720D34) shows page 1.
 *
 * The HD album (picto_subjects.c): the working album of the file in play at *dComIfG_save_info_pointer + 0x12C0 +
 * 0x700 + 0xB409C0: 12 slots of 0x50040 (a 0x40-byte header: +0 the game's checksum of the picto, +6 used, +0xC the
 * JPEG's size; then the JPEG, at most 0x50000), then a 0x40-byte block at +0x3C0300 (the order s8[12], -1 empty, +0xC
 * the count of pictos, and the album's other bytes). The album viewer makes each slot's texture again from its JPEG when its texture cache
 * entry (*(sIngameUi + 0x1D8) + slot * 0xA8) has 0 at +0x90, as the game's delete does; it keeps its counter until it
 * is opened again, so after a turn ZR goes to the Picto Box and back (the album is up while ZR says "Picto Box",
 * 0x48; in the Picto Box it says "Album", 0x49).
 *
 * Files, per save file f (0..2) and set s (a save that is not the one the pages were made with, e.g. a copied or new
 * file, starts a new set; old sets stay on disk): pi<f>a.bin / pi<f>b.bin the index (two copies written in turn),
 * pg<f>-<s>-<page>-<slot>.bin a picto slot, po<f>-<s>-<page>.bin a page's order block. Every file is a checked record
 * (rules.h): a lost or broken picto file empties that slot only; a lost index starts over with page 1, the game's.
 * Save states: the page shown comes back with the state; the other pages keep their latest files.
 * Idea from the GreenNaugahyde Android fork (MPL-2.0), mods/picto_pages.cpp; reimplemented as a guest mod with page 1
 * kept as the game's save. */
#define PG_SAVE_OBJECT 0x12C0
#define PG_WORKING (0x700 + 0xB409C0)
#define PG_SLOT 0x50040
#define PG_HEADER 0x40
#define PG_JPEG_MAX 0x50000
#define PG_ORDER 0x3C0300
#define PG_ORDER_LEN 0x40
#define PG_COUNT 0xC        /* in the order block: the pictos in the album (the Picto Box's "n / 12") */
#define PG_INGAME_UI 0x101F8344
#define PG_TEXTURES 0x1D8
#define PG_ALBUM 0x48
#define PG_FINDER 0x49
#define PG_L 0x0020u
#define PG_ZL 0x0080u
#define PG_ZR 0x0040u
#define PG_SLOT_TOUCH 4

static int pg_on;
static int pg_file = -1;        /* the save file in play (its last SaveMgr::sync), -1 none */
static qol_pg_index pg_ix;      /* its pages (pg_ix.count >= 1 once attached) */
static int pg_cur;              /* the page in the game's album now: 0 = the game's own */
static int pg_cur_used;         /* its pictos */
static unsigned char* pg_buf;   /* one record: the header and a whole slot */
static unsigned long long pg_epoch;
static int pg_request;          /* a turn asked for: -1 / +1 */
static int pg_reopen, pg_reopen_reads;  /* the viewer opened again after a turn: 1 to the Picto Box, 2, 3 back, 4 */
static u32 pg_held;             /* L / ZL held (edges turn) */
static int pg_shown, pg_claimed;
static int pg_swapped;          /* the game saves with page 1 put back for it */
static wwhd_hud_element pg_el;
static char pg_text[32];
static u32 pg_text_bytes;

static u32 pg_working(void) {
    u32 info = WWHD_GAME_DATA(WWHD_ADDR_dComIfG_save_info_pointer, u32);
    return qol_guest_ptr(info) ? info + PG_SAVE_OBJECT + PG_WORKING : 0;
}
static int pg_album_open(void) { return qol_zr_action() == PG_ALBUM; }

/* ---- names (flat names of the package's data folder) */
static char* pg_put_num(char* p, unsigned v) {
    char t[12];
    int n = 0;
    do t[n++] = (char)('0' + v % 10), v /= 10; while (v);
    while (n) *p++ = t[--n];
    return p;
}
static const char* pg_name(char kind, int page, int slot) {
    static char name[48];
    char* p = name;
    *p++ = 'p', *p++ = kind;
    p = pg_put_num(p, (unsigned)pg_file);
    if (kind == 'i') {
        *p++ = (char)slot;  /* 'a' / 'b' */
    } else {
        *p++ = '-', p = pg_put_num(p, pg_ix.set), *p++ = '-', p = pg_put_num(p, (unsigned)page);
        if (kind == 'g') *p++ = '-', p = pg_put_num(p, (unsigned)slot);
    }
    *p++ = '.', *p++ = 'b', *p++ = 'i', *p++ = 'n', *p = 0;
    return name;
}

/* ---- the album's pictos */
static int pg_used(u32 w, int slot) { return *(volatile u8*)(w + (u32)slot * PG_SLOT + 6) != 0; }
static int pg_count_used(u32 w) {
    int n = 0;
    for (int i = 0; i < 12; i++) n += pg_used(w, i);
    return n;
}
static unsigned pg_fingerprint(u32 w, unsigned* fp) {
    unsigned sums[12];
    int used[12];
    for (int i = 0; i < 12; i++) used[i] = pg_used(w, i), sums[i] = qol_ld32(w + (u32)i * PG_SLOT);
    return qol_fp_make(sums, used, fp);
}

/* ---- the index */
static int pg_write_index(void) {
    unsigned char rec[QOL_PG_HEAD + QOL_PG_INDEX_LEN];
    pg_ix.gen++;
    qol_pg_index_write(rec, &pg_ix);
    return wwhd_file_write(pg_name('i', 0, pg_ix.gen & 1 ? 'a' : 'b'), rec, sizeof rec) == (s32)sizeof rec;
}
static int pg_read_index(qol_pg_index* out) {
    unsigned char rec[QOL_PG_HEAD + QOL_PG_INDEX_LEN + 1];
    qol_pg_index a, b;
    int ga = qol_pg_index_read(rec, wwhd_file_read(pg_name('i', 0, 'a'), rec, sizeof rec), &a);
    int gb = qol_pg_index_read(rec, wwhd_file_read(pg_name('i', 0, 'b'), rec, sizeof rec), &b);
    if (ga && (!gb || a.gen > b.gen)) *out = a;
    else if (gb) *out = b;
    return ga || gb;
}

/* ---- a page to its files and back */
static int pg_buffer(void) {
    if (!pg_buf) pg_buf = (unsigned char*)wwhd_malloc(QOL_PG_HEAD + PG_SLOT);
    return pg_buf != 0;
}
/* writes the album as page `page`; with check, reads every file back (page 1 before it is left: it must come back) */
static int pg_store(u32 w, int page, int check) {
    if (!pg_buffer()) return 0;
    for (int i = 0; i < 12; i++) {
        u32 s = w + (u32)i * PG_SLOT, len = PG_HEADER;
        if (pg_used(w, i)) {
            u32 size = qol_ld32(s + 0xC);
            len += size < PG_JPEG_MAX ? size : PG_JPEG_MAX;
        }
        memcpy(pg_buf + QOL_PG_HEAD, (const void*)s, len);
        qol_record_seal(pg_buf, QOL_PG_SLOT_MAGIC, len);
        const char* name = pg_name('g', page, i);
        if (wwhd_file_write(name, pg_buf, QOL_PG_HEAD + len) != (s32)(QOL_PG_HEAD + len)) return 0;
        if (check && qol_record_check(pg_buf, wwhd_file_read(name, pg_buf, QOL_PG_HEAD + PG_SLOT), QOL_PG_SLOT_MAGIC, PG_SLOT) != (int)len) return 0;
    }
    memcpy(pg_buf + QOL_PG_HEAD, (const void*)(w + PG_ORDER), PG_ORDER_LEN);
    qol_record_seal(pg_buf, QOL_PG_ORDER_MAGIC, PG_ORDER_LEN);
    const char* name = pg_name('o', page, 0);
    if (wwhd_file_write(name, pg_buf, QOL_PG_HEAD + PG_ORDER_LEN) != (s32)(QOL_PG_HEAD + PG_ORDER_LEN)) return 0;
    return !check || qol_record_check(pg_buf, wwhd_file_read(name, pg_buf, QOL_PG_HEAD + PG_SLOT), QOL_PG_ORDER_MAGIC, PG_ORDER_LEN) == PG_ORDER_LEN;
}
/* puts page `page` into the album (a lost or broken slot file: that slot empty; a lost order: rebuilt). Returns the
 * pictos that came back, -1 when page 1 must come back whole and didn't. */
static int pg_load(u32 w, int page) {
    if (!pg_buffer()) return -1;
    int used[12], lost = 0;
    for (int i = 0; i < 12; i++) {
        u32 s = w + (u32)i * PG_SLOT;
        int len = qol_record_check(pg_buf, wwhd_file_read(pg_name('g', page, i), pg_buf, QOL_PG_HEAD + PG_SLOT), QOL_PG_SLOT_MAGIC, PG_SLOT);
        if (len < PG_HEADER) lost++, len = 0;
        memset((void*)s, 0, PG_SLOT);
        if (len) memcpy((void*)s, pg_buf + QOL_PG_HEAD, (u32)len);
        used[i] = len ? pg_used(w, i) : 0;
    }
    if (qol_record_check(pg_buf, wwhd_file_read(pg_name('o', page, 0), pg_buf, QOL_PG_HEAD + PG_SLOT), QOL_PG_ORDER_MAGIC, PG_ORDER_LEN) == PG_ORDER_LEN && !lost) {
        memcpy((void*)(w + PG_ORDER), pg_buf + QOL_PG_HEAD, PG_ORDER_LEN);
    } else {
        unsigned char order[12];
        qol_order_rebuild(used, order);
        memcpy((void*)(w + PG_ORDER), order, 12);
        int n = 0;
        for (int i = 0; i < 12; i++) n += used[i];
        *(volatile u8*)(w + PG_ORDER + PG_COUNT) = (u8)n;  /* (the album's count of pictos, as the Picto Box reads it) */
    }
    if (lost) wwhd_log_int("album pages: picto files lost (their slots are empty now)", lost);
    return pg_count_used(w);
}
/* page 1 must come back whole: its slots are checked before the album is touched */
static int pg_load_game_page(u32 w) {
    if (!pg_buffer()) return 0;
    for (int i = 0; i < 12; i++)
        if (qol_record_check(pg_buf, wwhd_file_read(pg_name('g', 0, i), pg_buf, QOL_PG_HEAD + PG_SLOT), QOL_PG_SLOT_MAGIC, PG_SLOT) < PG_HEADER) return 0;
    return pg_load(w, 0) >= 0;
}

/* the viewer makes the pictures again */
static void pg_refresh_viewer(void) {
    u32 ui = WWHD_GAME_DATA(PG_INGAME_UI, u32);
    u32 tex = qol_guest_ptr(ui) ? qol_ld32(ui + PG_TEXTURES) : 0;
    if (!qol_guest_ptr(tex)) return;
    for (u32 i = 0; i < 12; i++) *(volatile u32*)(tex + i * 0xA8 + 0x90) = 0;
}

/* the save file in play: its pages (a save that is not theirs starts a new set) */
static void pg_attach(int file) {
    u32 w = pg_working();
    pg_file = file, pg_cur = 0;
    pg_cur_used = w ? pg_count_used(w) : 0;
    qol_pg_index x;
    unsigned fp[12], n = w ? pg_fingerprint(w, fp) : 0;
    if (!pg_read_index(&x)) {
        x.gen = 0, x.set = 0, x.count = 1, x.fpn = 0;
    } else if (!qol_fp_same(x.fp, x.fpn, fp, n) && x.count > 1) {
        wwhd_log_int("album pages: this save's album is not the one its pages were made with: new set", (int)x.set + 1);
        x.set++, x.count = 1;
    }
    x.fpn = n;
    for (unsigned i = 0; i < n; i++) x.fp[i] = fp[i];
    pg_ix = x;
    wwhd_log_int("album pages: save file", file);
    wwhd_log_int("album pages: pages", (int)pg_ix.count);
}

/* turns from the page shown to page `to` (to == count: a new, empty page) */
static void pg_turn_to(int to) {
    u32 w = pg_working();
    if (!w || pg_file < 0 || to == pg_cur || to < 0 || to > (int)pg_ix.count) return;
    const int from = pg_cur, from_used = pg_count_used(w);
    if (!pg_store(w, from, from == 0)) {
        wwhd_log_int("album pages: page could not be written, no turn", from + 1);
        return;
    }
    int n;
    if (to == (int)pg_ix.count) {
        wwhd_clear_02725DA8((void*)w);
        pg_ix.count++;
        n = 0;
    } else {
        n = to == 0 ? (pg_load_game_page(w) ? pg_count_used(w) : -1) : pg_load(w, to);
        if (n < 0) {  /* page 1 lost: the page shown stays (and is page 1 for the game from now on) */
            wwhd_log("album pages: page 1 could not be read back; staying");
            pg_load(w, from);
            return;
        }
    }
    pg_cur = to, pg_cur_used = n;
    if (!from_used && from > 0 && from == (int)pg_ix.count - 1) pg_ix.count--;  /* an empty last page goes */
    pg_write_index();
    pg_refresh_viewer();
    if (pg_album_open()) pg_reopen = 1, pg_reopen_reads = 0;
    wwhd_log_int("album pages: page", pg_cur + 1);
}

static void pg_option_changed(void) {
    pg_on = wwhd_config_bool("album_pages", 0);
    if (!pg_on && pg_cur != 0) pg_turn_to(0);  /* off: the game's own album back at once */
}

/* ---- the game's save file and its saves */
WWHD_HOOK(WWHD_ADDR_saveMgrSync_02720D34, qol_pages_load_before, (void* mgr, u32 file)) {
    /* the page shown is written before the file's album replaces it */
    u32 w = pg_working();
    if (w && pg_file >= 0 && pg_cur != 0) pg_store(w, pg_cur, 0);
}
WWHD_HOOK_RETURN(WWHD_ADDR_saveMgrSync_02720D34, qol_pages_load, (void* mgr, u32 file)) {
    if (file < 3) pg_attach((int)file);  /* (reads only: the pages are ready when the option is turned on later) */
}
WWHD_HOOK(WWHD_ADDR_saveMgrSyncSlot_02721B34, qol_pages_save_before, (void* mgr, u32 file)) {
    u32 w = pg_working();
    pg_swapped = 0;
    if (!w || pg_file < 0 || pg_cur == 0) return;
    /* page 1 back for the game's save */
    if (!pg_store(w, pg_cur, 0) || !pg_load_game_page(w)) {
        wwhd_log("album pages: page 1 could not be put back for the save");
        return;
    }
    pg_swapped = 1;
}
WWHD_HOOK_RETURN(WWHD_ADDR_saveMgrSyncSlot_02721B34, qol_pages_save, (void* mgr, u32 file)) {
    u32 w = pg_working();
    if (!w || pg_file < 0) return;
    /* the game's album as saved: its fingerprint, and page 1's files (its slots' flags as the save left them) */
    pg_ix.fpn = pg_fingerprint(w, pg_ix.fp);
    if (pg_cur == 0 && pg_ix.count == 1 && !pg_on) return;  /* (nothing of this package in use) */
    pg_store(w, 0, 0);
    pg_write_index();
    if (pg_swapped) {
        pg_swapped = 0;
        if (pg_load(w, pg_cur) < 0) pg_cur = 0;
        pg_refresh_viewer();
    }
}

/* ---- a picto taken into a full album starts a new page first. hdPictographCamera::updateScale (026C1974, each frame
 * of the Picto Box's aim) takes the picto while the shutter is held (its input's hold, *0x101F5088 + 0x18, & 0x6001)
 * and nothing blocks it (*0x1018F3F4 + 0x14), if the album has room; else it says the album is full. Turned to a new
 * page before it runs, the album has room. */
#define PG_AIM_UI 0x101F5088
#define PG_AIM_OPTIONS 0x1018F3F4
WWHD_HOOK(0x026C1974, qol_pages_aim, (void* camera)) {
    if (!pg_on || pg_file < 0) return;
    u32 options = WWHD_GAME_DATA(PG_AIM_OPTIONS, u32), ui = WWHD_GAME_DATA(PG_AIM_UI, u32), w = pg_working();
    if (!w || !qol_guest_ptr(options) || !qol_guest_ptr(ui)) return;
    if (qol_ld32(options + 0x14) || !(qol_ld32(ui + 0x18) & 0x6001) || pg_count_used(w) < 12) return;
    wwhd_log("album pages: the album is full: a new page for the picto");
    pg_turn_to((int)pg_ix.count);
}

/* ---- turning: L / ZL in the album, the arrows by touch; the viewer opened again after a turn */
#define PG_BAR_Y 22.0f      /* the bar at the top of the TV picture (HUD: 1280 x 720 from the top left) */
#define PG_BAR_H 40.0f
#define PG_ARROW_W 90.0f
#define PG_BAR_X0 440.0f
#define PG_BAR_X1 840.0f

static int pg_touch(const wwhd_touch_event* e) {
    if (!pg_claimed) return 0;
    if (e->kind == WWHD_TOUCH_UP) {
        f32 x = e->x + 640.0f;  /* (layout units to the HUD's) */
        if (x < PG_BAR_X0 + PG_ARROW_W) pg_request = -1;
        else if (x > PG_BAR_X1 - PG_ARROW_W) pg_request = 1;
    }
    return 1;
}

static void pg_filter(wwhd_input_state* s) {
    unsigned long long epoch = wwhd_hud_epoch();
    if (epoch != pg_epoch) {  /* a save state loaded: the page shown is the state's; the count the files' (newer) */
        pg_epoch = epoch;
        qol_pg_index x;
        if (pg_file >= 0 && pg_read_index(&x) && x.set == pg_ix.set) {
            pg_ix.count = x.count > (unsigned)pg_cur ? x.count : (unsigned)pg_cur + 1;
            pg_ix.gen = x.gen;
        }
    }
    const int open = pg_on && pg_file >= 0 && pg_album_open();
    if (pg_reopen) {  /* ZR to the Picto Box and back: the viewer reads the album again */
        u8 zr = qol_zr_action();
        s->buttons &= ~(PG_L | PG_ZL | PG_ZR);
        if (++pg_reopen_reads > 45) pg_reopen = 0;  /* (something else came up) */
        else if (pg_reopen == 1 || pg_reopen == 3) {
            if (zr == (pg_reopen == 1 ? PG_FINDER : PG_ALBUM)) pg_reopen++, pg_reopen_reads = 0;
            else s->buttons |= PG_ZR;
        } else if (pg_reopen_reads > 2) {
            pg_reopen = pg_reopen == 2 ? 3 : 0, pg_reopen_reads = 0;
        }
        return;
    }
    if (open != pg_claimed) {
        pg_claimed = open;
        if (open) wwhd_touch_claim(PG_SLOT_TOUCH, PG_BAR_X0 - 640.0f, 360.0f - PG_BAR_Y - PG_BAR_H, PG_BAR_X1 - 640.0f, 360.0f - PG_BAR_Y);
        else wwhd_touch_claim(PG_SLOT_TOUCH, 0, 0, 0, 0);
    }
    pg_shown = open;
    if (!open) {
        pg_held = 0, pg_request = 0;
        return;
    }
    u32 down = s->buttons & (PG_L | PG_ZL) & ~pg_held;
    pg_held = s->buttons & (PG_L | PG_ZL);
    s->buttons &= ~(PG_L | PG_ZL);
    if (down & PG_L) pg_request = 1;
    if (down & PG_ZL) pg_request = -1;
    if (pg_request) {
        u32 w = pg_working();
        int to = qol_pg_turn(pg_cur, (int)pg_ix.count, pg_request, w ? pg_count_used(w) : 0);
        pg_request = 0;
        if (to >= 0) pg_turn_to(to);
    }
}

static void pg_draw(u32 list) {
    if (!pg_shown) return;
    char* p = pg_text;
    const char* a = "Page ";
    while (*a) *p++ = *a++;
    p = pg_put_num(p, (unsigned)pg_cur + 1);
    a = " of ";
    while (*a) *p++ = *a++;
    p = pg_put_num(p, pg_ix.count);
    pg_text_bytes = (u32)(p - pg_text);
    pg_el = (wwhd_hud_element){.kind = WWHD_HUD_RECT, .anchor = WWHD_HUD_TOP_LEFT, .x = PG_BAR_X0, .y = PG_BAR_Y,
                               .w = PG_BAR_X1 - PG_BAR_X0, .h = PG_BAR_H, .thickness = 1, .u1 = 1, .v1 = 1, .rgba = 0x101820C0};
    wwhd_hud_emit(list, &pg_el);
    pg_el = (wwhd_hud_element){.kind = WWHD_HUD_TEXT, .anchor = WWHD_HUD_TOP_LEFT, .x = 580, .y = PG_BAR_Y + 9, .size = 22,
                               .thickness = 1, .rgba = 0xFFFFFFFF, .text = pg_text, .text_bytes = pg_text_bytes};
    wwhd_hud_emit(list, &pg_el);
    static const char prev[] = "< ZL", next[] = "L >";
    pg_el = (wwhd_hud_element){.kind = WWHD_HUD_TEXT, .anchor = WWHD_HUD_TOP_LEFT, .x = PG_BAR_X0 + 14, .y = PG_BAR_Y + 9, .size = 22,
                               .thickness = 1, .rgba = pg_cur > 0 ? 0xFFD54AFF : 0x808080FF, .text = prev, .text_bytes = sizeof prev - 1};
    wwhd_hud_emit(list, &pg_el);
    pg_el = (wwhd_hud_element){.kind = WWHD_HUD_TEXT, .anchor = WWHD_HUD_TOP_LEFT, .x = PG_BAR_X1 - 60, .y = PG_BAR_Y + 9, .size = 22,
                               .thickness = 1, .rgba = 0xFFD54AFF, .text = next, .text_bytes = sizeof next - 1};
    wwhd_hud_emit(list, &pg_el);
}
