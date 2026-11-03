/* Songs by touch: while Link holds up the Wind Waker, play buttons for the songs he knows (a tap plays the song,
 * as if conducted) and note pads (up, left, right, down: a finger held on one is the right stick, to conduct by
 * touch). Option "song_touch" (Songs), off by default.
 *
 * Link conducts in daPy_lk_c::procTactWait (procedure 154). Each beat the game asks its judge (JAIZelInst::judge,
 * 0202A3B4: the notes conducted so far against the songs' patterns) which song they make; its answer (a melody
 * number, -1 none) starts the song: the chime, Link plays it, its effect. A tapped song is the judge's next answer,
 * once (the judge itself still runs, with its sounds). HD asks its judge only when a note is conducted, so a tap
 * also conducts one note (the right stick up for a moment). The game's own checks stay: only songs Link knows play, and
 * a story scene that waits for one song (mProcVar6 +0x69C0: its melody number, -1 free play) takes only that one,
 * so only its button shows then. Known songs: the collection's song bits (save + 0x20 + 0xBD, bit = melody).
 * Buttons and pads are drawn on the TV picture (HUD) and claimed for touches (two claims: the buttons' row, the
 * pads' square); the pads' finger turns into the right stick through the input filter.
 * Idea from the GreenNaugahyde Android fork (MPL-2.0), its play buttons and note pads; reimplemented as a guest mod. */
#define SONGS_TACT_WAIT 154
#define SONGS_COUNT 6
#define SONGS_KNOWN 0xBD
/* layout units: 1280 x 720 around the centre, +y up; the HUD's TV coordinates are 1280 x 720 from the top left */
#define SONGS_ROW_Y -285.0f     /* the buttons' row: centre y (at the bottom, under the game's own conducting display) */
#define SONGS_BUTTON_W 190.0f
#define SONGS_BUTTON_H 64.0f
#define SONGS_PAD_X -470.0f     /* the pads' square: centre (left of Link) */
#define SONGS_PAD_Y -60.0f
#define SONGS_PAD_R 120.0f      /* half its side */

static const char* const songs_names[SONGS_COUNT] = {"Wind's Requiem", "Ballad of Gales", "Command Melody",
                                                      "Earth God's Lyric", "Wind God's Aria", "Song of Passing"};
static int songs_on, songs_shown, songs_wait = -1;
static int songs_conducting;         /* Link holds up the Wind Waker (any feature may play a song then: sing.c) */
static u32 songs_known;
static int songs_pending = -1;       /* a tapped song: the judge's next answer */
static int songs_poke;               /* reads left of the note that makes the judge run (HD asks it only on a note) */
static int songs_pad = -1;           /* the pad under the finger: 0 up, 1 left, 2 right, 3 down; -1 none */
static int songs_finger = 0;         /* 1 a finger on the row, 2 on the pads */
static u32 songs_claimed;
static wwhd_hud_element songs_el;

static void songs_option_changed(void) { songs_on = wwhd_config_bool("song_touch", 0); }

static int songs_available(int m) {
    return (songs_known >> m & 1) && (songs_wait < 0 || songs_wait == m);
}
/* the buttons shown, left to right: their melody numbers; returns how many */
static int songs_buttons(int* out) {
    int n = 0;
    for (int m = 0; m < SONGS_COUNT; m++)
        if (songs_available(m)) out[n++] = m;
    return n;
}
static f32 songs_row_x0(int n) { return -(SONGS_BUTTON_W + 10.0f) * n / 2.0f; }

static void songs_step(u32 link) {
    daPy_lk_c* l = (daPy_lk_c*)link;
    u32 info = WWHD_GAME_DATA(WWHD_ADDR_dComIfG_save_info_pointer, u32);
    songs_known = qol_guest_ptr(info) ? *(volatile u8*)(info + 0x20 + SONGS_KNOWN) : 0;
    songs_wait = l->mProcVar6 >= 0 && l->mProcVar6 < SONGS_COUNT ? l->mProcVar6 : -1;
    songs_conducting = l->mCurProc == SONGS_TACT_WAIT;
    if (!songs_conducting) songs_pending = -1, songs_poke = 0;
    const int show = songs_on && songs_conducting;
    if (show != songs_shown || (show && songs_claimed != songs_known + (u32)(songs_wait + 1) * 256)) {
        songs_shown = show;
        songs_claimed = songs_known + (u32)(songs_wait + 1) * 256;
        int b[SONGS_COUNT];
        const int n = show ? songs_buttons(b) : 0;
        if (n) {
            const f32 x0 = songs_row_x0(n);
            wwhd_touch_claim(0, x0, SONGS_ROW_Y - SONGS_BUTTON_H / 2, x0 + (SONGS_BUTTON_W + 10.0f) * n, SONGS_ROW_Y + SONGS_BUTTON_H / 2);
        } else {
            wwhd_touch_claim(0, 0, 0, 0, 0);
        }
        if (show) wwhd_touch_claim(1, SONGS_PAD_X - SONGS_PAD_R, SONGS_PAD_Y - SONGS_PAD_R, SONGS_PAD_X + SONGS_PAD_R, SONGS_PAD_Y + SONGS_PAD_R);
        else wwhd_touch_claim(1, 0, 0, 0, 0);
        if (!show) songs_pad = -1, songs_finger = 0;
    }
}

/* plays song m (melody number) while Link conducts, if he knows it and the scene takes it: the judge's next answer */
static int songs_play(int m) {
    if (!songs_conducting || m < 0 || m >= SONGS_COUNT || !songs_available(m)) return 0;
    songs_pending = m;
    songs_poke = 4;
    return 1;
}

/* a finger in one of the claims */
static void songs_touch(const wwhd_touch_event* e) {
    if (e->kind == WWHD_TOUCH_DOWN) {
        if (e->y < SONGS_ROW_Y + SONGS_BUTTON_H / 2 + 1) {  /* the row (below the pads) */
            songs_finger = 1;
            int b[SONGS_COUNT];
            const int n = songs_buttons(b);
            const int i = (int)((e->x - songs_row_x0(n)) / (SONGS_BUTTON_W + 10.0f));
            if (i >= 0 && i < n) {
                songs_pending = b[i];
                songs_poke = 4;
                wwhd_log_int("songs: play button", b[i]);
            }
            return;
        }
        songs_finger = 2;
    }
    if (songs_finger == 2 && (e->kind == WWHD_TOUCH_DOWN || e->kind == WWHD_TOUCH_MOVE)) {
        const f32 dx = e->x - SONGS_PAD_X, dy = e->y - SONGS_PAD_Y;
        const f32 ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
        songs_pad = ax < 30.0f && ay < 30.0f ? -1 : ay >= ax ? (dy > 0 ? 0 : 3) : (dx < 0 ? 1 : 2);
    }
    if (e->kind == WWHD_TOUCH_UP || e->kind == WWHD_TOUCH_CANCEL) songs_finger = 0, songs_pad = -1;
}

/* the pads' finger as the right stick (the game conducts with it) */
static void songs_filter(wwhd_input_state* s) {
    if (songs_conducting && songs_poke > 0 && songs_pad < 0) {  /* a song to play: one note up, so the judge runs */
        songs_poke--;
        s->rx = 0;
        s->ry = 1;
        return;
    }
    if (!songs_shown || songs_pad < 0) return;
    static const f32 x[4] = {0, -1, 1, 0}, y[4] = {1, 0, 0, -1};
    s->rx = x[songs_pad];
    s->ry = y[songs_pad];
}

/* the judge: a tapped song is its next answer, once */
WWHD_GAME_ORIGINAL(WWHD_ADDR_judge_0202A3B4, s32, songs_judge_original, (u32 self, s32 now_beat, s32 c_pos));
WWHD_REPLACE(WWHD_ADDR_judge_0202A3B4, s32, songs_judge, (u32 self, s32 now_beat, s32 c_pos)) {
    const s32 answer = songs_judge_original(self, now_beat, c_pos);
    if (songs_pending < 0) return answer;  /* (set only by a feature that is on: the buttons, singing) */
    const int m = songs_pending;
    songs_pending = -1;
    if (!songs_available(m)) return answer;
    return m;
}

static void songs_draw(u32 list) {
    if (!songs_shown) return;
    int b[SONGS_COUNT];
    const int n = songs_buttons(b);
    const f32 x0 = songs_row_x0(n);
    for (int i = 0; i < n; i++) {
        const f32 left = 640.0f + x0 + i * (SONGS_BUTTON_W + 10.0f), top = 360.0f - SONGS_ROW_Y - SONGS_BUTTON_H / 2;
        songs_el = (wwhd_hud_element){.kind = WWHD_HUD_RECT, .anchor = WWHD_HUD_TOP_LEFT, .x = left, .y = top,
                                      .w = SONGS_BUTTON_W, .h = SONGS_BUTTON_H, .thickness = 1, .u1 = 1, .v1 = 1,
                                      .rgba = songs_pending == b[i] ? 0x3D7BD8E0 : 0x183048C8};
        wwhd_hud_emit(list, &songs_el);
        const char* name = songs_names[b[i]];
        u32 bytes = 0;
        while (name[bytes]) bytes++;
        songs_el = (wwhd_hud_element){.kind = WWHD_HUD_TEXT, .anchor = WWHD_HUD_TOP_LEFT, .x = left + 10, .y = top + 20,
                                      .size = 20, .thickness = 1, .rgba = 0xFFFFFFFF, .text = name, .text_bytes = bytes};
        wwhd_hud_emit(list, &songs_el);
    }
    /* the pads: a diamond of four squares */
    static const f32 px[4] = {0, -1, 1, 0}, py[4] = {1, 0, 0, -1};
    for (int i = 0; i < 4; i++) {
        const f32 cx = 640.0f + SONGS_PAD_X + px[i] * SONGS_PAD_R * 0.6f, cy = 360.0f - SONGS_PAD_Y - py[i] * SONGS_PAD_R * 0.6f;
        songs_el = (wwhd_hud_element){.kind = WWHD_HUD_CIRCLE, .anchor = WWHD_HUD_TOP_LEFT, .x = cx, .y = cy, .size = 38,
                                      .thickness = 1, .u1 = 1, .v1 = 1, .rgba = songs_pad == i ? 0xF0C040F0 : 0xFFFFFF60};
        wwhd_hud_emit(list, &songs_el);
    }
}
