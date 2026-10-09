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
    return 0;
}
