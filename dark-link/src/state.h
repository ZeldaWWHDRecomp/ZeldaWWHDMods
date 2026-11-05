/* Original Dark Link gameplay state; independent of guest memory for host tests. */
#pragma once

typedef struct {
    int capacity, gain, duration, damage_percent, free_transform;
} DLConfig;
typedef struct {
    float charge, remaining, total;
    int active, duration_scale;
} DLState;

static void dl_reset(DLState* s) { *s = (DLState){0}; }
/* Match the HUD's short/long magic capacity variants. Charge requirements
 * are independent. Preserve the remaining fraction if capacity changes while
 * transformed; this must neither refill nor repeatedly multiply the timer. */
static void dl_sync_magic_capacity(DLState* s, unsigned magic_capacity) {
    int scale=magic_capacity>16?2:1;
    int previous=s->duration_scale?s->duration_scale:1;
    if(s->active && scale!=previous) {
        float ratio=(float)scale/(float)previous;
        s->remaining*=ratio;s->total*=ratio;
    }
    s->duration_scale=scale;
}
static void dl_hit(DLState* s, const DLConfig* c, int lost_health) {
    if (!lost_health || s->active || c->free_transform) return;
    s->charge += (float)c->gain;
    if (s->charge > c->capacity) s->charge = (float)c->capacity;
}
static int dl_ready(const DLState* s, const DLConfig* c) {
    return c->free_transform || s->charge >= c->capacity;
}
static void dl_tick(DLState* s, const DLConfig* c, float dt, int request, int allowed) {
    if (s->charge > c->capacity) s->charge = (float)c->capacity;
    if (!allowed) return;
    if (request) {
        if (s->active) {
            /* Manual cancellation returns the displayed remaining fraction
             * to charge. Expiry/death/disable still use dl_reset. Keep the
             * sampled total: settings may have changed during this run. */
            float fraction=s->total>0?s->remaining/s->total:0;
            if(!(fraction>0)) fraction=0;
            if(fraction>1) fraction=1;
            s->charge=fraction*(float)c->capacity;
            s->active=0;s->remaining=0;s->total=0;
            return;
        }
        if (dl_ready(s, c)) {
            s->active = 1;
            s->remaining = (float)c->duration * (s->duration_scale?s->duration_scale:1);
            s->total = s->remaining;
            s->charge = 0;
            return;
        }
    }
    if (s->active && !c->free_transform && dt > 0.0f && dt <= 0.25f) {
        s->remaining -= dt;
        if (s->remaining <= 0.0001f) dl_reset(s);
    }
}
/* Native cc_at_check tests damage as signed 8-bit; values >=128 heal/wrap. */
static unsigned dl_damage(unsigned original, unsigned percent) {
    if (!original || original >= 128) return original;
    unsigned scaled = (original * percent + 99) / 100;
    return scaled > 127 ? 127 : scaled;
}
