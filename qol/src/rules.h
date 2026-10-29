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

/* ---- album pages: side files (pages.c). Every file is one record: a 16-byte header (magic, version, length of the
 * bytes after it, their FNV-1a sum; big-endian on every host) and the bytes. A record that fails its checks is ignored. */
#define QOL_PG_VERSION 1u
#define QOL_PG_SLOT_MAGIC 0x51505331u   /* "QPS1": one picto slot (its 0x40-byte header and its JPEG) */
#define QOL_PG_ORDER_MAGIC 0x51504F31u  /* "QPO1": a page's order block (0x40 bytes) */
#define QOL_PG_INDEX_MAGIC 0x51504931u  /* "QPI1": a save file's pages */
#define QOL_PG_HEAD 16u
static void qol_put32(unsigned char* p, unsigned v) { p[0] = (unsigned char)(v >> 24), p[1] = (unsigned char)(v >> 16), p[2] = (unsigned char)(v >> 8), p[3] = (unsigned char)v; }
static unsigned qol_get32(const unsigned char* p) { return (unsigned)p[0] << 24 | (unsigned)p[1] << 16 | (unsigned)p[2] << 8 | p[3]; }
static unsigned qol_sum(const unsigned char* p, unsigned n) {
    unsigned h = 2166136261u;
    for (unsigned i = 0; i < n; i++) h = (h ^ p[i]) * 16777619u;
    return h;
}
/* seals the record at rec: its `len` bytes are already at rec + QOL_PG_HEAD */
static void qol_record_seal(unsigned char* rec, unsigned magic, unsigned len) {
    qol_put32(rec, magic), qol_put32(rec + 4, QOL_PG_VERSION), qol_put32(rec + 8, len);
    qol_put32(rec + 12, qol_sum(rec + QOL_PG_HEAD, len));
}
/* the record's length when `got` bytes read from a file hold a whole, good record of that magic (at most max bytes), else -1 */
static int qol_record_check(const unsigned char* rec, int got, unsigned magic, unsigned max) {
    if (got < (int)QOL_PG_HEAD || qol_get32(rec) != magic || qol_get32(rec + 4) != QOL_PG_VERSION) return -1;
    unsigned len = qol_get32(rec + 8);
    if (len > max || (unsigned)got != QOL_PG_HEAD + len || qol_get32(rec + 12) != qol_sum(rec + QOL_PG_HEAD, len)) return -1;
    return (int)len;
}

/* a save file's pages: two copies (a, b) written in turn, the newer good one counts */
typedef struct {
    unsigned gen;    /* one more each write */
    unsigned set;    /* the files' set: a save that is not this one's starts a new set (the old files stay) */
    unsigned count;  /* pages, the game's own album (page 1) included */
    unsigned fpn, fp[12];  /* the game's album at its last save: its pictos' checksums, sorted */
} qol_pg_index;
#define QOL_PG_INDEX_LEN (16u * 4u)
static void qol_pg_index_write(unsigned char* rec, const qol_pg_index* x) {
    unsigned char* d = rec + QOL_PG_HEAD;
    qol_put32(d, x->gen), qol_put32(d + 4, x->set), qol_put32(d + 8, x->count), qol_put32(d + 12, x->fpn);
    for (unsigned i = 0; i < 12; i++) qol_put32(d + 16 + 4 * i, i < x->fpn ? x->fp[i] : 0);
    qol_record_seal(rec, QOL_PG_INDEX_MAGIC, QOL_PG_INDEX_LEN);
}
static int qol_pg_index_read(const unsigned char* rec, int got, qol_pg_index* x) {
    if (qol_record_check(rec, got, QOL_PG_INDEX_MAGIC, QOL_PG_INDEX_LEN) != (int)QOL_PG_INDEX_LEN) return 0;
    const unsigned char* d = rec + QOL_PG_HEAD;
    x->gen = qol_get32(d), x->set = qol_get32(d + 4), x->count = qol_get32(d + 8), x->fpn = qol_get32(d + 12);
    if (x->count < 1 || x->count > 999 || x->fpn > 12) return 0;
    for (unsigned i = 0; i < 12; i++) x->fp[i] = qol_get32(d + 16 + 4 * i);
    return 1;
}
/* the pictos' checksums (sums[i] for used[i]), sorted, into fp; returns how many */
static unsigned qol_fp_make(const unsigned* sums, const int* used, unsigned* fp) {
    unsigned n = 0;
    for (int i = 0; i < 12; i++) {
        if (!used[i]) continue;
        unsigned v = sums[i], j = n++;
        while (j > 0 && fp[j - 1] > v) fp[j] = fp[j - 1], j--;
        fp[j] = v;
    }
    return n;
}
/* the same album: both empty, or at least one picto in common (pictos taken or deleted since still match) */
static int qol_fp_same(const unsigned* a, unsigned na, const unsigned* b, unsigned nb) {
    if (!na && !nb) return 1;
    for (unsigned i = 0, j = 0; i < na && j < nb;) {
        if (a[i] == b[j]) return 1;
        if (a[i] < b[j]) i++;
        else j++;
    }
    return 0;
}
/* the order of the album's pictos when a page's order record is lost: the used slots in slot order, then none (-1) */
static void qol_order_rebuild(const int* used, unsigned char* order12) {
    int n = 0;
    for (int i = 0; i < 12; i++)
        if (used[i]) order12[n++] = (unsigned char)i;
    while (n < 12) order12[n++] = 0xFF;
}
/* the page a turn leads to: dir -1 / +1 from cur of count; past the last page a new, empty one when cur holds a picto
 * (count is then one more); -1 no turn */
static int qol_pg_turn(int cur, int count, int dir, int cur_used) {
    int to = cur + dir;
    if (to < 0) return -1;
    if (to < count) return to;
    return to == count && cur_used > 0 ? to : -1;
}
