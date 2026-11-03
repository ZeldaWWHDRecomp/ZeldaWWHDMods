/* More figurine orders: Carlov takes more than the 12 figurine orders the game keeps. Option "more_figurine_orders"
 * (Picto Box), off by default.
 *
 * The game's own order list stays as it is (12 orders at most, in the save): an order beyond it waits in a list of
 * this package, one per save file, and goes to Carlov's list through his own order (submitPhoto and his flags) when a
 * day passes and his list has room. The picto is checked as Carlov checks it (getMsg's checks, with the waiting
 * orders counted as on order) and leaves the album as with any order. So the game's save only ever holds what the
 * game itself could hold; the waiting orders are the package's file.
 *
 * Carlov (daNpcMt_c::getMsg 022A2208, replaced): with his list full and the option on, a talk goes on as with room
 * ("Let's see what you've got", 0x3675, instead of 0x3654 "come back tomorrow"), and a picto he takes goes to the
 * waiting list (his own getMsg runs: its order is a no-op on a full list, hdSaveInventory_c::setEntry / addCount stop
 * at 12, and it sets his flags and messages and erases the picto as for any order). A picto of a waiting figure gets
 * his "already on order" answer (0x3677). The postboxes (postbox.c) do the same.
 *
 * A day passing (dKankyo_DayProc 025542BC, at its start, before it marks the orders ready: event 2F01 -> 3080): if
 * the figurines of his list were handed over (4080), his list is emptied as he does it before his next order; then
 * the waiting orders go to it, oldest first, while it has room, with his order flags (2F01, 3401). They are ready that
 * day, as an order of the day before. Waiting orders made or on his list meanwhile (the option was off) are dropped.
 *
 * The list follows the game's saves: it is read when a save file is loaded (SaveMgr::sync 02720D34) and written when
 * the game saves (SaveMgr::syncSlot 02721B34), as fo<file>.bin in the package's data folder, with a stamp of the save
 * file's HD data (Carlov's list, the player's name and the play log): a list whose stamp doesn't match the loaded save
 * (another game saved in that file) is not used. A new game (hdSaveHolders_c::clearScratch 0272005C) starts with
 * none. Save states keep the list with the state. With the option off the waiting orders stay in the list and in the
 * file; they go on when it is turned on again. */
#define FO_QUEUE 0x8C        /* in the HD save object: the current card events, Carlov's orders */
#define FO_SLOT_QUEUE 0x50   /* a save file's: + file * 0x14 */
#define FO_SLOT_NAME 0xA0    /* + file * 0x40: its name (+0 the UTF-16 text) */
#define FO_SLOT_LOG 0x160    /* + file * 0xDC: its play log */
#define FO_MT_MSG_TABLE 0x888
#define FO_MSG_FULL 0x3654
#define FO_MSG_MAKING 0x3675
#define FO_MSG_ON_ORDER 0x3677
#define FO_TABLE_MAKING 0x101C22B0     /* l_msg_mt_making */
#define FO_TABLE_ON_ORDER 0x101C22E8   /* l_msg_mt_already_queued (HD) */

WWHD_GAME_ORIGINAL(WWHD_ADDR_daNpcMt_getMsg_022A2208, u32, fo_mt_msg_original, (void* carlov));

static int fo_on;
static int fo_file = -1;      /* the save file in play (its last load or save), -1 none */
static qol_fo_list fo_list;   /* its waiting orders */

static void fo_option_changed(void) { fo_on = wwhd_config_bool("more_figurine_orders", 0); }

static int fo_waiting(u32 figure) { return fo_on && qol_fo_has(&fo_list, figure); }
static int fo_room(void) { return fo_on && fo_list.count < QOL_FO_MAX; }
static void fo_hold(u32 figure) {
    if (qol_fo_add(&fo_list, figure)) {
        wwhd_log_int("more orders: Carlov's list is full: waiting, the figure", (int)figure);
        wwhd_log_int("more orders: waiting orders", (int)fo_list.count);
    }
}

/* ---- the save file's stamp (its HD data as the game loads and saves it) */
static unsigned fo_mix(unsigned h, u32 at, u32 bytes) {
    for (u32 i = 0; i < bytes; i++) h = (h ^ *(volatile u8*)(at + i)) * 16777619u;
    return h;
}
static unsigned fo_stamp(int file) {
    u32 s = qol_save();
    if (!s || file < 0 || file > 2) return 0;
    u32 hd = s + QOL_HD_SAVE, f = (u32)file;
    unsigned h = fo_mix(2166136261u, hd + FO_SLOT_QUEUE + f * 0x14, 0x14);
    h = fo_mix(h, hd + FO_SLOT_LOG + f * 0xDC, 0xDC);
    u32 name = qol_ld32(hd + FO_SLOT_NAME + f * 0x40);
    if (qol_guest_ptr(name))
        for (u32 i = 0; i < 32 && *(volatile u16*)(name + i * 2); i++) h = fo_mix(h, name + i * 2, 2);
    return h;
}
static const char* fo_name(int file) {
    static char name[8] = "fo0.bin";
    name[2] = (char)('0' + file);
    return name;
}

/* ---- the save file loaded, saved, a new game */
WWHD_HOOK_RETURN(WWHD_ADDR_saveMgrSync_02720D34, qol_orders_load, (void* mgr, u32 file)) {
    if (file > 2) return;
    unsigned char rec[QOL_PG_HEAD + QOL_FO_LEN(QOL_FO_MAX) + 1];
    qol_fo_list x;
    fo_file = (int)file;
    fo_list.count = 0;
    if (!qol_fo_read(rec, wwhd_file_read(fo_name(fo_file), rec, sizeof rec), &x)) return;
    const unsigned stamp = fo_stamp(fo_file);
    if (x.stamp != stamp) {
        if (x.count) wwhd_log_int("more orders: the waiting orders are another game's (not used), save file", fo_file);
        return;
    }
    fo_list = x;
    if (x.count) wwhd_log_int("more orders: waiting orders loaded", (int)x.count);
}
WWHD_HOOK_RETURN(WWHD_ADDR_saveMgrSyncSlot_02721B34, qol_orders_save, (void* mgr, u32 file)) {
    if (file > 2) return;
    unsigned char rec[QOL_PG_HEAD + QOL_FO_LEN(QOL_FO_MAX) + 1];
    fo_file = (int)file;
    fo_list.stamp = fo_stamp(fo_file);
    if (!fo_list.count) {  /* (no file needed: only one with orders is made empty) */
        qol_fo_list old;
        if (!qol_fo_read(rec, wwhd_file_read(fo_name(fo_file), rec, sizeof rec), &old) || !old.count) return;
    }
    const unsigned bytes = qol_fo_write(rec, &fo_list);
    if (wwhd_file_write(fo_name(fo_file), rec, bytes) != (s32)bytes) wwhd_log("more orders: the waiting orders could not be written");
    else wwhd_log_int("more orders: waiting orders saved", (int)fo_list.count);
}
WWHD_HOOK(0x0272005C, qol_orders_new_game, (void* holders)) {
    fo_list.count = 0;
    fo_file = -1;
}

/* ---- a day passes: waiting orders to Carlov's list */
WWHD_HOOK(WWHD_ADDR_dKankyo_DayProc_025542BC, qol_orders_day, (void)) {
    u32 ev = qol_events();
    if (!fo_on || !fo_list.count || !ev) return;
    const int handed_over = qol_ev_is(ev, 0x4080) != 0;  /* (his list is emptied before his next order) */
    unsigned char queued[QOL_FO_CARLOV], made[QOL_FO_MAX], out[QOL_FO_CARLOV];
    unsigned n = handed_over ? 0 : qol_mt_order_count(0) & 0xFF;
    if (n > QOL_FO_CARLOV) n = QOL_FO_CARLOV;
    for (unsigned i = 0; i < n; i++) queued[i] = (unsigned char)qol_mt_order(0, i);
    for (unsigned f = 0; f < QOL_FO_MAX; f++) made[f] = qol_mt_is_made(0, f) != 0;
    qol_fo_list next = fo_list;
    const unsigned go = qol_fo_take(&next, queued, n, made, out);
    const unsigned dropped = fo_list.count - next.count - go;
    if (dropped) wwhd_log_int("more orders: waiting orders made or ordered meanwhile (left the list)", (int)dropped);
    fo_list = next;
    if (!go) return;
    if (handed_over) {
        qol_ev_off(ev, 0x3080), qol_ev_off(ev, 0x4080), qol_ev_off(ev, 0x3F01), qol_ev_off(ev, 0x4040);
        qol_mt_clear_orders(0);
    }
    for (unsigned i = 0; i < go; i++) qol_mt_submit(0, out[i]);
    qol_ev_on(ev, 0x2F01);
    qol_ev_on(ev, 0x3401);
    wwhd_log_int("more orders: a new day: waiting orders to Carlov", (int)go);
    wwhd_log_int("more orders: still waiting", (int)fo_list.count);
}

/* ---- Carlov */
WWHD_REPLACE(WWHD_ADDR_daNpcMt_getMsg_022A2208, u32, qol_orders_carlov, (void* carlov)) {
    u8* p = qol_play();
    u32 ev = qol_events();
    if (!fo_on || !p || !ev) return fo_mt_msg_original(carlov);
    volatile u32* table = (volatile u32*)((u32)carlov + FO_MT_MSG_TABLE);
    if (!p[QOL_IN_PHOTO]) {  /* a talk: with his list full he goes on as with room */
        const u32 message = fo_mt_msg_original(carlov);
        if (message != FO_MSG_FULL || !fo_room()) return message;
        *table = (u32)&WWHD_GAME_DATA(FO_TABLE_MAKING, u32);
        return FO_MSG_MAKING;
    }
    /* a picto shown: his checks, in his order, with the waiting orders on order */
    const u32 figure = qol_figure_of(p[QOL_PIC_SUBJECT]);
    const int known = p[QOL_PIC_COLOUR] && figure != QOL_NO_FIGURE && !qol_mt_is_made(0, figure);
    if (known && qol_fo_has(&fo_list, figure)) {
        *table = (u32)&WWHD_GAME_DATA(FO_TABLE_ON_ORDER, u32);
        return FO_MSG_ON_ORDER;
    }
    const int full = (qol_mt_order_count(0) & 0xFF) >= QOL_FO_CARLOV && !qol_ev_is(ev, 0x4080);
    const int ordered = known && !qol_mt_has_order(0, figure) && !(p[QOL_PIC_FLAWS] & 3);
    if (full && ordered && !fo_room()) {  /* (all 134 waiting: cannot be) */
        *table = (u32)&WWHD_GAME_DATA(FO_TABLE_ON_ORDER, u32);
        return FO_MSG_ON_ORDER;
    }
    const u32 message = fo_mt_msg_original(carlov);
    if (full && ordered) fo_hold(figure);
    return message;
}
