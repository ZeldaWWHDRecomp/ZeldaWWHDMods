#include <assert.h>
#include <math.h>
#include <string.h>
#include "../src/rules.h"
int main(void) {
    /* sine / cosine of game angles */
    for (int a = -32768; a < 32768; a += 7) {
        float r = (float)a * 3.14159265f / 32768.0f;
        assert(fabsf(qol_sin((short)a) - sinf(r)) < 2e-4f);
        assert(fabsf(qol_cos((short)a) - cosf(r)) < 2e-4f);
    }
    /* puzzle: the gap at square 3 (top right) */
    float lx, ly;
    assert(qol_puzzle_slide(3, 2, &lx, &ly) == 1 && lx == 1 && ly == 0);    /* left of it: moves right */
    assert(qol_puzzle_slide(3, 0, &lx, &ly) == 3 && lx == 1);               /* the row's first: three pieces */
    assert(qol_puzzle_slide(3, 15, &lx, &ly) == 3 && ly == 1 && lx == 0);   /* bottom of its column: up */
    assert(qol_puzzle_slide(3, 7, &lx, &ly) == 1 && ly == 1);
    assert(qol_puzzle_slide(5, 6, &lx, &ly) == 1 && lx == -1);              /* right of it: moves left */
    assert(qol_puzzle_slide(9, 1, &lx, &ly) == 2 && ly == -1);              /* above it: moves down */
    assert(qol_puzzle_slide(3, 3, &lx, &ly) == 0 && lx == 0 && ly == 0);    /* the gap itself */
    assert(qol_puzzle_slide(3, 6, &lx, &ly) == 0);                          /* neither row nor column */
    assert(qol_puzzle_slide(-1, 6, &lx, &ly) == 0 && qol_puzzle_slide(3, 16, &lx, &ly) == 0);
    /* nearest point within reach, skipping points not on the picture */
    const float xs[3] = {0, 100, 200}, ys[3] = {0, 0, 0};
    const int ok[3] = {1, 0, 1};
    assert(qol_nearest(xs, ys, 0, 3, 90, 5, 50 * 50) == 1);
    assert(qol_nearest(xs, ys, ok, 3, 90, 5, 50 * 50) == -1);  /* (point 1 is off: 0 and 2 too far) */
    assert(qol_nearest(xs, ys, ok, 3, 160, 5, 50 * 50) == 2);
    assert(qol_nearest(xs, ys, 0, 0, 0, 0, 1) == -1);

    /* album pages: records */
    unsigned char rec[16 + 64];
    memset(rec + 16, 0xAB, 40);
    qol_record_seal(rec, QOL_PG_SLOT_MAGIC, 40);
    assert(qol_record_check(rec, 56, QOL_PG_SLOT_MAGIC, 64) == 40);
    assert(qol_record_check(rec, 55, QOL_PG_SLOT_MAGIC, 64) == -1);   /* cut short */
    assert(qol_record_check(rec, 56, QOL_PG_ORDER_MAGIC, 64) == -1);  /* another kind */
    assert(qol_record_check(rec, 56, QOL_PG_SLOT_MAGIC, 39) == -1);   /* too long for its kind */
    assert(qol_record_check(rec, -1, QOL_PG_SLOT_MAGIC, 64) == -1);   /* no file */
    rec[30] ^= 1;
    assert(qol_record_check(rec, 56, QOL_PG_SLOT_MAGIC, 64) == -1);   /* a changed byte */
    rec[30] ^= 1;
    rec[5] = 9;
    assert(qol_record_check(rec, 56, QOL_PG_SLOT_MAGIC, 64) == -1);   /* a newer version */
    /* the index */
    unsigned char ix[16 + QOL_PG_INDEX_LEN];
    qol_pg_index a = {7, 2, 3, 2, {5, 9}}, b;
    qol_pg_index_write(ix, &a);
    assert(qol_pg_index_read(ix, sizeof ix, &b) && b.gen == 7 && b.set == 2 && b.count == 3 && b.fpn == 2 && b.fp[1] == 9);
    assert(!qol_pg_index_read(ix, sizeof ix - 1, &b));
    a.count = 0;
    qol_pg_index_write(ix, &a);
    assert(!qol_pg_index_read(ix, sizeof ix, &b));  /* no page at all: bad */
    /* fingerprints */
    const unsigned sums[12] = {30, 10, 20, 99, 0, 0, 0, 0, 0, 0, 0, 0};
    const int used[12] = {1, 1, 1, 0};
    unsigned fp[12], fp2[12] = {20, 40};
    assert(qol_fp_make(sums, used, fp) == 3 && fp[0] == 10 && fp[1] == 20 && fp[2] == 30);
    assert(qol_fp_same(fp, 3, fp2, 2));          /* 20 in both: the same album with pictos changed */
    fp2[0] = 21;
    assert(!qol_fp_same(fp, 3, fp2, 2));         /* nothing in common */
    assert(qol_fp_same(fp, 0, fp2, 0) && !qol_fp_same(fp, 0, fp2, 1));
    /* order rebuilt from the used slots */
    unsigned char order[12];
    const int used2[12] = {0, 1, 0, 0, 1};
    qol_order_rebuild(used2, order);
    assert(order[0] == 1 && order[1] == 4 && order[2] == 0xFF && order[11] == 0xFF);
    /* turning pages */
    assert(qol_pg_turn(0, 1, -1, 5) == -1);
    assert(qol_pg_turn(0, 1, 1, 5) == 1);    /* a new page after a page with pictos */
    assert(qol_pg_turn(0, 1, 1, 0) == -1);   /* not after an empty one */
    assert(qol_pg_turn(1, 3, 1, 0) == 2 && qol_pg_turn(2, 3, -1, 0) == 1);
    assert(qol_pg_turn(2, 3, 1, 12) == 3 && qol_pg_turn(2, 3, 2, 12) == -1);

    /* singing: log2 and MIDI */
    for (float x = 0.01f; x < 5000.0f; x *= 1.37f) assert(fabsf(qol_log2(x) - log2f(x)) < 1e-4f);
    assert(fabsf(qol_midi(440.0f) - 69.0f) < 1e-3f && fabsf(qol_midi(261.63f) - 60.0f) < 1e-2f);
    /* every song in any key / octave, merged form too; small errors pass, big ones don't */
    for (int s = 0; s < QOL_SONGS; s++) {
        float f[8], g[8];
        int n = qol_song_notes(s, 0, 0, f);
        for (int i = 0; i < n; i++) g[i] = f[i] - 7.0f + (i % 2 ? 0.4f : -0.3f);  /* a fifth lower, a little off */
        assert(qol_sing_match(g, n, 0) == s);
        n = qol_song_notes(s, 1, 0, f);
        for (int i = 0; i < n; i++) g[i] = f[i] + 12.0f;
        assert(qol_sing_match(g, n, 0) == s);
    }
    {
        float f[8];
        int n = qol_song_notes(0, 0, 0, f);
        f[1] += 2.5f;  /* one note a whole tone off */
        assert(qol_sing_match(f, n, 0) == -1);
    }
    /* the Aria sung with its low note an octave up */
    {
        float f[8];
        int n = qol_song_notes(4, 0, 1, f);
        assert(qol_sing_match(f, n, 0) == 4);
    }
    /* the singer: Wind's Requiem sung at 30 steps per second, 0.3 s per note, a little vibrato, then a pause */
    {
        qol_singer g;
        qol_sing_reset(&g);
        float f[8];
        int n = qol_song_notes(0, 0, 0, f), song = -1;
        for (int i = 0; i < n; i++)
            for (int k = 0; k < 9; k++) song = qol_sing_step(&g, f[i] + 3.0f + (k % 3 - 1) * 0.15f, 1.0f / 30);
        assert(song == -1);
        for (int k = 0; k < 25 && song < 0; k++) song = qol_sing_step(&g, 0, 1.0f / 30);
        assert(song == 0);
        /* too short notes and a lone note: nothing */
        qol_sing_reset(&g);
        song = -1;
        for (int k = 0; k < 2; k++) song = qol_sing_step(&g, 60, 1.0f / 30);
        for (int k = 0; k < 30; k++) if (song < 0) song = qol_sing_step(&g, 0, 1.0f / 30);
        assert(song == -1 && g.count == 0);
    }

    /* figurine names: the first line after its colour tag, the player's name and capital tags, UTF-8 */
    {
        /* <colour 1>Aryll\n<colour ffff><colour 0>Birthplace... */
        const unsigned short aryll[] = {0x0E, 0, 3, 2, 1, 'A', 'r', 'y', 'l', 'l', '\n', 0x0E, 0, 3, 2, 0xFFFF, 'B', 'i', 0};
        char out[64];
        assert(qol_figure_name(aryll, 64, 0, 0, out, sizeof out) == 5 && !strcmp(out, "Aryll"));
        /* <colour 1><capital><player>'s Grandma<colour ffff> */
        const unsigned short grandma[] = {0x0E, 0, 3, 2, 1, 0x0E, 1, 11, 0, 0x0E, 2, 0, 0, '\'', 's', ' ', 'G', 'r', 'a', 'n', 'd', 'm', 'a',
                                          0x0E, 0, 3, 2, 0xFFFF, '\n', 'x', 0};
        const unsigned short link[] = {'l', 'i', 'n', 'k', 0};
        assert(qol_figure_name(grandma, 64, link, 5, out, sizeof out) > 0 && !strcmp(out, "Link's Grandma"));
        const unsigned short empty_player[] = {0};
        assert(qol_figure_name(grandma, 64, empty_player, 1, out, sizeof out) && !strcmp(out, "'s Grandma"));
        /* accents (UTF-8), an end tag, a trailing space, a 3-byte character */
        const unsigned short fr[] = {0x0E, 0, 3, 2, 1, 'F', 0xE9, 'e', 0x0F, 0, 3, ' ', 0x2014, ' ', '\n', 0};
        assert(qol_figure_name(fr, 64, 0, 0, out, sizeof out) == 8 && !strcmp(out, "F\xC3\xA9" "e \xE2\x80\x94"));
        /* capital on an accented letter */
        const unsigned short cap[] = {0x0E, 1, 11, 0, 0xE9, 't', 0};
        assert(qol_figure_name(cap, 64, 0, 0, out, sizeof out) && !strcmp(out, "\xC3\x89t"));
        /* a short buffer: whole characters only */
        assert(qol_figure_name(fr, 64, 0, 0, out, 4) == 3 && !strcmp(out, "F\xC3\xA9"));
        assert(qol_figure_name(fr, 64, 0, 0, out, 3) == 1 && !strcmp(out, "F"));
        /* nothing: an empty message, only tags, a broken tag at the end, no buffer */
        const unsigned short none[] = {0x0E, 0, 3, 2, 1, '\n', 0}, broken[] = {0x0E, 0, 3};
        assert(qol_figure_name(none, 64, 0, 0, out, sizeof out) == 0 && out[0] == 0);
        assert(qol_figure_name(broken, 3, 0, 0, out, sizeof out) == 0);
        assert(qol_figure_name(aryll, 64, 0, 0, out, 0) == 0);
        /* the scan stops at n units */
        assert(qol_figure_name(aryll, 7, 0, 0, out, sizeof out) == 2 && !strcmp(out, "Ar"));
    }

    /* more figurine orders: the waiting list */
    {
        qol_fo_list w = {0}, r;
        assert(qol_fo_add(&w, 5) && qol_fo_add(&w, 0) && qol_fo_add(&w, 133));
        assert(!qol_fo_add(&w, 5) && !qol_fo_add(&w, 134) && w.count == 3 && qol_fo_has(&w, 0) && !qol_fo_has(&w, 1));
        w.stamp = 0xABCD1234u;
        unsigned char rec[16 + 8 + QOL_FO_MAX];
        unsigned bytes = qol_fo_write(rec, &w);
        assert(bytes == 16 + 8 + 3);
        assert(qol_fo_read(rec, (int)bytes, &r) && r.stamp == 0xABCD1234u && r.count == 3 && r.figure[2] == 133);
        assert(!qol_fo_read(rec, (int)bytes - 1, &r));
        rec[16 + 8] = 0;  /* a doubled figure (0 twice): not good; the seal is checked first, so seal again */
        qol_record_seal(rec, QOL_FO_MAGIC, 8 + 3);
        assert(!qol_fo_read(rec, (int)bytes, &r));
        rec[16 + 8] = 200;  /* not a figure */
        qol_record_seal(rec, QOL_FO_MAGIC, 8 + 3);
        assert(!qol_fo_read(rec, (int)bytes, &r));
        qol_fo_list empty = {7, 0, {0}};
        bytes = qol_fo_write(rec, &empty);
        assert(qol_fo_read(rec, (int)bytes, &r) && r.count == 0 && r.stamp == 7);
        /* a full list */
        qol_fo_list full = {0};
        for (unsigned f = 0; f < QOL_FO_MAX; f++) assert(qol_fo_add(&full, QOL_FO_MAX - 1 - f));
        assert(!qol_fo_add(&full, 3) && full.count == QOL_FO_MAX);

        /* to Carlov's list: oldest first while it has room; made or ordered ones are dropped */
        unsigned char made[QOL_FO_MAX] = {0}, out[12], queued[12];
        qol_fo_list t = {0};
        for (unsigned f = 20; f < 40; f++) qol_fo_add(&t, f);  /* 20 waiting */
        made[21] = 1;                                             /* made meanwhile */
        queued[0] = 22;                                           /* on his list meanwhile */
        unsigned n = qol_fo_take(&t, queued, 1, made, out);
        assert(n == 11 && out[0] == 20 && out[1] == 23 && out[10] == 32);
        assert(t.count == 7 && t.figure[0] == 33 && t.figure[6] == 39);
        /* his list full: nothing goes, nothing is dropped */
        for (unsigned k = 0; k < 12; k++) queued[k] = (unsigned char)(100 + k);
        assert(qol_fo_take(&t, queued, 12, made, out) == 0 && t.count == 7);
        /* an empty list of his: up to 12 */
        assert(qol_fo_take(&t, queued, 0, made, out) == 7 && t.count == 0 && out[6] == 39);
        assert(qol_fo_take(&t, queued, 0, made, out) == 0);
    }
    /* the Game Over save window on the GamePad: a tap on Cancel / Save becomes its direction, a gap, then A */
    {
        unsigned out[2 + 3 * QOL_GO_HOLD + 2];
        /* touched at (x, y), then still touching at (ux, uy), then let go, then the reads after */
        #define GO_TAP(t, x, y, ux, uy) do { \
            int k = 0; \
            out[k++] = qol_gameover_tap_step(&(t), 1, 1, x, y); \
            assert(qol_gameover_tap_step(&(t), 1, 1, ux, uy) == 0); \
            while (k < (int)(sizeof out / sizeof *out)) out[k++] = qol_gameover_tap_step(&(t), 1, 0, 0, 0); \
        } while (0)
        qol_gameover_tap t;
        memset(&t, 0, sizeof t);
        GO_TAP(t, 0.35f, 0.69f, 0.35f, 0.69f);  /* Cancel */
        assert(out[0] == 0);
        for (int i = 0; i < QOL_GO_HOLD; i++) assert(out[1 + i] == QOL_GO_LEFT);
        for (int i = 0; i < QOL_GO_HOLD; i++) assert(out[1 + QOL_GO_HOLD + i] == 0);
        for (int i = 0; i < QOL_GO_HOLD; i++) assert(out[1 + 2 * QOL_GO_HOLD + i] == QOL_GO_A);
        assert(out[1 + 3 * QOL_GO_HOLD] == 0);
        GO_TAP(t, 0.65f, 0.69f, 0.65f, 0.69f);  /* Save */
        assert(out[1] == QOL_GO_RIGHT && out[1 + 2 * QOL_GO_HOLD] == QOL_GO_A);
        GO_TAP(t, 0.35f, 0.69f, 0.65f, 0.69f);  /* pressed on Cancel, let go on Save: nothing */
        for (unsigned i = 0; i < sizeof out / sizeof *out; i++) assert(out[i] == 0);
        GO_TAP(t, 0.5f, 0.3f, 0.5f, 0.3f);      /* the window's text: nothing */
        for (unsigned i = 0; i < sizeof out / sizeof *out; i++) assert(out[i] == 0);
        /* not at the prompt: nothing, and a tap begun there doesn't carry over */
        assert(qol_gameover_tap_step(&t, 0, 1, 0.35f, 0.69f) == 0 && qol_gameover_tap_step(&t, 0, 0, 0, 0) == 0);
        assert(qol_gameover_tap_step(&t, 1, 0, 0, 0) == 0);
        #undef GO_TAP
        /* from a GamePad claim's events: down and up within one read still make the tap */
        memset(&t, 0, sizeof t);
        qol_gameover_tap_event(&t, 0, 0.35f, 0.69f);
        qol_gameover_tap_event(&t, 2, 0.36f, 0.70f);
        assert(qol_gameover_tap_step(&t, 1, t.down, t.x, t.y) == QOL_GO_LEFT);
        for (int i = 1; i < 3 * QOL_GO_HOLD; i++) (void)qol_gameover_tap_step(&t, 1, t.down, t.x, t.y);
        assert(qol_gameover_tap_step(&t, 1, t.down, t.x, t.y) == 0);
        /* held across reads, then lifted on Save */
        qol_gameover_tap_event(&t, 0, 0.65f, 0.69f);
        assert(qol_gameover_tap_step(&t, 1, t.down, t.x, t.y) == 0);
        qol_gameover_tap_event(&t, 1, 0.66f, 0.70f);
        assert(qol_gameover_tap_step(&t, 1, t.down, t.x, t.y) == 0);
        qol_gameover_tap_event(&t, 2, 0.66f, 0.70f);
        assert(qol_gameover_tap_step(&t, 1, t.down, t.x, t.y) == QOL_GO_RIGHT);
        /* slid off the button, or cancelled: nothing */
        memset(&t, 0, sizeof t);
        qol_gameover_tap_event(&t, 0, 0.35f, 0.69f);
        qol_gameover_tap_event(&t, 2, 0.35f, 0.20f);
        assert(qol_gameover_tap_step(&t, 1, t.down, t.x, t.y) == 0);
        qol_gameover_tap_event(&t, 0, 0.35f, 0.69f);
        qol_gameover_tap_event(&t, 3, 0.35f, 0.69f);
        qol_gameover_tap_event(&t, 2, 0.35f, 0.69f);
        assert(qol_gameover_tap_step(&t, 1, t.down, t.x, t.y) == 0);
    }
    return 0;
}
