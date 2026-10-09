#pragma once
/* Freestanding guest math; no unresolvable host-libm imports. Angles reduced
 * before Taylor evaluation; flight uses a bounded yaw after each input step. */
#define DRAGON_PI 3.14159265358979323846f
static inline float dragon_wrap(float x) {
    while(x>DRAGON_PI)x-=2*DRAGON_PI;
    while(x<-DRAGON_PI)x+=2*DRAGON_PI;
    return x;
}
static inline float dragon_sin(float x) {
    x=dragon_wrap(x);
    if(x>DRAGON_PI*.5f)x=DRAGON_PI-x;
    if(x<-DRAGON_PI*.5f)x=-DRAGON_PI-x;
    float q=x*x;
    return x*(1+q*(-1.0f/6+q*(1.0f/120+q*(-1.0f/5040+q*(1.0f/362880+q*(-1.0f/39916800))))));
}
static inline float dragon_cos(float x) {return dragon_sin(x+DRAGON_PI*.5f);}
static inline float dragon_clamp(float x,float low,float high) {return x<low?low:x>high?high:x;}
static inline int dragon_finite(float x) {return x==x && x<=3.402823466e38f && x>=-3.402823466e38f;}
static inline float dragon_sqrt(float x) {
    if(x<=0)return 0;
    union {float f;unsigned u;} guess={x};guess.u=(guess.u>>1)+0x1fc00000;
    float r=guess.f;
    for(unsigned i=0;i<5;++i)r=(r+x/r)*.5f;
    return r;
}

static inline float dragon_blend(float rate,float delta) {
    if(delta==.5f)return 1-dragon_sqrt(1-rate);
    return dragon_clamp(rate*delta,0,1);
}
