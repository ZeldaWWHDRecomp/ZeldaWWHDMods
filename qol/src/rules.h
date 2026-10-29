/* QoL package: pure rules, without the game or the SDK, so qol/tests can check them on the host (test_rules.py).
 * Only plain C types here. */
#pragma once

/* sine and cosine of a game angle (0x10000 a full turn): a 7th-order series on -90..90 degrees (error below 2e-4) */
static float qol_sin(short a) {
    int i = a;
    if (i > 0x4000) i = 0x8000 - i;
    else if (i < -0x4000) i = -0x8000 - i;
    float x = (float)i * (3.14159265f / 32768.0f), x2 = x * x;
    return x * (1.0f - x2 / 6.0f * (1.0f - x2 / 20.0f * (1.0f - x2 / 42.0f)));
}
static float qol_cos(short a) { return qol_sin((short)(a + 0x4000)); }

/* the Cabana's picture puzzles (4 x 4, square 0 top left): the pieces from `square` to the gap slide one by one; the
 * stick pushes the way they move (left: the piece right of the gap moves left; up: the one below it moves up).
 * Returns how many pieces (0: not in the gap's row or column, or the gap itself) and the push in *lx, *ly. */
static int qol_puzzle_slide(int gap, int square, float* lx, float* ly) {
    *lx = 0, *ly = 0;
    if (gap < 0 || gap > 15 || square < 0 || square > 15 || square == gap) return 0;
    if (square / 4 == gap / 4) {
        *lx = square > gap ? -1.0f : 1.0f;
        return square > gap ? square - gap : gap - square;
    }
    if (square % 4 == gap % 4) {
        *ly = square > gap ? 1.0f : -1.0f;
        return (square > gap ? square - gap : gap - square) / 4;
    }
    return 0;
}

/* the point of n nearest to (x, y), when nearer than sqrt(reach2); -1 none. ok[i] 0: point i is not on the picture */
static int qol_nearest(const float* xs, const float* ys, const int* ok, int n, float x, float y, float reach2) {
    int best = -1;
    float bd = 1e30f;
    for (int i = 0; i < n; i++) {
        if (ok && !ok[i]) continue;
        float d = (xs[i] - x) * (xs[i] - x) + (ys[i] - y) * (ys[i] - y);
        if (d < bd) bd = d, best = i;
    }
    return bd <= reach2 ? best : -1;
}
