/* SPDX-License-Identifier: MPL-2.0
 * Run the shipped ELF through the real translator, using synthetic native
 * functions. This verifies integration/ABI, not native rendering or enemy AI. */
#define _GNU_SOURCE
#include "wwhd_guest_abi.h"
#include "test_addresses.h"
#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
enum { PLAY=0x11000000, LINK=0x12000000, TARGET=0x13000000, STACK=0x14000000,
       SAVE=0x15000000, MANAGER=0x16000000, EMITTER=0x17000000, COLOR=0x18000000 };
static const WWHDGuestModuleV1* mod;
static Cpu cpu;
static volatile int preempt[3];
static uint64_t step;
static double dt=1.0/30;
static int enabled=1, paused, menu, wall, failing;
static unsigned spawned, ended, collisions, lines, status_moves;
static uint32_t capsule, status, callbacks[3], emitters[3];
static float last_z;
static void own(uint32_t p,unsigned size) {
    assert(p>=mod->mem_base && p+size<=mod->mem_base+mod->mem_size);
}
static int near(float a,float b) { return fabsf(a-b)<0.05f; }
static void dispatch(Cpu* c) {
    for(unsigned i=0;i<mod->func_count;++i)
        if(mod->funcs[i].addr==c->pc) { mod->funcs[i].fn(c); return; }
    unsigned p=c->r[3];
    switch(c->pc) {
    case GAME_PLAY: c->r[3]=PLAY; return;
    case GAME_PAUSE: c->r[3]=paused; return;
    case GAME_MENU: c->r[3]=menu; return;
    case GAME_CAPSULE:
        own(p,0x138); capsule=p;
        /* Guard the capsule's two vtables against radius/geometry overwrites. */
        st32(p+0x130,0x10203040); st32(p+0x114,0x10203044); return;
    case GAME_STATUS: own(p,0x3c); status=p; return;
    case GAME_STATUS_INIT:
        assert(p==status && c->r[4]==120 && c->r[5]==255 && c->r[6]==LINK);
        st32(p+0xc,LINK); st32(p+0x10,ld32(LINK+4)); return;
    case GAME_STATUS_MOVE:
        assert(p==status+0x1c); ++status_moves; return;
    case GAME_RESET_HIT:
        assert(p==capsule); st32(p+0x54,0); return;
    case GAME_LINE_CTOR: own(p,0x6c); return;
    case GAME_LINE_SET:
        own(p,0x6c); assert(c->r[6]==LINK);
        assert(ld32(p)==p+0x58 && ld32(p+4)==p+0x64);
        assert(ld32(p+0x10)==0x1110 && ld32(p+0x20)==0x1120);
        assert(ld32(p+0x58)==0x1158 && ld32(p+0x64)==0x1164);
        for(unsigned i=0;i<7;++i) assert(ld8(p+0x5c+i)==0);
        assert(ld32(p+0x68)==1);
        for(unsigned i=0;i<12;i+=4) assert(isfinite(ldf32(c->r[4]+i)) && isfinite(ldf32(c->r[5]+i)));
        return;
    case GAME_LINE_CROSS:
        assert(p==PLAY+0x12a0); ++lines; c->r[3]=wall; return;
    case GAME_COLLISION:
        assert(p==PLAY+0x26a4 && c->r[4]==capsule); ++collisions;
        assert(ld32(capsule)&1); assert(ld32(capsule+0x10)==2);
        assert(ld32(capsule+0x18)==0 && ld32(capsule+0x2c)==0);
        assert(ld32(capsule+0x44)==status && ld32(capsule+0x50)==1);
        assert(ld32(capsule+0x58)==0);
        assert(ld32(capsule+0x130)==0x10203040 && ld32(capsule+0x114)==0x10203044);
        assert(near(ldf32(capsule+0x134),55));
        float dx=ldf32(capsule+0x124)-ldf32(capsule+0x118);
        float dy=ldf32(capsule+0x128)-ldf32(capsule+0x11c);
        float dz=ldf32(capsule+0x12c)-ldf32(capsule+0x120);
        assert(near(sqrtf(dx*dx+dy*dy+dz*dz),300));
        last_z=(ldf32(capsule+0x120)+ldf32(capsule+0x12c))*0.5f;
        return;
    case GAME_PARTICLE: {
        assert(p==MANAGER && c->r[4]==1 && c->r[9]==255 && !c->r[8]);
        unsigned i=c->r[5]==0x25?0:c->r[5]==0x26?1:2;
        assert(c->r[5]==(i==0?0x25:i==1?0x26:0x28));
        uint32_t cb=c->r[10]; own(cb,16); own(c->r[6],12);
        /* PPC arguments 9..12 really live on the caller's stack. */
        assert((int8_t)ld32(c->r[1]+8)==0);
        assert(ld32(ld32(c->r[1]+12))==0x102030ff+i);
        assert(ld32(ld32(c->r[1]+16))==0x405060ff+i);
        assert(ld32(c->r[1]+20)==0);
        assert(ld32(cb)==0x11223344);
        ++spawned; callbacks[i]=cb;
        if(failing) { c->r[3]=0; return; }
        emitters[i]=EMITTER+0x2000+i*0x400;
        st32(emitters[i]+0x5c,18); st32(emitters[i]+0x1e4,cb); st32(cb+12,emitters[i]); st8(cb+4,255); st8(cb+5,0); st16(cb+6,0);
        c->r[3]=emitters[i]; return;
    }
    case GAME_END:
        own(p,16); assert(ld32(p+12));
        st32(ld32(p+12)+0x1e4,0);st32(p+12,0); st8(p+5,1); ++ended; return;
    default: fprintf(stderr,"Unexpected native call %08X\n",c->pc); abort();
    }
}
static void hook(unsigned target,unsigned kind,unsigned actor) {
    int found=0;
    for(unsigned i=0;i<mod->hook_count;++i) if(mod->hooks[i].target==target && mod->hooks[i].kind==kind) {
        cpu.r[3]=actor; mod->hooks[i].fn(&cpu); found=1;
        assert(cpu.r[1]==STACK+0xff000);
    }
    assert(found);
}
static void original(Cpu* c,uint32_t a) { (void)c;(void)a; assert(!"observer only"); }
static uint64_t epoch=1;
static void svc_epoch(Cpu* c) { c->r[3]=(uint32_t)(epoch>>32); c->r[4]=(uint32_t)epoch; }
static void svc_step(Cpu* c) { c->r[3]=(uint32_t)(step>>32); c->r[4]=(uint32_t)step; }
static void svc_dt(Cpu* c) { c->f[1].ps0=dt; }
static void svc_bool(Cpu* c) { assert(!strcmp((char*)ppc_ptr(c->r[3]),"enabled")); c->r[3]=enabled; }
static void svc_copy(Cpu* c) { memcpy(ppc_ptr(c->r[3]),ppc_ptr(c->r[4]),c->r[5]); }
static void svc_set(Cpu* c) { memset(ppc_ptr(c->r[3]),c->r[4],c->r[5]); }
static PpcFunc service(const char* s) {
    if(!strcmp(s,"wwhd_hud_epoch")) return svc_epoch;
    if(!strcmp(s,"wwhd_logic_step")) return svc_step;
    if(!strcmp(s,"wwhd_logic_dt")) return svc_dt;
    if(!strcmp(s,"wwhd_config_bool")) return svc_bool;
    if(!strcmp(s,"memcpy")) return svc_copy;
    if(!strcmp(s,"memset")) return svc_set;
    fprintf(stderr,"Unexpected service %s\n",s); abort();
}
static void unimplemented(Cpu* c,uint32_t a,uint32_t insn) { (void)c; fprintf(stderr,"Unimplemented %08X %08X\n",a,insn); abort(); }
static void trap(Cpu* c,uint32_t a) { unimplemented(c,a,0); }
static void frame(void) { ++step; hook(GAME_LINK,2,LINK); hook(GAME_LINK,3,LINK); }
static void native_colors(int i,int ours) {
    cpu.r[4]=1; cpu.r[5]=i==0?0x25:i==1?0x26:0x28;
    cpu.r[6]=LINK+0x314; cpu.r[7]=LINK+0x328; cpu.r[8]=0; cpu.r[9]=255;
    cpu.r[10]=ours?LINK+0x6730+i*16:TARGET+0x6730+i*16;
    st32(cpu.r[1]+8,0); st32(cpu.r[1]+12,COLOR+i*8); st32(cpu.r[1]+16,COLOR+i*8+4); st32(cpu.r[1]+20,0);
    hook(GAME_PARTICLE,2,MANAGER);
}
static void begin(int proc,int active,int colors) {
    ++step; hook(GAME_LINK,2,LINK);
    hook(proc==0x56?GAME_ROLL:GAME_SPIN,2,LINK);
    st32(LINK+0x65f0,proc); st32(LINK+0x78bc,active?0x19:0x18);
    for(int i=0;i<colors;++i) native_colors(i,1);
    hook(GAME_LINK,3,LINK);
}
static void setup(void) {
    memset(ppc_ptr(mod->mem_base),0,mod->mem_size);
    memcpy(ppc_ptr(mod->mem_base),mod->image,mod->image_size);
    memset(ppc_ptr(PLAY),0,0x6000); memset(ppc_ptr(LINK),0,0x9000);
    memset(ppc_ptr(TARGET),0,0x9000); memset(ppc_ptr(EMITTER),0,0x4000);
    memset(&cpu,0,sizeof(cpu)); cpu.r[1]=STACK+0xff000;
    step=0; enabled=1; paused=menu=wall=failing=0; dt=1.0/30;
    spawned=ended=collisions=lines=status_moves=0; capsule=status=0; last_z=0;
    memset(callbacks,0,sizeof(callbacks)); memset(emitters,0,sizeof(emitters));
    st32(SAVE_PTR,SAVE); st16(SAVE+0x20,12); st16(SAVE+0x22,12);
    st32(PLAY+0x5b2c,LINK); st32(PLAY+0x5ab0,MANAGER);
    memcpy(ppc_ptr(PLAY+0x5134),"sea\0\0\0\0\0",8);
    st32(LINK+4,42); st16(LINK+0x69b0,0x103); stf32(LINK+0x69fc,180);
    st32(LINK+0x78bc+0x10,2); st8(LINK+0x78bc+0x14,4);
    st8(LINK+0x78bc+0x6c,1); st8(LINK+0x78bc+0x6d,0); st8(LINK+0x78bc+0x6e,15); st8(LINK+0x78bc+0x6f,1);
    st32(LINK+0xbb4+0x10,0x1110); st32(LINK+0xbb4+0x20,0x1120);
    st32(LINK+0xbb4+0x58,0x1158); st32(LINK+0xbb4+0x64,0x1164);
    for(int i=0;i<3;++i) {
        st32(LINK+0x6730+i*16,0x11223344); st32(LINK+0x673c+i*16,EMITTER+i*0x400);
        stf32(EMITTER+i*0x400+0x238,1.5f);
        st32(COLOR+i*8,0x102030ff+i); st32(COLOR+i*8+4,0x405060ff+i);
    }
    for(int i=0;i<8192;++i) {
        stf32(SINCOS+i*8,sinf(i*2.0*3.141592653589793/8192));
        stf32(SINCOS+i*8+4,cosf(i*2.0*3.141592653589793/8192));
    }
}

/* Model the native prepend order and cut-turn count-1 creation, including the
 * automatic particle that originally joined successive arcs across the back.
 * Storage is synthetic; native allocation/GPU calls are not simulated. */
static unsigned particles_created;
static void particle_pass(int i, unsigned limit) {
    uint32_t e=emitters[i], cb=callbacks[i], count=ld32(e+0x1b4);
    assert(ld32(e+0x254)&1); /* ordinary emission must stay disabled */
    unsigned pending=ld16(cb+6);
    if(pending) {
        assert(pending==12 && count==0);
        uint32_t points=ld32(cb+8), head=0;
        unsigned n=0;
        for(unsigned j=pending-1;j>0 && n<limit;--j) {
            uint32_t p=0x19000000+i*0x10000+n*0x200;
            memset(ppc_ptr(p),0,0x128);
            st32(p,p); st32(p+0xc,head); st32(p+0xd4,e);
            if(head) st32(head+8,p);
            for(unsigned k=0;k<12;k+=4) stf32(p+0x10+k,ldf32(points+j*12+k));
            stf32(p+0x78,-1); stf32(p+0x7c,2);
            head=p; ++n; ++particles_created;
        }
        st32(e+0x1ac,head); st32(e+0x1b4,n); st16(cb+6,0);
        for(unsigned k=0;k<12;k+=4) stf32(e+0x22c+k,ldf32(points+k));
    }
    hook(GAME_PARTICLES,2,e);
    for(uint32_t p=ld32(e+0x1ac);p;p=ld32(p+0xc)) {
        /* Native incFrame and physics precede the return hook. An unrenewed
         * particle would expire, and uncorrected velocity would trail behind. */
        float age=ldf32(p+0x78)+1;
        assert(age<ldf32(p+0x7c)); stf32(p+0x78,age);
        stf32(p+0x28,9999); stf32(p+0x2c,-10); stf32(p+0x30,-9999);
        /* Exercise correction after native animation, not only at creation. */
        stf32(p+0x8c,0.3f); stf32(p+0x90,0.4f); stf32(p+0x94,0.5f);
        st16(p+0xc0,0x2000);
    }
    hook(GAME_PARTICLES,3,e);
}
typedef struct { float x,y,z; } Vec;
static Vec sub(Vec a,Vec b) { return (Vec){a.x-b.x,a.y-b.y,a.z-b.z}; }
static float dot(Vec a,Vec b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
static Vec cross(Vec a,Vec b) { return (Vec){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
static Vec norm(Vec v,Vec fallback) {
    float n=dot(v,v); if(!(n>3.814697265625e-06f)) return fallback;
    n=sqrtf(n); return (Vec){v.x/n,v.y/n,v.z/n};
}
static Vec vec(uint32_t p) { return (Vec){ldf32(p),ldf32(p+4),ldf32(p+8)}; }
/* Reference the native dirTypePrevPtcl / JPADrawExecStripe width calculation.
 * This tests actual edge positions, not merely the centerline. GPU/material
 * operations remain outside this fixture. Emitter translation includes an
 * example local Y offset; the first-point tangent must not depend on it. */
static Vec stripe_edge(uint32_t p,uint32_t e,float width,int fix) {
    uint32_t prev=ld32(p+8), out=TARGET+0x8000;
    Vec pos=vec(p+0x28), reference;
    if(prev) reference=vec(ld32(prev)+0x28);
    else { reference=vec(e+0x22c); reference.y+=47; }
    Vec direction=sub(reference,pos);
    stf32(out,direction.x); stf32(out+4,direction.y); stf32(out+8,direction.z);
    if(fix) { cpu.r[4]=e; cpu.r[5]=out; hook(GAME_DIRECTION,3,p); }
    direction=norm(vec(out),(Vec){0,1,0});
    Vec side=norm(cross(vec(p+0x8c),direction),(Vec){1,0,0});
    Vec axis=norm(cross(direction,side),(Vec){0,0,0});
    float angle=(ld16(p+0xc0)>>3)*2.0f*3.141592653589793f/8192;
    return (Vec){pos.x+width*(sinf(angle)*axis.x+cosf(angle)*side.x),
                 pos.y+width*(sinf(angle)*axis.y+cosf(angle)*side.y),
                 pos.z+width*(sinf(angle)*axis.z+cosf(angle)*side.z)};
}
static void test_stripe_edges(void) {
    const int headings[]={0,0x2000,0x4000,-0x4000,-0x8000};
    for(int aim=0;aim<7;++aim) for(int hz=30;hz<=60;hz+=30) {
        setup(); dt=1.0/hz; Vec normal={0,1,0};
        if(aim<5) st16(LINK+0x32a,headings[aim]);
        else {
            Vec flight=aim==5?(Vec){500,100,1000}:(Vec){0,500,1};
            st32(LINK+0x65b0,TARGET); stf32(TARGET+0x37c,flight.x);
            stf32(TARGET+0x380,60+flight.y); stf32(TARGET+0x384,flight.z);
            normal=norm(cross(flight,(Vec){flight.z,0,-flight.x}),(Vec){0,1,0});
        }
        begin(0x55,1,3);
        for(int tick=0;tick<12;++tick) {
            if(tick) frame();
            for(int i=0;i<3;++i) {
                particle_pass(i,aim==4?5:11);
                uint32_t e=emitters[i], head=ld32(e+0x1ac); Vec origin=vec(head+0x28);
                for(uint32_t p=head;p;p=ld32(p+0xc)) {
                    if(p==head) {
                        stripe_edge(p,e,39,1);
                        Vec expected=sub(vec(p+0x28),vec(ld32(ld32(p+0xc))+0x28));
                        Vec actual=vec(TARGET+0x8000);
                        assert(dot(expected,expected)>1);
                        assert(dot(sub(actual,expected),sub(actual,expected))<0.0001);
                    }
                    Vec a=stripe_edge(p,e,-28,1), b=stripe_edge(p,e,39,1);
                    assert(fabsf(dot(sub(a,origin),normal))<0.01);
                    assert(fabsf(dot(sub(b,origin),normal))<0.01);
                    assert(fabsf(sqrtf(dot(sub(b,a),sub(b,a)))-67)<0.01);
                    if(ld32(p+8)) {
                        Vec native=stripe_edge(p,e,39,0);
                        assert(dot(sub(b,native),sub(b,native))<0.0001);
                    }
                }
            }
        }
        /* Both the endpoint hook and frame correction cease after detach. */
        enabled=0; frame();
        uint32_t p=ld32(emitters[0]+0x1ac);
        Vec a=stripe_edge(p,emitters[0],39,0), b=stripe_edge(p,emitters[0],39,1);
        assert(dot(sub(a,b),sub(a,b))<0.0001);
    }
    setup();
    stf32(TARGET,1); stf32(TARGET+4,2); stf32(TARGET+8,3);
    cpu.r[4]=EMITTER; cpu.r[5]=TARGET; hook(GAME_DIRECTION,3,LINK);
    assert(near(ldf32(TARGET),1)&&near(ldf32(TARGET+4),2)&&near(ldf32(TARGET+8),3));
    puts("Stripe edge regression: endpoint tangent, planar width, headings, elevated aim, 30/60 Hz, partial allocation and isolation passed");
}
static void assert_arc(int i, float center_z, unsigned expected) {
    uint32_t e=emitters[i], node=ld32(e+0x1ac); unsigned n=0;
    assert(ld32(e+0x1b4)==expected && ld16(callbacks[i]+6)==0);
    float previous_x=-151, previous_z=0;
    while(node) {
        uint32_t p=ld32(node);
        float x=ldf32(p+0x28), y=ldf32(p+0x2c), z=ldf32(p+0x30);
        assert(x>=previous_x && near(y,60));
        assert(z>=center_z-0.05 && z<=center_z+55.05);
        assert(fabsf(x*x/(150*150)+(z-center_z)*(z-center_z)/(55*55)-1)<0.01);
        if(expected==11 && n) assert(hypotf(x-previous_x,z-previous_z)<50);
        if(!n) assert(near(x,-150) && near(z,center_z));
        previous_x=x; previous_z=z; node=ld32(node+0xc); ++n;
    }
    assert(n==expected && near(previous_x,150) && near(previous_z,center_z));
}
static void test_open_arc(void) {
    for(int hz=30;hz<=60;hz+=30) {
        setup(); dt=1.0/hz; particles_created=0; begin(0x55,1,3);
        for(int tick=0;tick<hz;++tick) {
            if(tick) frame();
            for(int i=0;i<3;++i) { particle_pass(i,11); assert_arc(i,tick*1200.0f/hz,11); }
            assert(particles_created==33); /* never an old/new arc join */
        }
        paused=1; frame();
        for(int i=0;i<3;++i) { particle_pass(i,11); assert_arc(i,(hz-1)*1200.0f/hz,11); }
        paused=0; frame();
        for(int i=0;i<3;++i) { particle_pass(i,11); assert_arc(i,1200,11); }
        assert(particles_created==33);
        enabled=0; frame(); assert(ended==3);
        unsigned char before[0x128]; memcpy(before,ppc_ptr(0x19000000),sizeof(before));
        hook(GAME_PARTICLES,2,emitters[0]); hook(GAME_PARTICLES,3,emitters[0]);
        assert(!memcmp(before,ppc_ptr(0x19000000),sizeof(before))); /* normal expiry after stop */
    }
    setup(); particles_created=0; begin(0x55,1,3);
    particle_pass(0,5); assert_arc(0,0,5); frame(); particle_pass(0,5); assert_arc(0,40,5);
    assert(particles_created==5); /* partial native allocation is bounded */
    /* Same native function on Link's regular spin does absolutely nothing. */
    unsigned char before[0x400]; memset(ppc_ptr(EMITTER),0xa5,sizeof(before));
    memcpy(before,ppc_ptr(EMITTER),sizeof(before));
    hook(GAME_PARTICLES,2,EMITTER); hook(GAME_PARTICLES,3,EMITTER);
    assert(!memcmp(before,ppc_ptr(EMITTER),sizeof(before)));
    puts("Open stripe regression: stable ordered arc, no rear chords/history, 30/60 Hz, pause, partial allocation and isolation passed");
}

int main(int argc,char** argv) {
    assert(argc==2);
    assert(mmap(PPC_MEM_BASE,0x80000000ull,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0)==PPC_MEM_BASE);
    void* lib=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
    if(!lib) { puts(dlerror()); return 1; }
    WWHDGuestInitV1 init=(WWHDGuestInitV1)dlsym(lib,WWHD_GUEST_INIT_SYMBOL); assert(init);
    WWHDGuestHostV1 host={.size=sizeof(host),.abi_version=WWHD_GUEST_ABI_VERSION,
        .dispatch=dispatch,.unimplemented=unimplemented,.trap=trap,.core_preempt=preempt,
        .call_original=original,.service=service};
    mod=init(&host); assert(mod && mod->hook_count==10);
    for(unsigned i=0;i<mod->hook_count;++i) assert(mod->hooks[i].kind!=1);
    setup(); begin(0x55,1,3); assert(spawned==3 && !collisions);
    for(int i=0;i<3;++i) { assert(ld16(callbacks[i]+6)==12); own(ld32(callbacks[i]+8),12*12); assert(near(ldf32(emitters[i]+0x238),1.5)); assert(ld32(emitters[i]+0x5c)>0 && !(ld32(emitters[i]+0x254)&0x40)); }
    unsigned char before[0x9000]; memcpy(before,ppc_ptr(LINK),sizeof(before));
    for(int i=0;i<5;++i) frame(); assert(!collisions);
    frame(); assert(collisions==1 && near(last_z,240));
    assert(ld8(capsule+0x14)==4 && ld8(capsule+0x6e)==15 && ld8(capsule+0x6f)==1);
    assert(!memcmp(before,ppc_ptr(LINK),sizeof(before)));
    for(int i=6;i<60;++i) frame(); assert(spawned==3 && !ended && near(last_z,2400));
    frame(); assert(ended==3);
    puts("Full-heart launch, native ABI/storage, reach, range and unchanged Link passed");
    setup(); st16(SAVE+0x22,11); begin(0x55,1,3); assert(!spawned);
    st16(SAVE+0x22,12); frame(); assert(!spawned); begin(0x55,1,3); assert(spawned==3);
    setup(); begin(0x55,0,3); assert(!spawned); st32(LINK+0x78bc,0x19); frame(); assert(spawned==3);
    setup(); begin(0x55,0,3); st16(SAVE+0x22,11); st32(LINK+0x78bc,0x19); frame(); assert(!spawned);
    setup(); st16(LINK+0x69b0,0x102); begin(0x55,1,3); assert(!spawned);
    setup(); st32(LINK+0x65f0,0x42); st32(LINK+0x78bc,0x19); frame(); assert(!spawned);
    setup(); begin(0x56,1,3); assert(spawned==3); for(int i=0;i<20;++i) frame(); assert(spawned==3);
    setup(); st16(SAVE+0x20,80); st16(SAVE+0x22,80); begin(0x55,1,3); assert(spawned==3);
    setup(); st32(SAVE_PTR,0); begin(0x55,1,3); assert(!spawned);
    setup(); begin(0x55,1,2); assert(!spawned);
    setup(); enabled=0; begin(0x55,1,3); assert(!spawned);
    puts("Health/release gates, quick/charged/Hurricane, ordinary/weapon exclusions passed");
    setup(); begin(0x55,1,3); st16(SAVE+0x22,4); frame(); assert(!ended);
    paused=1; unsigned n=lines; frame(); assert(lines==n && !ended);
    paused=0; menu=1; frame(); assert(lines==n); menu=0; frame(); assert(lines>n);
    n=lines; hook(GAME_LINK,3,LINK); assert(lines==n); /* same logic pass */
    enabled=0; frame(); assert(ended==3 && !(ld32(capsule)&1));
    setup(); begin(0x55,1,3); wall=1; frame(); assert(ended==3 && !collisions);
    setup(); begin(0x55,1,3); st32(capsule+0x54,1); frame(); assert(ended==3 && !lines);
    setup(); begin(0x55,1,3); st32(capsule+0x54,0); frame(); assert(!ended); /* enemy not a shield */
    begin(0x55,1,3); assert(spawned==6 && ended==3);
    for(int reason=0;reason<7;++reason) {
        setup(); begin(0x55,1,3);
        if(reason==0) st16(SAVE+0x22,0);
        if(reason==1) st8(PLAY+0x514c,1);
        if(reason==2) st8(PLAY+0x5292,1);
        if(reason==3) st8(LINK+0x326,1);
        if(reason==4) st32(LINK+4,43);
        if(reason==5) { hook(GAME_DELETE,2,LINK); assert(ended==3); continue; }
        if(reason==6) { hook(GAME_LOAD,2,0); assert(ended==3); continue; }
        frame(); assert(ended==3);
    }
    setup(); begin(0x55,1,3); st32(PLAY+0x5ab0,MANAGER+0x1000); frame(); assert(!ended); /* no stale calls */
    puts("Pause, once-per-tick, shield/wall, replacement and lifetime cleanup passed");
    setup(); dt=1.0/60; begin(0x55,1,3); for(int i=0;i<120;++i) frame();
    assert(!ended && near(last_z,2400)); frame(); assert(ended==3);
    setup(); st32(LINK+0x65b0,TARGET); stf32(TARGET+0x37c,500); stf32(TARGET+0x380,160); stf32(TARGET+0x384,1000);
    begin(0x55,1,3); for(int i=0;i<10;++i) frame();
    assert(ldf32(capsule+0x7c)>0 && ldf32(capsule+0x80)>0 && ldf32(capsule+0x84)>0);
    setup(); failing=1; begin(0x55,1,3); assert(spawned==3); frame(); enabled=0; frame(); assert(!ended);
    puts("30/60-Hz range, elevated lock-on and particle allocation failure passed");
    /* Full-state epoch changes can arrive before Link executes. The first
     * particle hook must end the restored wave, not renew it indefinitely. */
    setup(); begin(0x55,1,3); ++epoch;
    hook(GAME_PARTICLES,2,emitters[0]); assert(ended==3);
    frame(); assert(spawned==3);
    /* A replaced particle pool invalidates all old addresses. */
    setup(); begin(0x55,1,3); ++epoch; st32(PLAY+0x5ab0,MANAGER+0x1000);
    frame(); assert(!ended);
    /* Delete/save-load may be the first hook after a full-state restore. */
    const unsigned cleanup_hooks[]={GAME_DELETE,GAME_LOAD};
    for(unsigned i=0;i<2;++i) {
        setup();begin(0x55,1,3);++epoch;hook(cleanup_hooks[i],2,LINK);assert(ended==3);
        setup();begin(0x55,1,3);++epoch;st32(PLAY+0x5ab0,MANAGER+0x1000);
        hook(cleanup_hooks[i],2,LINK);assert(!ended);
    }
    /* Native fallback: without callbacks from this mod, finite max-frame and
     * unrenewed particles reach native deferred emitter deletion. */
    setup(); begin(0x55,1,3); particle_pass(0,11);
    uint32_t e=emitters[0]; assert(ld32(e+0x5c)>0 && !(ld32(e+0x254)&0x40));
    float age=ldf32(ld32(e+0x1ac)+0x78), life=ldf32(ld32(e+0x1ac)+0x7c);
    assert(age+4>=life && ld32(e+0x5c)<16);
    puts("Full-state epoch, replaced particle pool and bounded native expiry passed");
    test_open_arc();
    test_stripe_edges();
    puts("Sword Wave translated integration: all checks passed");
    return 0;
}
