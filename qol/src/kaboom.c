/* Minigames by touch, Sploosh Kaboom: a tap on a square of the board aims there and fires. Option
 * "minigame_touch" (Minigames), off by default.
 *
 * The board (daMgBoard_c, its step mgboard_ExecuteWrapper 021C5634) keeps the cursor's square at +0x584 (column)
 * and +0x585 (row), 0..7, and its state at +0x6DC (3 while the player aims). The game shows the board from a
 * fixed camera: its 8 x 8 squares sit at the same place on the picture (measured: x -185..190, y 155..-215 in layout
 * units). While the board is played its area is claimed for touches; a tap there writes the square into the
 * cursor and presses A for a moment (A fires the cannon at the cursor). Columns run left to right, rows bottom
 * to top (measured: a tap on the second row from the top put the cursor on the second row from the bottom).
 * Idea from the GreenNaugahyde Android fork (MPL-2.0), mods/minigames.cpp; reimplemented as a guest mod with a
 * measured screen layout instead of a projection. */
#define KABOOM_COL 0x584
#define KABOOM_ROW 0x585
#define KABOOM_STATE 0x6DC
#define KABOOM_PLAYING 3
#define KABOOM_X0 -185.0f
#define KABOOM_X1 190.0f
#define KABOOM_Y0 155.0f   /* the top edge (row 7; row 0 is the bottom one, measured) */
#define KABOOM_Y1 -215.0f
#define KABOOM_SLOT 2      /* its touch claim */

static int kaboom_on;
static u32 kaboom_board;   /* the board actor while it runs */
static u32 kaboom_seen;    /* its last step (Link's steps count) */
static int kaboom_claimed;
static int kaboom_fire;    /* reads of A left for a tapped square */

static void kaboom_option_changed(void) { kaboom_on = wwhd_config_bool("minigame_touch", 0); }

WWHD_HOOK(WWHD_ADDR_mgboard_ExecuteWrapper, qol_board_step, (void* board)) {
    kaboom_board = (u32)board;
    kaboom_seen = 3;
}

static int kaboom_playing(void) {
    return kaboom_on && kaboom_board && kaboom_seen > 0 && *(volatile u32*)(kaboom_board + KABOOM_STATE) == KABOOM_PLAYING;
}

/* each of the game's controller reads (the input filter): the claim follows the board; a tapped square fires */
static void kaboom_filter(wwhd_input_state* s) {
    if (kaboom_seen > 0) kaboom_seen--;  /* (the board's step renews it; gone: no claim) */
    const int play = kaboom_playing();
    if (play != kaboom_claimed) {
        kaboom_claimed = play;
        if (play) wwhd_touch_claim(KABOOM_SLOT, KABOOM_X0, KABOOM_Y1, KABOOM_X1, KABOOM_Y0);
        else wwhd_touch_claim(KABOOM_SLOT, 0, 0, 0, 0);
    }
    if (kaboom_fire > 0) {
        kaboom_fire--;
        if (kaboom_fire >= 2) s->buttons |= 0x8000u;  /* A for two reads, then up for two */
        else s->buttons &= ~0x8000u;
    }
}

/* a finger in the board's claim: returns 1 when it was the board's */
static int kaboom_touch(const wwhd_touch_event* e) {
    if (!kaboom_claimed || e->x < KABOOM_X0 || e->x > KABOOM_X1 || e->y > KABOOM_Y0 || e->y < KABOOM_Y1) return 0;
    if (e->kind != WWHD_TOUCH_UP || !kaboom_playing()) return 1;
    int col = (int)((e->x - KABOOM_X0) / ((KABOOM_X1 - KABOOM_X0) / 8));
    int row = (int)((e->y - KABOOM_Y1) / ((KABOOM_Y0 - KABOOM_Y1) / 8));  /* (the board's row 0 is at the bottom) */
    if (col < 0 || col > 7 || row < 0 || row > 7) return 1;
    *(volatile s8*)(kaboom_board + KABOOM_COL) = (s8)col;
    *(volatile s8*)(kaboom_board + KABOOM_ROW) = (s8)row;
    kaboom_fire = 4;
    wwhd_log_int("minigames: Sploosh Kaboom square (row*8+column)", row * 8 + col);
    return 1;
}
