/* Runs through the translated PowerPC hooks, using synthetic FMAT/FTEX data.
 * This verifies actual binding lifetime and descriptor contents, not a game shader. */
enum { TEX_MATS=0x11100000, TEX_RES=0x11110000, TEX_REFS=0x11120000,
       TEX_ORIGINAL=0x11130000, TEX_NEXT=0x11131000, TEX_PIXELS=0x11140000 };
static uint32_t body_state(int m) { return TEX_RES+m*0x200+0x100; }
static uint32_t body_native[MATERIAL_COUNT][12];
static void body_states_restored(void) {
    for (int m=0;m<MATERIAL_COUNT;++m) for (int k=0;k<12;++k)
        assert(ld32(body_state(m)+k*4)==body_native[m][k]);
}
static void body_state_tinted(int m) {
    uint32_t state=body_state(m), blend=ld32(state+0x1c);
    assert((ld32(state)&3)==(body_native[m][0]&3)); /* opaque/cutout bucket */
    assert(ld32(state+8)==body_native[m][2]); /* depth writes/test */
    assert(ld32(state+12)==body_native[m][3] && ld32(state+16)==body_native[m][4]);
    assert((blend&31)==13 && ((blend>>8)&31)==0 && ((blend>>5)&7)==0);
    assert((blend&(1u<<29)) && ((blend>>16)&31)==1 && ((blend>>24)&31)==0 && ((blend>>21)&7)==0);
    assert(ld32(state+0x2c)==body_native[m][11]); /* constant alpha */
    for (int c=0;c<3;++c) {
        float scale=ldf32(state+0x20+4*c);
        assert(scale>0 && scale<=0.013f);
        /* Independently evaluate native CONST_COLOR/ZERO/ADD for dark and
         * bright pixels, against arbitrary backgrounds. Relative contrast
         * survives, while white output becomes near black. */
        float dark=.1f*scale, bright=1.0f*scale;
        assert(dark>0 && bright>dark && bright<.013f);
    }
}
static uint32_t texture_slot(int material) {
    return TEX_REFS+(MATERIAL_COUNT-1-material)*0x20+4; /* _a0 is slot ONE */
}
enum { SHAPE_PACKET=0x11150000, OTHER_PACKET=0x11150100 };
static void shape_material_begin(uint32_t model,int material) {
    st32(SHAPE_PACKET+0x9c,model);st32(SHAPE_PACKET+0x98,SHAPE_PACKET+0x200);
    st32(SHAPE_PACKET+0x200,SHAPE_PACKET+0x220);
    st16(SHAPE_PACKET+0x22e,model==MODEL?MATERIAL_COUNT-1-material:material);
    hook(0x027f16e8,2,SHAPE_PACKET,0);
}
static void shape_begin(uint32_t model) { shape_material_begin(model,0); }
static void shape_end(void) { hook(0x027f16e8,3,SHAPE_PACKET,0); }
static void texture_originals(void) {
    for (int m=0;m<MATERIAL_COUNT;++m) assert(ld32(texture_slot(m))==TEX_ORIGINAL);
    body_states_restored();
}
/* surface: 0 neutral body albedo, 1 red eyes, 2 black aura. */
static void texture_view_check(uint32_t view, uint32_t original, int surface) {
    assert(view!=original && view>=module->mem_base && view+0xa0<=module->mem_base+module->mem_size);
    for (int i=0;i<0xa0;i+=4) if (i!=0x88 && i!=0x94)
        assert(ld32(view+i)==ld32(original+i));
    uint32_t comp=ld32(view+0x88), reg=ld32(view+0x94), source=ld32(original+0x94);
    assert((comp&0xff)==(ld32(original+0x88)&0xff));
    assert((reg&~(0x1ffu<<16))==(source&~(0x1ffu<<16)));
    assert(((reg>>25)&7)==((source>>25)&7)); /* alpha selector */
    /* Independently decode the hardware component selectors (R,G,B,A,0,1).
     * RGB must not depend on the source pixel, including a bright source. */
    float samples[][6]={{.3f,.8f,.2f,.6f,0,1},{1,1,1,0,0,1}};
    for (int p=0;p<2;++p) for (int c=0;c<3;++c) {
        unsigned selector=(reg>>(16+3*c))&7;
        assert(selector<6 && selector==((comp>>(24-8*c))&7));
        float expected=surface==0 || (surface==1 && !c) ? 1.0f : 0.0f;
        assert(samples[p][selector]==expected);
    }
}
static void texture_tinted(int selected) {
    for (int m=0;m<MATERIAL_COUNT;++m) {
        uint32_t slot=texture_slot(m);
        assert(ld32(slot-4)==TEX_ORIGINAL);
        if(m==selected) {
            texture_view_check(ld32(slot),TEX_ORIGINAL,m>=1 && m<=6);
            if(m==0 || m==7) {body_state_tinted(m);continue;}
        } else assert(ld32(slot)==TEX_ORIGINAL); /* no other material rebound */
        for(int k=0;k<12;++k) assert(ld32(body_state(m)+4*k)==body_native[m][k]);
    }
}
static void test_textures(void) {
    uint8_t asset[0xa0], old_view[0xa0];
    for (int i=0;i<0xa0;i+=4) st32(TEX_ORIGINAL+i,0x12340000u+i);
    st32(TEX_ORIGINAL,0x46544558); st32(TEX_ORIGINAL+8,128); st32(TEX_ORIGINAL+12,128);
    st32(TEX_ORIGINAL+0x28,TEX_PIXELS);
    st32(TEX_ORIGINAL+0x88,0x02010000); /* alpha comes from R, not A */
    st32(TEX_ORIGINAL+0x94,0xa1000a55); /* W selector zero */
    memcpy(asset,ppc_ptr(TEX_ORIGINAL),sizeof asset);
    memcpy(ppc_ptr(TEX_NEXT),asset,sizeof asset);
    st32(TEX_NEXT+0x28,TEX_PIXELS+0x1000);
    st32(TEX_NEXT+0x88,0x00010203);
    st32(TEX_NEXT+0x94,0xa6000a55); /* next animation frame: alpha from A */
    st16(MODEL+0x2a,MATERIAL_COUNT); st32(MODEL+0x34,TEX_MATS);
    for (int m=0;m<MATERIAL_COUNT;++m) {
        unsigned index=MATERIAL_COUNT-1-m;
        uint32_t res=TEX_RES+m*0x200, native=TEX_MATS+index*0x3c;
        st32(MATERIALS+m*0x39c,res);
        st32(res,0x464d4154); st16(res+0xc,index); st8(res+0x10,2); st8(res+0x11,2);
        st32(res+0x2c,0x80-0x2c); /* sampler array */
        st32(res+0x20,0x100-0x20);
        uint32_t state=body_state(m);
        st32(state,m==7 ? 2 : 1); st32(state+4,0x1234); st32(state+8,0x37);
        st32(state+12,m==7 ? 1 : 0); stf32(state+16,.5f);
        st32(state+0x14,0x00cc0020); st32(state+0x18,0);
        st32(state+0x1c,0x40000504); /* dormant SRC_ALPHA blend must not affect alpha */
        for(int c=0;c<4;++c) stf32(state+0x20+c*4,.2f+c*.1f);
        for(int k=0;k<12;++k) body_native[m][k]=ld32(state+k*4);
        /* Wii U sampler: three GX2 words, handle (+0x0c), name (+0x10),
         * index (+0x14), padding; verified against BfresLibrary Sampler.Load.
         * Zero handles reproduce the video's ?,? / NO_ALBEDO failure. */
        st32(res+0x80+0xc,0); st32(res+0x98+0xc,0);
        st32(res+0x80+0x10,0xc0-(0x80+0x10));
        st32(res+0x98+0x10,0xc4-(0x98+0x10));
        st8(res+0x80+0x14,0); st8(res+0x98+0x14,1);
        memcpy(ppc_ptr(res+0xc0),"_a1\0_a0",8);
        st32(native,res); st32(native+0x2c,TEX_REFS+index*0x20);
        st32(texture_slot(m)-4,TEX_ORIGINAL); st32(texture_slot(m),TEX_ORIGINAL);
    }
    /* Every shape touches only its selected material, including all eyes. */
    for(int frame=0;frame<3;++frame) for(int m=0;m<MATERIAL_COUNT;++m) {
        shape_material_begin(MODEL,m);texture_tinted(m);shape_end();texture_originals();
    }
    shape_material_begin(MODEL,1);uint32_t eye_view=ld32(texture_slot(1));
    memcpy(old_view,ppc_ptr(eye_view),sizeof old_view);shape_end();
    shape_begin(MODEL);uint32_t body_view=ld32(texture_slot(0));shape_end();
    assert(body_view!=eye_view);
    /* Latest native animation is sampled independently for each shape. */
    for(int m=0;m<2;++m) {
        st32(texture_slot(m),TEX_NEXT);shape_material_begin(MODEL,m);
        texture_view_check(ld32(texture_slot(m)),TEX_NEXT,m);
        shape_end();assert(ld32(texture_slot(m))==TEX_NEXT);st32(texture_slot(m),TEX_ORIGINAL);
    }
    assert(!memcmp(old_view,ppc_ptr(eye_view),sizeof old_view));
    /* An unrelated nested draw sharing FMAT sees native state. */
    shape_begin(MODEL);texture_tinted(0);
    st32(OTHER_PACKET+0x9c,MODEL+0x3000);
    hook(0x027f16e8,2,OTHER_PACKET,0);texture_originals();
    hook(0x027f16e8,3,OTHER_PACKET,0);texture_tinted(0);shape_end();texture_originals();
    /* Nested shapes of this same model switch just their selected binding. */
    shape_begin(MODEL);texture_tinted(0);shape_material_begin(MODEL,1);texture_tinted(1);
    shape_end();texture_tinted(0);shape_end();texture_originals();
    shape_begin(MODEL);shape_end(); /* restore packet selector to body */
    hook(0x027f1fa8,2,SHAPE_PACKET,0);texture_tinted(0);
    hook(0x027f1fa8,3,SHAPE_PACKET,0);texture_originals();
    /* Restore a mod pointer even if the material/refs no longer validate. */
    const unsigned member=MATERIAL_COUNT-1;
    shape_begin(MODEL);st32(TEX_MATS+member*0x3c+0x2c,TEX_REFS+0x1000);shape_end();
    assert(ld32(texture_slot(0))==TEX_ORIGINAL);body_states_restored();
    st32(TEX_MATS+member*0x3c+0x2c,TEX_REFS+member*0x20);
    shape_begin(MODEL);st32(MODEL+0x34,TEX_MATS+0x1000);shape_end();
    assert(ld32(texture_slot(0))==TEX_ORIGINAL);
    st32(MODEL+0x34,TEX_MATS);
    for(int k=0;k<12;++k)st32(body_state(0)+4*k,body_native[0][k]);
    /* Another owner replacing the slot must keep its new pointer. */
    shape_begin(MODEL);st32(texture_slot(0),TEX_NEXT);shape_end();
    assert(ld32(texture_slot(0))==TEX_NEXT);st32(texture_slot(0),TEX_ORIGINAL);
    /* Shared resource identity changes BETWEEN draws never leave old state. */
    st32(LINK+4,43); shape_begin(MODEL); texture_originals(); shape_end();
    st32(LINK+4,42);
    st32(MODEL+0x34,TEX_MATS+0x1000);
    shape_begin(MODEL); texture_originals(); shape_end(); st32(MODEL+0x34,TEX_MATS);
    /* Unsupported states use black rather than exposing a white albedo. */
    const uint32_t unsupported_flags[]={3,0x11,0};
    for(int i=0;i<3;++i) {
        st32(body_state(0),unsupported_flags[i]); shape_begin(MODEL);
        texture_view_check(ld32(texture_slot(0)),TEX_ORIGINAL,3);
        shape_end(); assert(ld32(body_state(0))==unsupported_flags[i]);
        st32(body_state(0),body_native[0][0]);
    }
    /* Missing sampler/resource and unknown textures fail closed. */
    st8(TEX_RES+0xc5,'x'); st32(texture_slot(1),TEX_PIXELS);
    shape_begin(MODEL);
    assert(ld32(texture_slot(0))==TEX_ORIGINAL && ld32(texture_slot(1))==TEX_PIXELS);
    shape_end(); st8(TEX_RES+0xc5,'a'); st32(texture_slot(1),TEX_ORIGINAL);
    /* State captured while transformed has ordinary refs, even if the next
     * process runs without this module: no cleanup hook is needed on load. */
    unsigned char saved_refs[MATERIAL_COUNT*0x20], saved_states[MATERIAL_COUNT*0x200];
    shape_begin(MODEL); texture_tinted(0); shape_end();
    memcpy(saved_refs,ppc_ptr(TEX_REFS),sizeof saved_refs);
    memcpy(saved_states,ppc_ptr(TEX_RES),sizeof saved_states);
    enabled=0; ++revision; tick();
    memcpy(ppc_ptr(TEX_REFS),saved_refs,sizeof saved_refs);
    memcpy(ppc_ptr(TEX_RES),saved_states,sizeof saved_states);
    texture_originals();
    int lookups=player_lookups;
    for(int i=0;i<100;++i) {shape_begin(MODEL);shape_end();}
    assert(player_lookups==lookups);texture_originals();
    enabled=1; free_mode=1; ++revision; tick();
    lookups=player_lookups;shape_begin(MODEL);shape_end();assert(player_lookups==lookups);
    buttons(0);buttons(0x60000);tick();
    shape_begin(MODEL); texture_tinted(0); shape_end(); texture_originals();
    assert(!memcmp(asset,ppc_ptr(TEX_ORIGINAL),sizeof asset));
    for(int m=0;m<MATERIAL_COUNT;++m) st32(MATERIALS+m*0x39c,0);
    st32(MODEL+0x34,0); st16(MODEL+0x2a,0);
    free_mode=0; ++revision; tick(); restored();
    puts("Dark Link draw-scoped texture/blend restoration and snapshot identity passed");
}
