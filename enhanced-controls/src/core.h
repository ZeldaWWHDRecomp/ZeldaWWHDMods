/* SPDX-License-Identifier: MPL-2.0
 * Pure combat input policy. Timings adapted from the Enhanced fork and the
 * enhanced-on-main port; no host internals or game assets are required. */
#ifndef ENHANCED_CONTROLS_CORE_H
#define ENHANCED_CONTROLS_CORE_H

enum { EC_JUMP, EC_SLASH, EC_SPIN, EC_DODGE, EC_NONE = -1 };
enum { EC_A = 0x8000, EC_B = 0x4000, EC_ZL = 0x80 };
#include "editor_geometry.h"
typedef struct {
    int spin_pending, macro;
    double now, macro_start;
    float dodge_x, dodge_y;
} EcState;
static float ec_clamp(float v, float lo, float hi) {
    if (!(v >= lo)) return lo;
    return v > hi ? hi : v;
}
static void ec_cancel(EcState* s) {
    s->macro = EC_NONE; s->spin_pending = 0;
}
static void ec_init(EcState* s) {
    *s = (EcState){0}; ec_cancel(s);
}
/* Spin uses this timer only for pending-request expiry and button feedback. */
static int ec_active(EcState* s) {
    if (s->macro != EC_NONE && s->now-s->macro_start>=.26) {
        s->macro = EC_NONE; s->spin_pending = 0;
    }
    return s->macro != EC_NONE;
}
static void ec_tick(EcState* s, double dt) {
    if (dt > 0 && dt <= .25) s->now += dt;
    ec_active(s);
}
static void ec_start(EcState* s, int which, float stick_x) {
    s->macro = which; s->macro_start = s->now;
    s->spin_pending = which == EC_SPIN;
    s->dodge_x = stick_x > .35f ? 1 : stick_x < -.35f ? -1 : 0;
    s->dodge_y = s->dodge_x == 0 ? -1 : 0; /* VPAD +y forward */
}
static unsigned ec_buttons(EcState* s) {
    unsigned bits = 0;
    if (ec_active(s)) {
        double t = s->now-s->macro_start;
        if (s->macro != EC_SPIN) {
            bits |= EC_ZL;
            if (t >= .06 && t < .20) bits |= s->macro == EC_SLASH ? EC_B : EC_A;
        }
    }
    return bits;
}
/* Only dodge supplies stick input. A native spin leaves physical input alone. */
static int ec_stick(EcState* s, float* x, float* y) {
    if (!ec_active(s)) return 0;
    if (s->macro==EC_DODGE) { *x=s->dodge_x; *y=s->dodge_y; return 1; }
    return 0;
}
static int ec_take_spin(EcState* s) {
    if (!ec_active(s) || !s->spin_pending) return 0;
    s->spin_pending = 0; return 1;
}
#endif
