/* Mail pictos at postboxes: show a postbox of the Island Postal Service the Picto Box (its item button while facing
 * it), choose a picto in the game's own chooser, and the postbox takes the figurine order Carlov would take at the
 * Nintendo Gallery. Option "postbox_pictos" (Picto Box), off by default.
 *
 * The order is Carlov's own: the same checks with his functions (a colour picto, a gallery subject, not made yet,
 * not on order, face and body seen; HD keeps up to 12 orders), the same order list and event bits, and the picto
 * leaves the album as it does when he takes it. So the game's save holds exactly what handing the picto to Carlov
 * gives; nothing of this package is saved anywhere. The figurine is made by the next day, as usual: Carlov hands it
 * over at the gallery.
 *
 * The game's picto handover (daPy / d_event photoCheck): the Picto Box shown to an actor whose attention flags
 * (fopAc +0x39C) have 0x01000000 opens the album chooser, then a talk with mbInPhoto set (game info +0x52B2), the picto's
 * dSnap result at +0x5BE6 (subject), +0x5BE7 (flaws: 1 face, 2 body), +0x5BEA (colour), +0x5BEB (its album slot). The
 * postbox (daObjTpost: execute 023A4BB0) gets that flag while the option is on; its getMsg (023A5570) answers a handed
 * picto with daNpcMt_c::getMsg's checks and order (022A2208); next_msgStatus (023A55BC) then says "accepted for
 * delivery" (0xCF2, no postage) and ends, erasing the picto as Carlov's message chain does (eraseAndNotify 02725E0C,
 * play mItemPictureNumCount -1). Refused pictos come back out of the postbox ("This item cannot be delivered",
 * 0xCE9; setAnm putout, as for a letter it can't take).
 * Idea from the GreenNaugahyde Android fork (MPL-2.0), mods/postbox.cpp; reimplemented as a guest mod (no "same day
 * delivery": the game's own wait stays). */
#define PB_ATTN 0x39C
#define PB_ATTN_PHOTO 0x01000000u
#define PB_IN_PHOTO 0x52B2
#define PB_SUBJECT 0x5BE6
#define PB_FLAWS 0x5BE7
#define PB_COLOUR 0x5BEA
#define PB_SLOT 0x5BEB
#define PB_PICTURE_COUNT (0x12A0 + 0x48C6)  /* play mItemPictureNumCount (s16) */
#define PB_PUTOUT_DONE 0xA02                /* daObjTpost field_0x8EA: the item comes back out */
#define PB_MSG_ACCEPTED 0xCE8
#define PB_MSG_REFUSED 0xCE9
#define PB_MSG_THANKS 0xCF2

WWHD_GAME_FUNC(0x022A1F1C, u32, pb_change_photo_no, (u32 photo));
WWHD_GAME_FUNC(0x025BD600, u32, pb_photo_figure, (u32 photo));
WWHD_GAME_FUNC(0x0229FE28, u32, pb_is_made, (void* carlov, u32 figure));
WWHD_GAME_FUNC(0x022A1F64, u32, pb_has_order, (void* carlov, u32 figure));
WWHD_GAME_FUNC(0x022A037C, u32, pb_order_count, (void* carlov));
WWHD_GAME_FUNC(0x022A2008, void, pb_submit, (void* carlov, u32 figure));
WWHD_GAME_FUNC(0x0229FF54, void, pb_clear_orders, (void* carlov));
WWHD_GAME_FUNC(0x025B8B94, u32, pb_ev_is, (u32 events, u32 bit));
WWHD_GAME_FUNC(0x025B8B68, void, pb_ev_on, (u32 events, u32 bit));
WWHD_GAME_FUNC(0x025B8B7C, void, pb_ev_off, (u32 events, u32 bit));
WWHD_GAME_ORIGINAL(WWHD_ADDR_daObjTpost_c__getMsg_023A5570, u32, pb_get_msg_original, (void* box));
WWHD_GAME_ORIGINAL(WWHD_ADDR_daObjTpost_c__nextMsg_023A55BC, u16, pb_next_original, (void* box, u32* message));

static int pb_on, pb_flagged;
static u32 pb_talking;   /* the postbox answering a picto */
static int pb_taken;     /* its picto was taken (erased at the talk's end) */

static void pb_option_changed(void) { pb_on = wwhd_config_bool("postbox_pictos", 0); }

static u32 pb_events(void) {
    u32 info = WWHD_GAME_DATA(WWHD_ADDR_dComIfG_save_info_pointer, u32);
    return qol_guest_ptr(info) ? info + 0x644 : 0;  /* dSv_info_c: mSavedata (+0x20) .mEvent (+0x624) */
}

/* Carlov's checks and order for the picto handed over (daNpcMt_c::getMsg; his helpers work on the save, not on his
 * actor). Returns 1 when ordered. */
static int pb_take(void) {
    u8* p = qol_play();
    u32 ev = pb_events();
    if (!p || !ev) return 0;
    u32 photo = pb_change_photo_no(p[PB_SUBJECT]), figure = 0xFF;
    if (photo - 0x49 < 0x86) figure = pb_photo_figure(photo) & 0xFF;
    const char* why = 0;
    if (!p[PB_COLOUR]) why = "not a colour picto";
    else if (figure == 0xFF) why = "no figurine subject";
    else if (pb_is_made(0, figure)) why = "the figurine is made already";
    else if (pb_has_order(0, figure)) why = "the figurine is on order already";
    else if (p[PB_FLAWS] & 1) why = "the face is not seen";
    else if (p[PB_FLAWS] & 2) why = "the body is not seen";
    else if ((pb_order_count(0) & 0xFF) >= 12) why = "Carlov has 12 orders";
    if (why) {
        wwhd_log(why);
        wwhd_log_int("postbox: picto refused, its subject", p[PB_SUBJECT]);
        return 0;
    }
    if (pb_ev_is(ev, 0x4080)) {  /* the last figurines handed over and not talked about since: as Carlov does */
        pb_ev_off(ev, 0x3080), pb_ev_off(ev, 0x4080), pb_ev_off(ev, 0x3F01), pb_ev_off(ev, 0x4040);
        pb_clear_orders(0);
    }
    pb_submit(0, figure);
    pb_ev_on(ev, 0x2F01);
    pb_ev_on(ev, 0x3401);
    wwhd_log_int("postbox: figurine ordered", (int)figure);
    return 1;
}

/* the postbox's step: the photo flag while the option is on */
WWHD_HOOK(WWHD_ADDR_executeActor_023A4BB0, qol_postbox_step, (void* box)) {
    volatile u32* flags = (volatile u32*)((u32)box + PB_ATTN);
    if (pb_on) *flags |= PB_ATTN_PHOTO, pb_flagged = 1;
    else if (pb_flagged) *flags &= ~PB_ATTN_PHOTO;
}

WWHD_REPLACE(WWHD_ADDR_daObjTpost_c__getMsg_023A5570, u32, qol_postbox_msg, (void* box)) {
    u8* p = qol_play();
    if (pb_on && p && p[PB_IN_PHOTO]) {
        pb_taken = pb_take();
        pb_talking = (u32)box;
        if (!pb_taken) {  /* back out of the postbox, as a letter it can't take */
            wwhd_daObjTpost_c__setAnm(box, 3, 0);
            *(volatile u8*)((u32)box + PB_PUTOUT_DONE) = 1;
        }
        return pb_taken ? PB_MSG_ACCEPTED : PB_MSG_REFUSED;
    }
    return pb_get_msg_original(box);
}

WWHD_REPLACE(WWHD_ADDR_daObjTpost_c__nextMsg_023A55BC, u16, qol_postbox_next, (void* box, u32* message)) {
    if (pb_talking && pb_talking == (u32)box) {
        if (*message == PB_MSG_ACCEPTED) {  /* "accepted for delivery" (no postage: Carlov's orders are free) */
            *message = PB_MSG_THANKS;
            return 0xF;
        }
        if (pb_taken) {  /* the picto leaves the album, as when Carlov takes it */
            u8* p = qol_play();
            u32 info = WWHD_GAME_DATA(WWHD_ADDR_dComIfG_save_info_pointer, u32);
            if (p && qol_guest_ptr(info)) {
                wwhd_eraseAndNotify((void*)(info + 0x12C0 + 0x700 + 0xB409C0), p[PB_SLOT]);
                volatile s16* count = (volatile s16*)(p + PB_PICTURE_COUNT);
                *count = (s16)(*count - 1);
                wwhd_log_int("postbox: the picto left the album, its slot", p[PB_SLOT]);
            }
        }
        pb_talking = 0, pb_taken = 0;
        return 0x10;
    }
    return pb_next_original(box, message);
}
