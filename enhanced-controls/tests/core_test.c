/* SPDX-License-Identifier: MPL-2.0 */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "../src/core.h"

int main(void) {
    EcState s; ec_init(&s);
    ec_start(&s,EC_JUMP,0);
    assert(ec_buttons(&s)==EC_ZL);
    ec_tick(&s,.07); assert(ec_buttons(&s)==(EC_ZL|EC_A));
    ec_tick(&s,.14); assert(ec_buttons(&s)==EC_ZL);
    ec_tick(&s,.06); assert(!ec_buttons(&s));
    ec_start(&s,EC_SLASH,0); ec_tick(&s,.07); assert(ec_buttons(&s)==(EC_ZL|EC_B));
    /* Spin is a one-shot native request, never a pad/stick macro at 30/60 Hz. */
    for (int hz=30; hz<=60; hz+=30) {
        ec_init(&s); ec_start(&s,EC_SPIN,0);
        float x=.3f,y=.4f;
        assert(!ec_buttons(&s) && !ec_stick(&s,&x,&y) && x==.3f && y==.4f);
        assert(ec_take_spin(&s) && !ec_take_spin(&s));
        for(int frame=0;frame<hz;++frame) {
            ec_tick(&s,1.0/hz);
            assert(!ec_buttons(&s) && !ec_stick(&s,&x,&y) && !ec_take_spin(&s));
        }
        assert(!ec_active(&s));
        ec_start(&s,EC_SPIN,0); ec_cancel(&s); assert(!ec_take_spin(&s));
        ec_start(&s,EC_SPIN,0); ec_start(&s,EC_SLASH,0); assert(!ec_take_spin(&s));
        ec_start(&s,EC_SPIN,0); ec_tick(&s,.2); ec_tick(&s,.1); assert(!ec_take_spin(&s));
    }
    ec_start(&s,EC_DODGE,-.8f); assert(s.dodge_x==-1 && s.dodge_y==0);
    ec_start(&s,EC_DODGE,0); assert(s.dodge_x==0 && s.dodge_y==-1);
    ec_cancel(&s); assert(!ec_buttons(&s) && s.macro==EC_NONE);
    EcRect r={-150,-180,88,60}; assert(ec_inside(r,-150,-180)); assert(!ec_inside(r,0,0));
    assert(ec_clamp(5,0,1)==1 && ec_clamp(NAN,0,1)==0);
    assert(ec_d2(1,2,4,6)==25);
    puts("Enhanced core: macro timing, native spin requests and cancellation passed");
}
