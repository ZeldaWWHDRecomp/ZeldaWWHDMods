/* Sing the songs (experimental): while Link holds up the Wind Waker, sing, hum or whistle a song's notes into the
 * microphone and Link plays it, as if conducted. Option "sing_songs" (Songs), off by default; its manifest entry has
 * "permission": "microphone", so the port lets this mod open the microphone only while the option is on.
 *
 * The microphone: turning the option on opens it once at that moment (so the system asks for permission then, never
 * at install or start); after the answer it is closed again and opened only while Link conducts. Declined, the port
 * turns the option off again with a notice. Turning the option off closes the microphone at once. The mod gets only
 * the pitch of the sound (the port's analysis); no sound is kept, written, logged or sent anywhere.
 *
 * The notes: the pitch each controller read (wwhd_mic_pitch, clear enough) goes through the rules of rules.h (notes,
 * phrases, the songs' intervals in any key, octave and tempo); a phrase that is a song Link knows (and the scene takes)
 * plays it through songs.c's songs_play (the judge's next answer). A small HUD line shows the notes heard.
 * Idea from the GreenNaugahyde Android fork (MPL-2.0), its SongListener / SongMatcher; reimplemented as a guest mod on
 * the port's microphone service. */
#define SING_CLARITY 0.6f
#define SING_LEVEL 0.02f
#define SING_ASK_TIMEOUT 600  /* controller reads to wait for the answer to the first ask (~20 s) */

static int sing_on, sing_open;        /* the option; the microphone opened by this mod */
static int sing_asked;                /* the first open after turning the option on: 1 asking, 2 answered */
static int sing_wait;
static unsigned long long sing_serial;
static qol_singer sing_g;
static int sing_last_song = -1, sing_show_song;  /* the last song recognised, reads left to show it */
static int sing_state;
static wwhd_hud_element sing_el;
static char sing_text[64];
static u32 sing_text_bytes;

static const char* const sing_names[QOL_SONGS] = {"Wind's Requiem", "Ballad of Gales", "Command Melody",
                                                  "Earth God's Lyric", "Wind God's Aria", "Song of Passing"};

static void sing_close(void) {
    if (sing_open) wwhd_mic_close();
    sing_open = 0;
    qol_sing_reset(&sing_g);
}

static void sing_option_changed(void) {
    const int was = sing_on;
    sing_on = wwhd_config_bool("sing_songs", 0);
    if (sing_on && !was) {  /* turned on: ask now */
        sing_asked = 1, sing_wait = 0;
        sing_open = 1;
        sing_state = wwhd_mic_open();
        wwhd_log_int("sing: microphone asked, state", sing_state);
    }
    if (!sing_on) sing_close(), sing_asked = 0;
}

/* each controller read */
static void sing_filter(wwhd_input_state* s) {
    (void)s;
    if (!sing_on) return;
    if (sing_asked == 1) {  /* the first ask: wait for the answer, then close until Link conducts */
        sing_state = wwhd_mic_state();
        if (sing_state == WWHD_MIC_ASKING && ++sing_wait < SING_ASK_TIMEOUT) return;
        sing_asked = 2;
        wwhd_log_int("sing: microphone answer, state", sing_state);
        if (!songs_conducting) sing_close();
    }
    if (!songs_conducting) {
        if (sing_open) sing_close();
        if (sing_show_song > 0) sing_show_song--;
        return;
    }
    if (!sing_open) {
        sing_open = 1;
        sing_state = wwhd_mic_open();
        qol_sing_reset(&sing_g);
    }
    sing_state = wwhd_mic_state();
    if (sing_show_song > 0) sing_show_song--;
    if (sing_state != WWHD_MIC_OPEN) return;
    f32 p[3];
    unsigned long long serial = wwhd_mic_pitch(p);
    float midi = 0;
    if (serial != sing_serial && p[0] > 0 && p[1] >= SING_LEVEL && p[2] >= SING_CLARITY) midi = qol_midi(p[0]);
    sing_serial = serial;
    int song = qol_sing_step(&sing_g, midi, (float)wwhd_logic_dt());
    if (song >= 0) {
        sing_last_song = song, sing_show_song = 60;
        if (songs_play(song)) wwhd_log_int("sing: song recognised and played", song);
        else wwhd_log_int("sing: song recognised, not playable here", song);
    }
}

static void sing_draw(u32 list) {
    if (!sing_on || !songs_conducting) return;
    const char* t;
    if (sing_state == WWHD_MIC_OPEN) t = sing_show_song > 0 && sing_last_song >= 0 ? sing_names[sing_last_song] : "Sing a song";
    else if (sing_state == WWHD_MIC_ASKING) t = "Waiting for the microphone";
    else t = "No microphone";
    char* p = sing_text;
    while (*t && p < sing_text + 40) *p++ = *t++;
    if (sing_state == WWHD_MIC_OPEN) {  /* the notes heard so far */
        *p++ = ' ';
        for (int i = 0; i < sing_g.count + (sing_g.cur_steps > 0) && p < sing_text + 60; i++) *p++ = 'o';
    }
    sing_text_bytes = (u32)(p - sing_text);
    sing_el = (wwhd_hud_element){.kind = WWHD_HUD_RECT, .anchor = WWHD_HUD_TOP_LEFT, .x = 470, .y = 120, .w = 340, .h = 40,
                                 .thickness = 1, .u1 = 1, .v1 = 1, .rgba = 0x101820C0};
    wwhd_hud_emit(list, &sing_el);
    sing_el = (wwhd_hud_element){.kind = WWHD_HUD_TEXT, .anchor = WWHD_HUD_TOP_LEFT, .x = 486, .y = 129, .size = 22,
                                 .thickness = 1, .rgba = 0xFFFFFFFF, .text = sing_text, .text_bytes = sing_text_bytes};
    wwhd_hud_emit(list, &sing_el);
}
