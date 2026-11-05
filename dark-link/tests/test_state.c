#include <assert.h>
#include <stdio.h>
#include "state.h"

static void test_magic_duration(void) {
    const unsigned capacities[]={0,16,32};
    const int durations[]={7,20,300};
    for(unsigned d=0;d<3;++d) for(unsigned m=0;m<3;++m) {
        DLConfig c={100,5,durations[d],200,0};DLState s={0};
        dl_sync_magic_capacity(&s,capacities[m]);
        for(int i=0;i<19;++i)dl_hit(&s,&c,1);
        dl_tick(&s,&c,0,1,1);assert(!s.active && s.charge==95);
        dl_hit(&s,&c,1);dl_tick(&s,&c,0,1,1);
        int seconds=durations[d]*(m==2?2:1);
        assert(s.active && s.total==seconds && s.remaining==seconds && !s.charge);
        for(int i=0;i<seconds*4-1;++i) {
            dl_sync_magic_capacity(&s,capacities[m]);dl_tick(&s,&c,0.25f,0,1);
        }
        assert(s.active && s.remaining==0.25f);
        dl_tick(&s,&c,0.25f,0,1);assert(!s.active && !s.charge);
    }
    DLConfig c={100,5,20,200,0};DLState s={.charge=50};
    dl_sync_magic_capacity(&s,32);assert(s.charge==50); /* no extra hits */
    dl_sync_magic_capacity(&s,16);s.charge=100;dl_tick(&s,&c,0,1,1);
    for(int i=0;i<20;++i)dl_tick(&s,&c,0.25f,0,1);
    assert(s.remaining==15 && s.total==20);
    c.duration=7; /* base setting still applies only at next activation */
    dl_sync_magic_capacity(&s,32);assert(s.remaining==30 && s.total==40);
    for(int i=0;i<100;++i) {dl_sync_magic_capacity(&s,32);dl_tick(&s,&c,0.25f,0,0);}
    assert(s.remaining==30 && s.total==40); /* no refill, no pause drain */
    dl_sync_magic_capacity(&s,16);assert(s.remaining==15 && s.total==20);
    dl_tick(&s,&c,0,1,1);assert(!s.active);
    dl_sync_magic_capacity(&s,32);s.charge=100;dl_tick(&s,&c,0,1,1);
    assert(s.remaining==14 && s.total==14);
    c.free_transform=1;
    for(int i=0;i<1000;++i)dl_tick(&s,&c,0.25f,0,1);
    assert(s.active && s.remaining==14);
    dl_tick(&s,&c,0,1,1);assert(!s.active);
}

static void test_cancel_charge(void) {
    for(int scale=1;scale<=2;++scale) for(int quarter=0;quarter<=4;++quarter) {
        DLConfig c={100,5,20,200,0};DLState s={.charge=100};
        dl_sync_magic_capacity(&s,scale==2?32:16);dl_tick(&s,&c,0,1,1);
        s.remaining=s.total*quarter/4;
        c.duration=7; /* cancellation uses the active total, not this setting */
        dl_tick(&s,&c,0.25f,1,1);
        assert(!s.active && !s.remaining && !s.total && s.charge==quarter*25);
        for(int i=0;i<100;++i)dl_tick(&s,&c,0.25f,0,1);
        assert(s.charge==quarter*25); /* inactive charge does not drain */
        dl_hit(&s,&c,0);assert(s.charge==quarter*25);
        if(quarter<4) {dl_tick(&s,&c,0,1,1);assert(!s.active);}
        for(int i=0;i<(4-quarter)*5;++i)dl_hit(&s,&c,1);
        assert(s.charge==100);dl_tick(&s,&c,0,1,1);
        assert(s.active && s.total==7*scale);
    }
    DLConfig c={250,10,20,200,0};DLState s={.active=1,.remaining=15,.total=20};
    dl_tick(&s,&c,0,1,0);assert(s.active && !s.charge); /* paused request */
    dl_tick(&s,&c,0,1,1);assert(s.charge==187.5f);
    dl_sync_magic_capacity(&s,32);assert(s.charge==187.5f);
    for(int i=0;i<7;++i)dl_hit(&s,&c,1);
    assert(s.charge==250);dl_tick(&s,&c,0,1,1);assert(s.total==40);
    dl_reset(&s);assert(!s.charge && !s.active);
    s=(DLState){.active=1,.remaining=0.25f,.total=40};
    dl_tick(&s,&c,0.25f,0,1);assert(!s.active && !s.charge); /* natural expiry */
    s=(DLState){.active=1,.remaining=30,.total=20};
    dl_tick(&s,&c,0,1,1);assert(s.charge==250); /* bounded fraction */
    s=(DLState){.active=1,.remaining=10};
    dl_tick(&s,&c,0,1,1);assert(!s.charge); /* no invalid division */
}

int main(void) {
    test_magic_duration();
    test_cancel_charge();
    DLConfig c = {100, 5, 20, 200, 0};
    DLState s = {0};
    dl_hit(&s, &c, 0); assert(s.charge == 0);
    for (int i = 0; i < 19; ++i) dl_hit(&s, &c, 1);
    dl_tick(&s, &c, 0, 1, 1); assert(!s.active && s.charge == 95);
    dl_hit(&s, &c, 1); assert(dl_ready(&s, &c));
    dl_hit(&s, &c, 1); assert(s.charge == 100);
    dl_tick(&s, &c, 0, 1, 0); assert(!s.active); /* paused input */
    dl_tick(&s, &c, 0, 1, 1); assert(s.active && s.remaining == 20);
    dl_hit(&s, &c, 1); assert(s.charge == 0); /* no charge while transformed */
    for (int i = 0; i < 900; ++i) dl_tick(&s, &c, 1.0f/30, 0, 0);
    assert(s.remaining == 20); /* menus/cutscenes don't drain */
    for (int fps = 30; fps <= 60; fps += 30) {
        s = (DLState){.active=1, .remaining=20};
        for (int i = 0; i < fps * 20 - 1; ++i) dl_tick(&s, &c, 1.0f/fps, 0, 1);
        assert(s.active);
        for (int i = 0; i < 2; ++i) dl_tick(&s, &c, 1.0f/fps, 0, 1);
        assert(!s.active && s.charge == 0);
    }
    c.free_transform = 1;
    dl_tick(&s, &c, 0, 1, 1); assert(s.active);
    for (int i = 0; i < 3000; ++i) dl_tick(&s, &c, 1.0f/30, 0, 1);
    assert(s.active && s.remaining == 20);
    dl_tick(&s, &c, 0, 1, 1); assert(!s.active); /* manual cancellation */
    c.free_transform = 0; c.capacity = 25;
    s.charge = 100; dl_tick(&s, &c, 0, 0, 1); assert(s.charge == 25);
    dl_reset(&s); assert(!s.active && !s.charge && !s.remaining);
    assert(dl_damage(0, 500) == 0);
    assert(dl_damage(3, 200) == 6);
    assert(dl_damage(3, 125) == 4);
    assert(dl_damage(100, 500) == 127);
    assert(dl_damage(128, 200) == 128); /* native sentinel stays intact */
    puts("Dark Link gameplay tests passed");
}
