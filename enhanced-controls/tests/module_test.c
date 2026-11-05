/* SPDX-License-Identifier: MPL-2.0
 * Executes the real PPC-translated module against synthetic guest memory and
 * host services. No game instructions or assets are used. Linux test runner. */
#define _GNU_SOURCE
#include "wwhd_guest_abi.h"
#include "fixture.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "../src/editor.h"

extern const WWHDGuestModuleV1* wwhd_guest_module_v1(const WWHDGuestHostV1*);
static const WWHDGuestModuleV1* mod;
static Cpu cpu;
static uint32_t touch_fn, filter_fn, hud_fn;
static uint64_t step=1, epoch=1;
static int touch_enabled, editor_visible=1, claim_count, hud_count;
static int spins, equip_animation, guard_animation;
static int texture_calls, image_count, releases, fail_texture;
static float editor_x,editor_y=260;
static float claim[6][4];
static unsigned char files[2][512];
static int file_size[2], writes, reads, fail_write;
static FILE* capture;
static int texture_index[512];
static uint32_t texture_handle[512];
extern void test_hud_reset(void);
extern uint32_t test_hud_begin(uint64_t step,uint64_t epoch);
extern uint32_t test_hud_texture(void);
extern void test_hud_release(uint32_t image);
extern int test_hud_emit(uint32_t list,uint32_t kind,uint32_t anchor,uint32_t blend,
                        const float* f,uint32_t rgba,uint32_t image,const char* text,uint32_t bytes);
extern void test_hud_end(uint32_t list,unsigned commands);
static volatile int preempt_flags[3];
static const uint32_t LINK=0x18000000, SAVE=0x18010000,
                      SCRATCH=0x30040000;
static const char* string(uint32_t a) { return (const char*)ppc_ptr(a); }
static int own(uint32_t a, uint32_t n) { return a>=mod->mem_base && (uint64_t)a+n<=mod->mem_base+mod->mem_size; }
static int file_index(uint32_t name) {
    assert(own(name,1));
    if(!strcmp(string(name),"macro-layout-a.bin")) return 0;
    assert(!strcmp(string(name),"macro-layout-b.bin")); return 1;
}

static void call(uint32_t fn, uint32_t arg) {
    uint32_t list=fn==hud_fn?test_hud_begin(step,epoch):0;
    int before=hud_count;
    cpu.r[1]=0x30020000; cpu.r[3]=list?list:arg; cpu.pc=fn;
    for(uint32_t i=0;i<mod->func_count;++i) if(mod->funcs[i].addr==fn) {
        mod->funcs[i].fn(&cpu);
        if(list) test_hud_end(list,hud_count-before);
        return;
    }
    fprintf(stderr,"unknown callback %08x\n",fn); abort();
}
static void dispatch(Cpu* c) {
    if(c->pc==PLAY_GET) {c->r[3]=PLAY;return;}
    if(c->pc==SPIN) {
        assert(c->r[3]==LINK && c->r[4]==1);
        ++spins; st32(LINK+0x65F0,0x55); c->r[3]=1; return;
    }
    if(c->pc==EQUIPPING || c->pc==GUARDING) {
        assert(c->r[3]==LINK);
        c->r[3]=c->pc==EQUIPPING?equip_animation:guard_animation; return;
    }
    for(uint32_t i=0;i<mod->func_count;++i) if(mod->funcs[i].addr==c->pc) { mod->funcs[i].fn(c); return; }
    fprintf(stderr,"unexpected dispatch %08x\n",c->pc); abort();
}
static void unavailable(Cpu* c,uint32_t a,uint32_t insn) { (void)c; fprintf(stderr,"unsupported %08x %08x\n",a,insn); abort(); }
static void trap(Cpu* c,uint32_t a) { unavailable(c,a,0); }
static uint64_t timebase(void) { return step; }
static double reciprocal(double x) { return 1/x; }
static double rsqrt(double x) { return 1/sqrt(x); }
static void preempt(Cpu* c) { (void)c; abort(); }
static void original(Cpu* c,uint32_t a) { unavailable(c,a,0); }
static void s_copy(Cpu* c) { assert(c->r[5]<4096); memmove(ppc_ptr(c->r[3]),ppc_ptr(c->r[4]),c->r[5]); }
static void s_file_read(Cpu* c) {
    int i=file_index(c->r[3]); ++reads; assert(own(c->r[4],c->r[5]));
    if(!file_size[i]) { c->r[3]=(uint32_t)-1; return; }
    unsigned n=file_size[i]<(int)c->r[5]?(unsigned)file_size[i]:c->r[5];
    memcpy(ppc_ptr(c->r[4]),files[i],n); c->r[3]=n;
}
static void s_file_write(Cpu* c) {
    int i=file_index(c->r[3]); ++writes; assert(own(c->r[4],c->r[5]) && c->r[5]<=512);
    unsigned n=fail_write?9:c->r[5];
    memcpy(files[i],ppc_ptr(c->r[4]),n); file_size[i]=n; c->r[3]=n;
}
static void s_memset(Cpu* c) { memset(ppc_ptr(c->r[3]),c->r[4],c->r[5]); }
static void s_touch(Cpu* c) { touch_fn=c->r[3]; c->r[3]=1; }
static void s_filter(Cpu* c) { filter_fn=c->r[3]; c->r[3]=1; }
static void s_hud(Cpu* c) { hud_fn=c->r[3]; assert(c->r[4]==0); c->r[3]=1; }
static void s_log(Cpu* c) { assert(own(c->r[3],1)); }
static void s_read(Cpu* c) { assert(own(c->r[3],32)); memset(ppc_ptr(c->r[3]),0,32); }
static void s_emit(Cpu* c) {
    uint32_t a=c->r[4]; assert(own(a,72)); ++hud_count;
    if(ld32(a)==2) { ++image_count; assert(ld32(a+60)>0); assert(ldf32(a+20)==ldf32(a+24)); }
    if(ld32(a)==1) { uint32_t text=ld32(a+64), bytes=ld32(a+68); assert(own(text,bytes)); }
    if(capture) {
        fprintf(capture,"{\"kind\":%u,\"x\":%g,\"y\":%g,\"w\":%g,\"h\":%g,\"size\":%g,\"rgba\":%u,\"image\":%d,\"text\":\"%.*s\"}\n",
            ld32(a),ldf32(a+12),ldf32(a+16),ldf32(a+20),ldf32(a+24),ldf32(a+28),ld32(a+56),
            ld32(a)==2?texture_index[ld32(a+60)]:-1,ld32(a)==1?(int)ld32(a+68):0,ld32(a)==1?string(ld32(a+64)):"");
    }
    float geometry[11];
    for(unsigned i=0;i<11;++i) geometry[i]=ldf32(a+12+4*i);
    int ok=test_hud_emit(c->r[3],ld32(a),ld32(a+4),ld32(a+8),geometry,ld32(a+56),
        ld32(a)==2?texture_handle[ld32(a+60)]:0,
        ld32(a)==1?string(ld32(a+64)):"",ld32(a)==1?ld32(a+68):0);
    assert(ok); c->r[3]=ok;
}
static void u64(Cpu* c,uint64_t v) { c->r[3]=v>>32; c->r[4]=(uint32_t)v; }
static void s_step(Cpu* c) { u64(c,step); }
static void s_epoch(Cpu* c) { u64(c,epoch); }
static void s_dt(Cpu* c) { c->f[1].ps0=1./30; }
static void s_bool(Cpu* c) {
    const char* name=string(c->r[3]); assert(own(c->r[3],1));
    if(!strcmp(name,"touch-enabled")) {assert(c->r[4]==0);c->r[3]=touch_enabled;return;}
    assert(!strcmp(name,"editor-visible")); c->r[3]=editor_visible;
}
static void s_number(Cpu* c) {
    assert(own(c->r[3],1)); const char* name=string(c->r[3]);
    assert(!strcmp(name,"editor-x") || !strcmp(name,"editor-y"));
    c->f[1].ps0=!strcmp(name,"editor-x")?editor_x:editor_y;
}
static void s_claim(Cpu* c) {
    unsigned slot=c->r[3]; assert(slot<6); ++claim_count;
    for(int i=0;i<4;++i) claim[slot][i]=(float)c->f[i+1].ps0;
    c->r[3]=1;
}
static void s_texture(Cpu* c) {
    assert(c->r[3]==0 && own(c->r[4],1));
    assert(!strncmp(string(c->r[4]),"assets/combat_",14));
    ++texture_calls; assert(texture_calls<512);
    const char* path=string(c->r[4]);
    texture_index[texture_calls]=strstr(path,"jump")?0:strstr(path,"vertical")?1:strstr(path,"spin")?2:3;
    if(!fail_texture) texture_handle[texture_calls]=test_hud_texture();
    c->r[3]=fail_texture?0:texture_calls;
}
static void s_release(Cpu* c) { assert(c->r[3]); test_hud_release(texture_handle[c->r[3]]); ++releases; }
static PpcFunc service(const char* name) {
    struct { const char* name; PpcFunc fn; } all[]={
        {"memcpy",s_copy},{"memmove",s_copy},{"wwhd_file_read",s_file_read},{"wwhd_file_write",s_file_write},{"wwhd_hud_texture",s_texture},{"wwhd_hud_release",s_release},{"memset",s_memset},{"wwhd_touch_register",s_touch},{"wwhd_input_filter_register",s_filter},
        {"wwhd_hud_register",s_hud},{"wwhd_log",s_log},{"wwhd_input_read",s_read},
        {"wwhd_hud_emit",s_emit},{"wwhd_logic_step",s_step},{"wwhd_hud_epoch",s_epoch},
        {"wwhd_logic_dt",s_dt},{"wwhd_config_bool",s_bool},
        {"wwhd_config_float",s_number},{"wwhd_touch_claim",s_claim}};
    for(unsigned i=0;i<sizeof(all)/sizeof(all[0]);++i) if(!strcmp(name,all[i].name)) return all[i].fn;
    fprintf(stderr,"unexpected service %s\n",name); abort();
}
static void hook(uint32_t target,uint32_t arg) {
    for(uint32_t i=0;i<mod->hook_count;++i) if(mod->hooks[i].target==target) { call(mod->hooks[i].func,arg); return; }
    abort();
}
static void touch(unsigned kind,float x,float y) {
    st32(SCRATCH,kind); stf32(SCRATCH+4,x); stf32(SCRATCH+8,y); call(touch_fn,SCRATCH);
}
static unsigned pad(unsigned real) {
    memset(ppc_ptr(SCRATCH),0,32); st32(SCRATCH,real); call(filter_fn,SCRATCH); return ld32(SCRATCH);
}
static void next(void) { ++step; (void)pad(0); }
static void tap(float x,float y) { touch(0,x,y); touch(2,x,y); }
static void action(int id) { EcRect r=ee_action_rect(id); tap(r.x,r.y); }
static void tab(int i) { EcRect r=ee_tab(i); tap(r.x,r.y); }
static void open_editor(void) { tap(editor_x,editor_y); assert(claim[5][0]==-640 && claim[5][2]==640); }
static void reload_module(void) {
    test_hud_reset();
    memset(ppc_ptr(mod->mem_base),0,mod->mem_size);
    memcpy(ppc_ptr(mod->mem_base),mod->image,mod->image_size);
    hook(LINK_EXECUTE,LINK);
}
int main(void) {
    void* memory=mmap(PPC_MEM_BASE,1ull<<32,PROT_READ|PROT_WRITE,
        MAP_PRIVATE|MAP_ANONYMOUS|MAP_NORESERVE|MAP_FIXED_NOREPLACE,-1,0);
    assert(memory==PPC_MEM_BASE);
    WWHDGuestHostV1 host={sizeof(host),WWHD_GUEST_ABI_VERSION,dispatch,unavailable,trap,timebase,
        reciprocal,rsqrt,preempt,preempt_flags,original,service};
    mod=wwhd_guest_module_v1(&host); assert(mod && mod->hook_count==1);
    memcpy(ppc_ptr(mod->mem_base),mod->image,mod->image_size);
    memcpy(ppc_ptr(PLAY+0x5134),"sea",4);
    st32(PLAY+0x5B2C,LINK); st32(LINK,1); st32(LINK+4,7); st16(LINK+0x69B0,0x103);
    st32(LINK+0x65F0,4);
    st32(SAVE_PTR,SAVE); st16(SAVE+0x20,12); st16(SAVE+0x22,12);
    hook(LINK_EXECUTE,LINK);assert(touch_fn && filter_fn && hud_fn);
    call(hud_fn,1);assert(claim_count==0 && hud_count==0 && texture_calls==0);
    assert(pad(0x1000)==0x1000); /* default off preserves physical input */
    touch_enabled=1;next();assert(claim_count==6);
    assert(claim[4][0]==-48 && claim[4][2]==48);
    call(hud_fn,1); assert(hud_count==10 && image_count==4 && texture_calls==4);
    call(hud_fn,1); assert(texture_calls==4); /* cached, not decoded per frame */
    ++epoch; call(hud_fn,1); assert(texture_calls==8 && releases==0);
    fail_texture=1; ++epoch; hud_count=0; call(hud_fn,1);
    assert(hud_count==10 && texture_calls==12); /* text fallback */
    call(hud_fn,1); assert(texture_calls==12); /* no repeated failed loads */
    fail_texture=0; ++epoch; call(hud_fn,1); assert(texture_calls==16);
    int n=claim_count; next(); call(hud_fn,1); assert(claim_count==n);
    /* Real Y survives the macro; repeated reads do not advance its clock. */
    touch(0,-150,-180); assert(pad(0x1000)==(0x1000|0x80));
    next(); next(); assert(pad(0)==(0x80|0x8000)); assert(pad(0)==(0x80|0x8000));
    touch(2,-150,-180); assert(pad(0)==(0x80|0x8000));
    for(int i=0;i<7;++i) next(); assert(pad(0x1000)==0x1000);
    touch(0,50,-180); assert(pad(0)==0 && ldf32(SCRATCH+4)==0);
    st8(MENU_FLAG,1); assert(pad(0)==0); assert(claim[0][0]==claim[0][2]);
    st8(MENU_FLAG,0); next(); assert(pad(0)==0);
    hook(LINK_EXECUTE,LINK); assert(spins==0); /* menu discarded queued tap */
    /* No native dispatch from touch/filter/HUD. Lifting still commits one tap. */
    touch(0,50,-180); touch(2,50,-180);
    assert(pad(0x5000)==0x5000); call(hud_fn,1); assert(spins==0);
    memset(ppc_ptr(SCRATCH),0,32); st32(SCRATCH,0x5000);
    stf32(SCRATCH+4,.3f); stf32(SCRATCH+8,-.4f); call(filter_fn,SCRATCH);
    assert(ld32(SCRATCH)==0x5000 && ldf32(SCRATCH+4)==.3f && ldf32(SCRATCH+8)==-.4f);
    hook(LINK_EXECUTE,LINK); assert(spins==1);
    for(int i=0;i<12;++i) { next(); hook(LINK_EXECUTE,LINK); }
    assert(spins==1); /* holding/repeated hooks never restart the native attack */
    touch(0,50,-180); hook(LINK_EXECUTE,LINK); assert(spins==1); /* spin busy */
    /* Every unsupported proc refuses the request, including attack/damage/jump.
     * The stub changes proc on success just like native commonProcInit. */
    for(unsigned proc=0;proc<=0x100;++proc) {
        touch(3,0,0); st32(LINK+0x65F0,proc);
        int before=spins, allowed=proc==4||proc==6||proc==7||proc==8||proc==9||proc==0x17||proc==0x18;
        touch(0,50,-180); hook(LINK_EXECUTE,LINK); assert(spins==before+allowed);
        st32(LINK+0x65F0,4); hook(LINK_EXECUTE,LINK); assert(spins==before+allowed);
    }
    /* Revalidate sword/item/catch/curse/animation state when executing. */
    for(int reason=0;reason<8;++reason) {
        touch(3,0,0); st32(LINK+0x65F0,4); touch(0,50,-180); int before=spins;
        switch(reason) {
            case 0: st16(LINK+0x69B0,0xff); break;
            case 1: st32(LINK+0x8260,1); break;
            case 2: st32(LINK+0x65A4,0x18030000); break;
            case 3: st32(LINK+0x6594,0x18030000); break;
            case 4: st32(LINK+0x3B8,0x20); break;
            case 5: st16(CURSE,1); break;
            case 6: equip_animation=1; break;
            case 7: guard_animation=1; break;
        }
        hook(LINK_EXECUTE,LINK); assert(spins==before);
        st16(LINK+0x69B0,0x103); st32(LINK+0x8260,0); st32(LINK+0x65A4,0);
        st32(LINK+0x6594,0); st32(LINK+0x3B8,0); st16(CURSE,0);
        equip_animation=guard_animation=0;
        hook(LINK_EXECUTE,LINK); assert(spins==before); /* no delayed replay */
    }
    /* Pending requests cannot survive lifecycle changes or replacement macros. */
    for(int reason=0;reason<8;++reason) {
        touch(3,0,0); touch(0,50,-180); int before=spins;
        switch(reason) {
            case 0: touch(3,0,0); break;
            case 1: for(int i=0;i<9;++i) next(); break; /* request expired */
            case 2: ++epoch; break;
            case 3: st32(LINK+4,ld32(LINK+4)+1); break;
            case 4: touch(0,0,260); touch(2,0,260); break;
            case 5: st8(MENU_FLAG,1); break;
            case 6: step+=3; break;
            case 7: touch(0,-150,-180); break;
        }
        hook(LINK_EXECUTE,LINK); assert(spins==before);
        touch(0,100,185); touch(2,100,185); (void)pad(0);
        st8(MENU_FLAG,0); next();
        hook(LINK_EXECUTE,LINK); assert(spins==before);
    }
    touch(3,0,0);
    /* The removed pad leaves this part of the screen unclaimed and inert. */
    for(int i=0;i<6;++i)
        assert(!(claim[i][0]<=420 && claim[i][2]>=420 && claim[i][1]<=120 && claim[i][3]>=120));
    tap(420,120); next(); tap(420,120); assert(!pad(0));
    touch(4,2,0); touch(5,0,0); assert(!pad(0));
    touch(0,50,-180); assert(pad(0)==0 && ldf32(SCRATCH+4)==0); ++epoch; assert(!pad(0));
    int before=spins; hook(LINK_EXECUTE,LINK); assert(spins==before);
    /* Edit is a separate touch mode: cancel pending spins and swallow held pad
     * input until release. Dragging never executes a macro. */
    st32(LINK+0x65F0,4); touch(3,0,0); touch(0,50,-180); before=spins;
    open_editor(); assert(claim[0][0]==claim[0][2]); assert(pad(0x5000)==0);
    memset(ppc_ptr(SCRATCH),0,32); stf32(SCRATCH+4,.8f); stf32(SCRATCH+12,.6f);
    call(filter_fn,SCRATCH); assert(ldf32(SCRATCH+4)==0 && ldf32(SCRATCH+12)==0);
    hook(LINK_EXECUTE,LINK); assert(spins==before);
    call(hud_fn,1);
    int old_writes=writes, old_reads=reads;
    touch(0,-150,-180); touch(1,-400,-240); touch(2,-400,-240);
    action(EE_LARGER); action(EE_OPACITY); action(EE_SHAPE); action(EE_FILL); action(EE_GRID);
    int jump_releases=releases;action(EE_ICONS);call(hud_fn,1);
    assert(releases==jump_releases+1); /* image switches to its text label */
    tab(3);int released_before=releases;action(EE_ICONS);call(hud_fn,1);assert(releases==released_before+1);
    action(EE_ICONS);tab(0);
    action(EE_VISIBLE); /* hidden still selectable via tabs */
    tab(1); action(EE_SMALLER); tab(0);
    if(getenv("EC_HUD_DUMP")) {
        capture=fopen(getenv("EC_HUD_DUMP"),"w"); assert(capture); call(hud_fn,1); fclose(capture); capture=NULL;
    }
    assert(writes==old_writes && reads==old_reads); /* no IO while dragging/drawing */
    fail_write=1; action(EE_SAVE); assert(claim[5][2]==640 && writes==old_writes+1);
    fail_write=0; action(EE_SAVE); assert(claim[5][0]==claim[5][2] && writes==old_writes+2);
    assert(claim[0][0]==claim[0][2]); /* Jump hidden */
    assert(pad(0x4000)==0);
    memset(ppc_ptr(SCRATCH),0,32); stf32(SCRATCH+4,.05f); call(filter_fn,SCRATCH);
    assert(ldf32(SCRATCH+4)==0); /* small idle drift releases the input latch */
    stf32(SCRATCH+4,.05f); call(filter_fn,SCRATCH); assert(ldf32(SCRATCH+4)==.05f);
    assert(pad(0x1000)==0x1000);
    /* Save file survives a fresh module image; unhide restores exact position,
     * size and hitbox, then attacks work at the new position. */
    reload_module(); assert(claim[0][0]==claim[0][2]);
    open_editor(); action(EE_VISIBLE); action(EE_SAVE); (void)pad(0);
    assert(fabsf(claim[0][0]-(-400-46.2f))<.01f && fabsf(claim[0][1]-(-240-31.5f))<.01f);
    touch(0,-400,-240); assert(pad(0)==0x80); touch(3,0,0);
    open_editor(); touch(0,-400,-240); touch(2,-250,-100); action(EE_CANCEL); (void)pad(0);
    assert(fabsf(claim[0][0]-(-400-46.2f))<.01f); /* cancel discards drag */
    open_editor(); action(EE_RESET); action(EE_CANCEL); (void)pad(0);
    assert(fabsf(claim[0][0]-(-400-46.2f))<.01f); /* reset is also draft-only */
    /* Interrupted saves preserve the other complete generation. */
    open_editor(); touch(0,-400,-240); touch(2,-200,-100); fail_write=1; action(EE_SAVE);
    fail_write=0; reload_module(); assert(fabsf(claim[0][0]-(-400-46.2f))<.01f);
    /* Epoch/menu changes discard the editor draft, never write over disk. */
    open_editor(); action(EE_RESET); old_writes=writes; ++epoch; next();
    assert(claim[5][0]==claim[5][2] && writes==old_writes);
    assert(fabsf(claim[0][0]-(-400-46.2f))<.01f); (void)pad(0);
    open_editor(); action(EE_RESET); st8(MENU_FLAG,1); next(); st8(MENU_FLAG,0); next();
    assert(claim[5][0]==claim[5][2] && writes==old_writes); (void)pad(0);
    /* The only live settings relocate/hide the launcher, including mid-edit. */
    editor_x=200; editor_y=280; next(); assert(claim[4][0]==152 && claim[4][1]==256);
    open_editor(); editor_visible=0; next();
    assert(claim[4][0]==claim[4][2] && claim[5][0]==claim[5][2]);
    editor_visible=1; next(); (void)pad(0); open_editor(); action(EE_RESET); action(EE_SAVE); (void)pad(0);
    assert(claim[0][0]==-194 && claim[1][0]==-94); /* defaults saved */
    /* Master switch releases all claims even while editing, immediately
     * returning controller input and hiding all HUD controls. */
    open_editor();touch_enabled=0;next();
    for(int i=0;i<6;++i)assert(claim[i][0]==claim[i][2]);
    assert(pad(0x1000)==0x1000);int before_hidden=hud_count;call(hud_fn,1);assert(hud_count==before_hidden);
    touch_enabled=1;next();
    /* All four can be hidden; launcher is still reachable and tabs restore them. */
    open_editor(); for(int i=0;i<4;++i) { tab(i); action(EE_VISIBLE); } action(EE_SAVE); (void)pad(0);
    for(int i=0;i<4;++i) assert(claim[i][0]==claim[i][2]);
    assert(claim[4][2]>claim[4][0]); open_editor(); tab(2); action(EE_VISIBLE); action(EE_SAVE); (void)pad(0);
    assert(claim[2][0]==6 && claim[0][0]==claim[0][2]);
    /* Corrupt layouts safely fall back; a bad second file cannot poison the first. */
    memset(files,0,sizeof(files)); file_size[0]=file_size[1]=148; reload_module();
    assert(claim[0][0]==-194 && claim[3][0]==106);
    puts("Translated editor: input isolation, save/restart, cancel, reset, hidden buttons and IO failures passed");
    puts("Translated module: registration, claims, HUD, input, lifecycle and removed-pad regression passed");
    assert(munmap(memory,1ull<<32)==0);
}
