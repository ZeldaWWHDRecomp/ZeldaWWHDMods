/* Execute the translated guest module against synthetic memory and host services.
 * Linux-only integration driver; no game code/data or renderer is involved. */
#include "wwhd_guest_abi.h"
#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

enum { PLAY=0x11000000, LINK=0x11010000, SAVE=0x11020000, ENEMY=0x11030000,
       HIT=0x11040000, INPUT=0x11050000, MODEL=0x11060000, DATA=0x11070000,
       MATERIALS=0x11080000, EYE_ANIM=0x11090000, RESOURCE=0x110a0000,
       INSTANCES=0x110f0000, MATERIAL_COUNT=8 };
static const WWHDGuestModuleV1* module;
static uint32_t filter, hud, touch, step=1, revision=1;
static int diagnostics=1,show_gauge=1, player_lookups;
static double test_dt=1.0/30;
static char hud_text[8192];
static FILE* hud_trace;
static unsigned touch_claim_mask;
static int free_mode, enabled=1, paused, menu, touch_visible=1, claim_active, elements;
static const char* eye_names[]={"eyeL","eyeR","eyeLdamA","eyeRdamA","eyeLdamB","eyeRdamB"};
static int eye_indices[6]={1,2,3,4,5,6}, name_lookups, missing_color;
static uint32_t missing_slots;
static void shortcut_lookup(Cpu*);
static void magic_write(Cpu*);
static void magic_read(Cpu*);
static void magic_setting(Cpu*);
static void magic_texture(Cpu*);
static void magic_epoch(Cpu*);
static void magic_release(Cpu*);
static void magic_emit(uint32_t);
static uint32_t magic_archive;
static int aura_notifications;

static void execute(uint32_t fn, uint32_t a, uint32_t b) {
    Cpu c = {0}; c.r[1] = 0x1201fff0; c.r[3] = a; c.r[4] = b;
    for (uint32_t i=0; i<module->func_count; ++i) if (module->funcs[i].addr == fn) {
        module->funcs[i].fn(&c); return;
    }
    assert(!"missing callback");
}
static void hook(uint32_t address, uint32_t kind, uint32_t a, uint32_t b) {
    for (uint32_t i=0; i<module->hook_count; ++i) {
        const WWHDGuestHook* h = &module->hooks[i];
        if (h->target == address && h->kind == kind) { execute(h->func, a, b); return; }
    }
    assert(!"missing hook");
}
static const char* arg_string(Cpu* c) { return (const char*)ppc_ptr(c->r[3]); }
static void config_bool(Cpu* c) {
    const char* key=arg_string(c);
    c->r[3] = !strcmp(key,"free_transform") ? free_mode : !strcmp(key,"enabled") ? enabled :
        !strcmp(key,"show_hud") ? show_gauge : !strcmp(key,"touch_button") ? touch_visible : !strcmp(key,"appearance_diagnostics") ? diagnostics : c->r[4];
}
static void config_int(Cpu* c) { c->r[3]=c->r[4]; }
static void config_revision(Cpu* c) { c->r[3]=0; c->r[4]=revision; }
static void logic_step(Cpu* c) { c->r[3]=0; c->r[4]=step; }
static void logic_dt(Cpu* c) { c->f[1].ps0=test_dt; }
static void reg_filter(Cpu* c) { filter=c->r[3]; c->r[3]=1; }
static void reg_hud(Cpu* c) { hud=c->r[3]; c->r[3]=1; }
static void reg_touch(Cpu* c) { touch=c->r[3]; c->r[3]=1; }
static void claim(Cpu* c) { unsigned bit=1u<<c->r[3]; if(c->f[3].ps0>c->f[1].ps0 && c->f[4].ps0>c->f[2].ps0) touch_claim_mask|=bit; else touch_claim_mask&=~bit; claim_active=!!touch_claim_mask; c->r[3]=1; }
static void emit(Cpu* c) {
    ++elements;
    /* Guest HUD ABI: text pointer at 64, byte count at 68 (72-byte element). */
    uint32_t e=c->r[4];
    /* svc_hud_record validates ownership BEFORE copying text into Store.
     * Geometry-only replay cannot catch a guest-stack text pointer. All this
     * mod's HUD buffers are static, so they must fit its code/data/BSS span. */
    assert(e>=module->mem_base && (uint64_t)e+72<= (uint64_t)module->mem_base+module->mem_size);
    if(ld32(e)==1) {
        uint32_t text=ld32(e+64),bytes=ld32(e+68);
        if(text<module->mem_base || (uint64_t)text+bytes>(uint64_t)module->mem_base+module->mem_size) {
            fprintf(stderr,"HUD text buffer outside mod memory: %08x (%u bytes)\n",text,bytes);
            abort();
        }
    }
    assert(fputc('C',hud_trace)!=EOF);
    assert(fwrite(ppc_ptr(e),1,72,hud_trace)==72);
    if(ld32(e)==1) assert(fwrite(ppc_ptr(ld32(e+64)),1,ld32(e+68),hud_trace)==ld32(e+68));
    if (ld32(e)==2) magic_emit(e);
    if (ld32(e)==1) {
        uint32_t p=ld32(e+64), bytes=ld32(e+68);
        size_t used=strlen(hud_text);
        assert(bytes < 192 && used+bytes+2 < sizeof hud_text);
        memcpy(hud_text+used,ppc_ptr(p),bytes); hud_text[used+bytes]='\n'; hud_text[used+bytes+1]=0;
    }
    c->r[3]=1;
}
static void read_hud(void) {
    assert(fputc('B',hud_trace)!=EOF);
    hud_text[0]=0; execute(hud,1,0);
    assert(fputc('E',hud_trace)!=EOF);
}
static void expect_hud(const char* expected) {
    read_hud();
    if (!strstr(hud_text,expected)) { fprintf(stderr,"HUD missing %s in:\n%s",expected,hud_text); abort(); }
}
static void copy(Cpu* c) { memcpy(ppc_ptr(c->r[3]),ppc_ptr(c->r[4]),c->r[5]); }
static void clear(Cpu* c) { memset(ppc_ptr(c->r[3]),c->r[4],c->r[5]); }
static PpcFunc service(const char* name) {
    if (!strcmp(name,"wwhd_config_bool")) return config_bool;
    if (!strcmp(name,"wwhd_config_int")) return config_int;
    if (!strcmp(name,"wwhd_config_changed")) return config_revision;
    if (!strcmp(name,"wwhd_logic_step")) return logic_step;
    if (!strcmp(name,"wwhd_logic_dt")) return logic_dt;
    if (!strcmp(name,"wwhd_input_filter_register")) return reg_filter;
    if (!strcmp(name,"wwhd_hud_register")) return reg_hud;
    if (!strcmp(name,"wwhd_touch_register")) return reg_touch;
    if (!strcmp(name,"wwhd_touch_claim")) return claim;
    if (!strcmp(name,"wwhd_hud_emit")) return emit;
    if (!strcmp(name,"wwhd_file_read")) return magic_read;
    if (!strcmp(name,"wwhd_file_write")) return magic_write;
    if (!strcmp(name,"wwhd_setting_get")) return magic_setting;
    if (!strcmp(name,"wwhd_hud_texture")) return magic_texture;
    if (!strcmp(name,"wwhd_hud_epoch")) return magic_epoch;
    if (!strcmp(name,"wwhd_hud_release")) return magic_release;
    if (!strcmp(name,"memcpy")) return copy;
    if (!strcmp(name,"memset")) return clear;
    fprintf(stderr,"Unexpected service: %s\n",name); return 0;
}
static void dispatch(Cpu* c) {
    switch (c->pc) {
        case 0x0200000c: shortcut_lookup(c); return; /* synthetic pane vtable slot */
        case 0x026fbb24: c->r[3]=magic_archive; return;
        case 0x027df40c: ++aura_notifications; return;
        case 0x02000008: c->r[3]=30; return; /* BRK resource getFrameMax */
        case 0x02000004: return; /* synthetic frame interpolation: identity in f1 */
        case 0x025200d4: ++player_lookups; c->r[3]=PLAY; return;
        case 0x025986bc: c->r[3]=menu; return;
        case 0x025af2a4: c->r[3]=paused; return;
        case 0x027f9f0c:
            assert(c->r[4]>=4 && c->r[4]<=10); /* never request material/ambient lighting */
            c->r[3] = (missing_color || (missing_slots & (1u<<c->r[4]))) ? 0 : c->r[3]+4+c->r[4]*16; return;
        case 0x027df9b0:
            assert(c->r[3]==RESOURCE+0x100); ++name_lookups;
            for (int i=0;i<6;++i) if (!strcmp((const char*)ppc_ptr(c->r[4]),eye_names[i])) {
                c->r[3]=eye_indices[i]; return;
            }
            assert(!"unexpected material name (eyebrows must stay black)"); return;
        default: fprintf(stderr,"Unexpected native call: %08x\n",c->pc); abort();
    }
}
static void tick(void) { ++step; hook(0x0240ebb0,2,LINK,0); }
static void buttons(uint32_t value) { st32(INPUT,value); execute(filter,INPUT,0); }
static unsigned damage(unsigned power) {
    st8(HIT+8,power); hook(0x02518db0,3,HIT,0); return ld8(HIT+8);
}
static void hit(int lose_hp) {
    st8(ENEMY+0x3a1,10); st8(HIT+8,1);
    hook(0x025192a8,2,ENEMY,HIT);
    if (lose_hp) st8(ENEMY+0x3a1,9);
    hook(0x025192a8,3,ENEMY,HIT);
}
static uint32_t color(int material, int index) { return MATERIALS+material*0x39c+0xa4+index*16; }
static uint8_t original_materials[MATERIAL_COUNT*0x39c], original_instances[MATERIAL_COUNT*0x38];
static void restored(void) {
    assert(!memcmp(ppc_ptr(MATERIALS),original_materials,sizeof original_materials));
    assert(!memcmp(ppc_ptr(INSTANCES),original_instances,sizeof original_instances));
}
static float body_tint(int index, int channel) {
    const float shadow[]={0.006f,0.009f,0.014f};
    const float diffuse[]={0.035f,0.040f,0.050f};
    const float base[]={0.018f,0.021f,0.026f};
    return index==4 ? shadow[channel] : index==7 ? diffuse[channel] : base[channel];
}
static void tinted(void) {
    for (int m=0;m<MATERIAL_COUNT;++m) {
        uint32_t expected=0xa00007fbu;
        int eye=m>=1 && m<=6;
        assert(ld32(MATERIALS+m*0x39c+0xa0)==expected);
        for (int k=0;k<11;++k) {
            int changed=k>=4;
            assert((float)ldf32(color(m,k))==(changed ? (eye ? 1.0f : body_tint(k,0)) : 0.25f+k*0.01f));
            assert((float)ldf32(color(m,k)+4)==(changed ? (eye ? 0.0f : body_tint(k,1)) : 0.5f));
            assert((float)ldf32(color(m,k)+8)==(changed ? (eye ? 0.0f : body_tint(k,2)) : 0.75f));
            assert((float)ldf32(color(m,k)+12)==0.4f+m*0.1f);
        }
        if(!eye) for(int c=0;c<3;++c) {
            /* The two native lighting inputs must not collapse to the same
             * black. This is an input contract, not a game-shader simulation. */
            assert(ldf32(color(m,4)+c*4)>0 && ldf32(color(m,4)+c*4)<ldf32(color(m,7)+c*4));
            assert(ldf32(color(m,7)+c*4)<=0.05f);
        }
    }
}
/* Model the native mDoExt sequence: preparation completes first, followed by
 * viewCalc/submission inside the same wrapper. The color must still be live at
 * that later boundary. The 0.1.4 inner-entry-only scope cannot satisfy this. */
static void submit(uint32_t wrapper, int active) {
    hook(wrapper,2,MODEL,0);
    if (active) tinted(); else restored();
    /* Native modelEntry/modelUpdate has returned here; it is not a restore point. */
    hook(0x027f55fc,2,MODEL,0);
    if (active) tinted(); else restored();
    hook(0x027f583c,2,MODEL,DATA);
    if (active) tinted(); else restored();
    hook(0x027f583c,3,MODEL,DATA);
    hook(0x027f55fc,3,MODEL,0);
    if (active) tinted(); else restored();
    hook(wrapper,3,MODEL,0); restored();
}
static void prepare(int active) { submit(0x025e2e5c,active); }
#include "test_textures.h"
#include "test_magic.h"
#include "test_shortcut.h"
int main(int argc, char** argv) {
    assert(argc==3);
    hud_trace=fopen(argv[2],"wb");assert(hud_trace);
    void* memory=mmap(PPC_MEM_BASE,0x100000000ull,PROT_READ|PROT_WRITE,
        MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE|MAP_NORESERVE,-1,0);
    assert(memory==PPC_MEM_BASE);
    void* library=dlopen(argv[1],RTLD_NOW);
    if (!library) { fprintf(stderr,"%s\n",dlerror()); return 1; }
    WWHDGuestInitV1 init=(WWHDGuestInitV1)dlsym(library,WWHD_GUEST_INIT_SYMBOL); assert(init);
    volatile int preempt[3]={0};
    WWHDGuestHostV1 host={0}; host.size=sizeof host; host.abi_version=WWHD_GUEST_ABI_VERSION;
    host.core_preempt=preempt; host.service=service; host.dispatch=dispatch;
    module=init(&host); assert(module);
    memcpy(ppc_ptr(module->mem_base),module->image,module->image_size);
    for (unsigned i=0; i<module->hook_count; ++i) {
        assert(module->hooks[i].kind!=WWHD_GUEST_REPLACE);
        assert(module->hooks[i].target!=0x027f5018 && module->hooks[i].target!=0x027f4f1c && module->hooks[i].target!=0x027f4fe4);
    }
    st32(PLAY+0x5b2c,LINK); st32(LINK+4,42); st32(0x101f84dc,SAVE);
    st16(SAVE+0x20,80); st16(SAVE+0x22,80); st8(ENEMY+0x2da,2);
    st32(HIT,LINK+0x7b1c); st32(HIT+4,LINK); st8(HIT+10,1); st32(LINK+0x7b1c+0x10,2);
    tick(); assert(filter && hud && touch);
    /* Duration follows maximum magic even with the gauge hidden; neither
     * current magic nor the upgrade increases the twenty-hit requirement. */
    show_gauge=0;
    const unsigned magic_caps[]={0,16,32};
    for(unsigned m=0;m<3;++m) {
        enabled=0;++revision;tick();enabled=1;++revision;
        st8(SAVE+0x33,magic_caps[m]);st8(SAVE+0x34,0);tick();buttons(0);
        for(int i=0;i<19;++i)hit(1);
        buttons(0x60000);tick();assert(damage(3)==3);buttons(0);
        hit(1);buttons(0x60000);tick();assert(damage(3)==6);buttons(0);
        int seconds=m==2?40:20;
        for(int i=0;i<seconds*30-2;++i)tick();
        assert(damage(3)==6);
        for(int i=0;i<4;++i)tick();
        assert(damage(3)==3);
    }
    /* Cancel at exactly half duration for both sizes, then refill with ten
     * hits rather than twenty. Exercise the real input/execute/hit hooks. */
    test_dt=0.25;
    for(unsigned m=0;m<2;++m) {
        enabled=0;++revision;tick();enabled=1;++revision;
        st8(SAVE+0x33,m?32:16);tick();buttons(0);
        for(int i=0;i<20;++i)hit(1);
        buttons(0x60000);tick();assert(damage(3)==6);buttons(0);
        for(int i=0;i<(m?80:40);++i)tick();
        buttons(0x60000);tick();assert(damage(3)==3);buttons(0);
        for(int i=0;i<40;++i)tick(); /* retained charge stays put */
        for(int i=0;i<9;++i)hit(1);
        buttons(0x60000);tick();assert(damage(3)==3);buttons(0);
        hit(1);buttons(0x60000);tick();assert(damage(3)==6);buttons(0);
    }
    test_dt=1.0/30;enabled=0;++revision;tick();enabled=1;++revision;
    st8(SAVE+0x33,0);show_gauge=1;++revision;tick();buttons(0);

    buttons(0); assert(claim_active);
    elements=0; read_hud(); assert(elements>=5); expect_hud("Skip=NO_DRAW");
    for (int i=0; i<20; ++i) hit(0);
    buttons(0x60000); tick(); assert(damage(3)==3); /* blocked hits give no charge */
    buttons(0);
    for (int i=0; i<19; ++i) hit(1);
    buttons(0x60000); tick(); assert(damage(3)==3); /* need twenty */
    buttons(0); hit(1); buttons(0x60000); tick(); assert(damage(3)==6);
    buttons(0x60000); tick(); assert(damage(3)==6); /* held combo doesn't toggle again */
    assert(ld32(INPUT)==0); assert(damage(3)==6); /* no cumulative scaling */
    st32(HIT,LINK+0x200); assert(damage(3)==3); st32(HIT,LINK+0x7b1c);
    st32(HIT+4,ENEMY); assert(damage(3)==3); st32(HIT+4,LINK);
    assert(damage(100)==127); assert(damage(0)==0);
    st8(LINK+0x3ac,8); st8(LINK+0x69e8,1); assert(damage(100)==63);
    st8(LINK+0x3ac,0); st8(LINK+0x69e8,0);

    st32(LINK+0x448,MODEL);
    st32(MODEL+0xac,DATA); st32(DATA+0x0c,MATERIAL_COUNT); st32(DATA+0x10,MATERIALS);
    st32(DATA,RESOURCE); st32(RESOURCE+0x18,0x100-0x18);
    /* No eye controller pointers: names must identify all six eyes; material 7 represents an eyebrow. */
    for (int m=0; m<MATERIAL_COUNT; ++m) for (int k=0; k<11; ++k) {
        stf32(color(m,k),0.25f+k*0.01f); stf32(color(m,k)+4,0.5f);
        stf32(color(m,k)+8,0.75f); stf32(color(m,k)+12,0.4f+m*0.1f);
    }
    st32(MODEL+0x138,MATERIAL_COUNT); st32(MODEL+0x13c,INSTANCES);
    for (int m=0;m<MATERIAL_COUNT;++m) {
        /* absent/partly present/fully present tint mask; unrelated bits survive */
        st32(MATERIALS+m*0x39c+0xa0,0xa000000bu+(m==1 ? 0x90 : m==2 ? 0x7f0 : 0));
        for (int k=0;k<14;++k) st32(INSTANCES+m*0x38+k*4,0x12340000u+m*100+k);
    }
    memcpy(original_materials,ppc_ptr(MATERIALS),sizeof original_materials);
    memcpy(original_instances,ppc_ptr(INSTANCES),sizeof original_instances);
    prepare(1); assert(name_lookups==18);
    submit(0x025e2de0,1);
    /* Standalone view/joint calls also need their own bounded scopes. */
    hook(0x027f55fc,2,MODEL,0); tinted();
    hook(0x027f55fc,3,MODEL,0); restored();
    hook(0x027f583c,2,MODEL,DATA); tinted();
    hook(0x027f583c,3,MODEL,DATA); restored();
    /* Native changes between preparation calls require reapplying the tint,
     * without replacing the saved baseline with the already-tinted value. */
    hook(0x025e2e5c,2,MODEL,0);
    stf32(color(0,4),0.75f);
    hook(0x027f55fc,2,MODEL,0); tinted();
    hook(0x027f55fc,3,MODEL,0); tinted();
    hook(0x025e2e5c,3,MODEL,0); restored();
    /* Restore our seven mask bits, but retain unrelated native mask updates. */
    hook(0x025e2e5c,2,MODEL,0);
    st32(MATERIALS+0xa0,ld32(MATERIALS+0xa0)|0x4000);
    hook(0x025e2e5c,3,MODEL,0);
    assert(ld32(MATERIALS+0xa0)==0xa000400bu);
    st32(MATERIALS+0xa0,0xa000000bu); restored();
    hook(0x025e2e5c,2,MODEL+0x400,0); restored();
    hook(0x025e2e5c,3,MODEL+0x400,0); restored();
    for (int i=0;i<20;++i) hook(0x025e2e5c,2,MODEL,0);
    for (int i=0;i<20;++i) hook(0x025e2e5c,3,MODEL,0);
    restored();
    /* An absent individual TEV parameter must not activate its override bit. */
    missing_slots=1u<<5;
    hook(0x025e2e5c,2,MODEL,0);
    assert(ld32(MATERIALS+0xa0)==(0xa00007fbu & ~(1u<<5)));
    assert((float)ldf32(color(0,5))==0.30f);
    assert((float)ldf32(color(0,6))==body_tint(6,0));
    hook(0x025e2e5c,3,MODEL,0); restored(); missing_slots=0;
    missing_color=1; prepare(0); missing_color=0;
    st32(DATA+0x0c,97); prepare(0); st32(DATA+0x0c,MATERIAL_COUNT);
    st32(LINK+0x3bc,0x800); prepare(0); st32(LINK+0x3bc,0);
    /* Missing/out-of-range names must never make a body material red. */
    for (int i=0;i<6;++i) eye_indices[i]=i%2 ? MATERIAL_COUNT : -1;
    hook(0x025e2e5c,2,MODEL,0);
    assert((float)ldf32(color(1,4))==body_tint(4,0) && (float)ldf32(color(2,4))==body_tint(4,0));
    hook(0x025e2e5c,3,MODEL,0); restored();
    st32(LINK+0x604,EYE_ANIM); st32(MATERIALS+0x39c+0x24,EYE_ANIM);
    hook(0x025e2e5c,2,MODEL,0); assert((float)ldf32(color(1,4))==1.0f);
    hook(0x025e2e5c,3,MODEL,0);
    st32(LINK+0x604,0); st32(MATERIALS+0x39c+0x24,0); restored();
    for (int i=0;i<6;++i) eye_indices[i]=i+1;
    /* Hands and hair use the matte palette even when sharing this synthetic
     * material table; eye names apply only to the current player's body. */
    const uint32_t gear_fields[]={0xcf4,0x44c,0xcf8,0xe80,0xe7c,0x4440};
    for (int part=0;part<6;++part) {
        uint32_t model=MODEL+0x400+part*0x400;
        uint32_t field=LINK+gear_fields[part];
        if (part==5) st16(LINK+0x69b0,0x103);
        st32(field,model); st32(model+0xac,DATA);
        int previous_lookups=name_lookups;
        hook(0x025e2e5c,2,model,0);
        assert(name_lookups==previous_lookups);
        for (int m=0;m<MATERIAL_COUNT;++m) for (int k=4;k<=10;++k)
            for (int c=0;c<3;++c) assert((float)ldf32(color(m,k)+c*4)==body_tint(k,c));
        hook(0x025e2e5c,3,model,0); restored(); st32(field,0); st16(LINK+0x69b0,0);
    }
    /* Aura gets neither charcoal colour nor eye red, even when the material
     * table is shared in this fixture. Its native alpha remains untouched. */
    st32(LINK+0x5700,MODEL+0x2000); st32(MODEL+0x2000+0xac,DATA);
    hook(0x025e2e5c,2,MODEL+0x2000,0);
    for(int m=0;m<MATERIAL_COUNT;++m) for(int k=4;k<=10;++k) {
        for(int c=0;c<3;++c) assert(ldf32(color(m,k)+c*4)==0);
        assert((float)ldf32(color(m,k)+12)==0.4f+m*0.1f);
    }
    hook(0x025e2e5c,3,MODEL+0x2000,0); restored(); st32(LINK+0x5700,0);
    test_textures();
    test_magic();
    test_shortcut();
    test_aura();
    /* Cancellation must leave the next preparation untouched. */
    buttons(0); buttons(0x60000); tick(); assert(damage(3)==3); prepare(0);
    buttons(0); for (int i=0;i<20;++i) hit(1);
    buttons(0x60000); tick(); assert(damage(3)==6); prepare(1);

    paused=1; buttons(0); assert(!claim_active); elements=0; execute(hud,1,0); assert(!elements);
    for (int i=0;i<900;++i) tick();
    paused=0; tick(); assert(damage(3)==6); /* timer suspended */
    st8(PLAY+0x5292,1); buttons(0); assert(!claim_active);
    for (int i=0;i<900;++i) tick();
    st8(PLAY+0x5292,0); tick(); assert(damage(3)==6);
    st8(PLAY+0x514c,1); tick(); assert(damage(3)==3); prepare(0); /* stage transition resets */
    st8(PLAY+0x514c,0); free_mode=1; ++revision; tick();
    buttons(0); buttons(0x60000); tick(); assert(damage(3)==6);
    prepare(1);
    buttons(0); buttons(0x60000); tick(); assert(damage(3)==3); prepare(0);
    buttons(0); buttons(0x60000); tick(); assert(damage(3)==6); prepare(1);
    /* DARK touch cancellation, move/up do not toggle, second tap activates. */
    stf32(INPUT+0x104,348);stf32(INPUT+0x108,-222);
    st32(INPUT+0x100,0); execute(touch,INPUT+0x100,0); tick();
    assert(damage(3)==3); prepare(0);
    st32(INPUT+0x100,1); execute(touch,INPUT+0x100,0); tick();
    st32(INPUT+0x100,2); execute(touch,INPUT+0x100,0); tick();
    assert(damage(3)==3); prepare(0);
    stf32(INPUT+0x104,348);stf32(INPUT+0x108,-222);
    st32(INPUT+0x100,0); execute(touch,INPUT+0x100,0); tick();
    assert(damage(3)==6); prepare(1);
    touch_visible=0; ++revision; tick(); buttons(0); assert(!claim_active);
    st16(SAVE+0x22,0); tick(); assert(damage(3)==3); prepare(0);
    st16(SAVE+0x22,80); tick(); buttons(0x60000); tick(); assert(damage(3)==6);
    hook(0x025e2e5c,2,MODEL,0);
    enabled=0; ++revision; tick(); assert(damage(3)==3);
    /* Restore even when enabled changes between entry and return. */
    hook(0x025e2e5c,3,MODEL,0); restored(); prepare(0);
    enabled=1; ++revision; tick(); buttons(0); buttons(0x60000); tick(); assert(damage(3)==6);
    hook(0x0240ee28,2,LINK,0); assert(damage(3)==3 && !claim_active); prepare(0);
    puts("Dark Link translated guest-hook tests passed");
    assert(fclose(hud_trace)==0);
    dlclose(library); munmap(memory,0x100000000ull);
}
