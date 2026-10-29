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
    return 0;
}
