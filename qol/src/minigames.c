/* Minigames by touch (option "minigame_touch", Minigames; off by default), besides Sploosh Kaboom (kaboom.c):
 *   - the Cabana's picture puzzles: a tap on a piece in the gap's row or column, or a drag towards the gap, slides it
 *     there (with the pieces between);
 *   - mail sorting at the Rito post: a tap on a slot throws the letter there;
 *   - Spectacle Island's cannon game: a drag turns the cannon (sideways: left / right, up / down: farther / nearer),
 *     a tap fires;
 *   - the auction: a tap anywhere bids (the bid's number entry is the port's own "talks by touch");
 *   - the Nintendo Gallery: a finger held on a figurine turns it (the stick, from where it came down), a pinch (two
 *     fingers; the mouse wheel on a computer) zooms in and out (the C stick).
 * The game is played through its own input: the input filter presses A and pushes the stick for some controller reads,
 * as a player would; only the cursor of the board games (the mail slot) is set directly, as kaboom.c does. Each
 * minigame claims the picture only while it runs (one claim, slot 3), so the player's thumbs work the game again after.
 *
 * Fields (the HD decompilation's layouts, ZeldaWWHDDecomp/wwhd, wwhd_src/include/d/actor):
 *   daObjApzl_c (its _execute 02318640, r3 the puzzle): +0xB8D mBlankIdx, +0xB8F mMoveTimer, +0xB92 mPiecePos[16]
 *     (piece -> square, 0 top left .. 15), +0xBA2 mState (3 playing). set_mtx puts square p at ((p % 4) * 40 - 60,
 *     60 - (p / 4) * 40, 0) of the actor (current.pos +0x314, current.angle.y +0x322). move_piece: stick left moves the
 *     piece right of the gap left (and so on); then 5 steps of mMoveTimer.
 *   daNpc_Bmsw_c / daNpc_Btsw_c (Execute 0220EFF0 / 022197C4): the cursor's column -1..1 at +0xD54 / +0xD44, its row
 *     0 (bottom) .. 1 at the byte after, mActionStatus +0xD7D / +0xD6D (1 while sorting); the slots' cursor places
 *     cursor_pos_data[row][column + 1] (world: x -52 / -140 / -227, y 804 / 889, z 996). A throws at the cursor.
 *   daCanon (canon_execute 02109AF8): home.angle +0x2F8, shape_angle +0x328 (x: pitch, from home .. home + 0x1000;
 *     y: yaw, home +- 0x1200); its PadMove turns towards the stick, so angles set here stay.
 *   the game's play state (dComIfG_play_c, qol_play()): +0x5CEA mMiniGameType (3 the cannon game, 5 the auction,
 *     7 mail sorting), +0x5BB2 mMesgStatus (a message box is open), +0x5BB6 / +0x5BB7 the B / A prompts (+3 forced):
 *     A "Bid" 0x25 at the auction; looking at a figurine: A "Info" 0x21 and B "Cancel" 0x27 (measured: not forced).
 * Idea from the GreenNaugahyde Android fork (MPL-2.0), mods/minigames.cpp; reimplemented as a guest mod with the
 * port's projection service (wwhd_screen_point) and claims. */
#define MG_SLOT 3
#define MG_TYPE 0x5CEA
#define MG_MESG 0x5BB2
#define MG_B_ACTION 0x5BB6
#define MG_A_ACTION 0x5BB7
#define MG_POS 0x314
#define MG_ANGLE_Y 0x322
#define MG_A 0x8000u

enum { MG_NONE, MG_PUZZLE, MG_MAIL, MG_MAIL2, MG_CANNON, MG_AUCTION, MG_FIGURE };
static const char* const mg_names[] = {"none", "Cabana puzzle", "mail sorting", "mail sorting", "cannon game",
                                       "auction", "figurine"};

static int mg_on;
static int mg_mode, mg_claimed;
/* the actors as their steps saw them: the actor and the reads left before it counts as gone */
static u32 mg_puzzle, mg_mail, mg_mail2, mg_cannon;
static int mg_puzzle_seen, mg_mail_seen, mg_mail2_seen, mg_cannon_seen;

static void mg_option_changed(void) { mg_on = wwhd_config_bool("minigame_touch", 0); }

static inline u8 mg_play8(u32 off) { u8* p = qol_play(); return p ? p[off] : 0; }
static inline u8 mg_action(u32 off) { u8 f = mg_play8(off + 3); return f ? f : mg_play8(off); }
static inline f32 mg_ldf(u32 a) { return *(volatile f32*)a; }

/* ---- presses: buttons and the stick for some controller reads, one after the other */
typedef struct {
    u32 buttons;
    f32 lx, ly;
    int reads;
} mg_press;
#define MG_PRESSES 24
static mg_press mg_q[MG_PRESSES];
static int mg_qn;
static void mg_push(u32 buttons, f32 lx, f32 ly, int reads) {
    if (mg_qn < MG_PRESSES) { mg_q[mg_qn].buttons = buttons; mg_q[mg_qn].lx = lx; mg_q[mg_qn].ly = ly; mg_q[mg_qn].reads = reads; mg_qn++; }
}
static int mg_pressing(void) { return mg_qn > 0; }
static void mg_tap_a(void) {
    if (mg_pressing()) return;
    mg_push(MG_A, 0, 0, 2);
    mg_push(0, 0, 0, 2);
}

/* the sticks held by a finger (the figurine), else none */
static int mg_hold;
static f32 mg_hold_lx, mg_hold_ly, mg_hold_ry;

/* ---- the picture: a world point where the drawn camera shows it (layout units) */
static int mg_project3(f32 x, f32 y, f32 z, f32* sx, f32* sy) {
    f32 w[3] = {x, y, z}, o[2];
    if (!wwhd_screen_point(w, o)) return 0;
    *sx = o[0];
    *sy = o[1];
    return 1;
}
static f32 mg_dist2(f32 ax, f32 ay, f32 bx, f32 by) { return (ax - bx) * (ax - bx) + (ay - by) * (ay - by); }
static f32 mg_clamp1(f32 v) { return v < -1 ? -1 : v > 1 ? 1 : v; }

/* ---- the Cabana's picture puzzles */
#define PZ_BLANK 0xB8D
#define PZ_TIMER 0xB8F
#define PZ_PIECES 0xB92
#define PZ_STATE 0xBA2
#define PZ_PLAYING 3

static int pz_playing(u32 a) { return a && *(volatile u8*)(a + PZ_STATE) == PZ_PLAYING; }
static int pz_blank_square(u32 a) {
    u8 b = *(volatile u8*)(a + PZ_BLANK);
    return b < 16 ? *(volatile u8*)(a + PZ_PIECES + b) : -1;
}
/* square p (0 top left .. 15) on the picture: the actor's place turned by its angle (mDoMtx_YrotM) */
static int pz_square_xy(u32 a, int p, f32* sx, f32* sy) {
    f32 lx = (f32)(p % 4) * 40.0f - 60.0f, ly = 60.0f - (f32)(p / 4) * 40.0f;
    s16 ang = *(volatile s16*)(a + MG_ANGLE_Y);
    f32 c = qol_cos(ang), s = qol_sin(ang);
    return mg_project3(mg_ldf(a + MG_POS) + lx * c, mg_ldf(a + MG_POS + 4) + ly, mg_ldf(a + MG_POS + 8) - lx * s, sx, sy);
}
/* the square under (x, y): the nearest within a little over half a square, -1 none */
static int pz_square_at(u32 a, f32 x, f32 y) {
    f32 xs[16], ys[16];
    int ok[16];
    for (int p = 0; p < 16; p++) ok[p] = pz_square_xy(a, p, &xs[p], &ys[p]);
    if (!ok[0] || !ok[5]) return -1;
    return qol_nearest(xs, ys, ok, 16, x, y, 0.75f * 0.75f * mg_dist2(xs[0], ys[0], xs[5], ys[5]) / 2.0f);
}
/* slides the pieces from square p to the gap: one stick push per piece (the way they move), each waiting out the move */
static int pz_slide(u32 a, int p) {
    f32 lx, ly;
    int n = qol_puzzle_slide(pz_blank_square(a), p, &lx, &ly);
    for (int i = 0; i < n; i++) {
        mg_push(0, lx, ly, 2);
        mg_push(0, 0, 0, 7);  /* (the move's 5 steps of mMoveTimer, then the stick in the middle) */
    }
    if (n) wwhd_log_int("minigames: Cabana puzzle slides from square", p);
    return n;
}
static int pz_down, pz_cell, pz_done;
static f32 pz_x0, pz_y0;
static void pz_touch(const wwhd_touch_event* e) {
    u32 a = mg_puzzle;
    if (!pz_playing(a)) return;
    if (e->kind == WWHD_TOUCH_DOWN) {
        pz_down = 1, pz_done = 0, pz_x0 = e->x, pz_y0 = e->y;
        pz_cell = pz_square_at(a, e->x, e->y);
    } else if (e->kind == WWHD_TOUCH_MOVE && pz_down) {
        /* dragged half a square towards the gap: the pieces slide */
        int b = pz_blank_square(a), p = pz_cell;
        f32 px, py, bx, by;
        if (p < 0 || b < 0 || pz_done || mg_pressing()) return;
        if (!pz_square_xy(a, p, &px, &py) || !pz_square_xy(a, b, &bx, &by)) return;
        f32 dx = bx - px, dy = by - py, len2 = dx * dx + dy * dy;
        int squares = p % 4 - b % 4, rows = p / 4 - b / 4;
        if (squares < 0) squares = -squares;
        if (rows < 0) rows = -rows;
        if (rows > squares) squares = rows;
        if (len2 < 1 || squares == 0) return;
        f32 along = (e->x - pz_x0) * dx + (e->y - pz_y0) * dy;  /* (times the length) */
        if (along > 0.5f * len2 / (f32)squares) pz_done = pz_slide(a, p);
    } else if (e->kind == WWHD_TOUCH_UP && pz_down) {
        pz_down = 0;
        if (!pz_done && !mg_pressing() && pz_square_at(a, e->x, e->y) == pz_cell) pz_slide(a, pz_cell);
    } else {
        pz_down = 0;
    }
}

WWHD_HOOK(WWHD_ADDR_daObjApzl_c___execute, qol_puzzle_step, (void* puzzle)) {
    /* (a room has two puzzles: the one being played is followed) */
    u32 a = (u32)puzzle;
    if (pz_playing(a) || a == mg_puzzle || !mg_puzzle_seen || !pz_playing(mg_puzzle)) mg_puzzle = a, mg_puzzle_seen = 3;
}

/* ---- mail sorting at the Rito post (Koboli's daNpc_Bmsw_c, Baito's daNpc_Btsw_c) */
#define MAIL_CURSOR 0xD54
#define MAIL_STATUS 0xD7D
#define MAIL2_CURSOR 0xD44
#define MAIL2_STATUS 0xD6D
static const f32 mail_slot_x[3] = {-52.0f, -140.0f, -227.0f}, mail_slot_y[2] = {804.0f, 889.0f};
#define MAIL_SLOT_Z 996.0f

static int mail_sorting(u32 a, u32 status) { return a && mg_play8(MG_TYPE) == 7 && *(volatile s8*)(a + status) == 1; }
/* the slot under (x, y): row * 3 + (column + 1), -1 none */
static int mail_slot_at(f32 x, f32 y) {
    f32 xs[6], ys[6];
    int ok[6];
    for (int q = 0; q < 6; q++) ok[q] = mg_project3(mail_slot_x[q % 3], mail_slot_y[q / 3], MAIL_SLOT_Z, &xs[q], &ys[q]);
    if (!ok[0] || !ok[4]) return -1;
    return qol_nearest(xs, ys, ok, 6, x, y, 0.8f * 0.8f * mg_dist2(xs[0], ys[0], xs[4], ys[4]) / 2.0f);
}
static void mail_touch(const wwhd_touch_event* e) {
    u32 a = mg_mode == MG_MAIL ? mg_mail : mg_mail2;
    u32 cursor = mg_mode == MG_MAIL ? MAIL_CURSOR : MAIL2_CURSOR;
    if (e->kind != WWHD_TOUCH_UP || !a) return;
    int q = mail_slot_at(e->x, e->y);
    if (q < 0 || mg_pressing()) return;
    *(volatile s8*)(a + cursor) = (s8)(q % 3 - 1);
    *(volatile s8*)(a + cursor + 1) = (s8)(q / 3);
    mg_tap_a();
    wwhd_log_int("minigames: mail sorting slot (row*3+column)", q);
}
WWHD_HOOK(WWHD_ADDR_daNpc_Bmsw_Execute, qol_mail_step, (void* npc)) { mg_mail = (u32)npc, mg_mail_seen = 3; }
WWHD_HOOK(WWHD_ADDR_daNpc_Btsw_Execute, qol_mail2_step, (void* npc)) { mg_mail2 = (u32)npc, mg_mail2_seen = 3; }

/* ---- Spectacle Island's cannon game */
#define CANNON_HOME 0x2F8
#define CANNON_SHAPE 0x328
#define CANNON_YAW_PER_UNIT (4608.0f / 450.0f)   /* a drag across 450 layout units: the whole turn one way */
#define CANNON_PITCH_PER_UNIT (4096.0f / 500.0f)
static int cannon_down, cannon_moved;
static f32 cannon_x0, cannon_y0;
static s16 cannon_yaw0, cannon_pitch0;
static void cannon_touch(const wwhd_touch_event* e) {
    u32 a = mg_cannon;
    if (!a) return;
    if (e->kind == WWHD_TOUCH_DOWN) {
        cannon_down = 1, cannon_moved = 0, cannon_x0 = e->x, cannon_y0 = e->y;
        cannon_pitch0 = *(volatile s16*)(a + CANNON_SHAPE);
        cannon_yaw0 = *(volatile s16*)(a + CANNON_SHAPE + 2);
        return;
    }
    if (!cannon_down) return;
    if (e->kind == WWHD_TOUCH_CANCEL) { cannon_down = 0; return; }
    f32 dx = e->x - cannon_x0, dy = e->y - cannon_y0;
    if (dx * dx + dy * dy > 12.0f * 12.0f) cannon_moved = 1;
    if (cannon_moved) {
        int hy = *(volatile s16*)(a + CANNON_HOME + 2), hx = *(volatile s16*)(a + CANNON_HOME);
        int yaw = cannon_yaw0 - (int)(dx * CANNON_YAW_PER_UNIT), pitch = cannon_pitch0 - (int)(dy * CANNON_PITCH_PER_UNIT);
        if (yaw < hy - 0x1200) yaw = hy - 0x1200;
        if (yaw > hy + 0x1200) yaw = hy + 0x1200;
        if (pitch < hx) pitch = hx;
        if (pitch > hx + 0x1000) pitch = hx + 0x1000;
        *(volatile s16*)(a + CANNON_SHAPE + 2) = (s16)yaw;
        *(volatile s16*)(a + CANNON_SHAPE) = (s16)pitch;
    }
    if (e->kind == WWHD_TOUCH_UP) {
        cannon_down = 0;
        if (!cannon_moved) {
            mg_tap_a();
            wwhd_log("minigames: cannon game: fire");
        } else {
            wwhd_log_int("minigames: cannon game: turned to yaw", *(volatile s16*)(a + CANNON_SHAPE + 2));
        }
    }
}
WWHD_HOOK(WWHD_ADDR_canon_execute, qol_cannon_step, (void* cannon)) { mg_cannon = (u32)cannon, mg_cannon_seen = 3; }

/* ---- the auction: a tap bids (A) while A says "Bid" */
static void auction_touch(const wwhd_touch_event* e) {
    if (e->kind != WWHD_TOUCH_DOWN || mg_pressing()) return;
    mg_push(MG_A, 0, 0, 1);
    mg_push(0, 0, 0, 1);
    wwhd_log("minigames: auction: A (the gauge rises)");
}

/* ---- a figurine in the Nintendo Gallery: a finger held turns it (the stick, 120 layout units = all the way), a pinch
 * zooms (the C stick up / down: spread apart = nearer) */
static f32 fig_x0, fig_y0;
static int fig_down;
static void figure_touch(const wwhd_touch_event* e) {
    if (e->kind == WWHD_TOUCH_DOWN) {
        fig_down = 1, fig_x0 = e->x, fig_y0 = e->y;
    } else if (e->kind == WWHD_TOUCH_MOVE && fig_down) {
        mg_hold = 1;
        mg_hold_lx = mg_clamp1((e->x - fig_x0) / 120.0f);
        mg_hold_ly = mg_clamp1((e->y - fig_y0) / 120.0f);
    } else if (e->kind == WWHD_TOUCH_PINCH) {
        mg_hold = 1;
        mg_hold_lx = mg_hold_ly = 0;
        mg_hold_ry = mg_clamp1((e->x - 1.0f) * 3.0f);
    } else if (e->kind == WWHD_TOUCH_PINCH_END) {
        mg_hold_ry = 0;
        mg_hold = fig_down;
        mg_hold_lx = mg_hold_ly = 0;
        fig_x0 = fig_y0 = 1e9f;  /* (the finger left after a pinch: no turn until it comes down again) */
    } else {
        fig_down = 0;
        mg_hold = 0, mg_hold_lx = mg_hold_ly = mg_hold_ry = 0;
    }
}

/* the places a minigame's touches aim at, once when it starts (to check them against the picture) */
static void mg_log_xy(const char* what, int ok, f32 x, f32 y) {
    if (!ok) { wwhd_log(what); return; }
    wwhd_log_float(what, x);
    wwhd_log_float("  y", y);
}
static void mg_log_layout(int mode) {
    f32 x = 0, y = 0;
    if (mode == MG_PUZZLE) {
        mg_log_xy("minigames: puzzle square 0 at x", pz_square_xy(mg_puzzle, 0, &x, &y), x, y);
        mg_log_xy("minigames: puzzle square 15 at x", pz_square_xy(mg_puzzle, 15, &x, &y), x, y);
        wwhd_log_int("minigames: puzzle gap at square", pz_blank_square(mg_puzzle));
        for (int i = 0; i < 16; i++) wwhd_log_int("minigames: puzzle piece square", *(volatile u8*)(mg_puzzle + PZ_PIECES + i));
    } else if (mode == MG_MAIL || mode == MG_MAIL2) {
        for (int q = 0; q < 6; q++) mg_log_xy("minigames: mail slot at x", mg_project3(mail_slot_x[q % 3], mail_slot_y[q / 3], MAIL_SLOT_Z, &x, &y), x, y);
    }
}

/* ---- which minigame runs now */
static int mg_current(void) {
    if (!mg_on || !qol_play()) return MG_NONE;
    u8 type = mg_play8(MG_TYPE);
    if (mg_play8(MG_MESG)) return MG_NONE;  /* a message: the port's talks by touch have the picture */
    if (mg_action(MG_A_ACTION) == 0x21 && mg_action(MG_B_ACTION) == 0x27) return MG_FIGURE;
    if (mg_puzzle_seen && pz_playing(mg_puzzle)) return MG_PUZZLE;
    if (mg_mail_seen && mail_sorting(mg_mail, MAIL_STATUS)) return MG_MAIL;
    if (mg_mail2_seen && mail_sorting(mg_mail2, MAIL2_STATUS)) return MG_MAIL2;
    if (mg_cannon_seen && type == 3) return MG_CANNON;
    if (type == 5 && mg_action(MG_A_ACTION) == 0x25) return MG_AUCTION;
    return MG_NONE;
}

/* a finger in the minigame's claim: returns 1 when it was the minigame's */
static int mg_touch(const wwhd_touch_event* e) {
    if (!mg_claimed) return 0;
    switch (mg_mode) {
    case MG_PUZZLE: pz_touch(e); break;
    case MG_MAIL:
    case MG_MAIL2: mail_touch(e); break;
    case MG_CANNON: cannon_touch(e); break;
    case MG_AUCTION: auction_touch(e); break;
    case MG_FIGURE: figure_touch(e); break;
    default: return 0;
    }
    return 1;
}

/* each of the game's controller reads (the input filter): the claim follows the minigame; the presses and holds */
static void mg_filter(wwhd_input_state* s) {
    if (mg_puzzle_seen > 0) mg_puzzle_seen--;
    if (mg_mail_seen > 0) mg_mail_seen--;
    if (mg_mail2_seen > 0) mg_mail2_seen--;
    if (mg_cannon_seen > 0) mg_cannon_seen--;
    int mode = mg_current();
    if (mode != mg_mode) {  /* (fingers and presses from before don't carry over) */
        if (mode) wwhd_log(mg_names[mode]);
        mg_log_layout(mode);
        mg_mode = mode;
        mg_qn = 0;
        mg_hold = 0, mg_hold_lx = mg_hold_ly = mg_hold_ry = 0;
        pz_down = cannon_down = fig_down = 0;
    }
    if (!!mode != mg_claimed) {
        mg_claimed = !!mode;
        if (mode) wwhd_touch_claim(MG_SLOT, -2000.0f, -360.0f, 2000.0f, 360.0f);  /* the whole picture */
        else wwhd_touch_claim(MG_SLOT, 0, 0, 0, 0);
    }
    if (mg_hold) {
        s->lx = mg_hold_lx, s->ly = mg_hold_ly;
        if (mg_hold_ry != 0) s->ry = mg_hold_ry;
    }
    if (mg_qn > 0) {
        s->buttons |= mg_q[0].buttons;
        if (mg_q[0].buttons == 0 || mg_q[0].lx != 0 || mg_q[0].ly != 0) s->lx = mg_q[0].lx, s->ly = mg_q[0].ly;
        if (--mg_q[0].reads <= 0) {
            for (int i = 1; i < mg_qn; i++) mg_q[i - 1] = mg_q[i];
            mg_qn--;
        }
    }
}
