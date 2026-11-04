/* SPDX-License-Identifier: MPL-2.0 */
#include "core.h"
#include <assert.h>
#include <stdio.h>
static EhpState s;
static const EhpKey k={0x12000000,42,0xBC};
static const EhpVec p={0,100,0};
static int popup_count(void) {
    int n=0; for(int i=0;i<EHP_POPUPS;++i) n+=s.popups[i].damage!=0; return n;
}
int main(void) {
    ehp_observe(&s,k,p,12,12,1); assert(!popup_count());
    ehp_observe(&s,k,p,8,12,1); assert(s.popups[0].damage==4);
    ehp_observe(&s,k,p,8,12,1); assert(popup_count()==1);
    ehp_observe(&s,k,p,12,12,1); assert(popup_count()==1); /* healing */
    ehp_observe(&s,k,p,-4,12,1); assert(s.popups[1].damage==12); /* overkill */
    int indices[EHP_BARS];
    assert(ehp_nearest(&s,p,2000,0,indices)==0);
    ehp_forget(&s,k); assert(popup_count()==2); /* kill number survives */
    for(int i=0;i<31;++i) ehp_tick(&s,1.f/30);
    assert(!popup_count());
    ehp_observe(&s,k,p,10,0,1); assert(s.enemies[0].max_hp==10);
    EhpKey reused=k; reused.id++;
    ehp_observe(&s,reused,p,2,2,1); assert(!popup_count());
    ehp_observe(&s,reused,p,1,2,0); assert(!popup_count());
    assert(ehp_nearest(&s,p,100,1,indices)==1);
    ehp_clear(&s);
    /* Same final HP after a transient stun does not produce a damage number. */
    ehp_observe(&s,k,p,8,8,1); ehp_observe(&s,k,p,8,8,1);
    assert(!popup_count()); assert(!ehp_nearest(&s,p,100,1,indices));
    /* Nearest selection is bounded, ordered, and excludes out-of-range actors. */
    ehp_clear(&s);
    for(unsigned i=0;i<EHP_ACTORS+30;++i) {
        EhpKey a={0x12000000+i*0x1000,1+i,0xBC};
        EhpVec v={(float)(EHP_ACTORS-i)*10,0,0};
        ehp_observe(&s,a,v,8,8,1);
    }
    EhpVec zero={0};
    int n=ehp_nearest(&s,zero,4000,0,indices); assert(n==EHP_BARS);
    for(int i=1;i<n;++i) assert(ehp_distance2(s.enemies[indices[i-1]].position,zero)<=ehp_distance2(s.enemies[indices[i]].position,zero));
    assert(ehp_nearest(&s,zero,15,0,indices)==1);
    for(int i=0;i<20;++i) ehp_tick(&s,1.f/30);
    assert(!ehp_nearest(&s,zero,4000,0,indices));
    ehp_clear(&s);
    for(unsigned i=0;i<EHP_POPUPS+20;++i) {
        ehp_observe(&s,k,p,10,10,1); ehp_observe(&s,k,p,9,10,1);
    }
    assert(popup_count()==EHP_POPUPS);
    ehp_clear(&s); assert(!popup_count());
    puts("HP tracking, overkill, healing, identity reuse, expiry and capacity tests passed");
}
