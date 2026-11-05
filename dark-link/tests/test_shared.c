/* Execute TWO independently translated PPC modules at distinct loader bases.
 * Separate touch editors and isolated persistence, in both hook orders.
 * No game assets or instructions. */
#define _GNU_SOURCE
#include "wwhd_guest_abi.h"
#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
static const WWHDGuestModuleV1* mods[2];
static int current, installed[2]={1,1}, order, fail_write;
static uint32_t callbacks[2][3], claims[2];
static float rectangles[2][8][4], dark_x,dark_y;
static uint64_t epoch=1,step=1;
static unsigned char files[2][2][512];
static unsigned sizes[2][2],writes;
static int edits, tabs, pictures, textures[2];
static const uint32_t PLAY=0x1046f0b0, LINK=0x18000000, SAVE=0x18010000, INPUT=0x18020000;
extern void shared_reset(void);
extern uint32_t shared_begin(int,uint64_t);
extern uint32_t shared_texture(int);
extern void shared_release(int,uint32_t);
extern int shared_emit(int,uint32_t,const unsigned char*,const char*);
extern void shared_end(int,uint32_t);
static int own(uint32_t a,uint32_t n) {
    const WWHDGuestModuleV1* m=mods[current];
    return a>=m->mem_base && (uint64_t)a+n<=(uint64_t)m->mem_base+m->mem_size;
}
static const char* str(uint32_t a) { assert(own(a,1)); return (const char*)ppc_ptr(a); }
static void invoke(int owner,uint32_t fn,Cpu* c) {
    int previous=current; current=owner;
    const WWHDGuestModuleV1* m=mods[owner];
    for(uint32_t i=0;i<m->func_count;++i) if(m->funcs[i].addr==fn) {
        m->funcs[i].fn(c);current=previous;return;
    }
    abort();
}
static void run(int owner,uint32_t fn,uint32_t a) {
    Cpu c={0};c.r[1]=0x30020000;c.r[3]=a; invoke(owner,fn,&c);
}
static void hooks(uint32_t target,Cpu* c) {
    Cpu original=*c;
    for(int n=0;n<2;++n) {
        int owner=order?n:1-n;if(!installed[owner]) continue;
        const WWHDGuestModuleV1* m=mods[owner];
        for(uint32_t i=0;i<m->hook_count;++i) if(m->hooks[i].target==target && m->hooks[i].kind==2) {
            Cpu child=original;child.r[1]-=0x2000; invoke(owner,m->hooks[i].func,&child);
        }
    }
}
static void dispatch(Cpu* c) {
    switch(c->pc) {
        case 0x025200d4:c->r[3]=PLAY;return;
        case 0x025986bc:case 0x025af2a4:case 0x023d794c:case 0x02443728:c->r[3]=0;return;
        default:fprintf(stderr,"unexpected native %08x\n",c->pc);abort();
    }
}
static void copy(Cpu* c) { memmove(ppc_ptr(c->r[3]),ppc_ptr(c->r[4]),c->r[5]); }
static void clear(Cpu* c) { memset(ppc_ptr(c->r[3]),c->r[4],c->r[5]); }
static void touch_register(Cpu* c) { assert(own(c->r[3],4));callbacks[current][0]=c->r[3];c->r[3]=1; }
static void filter_register(Cpu* c) { assert(own(c->r[3],4));callbacks[current][1]=c->r[3];c->r[3]=1; }
static void hud_register(Cpu* c) { assert(own(c->r[3],4));callbacks[current][2]=c->r[3];c->r[3]=1; }
static void boolean(Cpu* c) {
    const char* key=str(c->r[3]);
    if(current && !strcmp(key,"touch-enabled")) {c->r[3]=1;return;}
    c->r[3]=!strcmp(key,"show_hud") || !strcmp(key,"appearance_diagnostics")?0:
        !strcmp(key,"free_transform")?1:c->r[4];
}
static void integer(Cpu* c) { c->r[3]=c->r[4]; }
static void floating(Cpu* c) { (void)c; }
static void revision(Cpu* c) { c->r[3]=0;c->r[4]=1; }
static void get_step(Cpu* c) { c->r[3]=step>>32;c->r[4]=(uint32_t)step; }
static void get_epoch(Cpu* c) { c->r[3]=epoch>>32;c->r[4]=(uint32_t)epoch; }
static void dt(Cpu* c) { c->f[1].ps0=1./30; }
static void read_pad(Cpu* c) { assert(own(c->r[3],32));memset(ppc_ptr(c->r[3]),0,32); }
static void claim(Cpu* c) {
    unsigned i=c->r[3];assert(i<8);
    for(int k=0;k<4;++k) rectangles[current][i][k]=(float)c->f[k+1].ps0;
    if(c->f[3].ps0>c->f[1].ps0 && c->f[4].ps0>c->f[2].ps0) claims[current]|=1u<<i;
    else claims[current]&=~(1u<<i);
    c->r[3]=1;
}
static unsigned file_index(uint32_t p) {
    const char* name=str(p); assert(strstr(name,current?"macro-layout-":"touch-layout-"));
    assert(strstr(name,"-a.bin") || strstr(name,"-b.bin"));return strstr(name,"-b.bin")?1:0;
}
static void file_read(Cpu* c) {
    unsigned i=file_index(c->r[3]);assert(own(c->r[4],c->r[5]));
    unsigned n=sizes[current][i];if(!n) {c->r[3]=-1;return;} assert(n<=c->r[5]);
    memcpy(ppc_ptr(c->r[4]),files[current][i],n);c->r[3]=n;
}
static void file_write(Cpu* c) {
    unsigned i=file_index(c->r[3]);assert(own(c->r[4],c->r[5]) && c->r[5]<=512);
    unsigned n=fail_write?8:c->r[5];memcpy(files[current][i],ppc_ptr(c->r[4]),n);
    sizes[current][i]=n;++writes;c->r[3]=n;
}
static void texture(Cpu* c) {
    assert(c->r[3]==0);const char* path=str(c->r[4]);
    assert(current?!strncmp(path,"assets/combat_",14):!strcmp(path,"assets/dark_link.png"));
    ++textures[current];c->r[3]=shared_texture(current);
}
static void release(Cpu* c) { shared_release(current,c->r[3]);c->r[3]=1; }
static void emit(Cpu* c) {
    uint32_t p=c->r[4];assert(own(p,72));
    const char* text="";
    if(ld32(p)==1) {
        assert(own(ld32(p+64),ld32(p+68)));text=(const char*)ppc_ptr(ld32(p+64));
        if((ld32(p+68)==4 && !memcmp(text,"Edit",4)) || (ld32(p+68)==7 && !memcmp(text,"DL Edit",7))) ++edits;
        if(ld32(p+68)==9 && !memcmp(text,"Dark Link",9)) ++tabs;
    }
    if(ld32(p)==2) {
        ++pictures;
        if(!current) { dark_x=ldf32(p+12)+ldf32(p+20)/2-640;dark_y=360-ldf32(p+16)-ldf32(p+24)/2; }
    }
    assert(shared_emit(current,c->r[3],ppc_ptr(p),text));c->r[3]=1;
}
static void log_message(Cpu* c) { (void)str(c->r[3]); }
static void no_setting(Cpu* c) { c->r[3]=0; }
static PpcFunc service(const char* name) {
    struct {const char* name;PpcFunc fn;} all[]={
        {"memcpy",copy},{"memset",clear},{"wwhd_touch_register",touch_register},
        {"wwhd_input_filter_register",filter_register},{"wwhd_hud_register",hud_register},
        {"wwhd_config_bool",boolean},{"wwhd_config_int",integer},{"wwhd_config_float",floating},
        {"wwhd_config_changed",revision},{"wwhd_logic_step",get_step},{"wwhd_hud_epoch",get_epoch},
        {"wwhd_logic_dt",dt},{"wwhd_input_read",read_pad},{"wwhd_touch_claim",claim},
        {"wwhd_file_read",file_read},{"wwhd_file_write",file_write},{"wwhd_hud_texture",texture},
        {"wwhd_hud_release",release},{"wwhd_hud_emit",emit},{"wwhd_log",log_message},{"wwhd_setting_get",no_setting}};
    for(unsigned i=0;i<sizeof(all)/sizeof(all[0]);++i) if(!strcmp(name,all[i].name)) return all[i].fn;
    fprintf(stderr,"unexpected service %s\n",name);abort();
}
static void tick(void) { ++step;Cpu c={0};c.r[1]=0x30020000;c.r[3]=LINK;hooks(0x0240ebb0,&c); }
static void touch(int owner,unsigned kind,float x,float y) {
    st32(INPUT,kind);stf32(INPUT+4,x);stf32(INPUT+8,y);run(owner,callbacks[owner][0],INPUT);
}
static void tap(int owner,float x,float y) { touch(owner,0,x,y);touch(owner,2,x,y); }
static void draw(void) {
    edits=tabs=pictures=0;
    for(int n=0;n<2;++n) { int i=order?n:1-n;if(!installed[i]) continue;
        uint32_t list=shared_begin(i,step);run(i,callbacks[i][2],list);shared_end(i,list); }
}
static void pad(int owner,unsigned buttons) {
    memset(ppc_ptr(INPUT),0,32);st32(INPUT,buttons);run(owner,callbacks[owner][1],INPUT);
}
static void reload(void) {
    shared_reset();++epoch;memset(claims,0,sizeof(claims));memset(callbacks,0,sizeof(callbacks));
    for(int i=0;i<2;++i) {memset(ppc_ptr(mods[i]->mem_base),0,mods[i]->mem_size);memcpy(ppc_ptr(mods[i]->mem_base),mods[i]->image,mods[i]->image_size);}
    tick();tick();
}
static void action(int owner,int a) { tap(owner,a<7?-480+160*a:-300+200*(a-7),a<7?245:185); }
static void drag(int owner,float x,float y,float xx,float yy) {touch(owner,0,x,y);touch(owner,1,xx,yy);touch(owner,2,xx,yy);}
int main(int argc,char** argv) {
    assert(argc==3);
    assert(mmap(PPC_MEM_BASE,1ull<<32,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_NORESERVE|MAP_FIXED_NOREPLACE,-1,0)==PPC_MEM_BASE);
    volatile int preempt[3]={0};WWHDGuestHostV1 host={0};host.size=sizeof(host);host.abi_version=WWHD_GUEST_ABI_VERSION;
    host.core_preempt=preempt;host.service=service;host.dispatch=dispatch;
    for(int i=0;i<2;++i) {void* lib=dlopen(argv[1+i],RTLD_NOW);if(!lib){fprintf(stderr,"%s\n",dlerror());abort();}
        WWHDGuestInitV1 init=(WWHDGuestInitV1)dlsym(lib,WWHD_GUEST_INIT_SYMBOL);assert(init);mods[i]=init(&host);assert(mods[i]);}
    assert(mods[0]->mem_base+mods[0]->mem_size<=mods[1]->mem_base);
    st32(PLAY+0x5b2c,LINK);memcpy(ppc_ptr(PLAY+0x5134),"sea",4);st32(LINK,1);st32(LINK+4,7);
    st32(LINK+0x65f0,4);st16(LINK+0x69b0,0x103);st32(0x101f84dc,SAVE);st16(SAVE+0x20,80);st16(SAVE+0x22,80);
    for(int i=0;i<2;++i) for(unsigned h=0;h<mods[i]->hook_count;++h)
        assert(mods[i]->hooks[h].target!=0x0267598c);
    for(order=0;order<2;++order) {
        memset(sizes,0,sizeof(sizes));reload();draw();assert(edits==2 && pictures==2);
        assert(dark_x==348 && dark_y==-222 && claims[0]==3 && claims[1]==31);
        tap(1,0,260);draw();assert(tabs==0 && edits==1 && claims[0]==3 && claims[1]==32);
        pad(1,0x4000);assert(ld32(INPUT)==0);
        drag(1,-150,-180,-400,-200);action(1,9);draw();assert(dark_x==348 && dark_y==-222);
        assert(!sizes[0][0] && !sizes[0][1]);
        tap(0,120,260);draw();assert(tabs==1 && edits==1 && claims[0]==4 && claims[1]==31);
        pad(0,0x60000);assert(ld32(INPUT)==0);
        drag(0,348,-222,450,-100);action(0,10);draw();
        assert(dark_x==450 && dark_y==-100 && edits==2);reload();draw();assert(dark_x==450 && dark_y==-100);
        assert(!sizes[1][0] && !sizes[1][1]);
        /* Short writes keep only this editor open; cancel restores its save. */
        tap(0,120,260);drag(0,450,-100,300,-80);fail_write=1;action(0,10);draw();
        assert(edits==1 && tabs==1 && dark_x==300);fail_write=0;action(0,9);draw();assert(dark_x==450);
        reload();draw();assert(dark_x==450);
        tap(0,120,260);action(0,6);action(0,10);draw();assert(pictures==1 && claims[0]==2);
        tap(0,120,260);draw();assert(pictures==2 && tabs==1);action(0,6);action(0,10);draw();assert(pictures==2);
        installed[1]=0;reload();draw();assert(edits==1 && pictures==1);
        tap(0,120,260);drag(0,450,-100,250,-80);action(0,10);reload();draw();assert(dark_x==250 && dark_y==-80);
        installed[0]=0;installed[1]=1;reload();draw();assert(edits==1 && pictures==1);
        tap(1,0,260);draw();assert(tabs==0);action(1,9);
        installed[0]=1;
    }
    assert(writes && textures[0] && textures[1]);puts("Separate editors: both load orders, isolated saves, reload/cancel, short writes, hidden controls and standalone passed");
}
