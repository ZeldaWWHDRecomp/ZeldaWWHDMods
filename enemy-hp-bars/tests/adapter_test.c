/* SPDX-License-Identifier: MPL-2.0
 * Execute the shipped PowerPC code after the real install-time translation.
 * Synthetic game memory/services verify integration, not in-game visuals/AI.
 */
#define _GNU_SOURCE
#include "wwhd_guest_abi.h"
#include "test_addresses.h"
#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
enum { PLAY=0x11000000, LINK=0x12000000, ENEMY=0x13000000, STACK=0x14000000 };
static const WWHDGuestModuleV1 *mod;
static Cpu cpu;
static volatile int preempt[3];
static uint64_t step, epoch, revision;
static uint32_t callback;
static unsigned rectangles, texts, emissions;
static int enable=1, bars=1, numbers=1, values, after_damage, visible=1;
static char last_text[32];
static float size=100, distance=2000;
static unsigned char before_play[0x6000], before_link[0x400], before_enemy[0x400];
static void dispatch(Cpu *c) {
    for(unsigned i=0;i<mod->func_count;++i)
        if(mod->funcs[i].addr==c->pc) { mod->funcs[i].fn(c); return; }
    if(c->pc==GAME_PLAY) { c->r[3]=PLAY; return; }
    fprintf(stderr,"Unexpected game call %08X\n",c->pc); abort();
}
static void hook(unsigned target,unsigned kind,unsigned actor) {
    int found=0;
    for(unsigned i=0;i<mod->hook_count;++i) {
        if(mod->hooks[i].target!=target || mod->hooks[i].kind!=kind) continue;
        cpu.r[3]=actor;
        mod->hooks[i].fn(&cpu);
        assert(cpu.r[1]==STACK+0xFF000); found=1;
    }
    assert(found);
}
static void original(Cpu *c,uint32_t address) {
    (void)c; (void)address; assert(!"observer must not call originals");
}
static void svc_step(Cpu *c) { c->r[3]=(uint32_t)(step>>32); c->r[4]=(uint32_t)step; }
static void svc_epoch(Cpu *c) { c->r[3]=(uint32_t)(epoch>>32); c->r[4]=(uint32_t)epoch; }
static void svc_revision(Cpu *c) { c->r[3]=0; c->r[4]=(uint32_t)revision; }
static void svc_dt(Cpu *c) { c->f[1].ps0=1.0/30; }
static void svc_bool(Cpu *c) {
    const char *id=(const char *)ppc_ptr(c->r[3]);
    if(!strcmp(id,"enabled")) c->r[3]=enable;
    else if(!strcmp(id,"hp_bars")) c->r[3]=bars;
    else if(!strcmp(id,"damage_numbers")) c->r[3]=numbers;
    else if(!strcmp(id,"show_values")) c->r[3]=values;
    else assert(!"unknown boolean");
}
static void svc_float(Cpu *c) {
    const char *id=(const char *)ppc_ptr(c->r[3]);
    if(!strcmp(id,"scale")) c->f[1].ps0=size;
    else if(!strcmp(id,"distance")) c->f[1].ps0=distance;
    else assert(!"unknown number option");
}
static void svc_string(Cpu *c) {
    const char *value=after_damage?"After damage":"Always";
    assert(c->r[5]>strlen(value)); strcpy((char *)ppc_ptr(c->r[4]),value);
    c->r[3]=(uint32_t)strlen(value);
}
static void svc_hud(Cpu *c) { callback=c->r[3]; assert(c->r[4]==0); c->r[3]=1; }
static void svc_project(Cpu *c) {
    /* Inputs and output must be in persistent mod memory, per host contract. */
    assert(c->r[3]>=mod->mem_base && c->r[3]+12<=mod->mem_base+mod->mem_size);
    assert(c->r[4]>=mod->mem_base && c->r[4]+8<=mod->mem_base+mod->mem_size);
    stf32(c->r[4],100); stf32(c->r[4]+4,0); c->r[3]=visible;
}
static void svc_emit(Cpu *c) {
    uint32_t p=c->r[4];
    assert(p>=mod->mem_base && p+72<=mod->mem_base+mod->mem_size);
    for(unsigned i=12;i<=32;i+=4) assert(isfinite(ldf32(p+i)));
    if(ld32(p)==1) {
        unsigned n=ld32(p+68),s=ld32(p+64);
        assert(n<sizeof(last_text) && s>=mod->mem_base && s+n<=mod->mem_base+mod->mem_size);
        memcpy(last_text,ppc_ptr(s),n); last_text[n]=0; ++texts;
    } else { assert(ld32(p)==0); ++rectangles; }
    assert(++emissions<=1024); c->r[3]=1;
}
static void svc_copy(Cpu *c) { memcpy(ppc_ptr(c->r[3]),ppc_ptr(c->r[4]),c->r[5]); }
static void svc_set(Cpu *c) { memset(ppc_ptr(c->r[3]),c->r[4],c->r[5]); }
static PpcFunc service(const char *s) {
    if(!strcmp(s,"wwhd_logic_step")) return svc_step;
    if(!strcmp(s,"wwhd_logic_dt")) return svc_dt;
    if(!strcmp(s,"wwhd_hud_epoch")) return svc_epoch;
    if(!strcmp(s,"wwhd_config_changed")) return svc_revision;
    if(!strcmp(s,"wwhd_config_bool")) return svc_bool;
    if(!strcmp(s,"wwhd_config_float")) return svc_float;
    if(!strcmp(s,"wwhd_config_string")) return svc_string;
    if(!strcmp(s,"wwhd_hud_register")) return svc_hud;
    if(!strcmp(s,"wwhd_hud_emit")) return svc_emit;
    if(!strcmp(s,"wwhd_screen_point")) return svc_project;
    if(!strcmp(s,"memcpy")) return svc_copy;
    if(!strcmp(s,"memset")) return svc_set;
    fprintf(stderr,"Unexpected service %s\n",s); abort();
}
static void unimplemented(Cpu *c,uint32_t a,uint32_t insn) {
    (void)c; fprintf(stderr,"Unimplemented %08X %08X\n",a,insn); abort();
}
static void trap(Cpu *c,uint32_t a) { unimplemented(c,a,0); }
static void frame(void) { ++step; hook(GAME_LINK,2,LINK); }
static void draw(void) {
    emissions=rectangles=texts=0; last_text[0]=0;
    assert(callback); cpu.pc=callback; cpu.r[3]=1; dispatch(&cpu);
}
static void actor(unsigned id,int hp) {
    memset(ppc_ptr(ENEMY),0,0x400);
    st32(ENEMY+4,id); st16(ENEMY+0xE,0xBC);
    st8(ENEMY+0x326,0); st8(ENEMY+0x3A0,10); st8(ENEMY+0x3A1,hp);
    stf32(ENEMY+0x314,100); stf32(ENEMY+0x37C,100); stf32(ENEMY+0x380,250);
}
static void execute(int hp) {
    hook(GAME_MOBLIN,2,ENEMY); st8(ENEMY+0x3A1,hp); hook(GAME_MOBLIN,3,ENEMY);
}
static void setup(void) {
    memset(ppc_ptr(mod->mem_base),0,mod->mem_size);
    memcpy(ppc_ptr(mod->mem_base),mod->image,mod->image_size);
    memset(ppc_ptr(PLAY),0,0x6000); memset(ppc_ptr(LINK),0,0x400);
    memset(&cpu,0,sizeof(cpu)); cpu.r[1]=STACK+0xFF000;
    step=0; epoch=1; revision=1; callback=0;
    enable=bars=numbers=visible=1; values=after_damage=0; size=100; distance=2000;
    memcpy(ppc_ptr(PLAY+0x5134),"Cave09\0",8);
    st32(PLAY+0x5B2C,LINK); st32(LINK+4,10); st16(LINK+0xE,0xA8);
    actor(20,10); frame();
}
int main(int argc,char **argv) {
    assert(argc==2);
    void *memory=mmap(PPC_MEM_BASE,0x80000000ull,PROT_READ|PROT_WRITE,
        MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
    assert(memory==PPC_MEM_BASE);
    void *lib=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
    if(!lib) { puts(dlerror()); return 1; }
    WWHDGuestInitV1 init=(WWHDGuestInitV1)dlsym(lib,WWHD_GUEST_INIT_SYMBOL); assert(init);
    WWHDGuestHostV1 host={.size=sizeof(host),.abi_version=WWHD_GUEST_ABI_VERSION,
        .dispatch=dispatch,.unimplemented=unimplemented,.trap=trap,.core_preempt=preempt,
        .call_original=original,.service=service};
    mod=init(&host); assert(mod);
    for(unsigned i=0;i<mod->hook_count;++i) assert(mod->hooks[i].kind!=1);
    /* Actual host order (interp.cpp::hook_025DE788): actor Execute hooks,
     * guestmods::frame(new counter), then guestmods::draw_frame. The old exact
     * equality check dropped EVERY list in this sequence. Also cover repeated
     * presentation callbacks without mutating the completed pass's snapshots. */
    setup(); execute(6); ++step; draw();
    assert(rectangles==3 && texts==2 && !strcmp(last_text,"4"));
    draw(); assert(rectangles==3 && texts==2 && !strcmp(last_text,"4"));
    for(int hp=5;hp>=1;--hp) {
        hook(GAME_LINK,2,LINK); execute(hp); ++step; draw();
        assert(rectangles==3 && texts==(unsigned)(2*(7-hp)) && !strcmp(last_text,"1"));
    }
    hook(GAME_LINK,2,LINK); execute(0); ++step; draw();
    assert(!rectangles && texts==14 && !strcmp(last_text,"1"));
    ++step; draw(); assert(!emissions); /* no actor update: older than one publication */
    setup(); execute(6); ++step; ++epoch; draw(); assert(!emissions);
    setup(); execute(6); --step; draw(); assert(!emissions); /* counter rewind */
    puts("Host phase-order regression: execute -> counter publication -> HUD passed");
    setup(); execute(6); draw(); assert(rectangles==3 && texts==2 && !strcmp(last_text,"4"));
    visible=0; draw(); assert(!emissions); visible=1;
    /* Hooks and rendering must leave every native byte unchanged. */
    memcpy(before_play,ppc_ptr(PLAY),sizeof(before_play));
    memcpy(before_link,ppc_ptr(LINK),sizeof(before_link));
    memcpy(before_enemy,ppc_ptr(ENEMY),sizeof(before_enemy));
    frame(); execute(6); draw();
    assert(!memcmp(before_play,ppc_ptr(PLAY),sizeof(before_play)));
    assert(!memcmp(before_link,ppc_ptr(LINK),sizeof(before_link)));
    assert(!memcmp(before_enemy,ppc_ptr(ENEMY),sizeof(before_enemy)));
    setup(); execute(-5); draw(); assert(!rectangles && !strcmp(last_text,"10"));
    hook(GAME_DELETE,2,ENEMY); memset(ppc_ptr(ENEMY),0xFF,0x400);
    draw(); assert(!rectangles && !strcmp(last_text,"10")); /* no retained actor dereference */
    setup(); execute(10); draw(); assert(rectangles==3 && !texts); /* final HP restored after stun */
    actor(21,2); execute(2); draw(); assert(!texts); /* recycled memory */
    setup(); after_damage=1; ++revision; frame(); execute(10); draw(); assert(!emissions);
    frame(); execute(8); draw(); assert(rectangles==3 && texts==2);
    for(int i=0;i<125;++i) { frame(); execute(8); }
    draw(); assert(!emissions);
    setup(); numbers=0; values=1; ++revision; frame(); execute(6); draw();
    assert(texts==2 && !strcmp(last_text,"6/10"));
    bars=0; ++revision; frame(); execute(5); draw(); assert(!emissions);
    setup(); execute(6); ++epoch; frame(); execute(6); draw(); assert(rectangles==3 && !texts);
    st8(PLAY+0x5292,1); frame(); draw(); assert(!emissions); /* event */
    st8(PLAY+0x5292,0); st8(PLAY+0x514C,1); frame(); draw(); assert(!emissions); /* transition */
    setup(); execute(6); st8(LINK+0x326,1); frame(); draw(); assert(!emissions); /* room */
    setup(); memcpy(ppc_ptr(PLAY+0x5134),"Znpfl81",8); frame(); execute(6); draw(); assert(emissions);
    setup(); st16(ENEMY+0xE,0xE3); st32(ENEMY+0xB0,6);
    hook(GAME_OCTOROK,2,ENEMY); hook(GAME_OCTOROK,3,ENEMY); draw(); assert(!emissions); /* rock */
    setup(); stf32(ENEMY+0x314,5000); execute(6); draw(); assert(!emissions); /* distance */
    setup(); enable=0; ++revision; frame(); execute(6); draw(); assert(!emissions);
    setup(); execute(6); hook(GAME_LOAD,2,0); draw(); assert(!emissions);
    setup(); size=NAN; distance=INFINITY; ++revision; frame(); execute(6); draw(); assert(rectangles==3);
    puts("Translated adapter: HP changes, fatal hits, deletion, identity, options, projection, state loads and native memory isolation passed");
    dlclose(lib); munmap(memory,0x80000000ull);
}
