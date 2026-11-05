/* Bounded, on-screen observations for 1.0.0. No game writes or file logging. */
#pragma once
enum {
    DL_PROBE_OK, DL_PROBE_NO_DRAW, DL_PROBE_FROZEN, DL_PROBE_MODEL_DATA,
    DL_PROBE_MATERIALS, DL_PROBE_FMAT, DL_PROBE_SAMPLERS, DL_PROBE_NO_ALBEDO,
    DL_PROBE_INDEX, DL_PROBE_RESOURCE, DL_PROBE_REFS, DL_PROBE_TEXTURE,
    DL_PROBE_CAPACITY, DL_PROBE_DUPLICATE
};
typedef struct {
    u32 draws, shapes[4], materials, native_materials, albedo, views, eyes, eye_materials;
    u32 reason, skipped;
    char body_samplers[48], eye_samplers[48];
} DLAppearanceProbe;
static DLAppearanceProbe dl_probe = {.reason=DL_PROBE_NO_DRAW};
static int dl_diagnostics = 0;
static void dl_diag_hud(u32 list);
static void dl_probe_increment(u32* value) { if (*value < 999999u) ++*value; }
static void dl_probe_skip(u32 reason) {
    if (!dl_probe.skipped) dl_probe.reason = reason;
    dl_probe_increment(&dl_probe.skipped);
}
static char dl_probe_lines[4][192];
static u32 dl_probe_append(char* line, u32 n, const char* s) {
    while (*s && n < 191) line[n++] = *s++;
    line[n] = 0; return n;
}
static u32 dl_probe_number(char* line, u32 n, u32 value) {
    char digits[10]; u32 count = 0;
    do { digits[count++] = '0' + value % 10; value /= 10; } while (value && count < 10);
    while (count && n < 191) line[n++] = digits[--count];
    line[n] = 0; return n;
}

static u32 dl_probe_hex(char* line, u32 n, u32 value) {
    static const char digits[] = "0123456789ABCDEF";
    for (int shift = 28; shift >= 0 && n < 191; shift -= 4) line[n++] = digits[(value >> shift) & 15];
    line[n] = 0; return n;
}
