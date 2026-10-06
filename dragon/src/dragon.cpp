// MOD_DRAGON: local prototype, no asset payloads or persistent save changes.
// HD layout/address references are documented in dragon/README.md of the mods repository.
#include "dragon.h"
#include "dragon_song.h"
#include "dragon_quest.h"
#include "runtime.h"
#include "input.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <mutex>

namespace interp { uint64_t logic_steps(); }
namespace dragon {
namespace {
constexpr uint32_t play = 0x1046F0B0, tag = 0x4452474E;
struct Vec { float x=0, y=0, z=0; };
enum class Phase { idle, loading, approach, launch, takeoff, riding, leaving, song };
struct Ride {
    Phase phase=Phase::idle;
    uint32_t player=0, actor=0, id=~0u, noop=0, buttons=0;
    unsigned ticks=0, blockedTicks=0, songBoat=0;
    Vec pos, grabOffset, audioPos, launchLink, hookPos, handOffset{0,140,0};
    bool grabValid=false, songWind=false;
    float liftStart=0, liftTarget=4000;
    float yaw=0, pitch=0, bank=0;
    uint32_t oldLeafFlag=0, rope=0, hookModel=0;
} ride;
unsigned song_notes=0; bool song_pending=false, summon_pending=false, boat_pending=false;
thread_local bool valoo_resources=false;
struct Resources { bool old=valoo_resources; Resources(bool active=true) { valoo_resources=active; } ~Resources() { valoo_resources=old; } };
std::mutex hud_mutex;
Hud hud_state;
constexpr float min_height=800, max_height=16000, rope_length=350;
constexpr float pi=3.14159265358979323846f;
Vec read_pos(uint32_t a) { return {(float)ldf32(a+0x314),(float)ldf32(a+0x318),(float)ldf32(a+0x31C)}; }
void vec(uint32_t a, Vec p) { stf32(a,p.x); stf32(a+4,p.y); stf32(a+8,p.z); }
bool sea() { return std::memcmp(ppc_ptr(play+0x5134),"sea\0",4)==0 && !ld8(play+0x514C); }
bool event() { return ld8(play+0x5292)!=0; }
// Isolate guest register clobbers and reserve outgoing linkage/arguments away from caller data.
uint32_t call(Cpu* c, uint32_t fn, std::initializer_list<uint32_t> args,
              std::initializer_list<float> floats={}) {
    Cpu sub=*c; sub.r[1]-=0x100;
    st32(sub.r[1],c->r[1]);
    unsigned r=3; for(auto a:args) { if(r<=10) sub.r[r]=a; else st32(sub.r[1]+8+(r-11)*4,a); ++r; }
    unsigned f=1; for(auto a:floats) sub.f[f++].ps0=a;
    sub.pc=fn; ppc_dispatch(&sub); return sub.r[3];
}
uint32_t temp(Cpu* c) { return c->r[1]-0x80; }
void hold_proc(Cpu* c) {
    if(ride.phase==Phase::song) {
        if(ride.songBoat) st32(play+0x5CD8,ld32(play+0x5CD8)|0x10000);
        call(c,0x023E2DC4,{ride.player});
    } // native boat-relative conducting position
    c->r[3]=1;
}
void place_link(uint32_t p) {
    // Link hangs below the animated claw on the short carry rope.
    Vec v=ride.phase==Phase::launch ? ride.launchLink : Vec{ride.pos.x+ride.grabOffset.x-ride.handOffset.x,ride.pos.y+ride.grabOffset.y-ride.handOffset.y-rope_length,ride.pos.z+ride.grabOffset.z-ride.handOffset.z};
    vec(p+0x314,v); vec(p+0x300,v); vec(0x1046CD48,v);
    vec(p+0x33C,{}); stf32(p+0x370,0); stf32(p+0x374,0);
    auto angle=(uint16_t)(int32_t)(ride.yaw*32768/pi);
    st16(p+0x322,angle); st16(p+0x32A,angle); st16(p+0x6926,angle);
    st16(0x1046CD0A,angle); st16(0x1046CD12,angle);
}
bool tagged(uint32_t a) { return a && ld32(a+0xB0)==tag; }
void release(Cpu* c, uint32_t p, const char* why) {
    if((ride.phase==Phase::riding || ride.phase==Phase::takeoff || ride.phase==Phase::launch) && p && p==ld32(play+0x5B2C)) {
        place_link(p);
        call(c,0x0206A590,{p,0x2821,0}); // existing rope uncoil/release
        LOG("[dragon] audio: release rope 2821");
        st32(play+0x5CDC,(ld32(play+0x5CDC)&~0x20u)|ride.oldLeafFlag);
        call(c,0x023DFDD8,{p,0x27}); // restore the stock fall PTMF even if already in FALL
        call(c,0x023F6564,{p,2},{6.0f});
        stf32(p+0x340,8); stf32(p+0x370,12);
        LOG("[dragon] release: %s; Link %.1f %.1f %.1f",why,ride.pos.x,read_pos(p).y,ride.pos.z);
    }
    ride.phase=(ride.actor||ride.id!=~0u)?Phase::leaving:Phase::idle; ride.ticks=0;
}
int profile() {
    // Resolve Valoo by its create entry in the HD profile table; no guessed actor ID.
    for(int i=0;i<0x200;++i) {
        uint32_t p=ld32(0x101F3EE0+i*4);
        if(p<0x10000000||p>0x11000000) continue;
        uint32_t methods=ld32(p+0x24);
        if(methods>=0x10000000 && methods<0x11000000 && ld32(methods)==0x0212A830) return i;
    }
    return -1;
}
bool summon(Cpu* c,uint32_t p) {
    if(ride.phase!=Phase::idle || !sea() || event()) return false;
    int name=profile(); if(name<0) { LOG("[dragon] Valoo profile unresolved"); return false; }
    ride.player=p; ride.pos=read_pos(p); ride.pos.y+=1700;
    ride.yaw=(int16_t)ld16(p+0x32A)*pi/32768;
    ride.pos.x-=std::sin(ride.yaw)*1800; ride.pos.z-=std::cos(ride.yaw)*1800;
    uint32_t t=temp(c); vec(t,ride.pos); st16(t+16,0); st16(t+18,ld16(p+0x32A)); st16(t+20,0);
    vec(t+32,{0.12f,0.12f,0.12f});
    ride.id=call(c,0x025D5834,{(uint32_t)name,tag,t,~0u,t+16,t+32,0xFF,0});
    if(ride.id==~0u) { LOG("[dragon] spawn request failed"); ride=Ride{}; return false; }
    ride.phase=Phase::loading; ride.ticks=0;
    LOG("[dragon] summon: profile %d, process %u",name,ride.id); return true;
}
void begin_launch(Cpu* c,uint32_t p) {
    ride.launchLink=read_pos(p); ride.oldLeafFlag=ld32(play+0x5CDC)&0x20;
    call(c,0x023F6564,{p,2},{0.0f});
    if(!ride.noop) ride.noop=dispatch::register_host(hold_proc,"dragon_carried_proc");
    st16(p+0x65F4,0); st16(p+0x65F6,0xFFFF); st32(p+0x65F8,ride.noop);
    call(c,0x023E0A04,{p,8,~0u},{0.0f,0.0f,3.0f}); // existing aiming stance
    call(c,0x023DE7E8,{p,0xE4,2,~0u},{0.0f,0.0f,-1.0f}); // upper rope throw wait
    ride.phase=Phase::launch; ride.ticks=0;
    LOG("[dragon] grapple: overhead position reached; preparing automatic throw");
}
void pickup(Cpu* c,uint32_t p) {
    call(c,0x0206A590,{ride.actor,0x4841,0});
    call(c,0x0206A590,{p,0x2830,0});
    call(c,0x023E0A04,{p,0x78,~0u},{0.5f,0.0f,4.0f}); // existing ROPEWAIT hanging pose
    ride.liftStart=ride.pos.y;
    float water=(float)ldf32(p+0x6A28);
    if(!std::isfinite(water)||std::fabs(water)>=10000) water=0;
    ride.liftTarget=std::max(ride.pos.y,water+4000);
    ride.phase=Phase::takeoff; ride.ticks=0;
    LOG("[dragon] grapple: hook attached to animated claw; rope taut; automatic lift");
}
struct HeightBounds { float minimum, maximum, water; };
HeightBounds height_bounds(uint32_t p) {
    float ground=(float)ldf32(p+0x8A0), water=(float)ldf32(p+0x6A28);
    float water_level=std::isfinite(water)&&std::fabs(water)<10000 ? water : 0;
    float clearance=std::max(min_height,ride.handOffset.y+rope_length-ride.grabOffset.y+min_height);
    float floor=water_level+clearance;
    if(std::isfinite(ground)&&ground>-1e8f) floor=std::max(floor,ground+std::max(470.0f,clearance));
    if(std::isfinite(water)&&water>-1e8f) floor=std::max(floor,water+clearance);
    float ceiling=water_level+max_height;
    floor=std::min(floor,ceiling);
    return {floor,ceiling,water_level};
}
// Sweep a sampled carrier envelope against native background collision polygons.
// This includes registered moving background geometry, not every decorative mesh.
bool blocked(Cpu* c, Vec from, Vec to) {
    Cpu work=*c; work.r[1]-=0x500;
    uint32_t q=work.r[1]+0x180, start=q+0x80, end=q+0x90;
    call(&work,0x02008FEC,{q});
    for(int i=0;i<7;++i) st8(q+0x5C+i,i==0 ? 1 : 0);
    st32(q+0x68,1); st32(q,q+0x58); st32(q+4,q+0x64);
    st32(q+0x10,0x1002A8C8); st32(q+0x20,0x1002A8D8);
    st32(q+0x58,0x1002A8F8); st32(q+0x64,0x1002A8E8);
    Vec offsets[]={{0,0,0},{0,500,0},{0,-250,0},
                   {600,0,0},{-600,0,0},{0,0,600},{0,0,-600},
                   {ride.grabOffset.x,ride.grabOffset.y-ride.handOffset.y-rope_length,ride.grabOffset.z}};
    float dx=to.x-from.x,dy=to.y-from.y,dz=to.z-from.z;
    float length=std::sqrt(dx*dx+dy*dy+dz*dz);
    if(length<0.01f) return false;
    float ahead=200/length;
    for(Vec offset:offsets) {
        vec(start,{from.x+offset.x,from.y+offset.y,from.z+offset.z});
        vec(end,{to.x+offset.x+dx*ahead,to.y+offset.y+dy*ahead,to.z+offset.z+dz*ahead});
        call(&work,0x024F1AFC,{q,start,end,ride.actor});
        if(call(&work,0x02008860,{play+0x12A0,q})) {
            if(ride.blockedTicks==0) LOG("[dragon] collision ray bg %u poly %u hit %.0f %.0f %.0f",ld16(q+0x16),ld16(q+0x14),ldf32(q+0x40),ldf32(q+0x44),ldf32(q+0x48));
            return true;
        }
    }
    return false;
}
void move_checked(Cpu* c, Vec previous) {
    if(blocked(c,previous,ride.pos)) {
        Vec vertical={previous.x,ride.pos.y,previous.z};
        ride.pos=blocked(c,previous,vertical) ? previous : vertical;
        if(++ride.blockedTicks%30==1) LOG("[dragon] obstacle: carrier movement stopped");
    } else ride.blockedTicks=0;
}
void fly(uint32_t p,const input::PadState& pad) {
    ride.yaw+=pad.lx*0.035f;
    ride.bank+=(-pad.lx*0.20f-ride.bank)*0.10f;
    ride.pitch+=(pad.ly*0.65f-ride.pitch)*0.08f;
    float dx=std::sin(ride.yaw)*140, dz=std::cos(ride.yaw)*140;
    ride.pos.x=std::clamp(ride.pos.x+dx,-345000.0f,345000.0f);
    ride.pos.z=std::clamp(ride.pos.z+dz,-345000.0f,345000.0f);
    auto bounds=height_bounds(p);
    ride.pos.y=std::clamp(ride.pos.y+std::sin(ride.pitch)*160,bounds.minimum,bounds.maximum);
}
}
Hud hud() { std::lock_guard<std::mutex> lock(hud_mutex); return hud_state; }
bool enabled() {
    static const bool on=[] {
        const char* e=std::getenv("WWHD_MOD_DRAGON"); bool v=e && std::strcmp(e,"1")==0;
        if(v) LOG("[dragon] mod enabled (WWHD_MOD_DRAGON=1)");
        return v;
    }();
    return on;
}
void tact(Cpu* c,PpcFunc original) { original(c); }
void song_judge(Cpu* c,PpcFunc original) {
    unsigned index=c->r[3], direction=c->r[4];
    original(c);
    if(!enabled()) return;
    uint32_t p=ld32(play+0x5B2C);
    if(!p || !quest::song_allowed(p) || ride.phase!=Phase::idle || !sea() || ld32(p+0x65F0)!=0x9A || ld32(p+0x69C0)!=~0u) { song_notes=0; return; }
    unsigned meter=call(c,0x025E1EFC,{});
    constexpr unsigned pattern[]={1,2,1,4,3,2};
    if(index==0) song_notes=0;
    if(meter==6 && index==song_notes && index<6 && direction==pattern[index]) ++song_notes;
    else song_notes=0;
    LOG("[dragon-song] beat %u direction %u meter %u matched %u step %llu",index,direction,meter,song_notes,(unsigned long long)interp::logic_steps());
    if(song_notes==6) { song_pending=true; c->r[3]=~0u; }
}
void end_song(Cpu* c,uint32_t p) {
    song::stop();
    if(ride.songWind) { call(c,0x023D4538,{p+0x67FC}); ride.songWind=false; }
    call(c,0x023D4538,{p+0x66F8});
    st8(play+0x5BD1,0); st16(play+0x52B8,ld16(play+0x52B8)|8);
    uint32_t cam=call(c,0x024F8044,{});
    call(c,0x0253E860,{cam,ld32(p+4)});
    if(ride.songBoat && sea()) { st32(play+0x5CD8,ld32(play+0x5CD8)|0x10000); call(c,0x023E2DC4,{p}); }
    call(c,0x023F2048,{p}); call(c,0x025E1E94,{});
    boat_pending=ride.songBoat!=0 && sea(); ride.songBoat=0;
    st32(play+0x5CDC,ld32(play+0x5CDC)&~1u);
    ride.phase=Phase::idle; song_notes=0;
}
void execute(Cpu* c,PpcFunc original) {
    if(!enabled()) { original(c); return; }
    uint32_t p=c->r[3]; auto pad=input::read();
    uint32_t pressed=pad.buttons&~ride.buttons; ride.buttons=pad.buttons;
    if(ride.phase==Phase::song && ((pressed&input::kB) || !sea() || p!=ride.player || ld32(p+0x65F8)!=ride.noop)) {
        if(p==ride.player) end_song(c,p);
        else { song::stop(); ride.phase=Phase::idle; song_notes=0; ride.songWind=false; } song_pending=false; summon_pending=false;
        LOG("[dragon-song] cancelled");
    }
    bool riding=ride.phase==Phase::riding || ride.phase==Phase::takeoff || ride.phase==Phase::launch;
    if(riding && (p!=ride.player || !sea() || event() || !tagged(ride.actor))) release(c,ride.player,p!=ride.player?"player changed":!sea()?"stage changed":event()?"event started":"carrier missing");
    if(ride.phase==Phase::riding) {
        if(pressed&input::kA) release(c,p,"jump");
        else { Vec previous=ride.pos; fly(p,pad); move_checked(c,previous); place_link(p); st32(play+0x5CDC,ld32(play+0x5CDC)|0x20); }
    }
    if(ride.phase==Phase::takeoff || ride.phase==Phase::launch) {
        if(pressed&input::kA) release(c,p,"pickup cancelled");
        else { place_link(p); st32(play+0x5CDC,ld32(play+0x5CDC)|0x20); }
    }
    bool quiet=quest::tick(c,p,pressed,ride.phase==Phase::idle);
    uint32_t controller=ld32(0x101F5088), inputSaved[10]{};
    constexpr uint32_t offsets[]={0x18,0x1C,0x20,0x124,0x130,0x134,0x138,0x13C,0x140,0x144};
    if(quiet && controller) for(unsigned i=0;i<10;++i) {inputSaved[i]=ld32(controller+offsets[i]);st32(controller+offsets[i],0);}
    original(c);
    if(quiet && controller) for(unsigned i=0;i<10;++i) st32(controller+offsets[i],inputSaved[i]);
    Cpu saved=*c;
    if((boat_pending || summon_pending) && (p!=ride.player || !sea())) { boat_pending=false; summon_pending=false; }
    if(ride.phase==Phase::idle && !event()) {
        if(boat_pending) { boat_pending=false; call(c,0x023F1E60,{p}); LOG("[dragon-song] boat control restored"); }
        if(summon_pending) { summon_pending=false; summon(c,p); }
    }
    if(song_pending && ride.phase==Phase::idle) {
        song_pending=false; ride.player=p; ride.songBoat=call(c,0x023E26EC,{p,0});
        call(c,0x025E1988,{0x896});
        call(c,0x02439D54,{p,0,0,1}); // reuse conducting and baton glow, without stock song execution
        uint32_t model=ld32(p+0x4440);
        if(model && !ld32(p+0x6800)) {
            call(c,0x023D457C,{p+0x67FC,0x32,model+0xC8,p+0x314,0}); // existing Leaf wind gust
            ride.songWind=ld32(p+0x6800)!=0;
        }
        st32(p+0x65F0,0x9A); st32(p+0x69C0,~0u); st32(p+0x69C4,~0u);
        if(!ride.noop) ride.noop=dispatch::register_host(hold_proc,"dragon_carried_proc");
        st16(p+0x65F4,0); st16(p+0x65F6,0xFFFF); st32(p+0x65F8,ride.noop);
        ride.phase=Phase::song; ride.ticks=0; song::start();
        LOG("[dragon-song] Call of the Sky recognized; conducting (boat %u)",ride.songBoat);
    } else if(ride.phase==Phase::song) {
        song::tick(++ride.ticks);
        if(ride.ticks>=90) {
            quest::learned(); end_song(c,p); summon_pending=true;
            LOG("[dragon-song] performance complete; summon queued");
        }
    }
    if(ride.phase==Phase::loading && ++ride.ticks>300) { LOG("[dragon] load timeout"); release(c,p,"load timeout"); }
    if(ride.phase==Phase::approach && p==ride.player && ride.grabValid) {
        Vec target=read_pos(p);
        target.x-=ride.grabOffset.x; target.y+=ride.handOffset.y+rope_length-ride.grabOffset.y; target.z-=ride.grabOffset.z;
        ride.pos.x+=(target.x-ride.pos.x)*0.08f;
        ride.pos.y+=(target.y-ride.pos.y)*0.08f;
        ride.pos.z+=(target.z-ride.pos.z)*0.08f;
        if(std::fabs(target.x-ride.pos.x)+std::fabs(target.y-ride.pos.y)+std::fabs(target.z-ride.pos.z)<45) begin_launch(c,p);
    } else if(ride.phase==Phase::launch) {
        if(ld32(p+0x65F8)!=ride.noop) release(c,p,"launch state changed");
        else {
            ++ride.ticks;
            if(ride.ticks==8) {
                call(c,0x023DE7E8,{p,0xE2,2,0xB},{1.3f,1.0f,-1.0f}); // existing upper ROPE THROW
                call(c,0x0206A590,{p,0x2817,0});
                LOG("[dragon] grapple: automatic rope launch");
            }
            Vec hand={(float)ldf32(p+0x3F0),(float)ldf32(p+0x3F4),(float)ldf32(p+0x3F8)};
            Vec claw={ride.pos.x+ride.grabOffset.x,ride.pos.y+ride.grabOffset.y,ride.pos.z+ride.grabOffset.z};
            float t=std::clamp((ride.ticks-8.0f)/18,0.0f,1.0f);
            ride.hookPos={hand.x+(claw.x-hand.x)*t,hand.y+(claw.y-hand.y)*t,hand.z+(claw.z-hand.z)*t};
            if(ride.ticks>=32) pickup(c,p);
        }
    } else if(ride.phase==Phase::takeoff) {
        Vec previous=ride.pos;
        float t=std::min(1.0f,++ride.ticks/60.0f), eased=t*t*(3-2*t);
        ride.pos.y=ride.liftStart+(ride.liftTarget-ride.liftStart)*eased;
        ride.pos.x+=std::sin(ride.yaw)*60; ride.pos.z+=std::cos(ride.yaw)*60;
        ride.pitch+=(0.28f-ride.pitch)*0.12f;
        move_checked(c,previous);
        place_link(p);
        if(ride.ticks>=60) {
            ride.phase=Phase::riding; ride.ticks=0;
            LOG("[dragon] cruising height reached: dragon view and player control active; A releases Link");
        }
    }
    if(ride.phase==Phase::takeoff || ride.phase==Phase::riding) {
        Vec root=read_pos(p);
        Vec offset={(float)ldf32(p+0x3F0)-root.x,(float)ldf32(p+0x3F4)-root.y,(float)ldf32(p+0x3F8)-root.z};
        if(std::isfinite(offset.y) && std::fabs(offset.x)+std::fabs(offset.y)+std::fabs(offset.z)<500) ride.handOffset=offset;
    }
    if(ride.phase==Phase::takeoff && ld32(p+0x65F8)!=ride.noop) release(c,p,"grab state changed");
    if(ride.phase==Phase::riding) {
        // A competing normal player transition releases ownership instead of overwriting it.
        if(ld32(p+0x65F8)!=ride.noop) release(c,p,"player state changed");
        else {
            // Stock execute refreshes water/ground samples; enforce bounds against that new sample.
            auto bounds=height_bounds(p);
            ride.pos.y=std::clamp(ride.pos.y,bounds.minimum,bounds.maximum);
            place_link(p); if(++ride.ticks%60==0) LOG("[dragon] flight %.0f %.0f %.0f room %d stay %d ground %.0f",ride.pos.x,ride.pos.y,ride.pos.z,(int8_t)ld8(p+0x326),(int8_t)ld8(0x1047E6C8),ldf32(p+0x8A0)); }
    }
    {
        std::lock_guard<std::mutex> lock(hud_mutex);
        auto bounds=height_bounds(p);
        hud_state={sea() ? (int)ride.phase : -1,ride.pos.y-bounds.water,bounds.minimum-bounds.water,max_height,(int)song_notes,ld32(p+0x65F0)==0x9A};
    }
    *c=saved;
}
void camera(Cpu* c,PpcFunc original) {
    uint32_t cam=c->r[3]; original(c);
    if(!enabled() || (ride.phase!=Phase::takeoff && ride.phase!=Phase::riding && ride.phase!=Phase::launch)) return;
    Cpu saved=*c;
    Vec center={ride.pos.x,ride.pos.y+120,ride.pos.z};
    if(ride.phase==Phase::launch) center={ride.launchLink.x,ride.launchLink.y+300,ride.launchLink.z};
    Vec eye={ride.pos.x-std::sin(ride.yaw)*1800,ride.pos.y+750,ride.pos.z-std::cos(ride.yaw)*1800};
    Vec previous={(float)ldf32(cam+0x1C),(float)ldf32(cam+0x20),(float)ldf32(cam+0x24)};
    eye={previous.x+(eye.x-previous.x)*0.18f,previous.y+(eye.y-previous.y)*0.18f,previous.z+(eye.z-previous.z)*0.18f};
    uint32_t t=temp(c); vec(t,center); vec(t+16,eye);
    call(c,0x025151BC,{cam,t,t+16,0},{60.0f}); // native camera Reset(center,eye,fov,bank)
    *c=saved;
}
void position(Cpu* c,PpcFunc original) {
    if(enabled()&&(ride.phase==Phase::riding || ride.phase==Phase::takeoff || ride.phase==Phase::launch)&&c->r[3]==ride.player) { place_link(c->r[3]); c->r[3]=1; return; }
    original(c);
}
void solid_heap(Cpu* c,PpcFunc original) {
    if(enabled() && tagged(c->r[3])) c->r[5]=0x18000;
    original(c);
}
void actor_heap(Cpu* c,PpcFunc original) {
    uint32_t actor=c->r[3]; original(c);
    if(!enabled()||!tagged(actor)||!c->r[3]) return;
    Cpu saved=*c;
    Resources linkResources(false);
    uint32_t key=temp(c); st32(key,0x1001100C); st32(key+4,0x10010DEC);
    uint32_t data=call(c,0x026066C4,{ld32(0x101F4F28),key,0x2E});
    ride.hookModel=data ? call(c,0x025E38E0,{data,0,0x11020203}) : 0;
    if(ride.hookModel) vec(ride.hookModel+0xBC,{1,1,1});
    LOG("[dragon] grapple hook model in carrier solid heap: %08X",ride.hookModel);
    *c=saved; if(!ride.hookModel) c->r[3]=0;
}
void actor_create(Cpu* c,PpcFunc original) {
    uint32_t a=c->r[3];
    if(!enabled()||!tagged(a)) { original(c); return; }
    Resources scope; original(c);
    Cpu saved=*c;
    if(c->r[3]==4) { // cPhs_COMPLEATE_e
        ride.actor=a; ride.audioPos=ride.pos;
        ride.rope=call(c,0x025E9960,{0});
        if(ride.rope && !call(c,0x025E9B80,{ride.rope,1,8,0})) { call(c,0x025E99E0,{ride.rope,3}); ride.rope=0; }
        if(ride.phase==Phase::loading) ride.phase=Phase::approach;
        call(c,0x02129DB4,{a,0x10,2,~0u},{5.0f,1.0f}); // existing Dr WAIT1 loop, no BAS
        stf32(a+0x364,100000);
        st32(a+0x2E0,(ld32(a+0x2E0)&~0x100u)|0x80); // summoned mount must keep executing/drawing
        LOG("[dragon] Demo45 Valoo flight archive/heap ready: actor %08X",a);
    } else if(c->r[3]==5) { LOG("[dragon] Dr creation error"); ride.phase=Phase::idle; ride.id=~0u; }
    *c=saved;
}
void actor_execute(Cpu* c,PpcFunc original) {
    uint32_t a=c->r[3];
    if(!enabled()||!tagged(a)) { original(c); return; }
    if(!sea()) { call(c,0x025D57E0,{a}); c->r[3]=1; return; }
    if(ride.phase==Phase::leaving) {
        ride.pos.y+=35; ride.pos.z+=80;
        if(++ride.ticks>90) { call(c,0x025D57E0,{a}); c->r[3]=1; return; }
    }
    vec(a+0x300,ride.pos); vec(a+0x314,ride.pos); vec(a+0x37C,ride.pos);
    vec(a+0x330,{0.12f,0.12f,0.12f});
    // daDr_setMtx uses current.angle; update both current and shape rotations.
    for(uint32_t off:{0x320u,0x328u}) {
        st16(a+off,(uint16_t)(int32_t)(-ride.pitch*32768/pi));
        st16(a+off+2,(uint16_t)(int32_t)(ride.yaw*32768/pi));
        st16(a+off+4,(uint16_t)(int32_t)(ride.bank*32768/pi));
    }
    // Existing flying-actor level sound: refreshed each frame as in native Hr code.
    float ax=ride.pos.x-ride.audioPos.x, ay=ride.pos.y-ride.audioPos.y, az=ride.pos.z-ride.audioPos.z;
    float speed=std::sqrt(ax*ax+ay*ay+az*az);
    uint32_t intensity=(uint32_t)std::clamp(speed*100/160,0.0f,100.0f);
    call(c,0x0206A590,{a,0x5125,intensity});
    if(ride.phase==Phase::takeoff || ride.phase==Phase::riding)
        call(c,0x0206A590,{a,0x5076,intensity}); // existing Helmaroc wind layer
    ride.audioPos=ride.pos;
    uint32_t morf=ld32(a+0x3D0);
    call(c,0x025E535C,{morf,a+0x314,0,0});
    call(c,0x02129F0C,{a});
    uint32_t model=ld32(morf+0x90), block=model ? ld32(model+0x2C) : 0, matrices=block ? ld32(block+0x10) : 0;
    if(matrices>=0x10000000 && matrices<0x70000000) {
        // Demo45 j_dr_ashi_l3: skeleton bone 5, original_matrix_index 3.
        uint32_t matrix=matrices+3*0x30;
        Vec claw={(float)ldf32(matrix+0xC),(float)ldf32(matrix+0x1C),(float)ldf32(matrix+0x2C)};
        Vec delta={claw.x-ride.pos.x,claw.y-ride.pos.y,claw.z-ride.pos.z};
        if(std::isfinite(delta.x)&&std::isfinite(delta.y)&&std::isfinite(delta.z) && std::fabs(delta.x)+std::fabs(delta.y)+std::fabs(delta.z)<10000) {
            ride.grabOffset=delta; ride.grabValid=true;
        }
    }
    c->r[3]=1;
}
void actor_draw(Cpu* c,PpcFunc original) {
    uint32_t actor=c->r[3]; original(c);
    if(!enabled()||!tagged(actor)||!ride.rope ||
       (ride.phase!=Phase::riding && ride.phase!=Phase::takeoff && ride.phase!=Phase::launch)) return;
    Cpu saved=*c;
    uint32_t points=ld32(ld32(ride.rope+0x144));
    Vec claw={ride.pos.x+ride.grabOffset.x,ride.pos.y+ride.grabOffset.y,ride.pos.z+ride.grabOffset.z};
    Vec hands=read_pos(ride.player);
    if(ride.phase==Phase::launch) hands={(float)ldf32(ride.player+0x3F0),(float)ldf32(ride.player+0x3F4),(float)ldf32(ride.player+0x3F8)};
    else hands={hands.x+ride.handOffset.x,hands.y+ride.handOffset.y,hands.z+ride.handOffset.z};
    if(ride.phase==Phase::launch) {
        if(ride.ticks<8) { *c=saved; return; }
        claw=ride.hookPos;
    }
    if(ride.hookModel) {
        uint32_t matrix=ride.hookModel+0xC8;
        Vec z={claw.x-hands.x,claw.y-hands.y,claw.z-hands.z};
        float length=std::sqrt(z.x*z.x+z.y*z.y+z.z*z.z);
        z=length>0.01f ? Vec{z.x/length,z.y/length,z.z/length} : Vec{0,1,0};
        Vec x={std::cos(ride.yaw)*z.y,z.z*std::sin(ride.yaw)-z.x*std::cos(ride.yaw),-std::sin(ride.yaw)*z.y};
        float width=std::sqrt(x.x*x.x+x.y*x.y+x.z*x.z);
        x=width>0.01f ? Vec{x.x/width,x.y/width,x.z/width} : Vec{1,0,0};
        Vec y={z.y*x.z-z.z*x.y,z.z*x.x-z.x*x.z,z.x*x.y-z.y*x.x};
        float m[12]={x.x*.2f,y.x*.2f,z.x*.2f,claw.x-z.x*3,
                     x.y*.2f,y.y*.2f,z.y*.2f,claw.y-z.y*3,
                     x.z*.2f,y.z*.2f,z.z*.2f,claw.z-z.z*3};
        for(int i=0;i<12;++i) stf32(matrix+i*4,m[i]);
        uint32_t env=call(c,0x02555D0C,{});
        call(c,0x02562F5C,{env,ride.hookModel,actor+0x110});
        call(c,0x025E2DE0,{ride.hookModel,0});
    }
    for(int i=0;i<8;++i) {
        float t=i/7.0f;
        vec(points+i*12,{claw.x+(hands.x-claw.x)*t,claw.y+(hands.y-claw.y)*t,claw.z+(hands.z-claw.z)*t});
    }
    uint32_t color=temp(c); st32(color,0xB99158FF);
    call(c,0x025EA548,{ride.rope,8,color,0,actor+0x110},{5.0f});
    uint32_t id=call(c,ld32(ld32(ride.rope+0x130)+0x14),{ride.rope});
    if(id<8) call(c,0x025EDD04,{play+0x5FB4+id*0x9C,ride.rope});
    *c=saved;
}
void actor_delete(Cpu* c,PpcFunc original) {
    uint32_t a=c->r[3];
    if(enabled()&&a==ride.actor) {
        if(ride.phase==Phase::riding || ride.phase==Phase::takeoff || ride.phase==Phase::launch) release(c,ride.player,"carrier deleted");
        if(ride.rope) { call(c,0x025E99E0,{ride.rope,3}); ride.rope=0; }
        uint32_t noop=ride.noop, buttons=ride.buttons;
        ride=Ride{}; ride.noop=noop; ride.buttons=buttons;
        LOG("[dragon] carrier deleted");
    }
    if(enabled()&&tagged(a)) { Resources scope; original(c); }
    else original(c);
}
void resource_phase(Cpu* c,PpcFunc original) {
    if(enabled()&&valoo_resources) c->r[4]=0x100041E8; // Demo45 on-machine archive
    original(c);
}
void resource_get(Cpu* c,PpcFunc original) {
    if(!enabled()||!valoo_resources) { original(c); return; }
    uint32_t key=c->r[4], old=ld32(key), index=c->r[5];
    if(index==8) { c->r[3]=0; return; } // omit Dr-specific BAS
    st32(key,0x100041E8);
    c->r[5]=index==0x13 ? 43 : 16; // dr_comp.bdl + 45_cut09_dr_fly_l.bck
    original(c); st32(key,old);
}
}
