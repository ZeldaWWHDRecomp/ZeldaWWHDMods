// Original Call of the Sky melody, synthesized locally. No game audio payloads.
#include "dragon_song.h"
#include "dragon.h"
#include <atomic>
#include <algorithm>
#include <cmath>
#include <vector>
namespace dragon::song {
namespace { std::atomic<unsigned> revision{0}; std::atomic<bool> active{false}; std::atomic<unsigned> position{0}; }
void start() { position.store(0); revision.fetch_add(1); active.store(true); }
void tick(unsigned value) { position.store(value); }
void stop() { active.store(false); }
const int16_t* mix(const int16_t* source,int frames) {
    if(!enabled() || !active.load()) return source;
    // Only the audio producer owns phase and scratch storage.
    static unsigned seen=0; static double phase=0; static std::vector<int16_t> buffer;
    unsigned current=revision.load(); if(current!=seen) { seen=current; phase=0; }
    buffer.assign(source,source+frames*2);
    constexpr double frequencies[]={523.251,587.330,783.991,659.255,880.000,783.991};
    constexpr unsigned lengths[]={12,12,18,12,12,24}; // 90 guest ticks, exactly three game seconds
    unsigned offset=position.load(), note=0;
    while(note<6 && offset>=lengths[note]) offset-=lengths[note++];
    if(note==6) return source;
    double t=offset/30.0, duration=lengths[note]/30.0;
    double envelope=std::min(1.0,t/.025)*std::min(1.0,(duration-t)/.09);
    for(int i=0;i<frames;++i) {
        double sample=(std::sin(phase)+.18*std::sin(2*phase)+.07*std::sin(3*phase))*envelope*3100;
        phase=std::fmod(phase+6.283185307179586*frequencies[note]/48000.0,6.283185307179586);
        for(int channel=0;channel<2;++channel) buffer[i*2+channel]=(int16_t)std::clamp((int)buffer[i*2+channel]+(int)sample,-32768,32767);
    }
    return buffer.data();
}
}
