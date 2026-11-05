/* SPDX-License-Identifier: MPL-2.0 */
#include "core.h"
static int same(EhpKey a, EhpKey b) {
    return a.address == b.address && a.id == b.id && a.profile == b.profile;
}
void ehp_clear(EhpState *s) { *s = (EhpState){0}; }
float ehp_distance2(EhpVec a, EhpVec b) {
    float x = a.x-b.x, y = a.y-b.y, z = a.z-b.z;
    return x*x+y*y+z*z;
}
void ehp_tick(EhpState *s, float dt) {
    if (!(dt > 0 && dt <= .25f)) return;
    for (int i=0; i<EHP_ACTORS; ++i) {
        EhpEnemy *e = &s->enemies[i];
        if (!e->used) continue;
        e->unseen += dt;
        if (e->since_hit < 10) e->since_hit += dt;
        if (e->unseen > .5f) e->used = 0;
    }
    for (int i=0; i<EHP_POPUPS; ++i) {
        EhpPopup *p = &s->popups[i];
        if (p->damage && (p->age += dt) >= EHP_LIFETIME) p->damage = 0;
    }
}
void ehp_forget(EhpState *s, EhpKey key) {
    for (int i=0; i<EHP_ACTORS; ++i)
        if (s->enemies[i].used && same(s->enemies[i].key, key)) s->enemies[i].used = 0;
    /* Popups contain position snapshots and remain valid after actor deletion. */
}
void ehp_observe(EhpState *s, EhpKey key, EhpVec position, int hp, int maximum, int numbers) {
    if (!key.address || !key.id || key.id == ~0u) return;
    EhpEnemy *found = 0, *free_slot = 0;
    for (int i=0; i<EHP_ACTORS; ++i) {
        EhpEnemy *e = &s->enemies[i];
        if (e->used && same(e->key, key)) { found=e; break; }
        /* Memory reuse never transfers the previous actor's HP baseline. */
        if (e->used && e->key.address == key.address) e->used=0;
        if (!e->used && !free_slot) free_slot=e;
    }
    if (hp < 0) hp=0;
    if (!found) {
        if (!free_slot || !hp) return;
        found=free_slot;
        *found=(EhpEnemy){.key=key, .used=1, .hp=hp, .since_hit=10};
    } else if (hp < found->hp) {
        if (numbers) {
            unsigned n=s->cursor++;
            s->popups[n%EHP_POPUPS]=(EhpPopup){position, found->hp-hp, 0, n};
        }
        found->since_hit=0;
    }
    if (maximum < hp) maximum=hp;
    if (maximum > found->max_hp) found->max_hp=maximum;
    found->hp=hp;
    found->position=position;
    found->unseen=0;
}
int ehp_nearest(const EhpState *s, EhpVec player, float distance, int after_hit, int *indices) {
    float distances[EHP_BARS];
    int count=0;
    for (int i=0; i<EHP_ACTORS; ++i) {
        const EhpEnemy *e=&s->enemies[i];
        if (!e->used || e->hp<=0 || e->max_hp<=0 || e->unseen>.1f ||
            (after_hit && e->since_hit>4)) continue;
        float d=ehp_distance2(player,e->position);
        if (!(d<=distance*distance)) continue;
        int at=count;
        if (at==EHP_BARS) {
            if (d>=distances[at-1]) continue;
            --at;
        } else ++count;
        while (at>0 && d<distances[at-1]) {
            indices[at]=indices[at-1]; distances[at]=distances[at-1]; --at;
        }
        indices[at]=i; distances[at]=d;
    }
    return count;
}
