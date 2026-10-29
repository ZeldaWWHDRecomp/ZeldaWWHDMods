#include <assert.h>
#include <math.h>
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
    return 0;
}
