#include <assert.h>
#include <math.h>
#include "../src/flight_math.h"
int main(void) {
    for(int i=-10000;i<=10000;++i) {
        float angle=i*DRAGON_PI/10000;
        assert(fabsf(dragon_sin(angle)-sinf(angle))<.000002f);
        assert(fabsf(dragon_cos(angle)-cosf(angle))<.000002f);
    }
    for(float value=.001f;value<1e12f;value*=1.1f)
        assert(fabsf(dragon_sqrt(value)/sqrtf(value)-1)<.000001f);
    assert(!dragon_finite(INFINITY) && !dragon_finite(NAN));
    assert(dragon_finite(0) && dragon_clamp(-1,0,2)==0 && dragon_clamp(3,0,2)==2);
    float at30=0,at60=0;
    for(unsigned i=0;i<60;++i)at30+=(1-at30)*dragon_blend(.08f,1);
    for(unsigned i=0;i<120;++i)at60+=(1-at60)*dragon_blend(.08f,.5f);
    assert(fabsf(at30-at60)<.000002f);
    float clock30=0,clock60=0;
    unsigned throw30=0,throw60=0;
    for(unsigned i=0;i<90;++i) {float before=clock30;clock30+=1;throw30+=before<8 && clock30>=8;}
    for(unsigned i=0;i<180;++i) {float before=clock60;clock60+=.5f;throw60+=before<8 && clock60>=8;}
    assert(clock30==90 && clock60==90 && throw30==1 && throw60==1);
    return 0;
}
