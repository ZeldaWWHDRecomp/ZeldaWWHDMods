/* Game Over prompt on the GamePad. At Game Over the game reuses its save/options window ("Would you like to save?")
 * with the screen routing 4 (the TV): the TV shows it, and the game takes the answer from the controller only. It
 * draws the window into the GamePad picture too, but then covers the whole GamePad picture with its Game Over
 * black pane, so the GamePad (or the phone showing it) is black and the window's touch buttons do nothing.
 * While that window is up:
 * - the black pane's opacity is held at 0: the GamePad shows the window (over its item screen);
 * - a tap on its Cancel or Save becomes the input the prompt reads (rules.h qol_gameover_tap_step).
 * The TV's own black backdrop is another pane and stays as it is. On by default (unlike the other extras).
 * Ports from mod manager 1.12.0 take GamePad touch claims: the two buttons are claimed while the prompt is up, so a
 * finger on them goes only to this extra (the game doesn't answer it with its "not here" mark). Older ports refuse the
 * claim: the extra then reads the finger from the input filter, as the game does. */
#ifndef WWHD_TOUCH_GAMEPAD
#define WWHD_TOUCH_GAMEPAD 0x100  /* (SDK headers from before mod manager 1.12.0) */
#endif
#define GO_INGAME_UI 0x101F8344      /* sIngameUi (hdIngameUi_c*, 0 outside gameplay) */
#define GO_ROUTING 0x101F8348        /* the UI manager's screen routing: 2 the GamePad's menus, 4 the TV */
#define GO_SAVE_WINDOW_A 0x1049ED08  /* the in-game UI's state descriptors of the save/options window */
#define GO_SAVE_WINDOW_B 0x1049ED34
#define GO_UI_STATE 0x18             /* hdIngameUi_c: its state machine's current state object (+8: its descriptor) */
#define GO_UI_PENDING 0x14           /* ... with none, the pending descriptor */
#define GO_BLACK_PANE 0x1E8          /* hdIngameUi_c: the Game Over black pane (GameOverBlackPane_show, 0266A064) */
#define GO_PANE_ALPHA 0x54           /* its opacity (f32, 0..1) */
static int go_on;
static qol_gameover_tap go_tap;
static int go_claim;  /* while the prompt is up: 1 the buttons are claimed, -1 the port refused (older ports) */
static void gameover_option_changed(void) { go_on = wwhd_config_bool("gameover_gamepad", 1); }

/* the Game Over save window is up: the in-game UI (its address) */
static u32 go_prompt(void) {
    const u32 ui = WWHD_GAME_DATA(GO_INGAME_UI, u32);
    if (!qol_guest_ptr(ui) || WWHD_GAME_DATA(GO_ROUTING, u32) != 4) return 0;
    const u32 current = qol_ld32(ui + GO_UI_STATE);
    const u32 descriptor = qol_guest_ptr(current) ? qol_ld32(current + 8) : qol_ld32(ui + GO_UI_PENDING);
    if (descriptor != (u32)&WWHD_GAME_DATA(GO_SAVE_WINDOW_A, u8) && descriptor != (u32)&WWHD_GAME_DATA(GO_SAVE_WINDOW_B, u8))
        return 0;
    return ui;
}

static void go_claims(int up) {
    if (up && !go_claim) {
        int ok = 1;
        for (u32 b = 0; b < 2; b++) {
            const float* r = qol_gameover_buttons[b];
            ok &= wwhd_touch_claim(WWHD_TOUCH_GAMEPAD | b, r[0] * 854.0f, r[1] * 480.0f, r[2] * 854.0f, r[3] * 480.0f);
        }
        if (!ok)
            for (u32 b = 0; b < 2; b++) wwhd_touch_claim(WWHD_TOUCH_GAMEPAD | b, 0, 0, 0, 0);
        go_claim = ok ? 1 : -1;
    } else if (!up && go_claim) {
        if (go_claim > 0)
            for (u32 b = 0; b < 2; b++) wwhd_touch_claim(WWHD_TOUCH_GAMEPAD | b, 0, 0, 0, 0);
        go_claim = 0;
    }
}

/* a finger on a claimed button (GamePad coordinates, 854 x 480) */
static void gameover_touch(const wwhd_touch_event* e) {
    if (go_claim > 0) qol_gameover_tap_event(&go_tap, (int)(e->kind & ~WWHD_TOUCH_GAMEPAD), e->x / 854.0f, e->y / 480.0f);
}

static void gameover_filter(wwhd_input_state* s) {
    const u32 ui = go_on ? go_prompt() : 0;
    if (ui) {
        const u32 pane = qol_ld32(ui + GO_BLACK_PANE);
        if (qol_guest_ptr(pane)) *(volatile f32*)(pane + GO_PANE_ALPHA) = 0.0f;
    }
    go_claims(ui != 0);
    if (go_claim > 0) s->buttons |= qol_gameover_tap_step(&go_tap, ui != 0, go_tap.down, go_tap.x, go_tap.y);
    else s->buttons |= qol_gameover_tap_step(&go_tap, ui != 0, s->touch != 0, s->tx, s->ty);
}
