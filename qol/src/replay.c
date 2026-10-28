/* Replay conducted songs: as on the GameCube, after a song is conducted Link plays it back (his animation and the
 * tune) before its effect. Option "replay_songs" (Songs), off by default.
 *
 * HD's procTactPlay (0243AC64) begins with a branch the GameCube's hasn't: in free play (mProcVar4 +0x691E == 0,
 * mProcVar7 +0x69C4 != 0) it plays its sound and goes straight to the song's effect, so the replay never comes.
 * While Link is in tact play (procedure 155) and its animation runs (mProcVar0 +0x6916 == 0), mProcVar4 is held at 1
 * at each step's start, so that branch waits and the GameCube's replay runs; once the animation is done mProcVar0
 * is set, mProcVar4 goes back to 0 and HD's branch gives the effect as before. (mProcVar4 is the GameCube's
 * "an event waits for this song", only read once the replay is done.)
 * Idea from the GreenNaugahyde Android fork (MPL-2.0), mods/songs.cpp; reimplemented as a guest mod. */
#define REPLAY_TACT_PLAY 155

static int replay_on;
static u32 replay_link;  /* the Link whose mProcVar4 this holds */

static void replay_option_changed(void) { replay_on = wwhd_config_bool("replay_songs", 0); }

static void replay_step(u32 link) {
    daPy_lk_c* l = (daPy_lk_c*)link;
    const int playing = replay_on && l->mCurProc == REPLAY_TACT_PLAY && l->mProcVar0 == 0;
    if (playing) {
        if (replay_link != link) wwhd_log("replay: Link plays the song back");
        l->mProcVar4 = 1;
        replay_link = link;
    } else if (replay_link == link) {
        l->mProcVar4 = 0;
        replay_link = 0;
    }
}
