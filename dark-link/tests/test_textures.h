/* Runs through the translated PowerPC hooks, using synthetic FMAT/FTEX data.
 * This verifies actual binding lifetime and descriptor contents, not a game shader. */
enum { TEX_MATS=0x11100000, TEX_RES=0x11110000, TEX_REFS=0x11120000,
       TEX_ORIGINAL=0x11130000, TEX_NEXT=0x11131000, TEX_PIXELS=0x11140000 };
static uint32_t texture_slot(int material) {
    return TEX_REFS+(MATERIAL_COUNT-1-material)*0x20+4; /* _a0 is slot ONE */
}
static void texture_refresh(void) {
    hook(0x027f50f8,2,MODEL,0);
    hook(0x027f50f8,3,MODEL,0);
}
static void texture_originals(void) {
    for (int m=0;m<MATERIAL_COUNT;++m) assert(ld32(texture_slot(m))==TEX_ORIGINAL);
}
static void texture_view_check(uint32_t view, uint32_t original, int eye) {
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
        assert(samples[p][selector]==(eye && !c ? 1.0f : 0.0f));
    }
}
static void texture_tinted(void) {
    for (int m=0;m<MATERIAL_COUNT;++m) {
        uint32_t slot=texture_slot(m);
        assert(ld32(slot-4)==TEX_ORIGINAL); /* toon sampler stays original */
        texture_view_check(ld32(slot),TEX_ORIGINAL,m>=1 && m<=6);
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
    texture_refresh(); texture_tinted();
    expect_hud("DL 0.1.36 ON");
    expect_hud("Body J/N=8/8  Albedo=8  Views=8  Eyes found/bound=6/6");
    expect_hud("Samplers body: _a1,_a0  eyes: _a1,_a0");
    /* A handle resembling a valid _a0 relative pointer must never select the
     * toon slot or affect diagnostic names. Only the actual +0x10 name counts. */
    hook(0x027f50f8,2,MODEL,0);
    st32(TEX_RES+0x80+0xc,0xc4-(0x80+0xc));
    st32(TEX_RES+0x98+0xc,0xc0-(0x98+0xc));
    hook(0x027f50f8,3,MODEL,0); texture_tinted();
    expect_hud("Samplers body: _a1,_a0  eyes: _a1,_a0");
    hook(0x027f50f8,2,MODEL,0);
    st32(TEX_RES+0x98+0x10,0); /* missing name even though the handle looks valid */
    st32(TEX_RES+0x98+0xc,0xc4-(0x98+0xc));
    hook(0x027f50f8,3,MODEL,0);
    assert(ld32(texture_slot(0))==TEX_ORIGINAL && ld32(texture_slot(0)-4)==TEX_ORIGINAL);
    expect_hud("Skip=NO_ALBEDO  Count=1");
    expect_hud("Samplers body: _a1,?  eyes: _a1,_a0");
    hook(0x027f50f8,2,MODEL,0);
    st32(TEX_RES+0x80+0xc,0); st32(TEX_RES+0x98+0xc,0);
    st32(TEX_RES+0x98+0x10,0xc4-(0x98+0x10));
    hook(0x027f50f8,3,MODEL,0); texture_tinted();
    uint32_t body_view=ld32(texture_slot(0)), eye_view=ld32(texture_slot(1));
    assert(body_view!=eye_view); /* the same input texture needs two distinct views */
    memcpy(old_view,ppc_ptr(eye_view),sizeof old_view);
    /* Returning from the outer submission must NOT remove queued bindings. */
    hook(0x025e2e5c,2,MODEL,0); hook(0x025e2e5c,3,MODEL,0); texture_tinted();
    expect_hud("Skip=OK  Count=0  Kept=8  Lost=0");
    st32(texture_slot(2),TEX_NEXT);
    hook(0x025e2e5c,2,MODEL,0); hook(0x025e2e5c,3,MODEL,0);
    expect_hud("Kept=7  Lost=1");
    st32(texture_slot(2),eye_view);
    hook(0x027f50f8,2,MODEL,0); texture_originals();
    st32(texture_slot(1),TEX_NEXT); /* native BTP refresh changes the eye frame */
    hook(0x027f50f8,3,MODEL,0);
    texture_view_check(ld32(texture_slot(1)),TEX_NEXT,1);
    assert(!memcmp(old_view,ppc_ptr(eye_view),sizeof old_view)); /* published view immutable */
    hook(0x027f50f8,2,MODEL,0);
    assert(ld32(texture_slot(1))==TEX_NEXT);
    st32(texture_slot(1),TEX_ORIGINAL);
    hook(0x027f50f8,3,MODEL,0); texture_tinted();
    assert(ld32(texture_slot(0))==body_view && ld32(texture_slot(1))==eye_view); /* cache reuse */
    /* Actual nesting: viewCalc performs base refresh itself, then face
     * animation can replace those bindings before viewCalc returns. A repair
     * at viewCalc ENTRY cannot cover this sequence (0.1.10 regression). */
    hook(0x025e2e5c,2,MODEL,0);
    hook(0x027f55fc,2,MODEL,0);
    texture_refresh();
    for (int m=0;m<7;++m) st32(texture_slot(m),TEX_NEXT);
    hook(0x027f55fc,3,MODEL,0);
    for (int m=0;m<7;++m) texture_view_check(ld32(texture_slot(m)),TEX_NEXT,m>0);
    hook(0x025e2e5c,3,MODEL,0);
    expect_hud("Kept=8  Lost=0  Rebound=7");
    expect_hud("Late calls=1 changed=7 stale=0 rejected=0");
    hook(0x027f50f8,2,MODEL,0);
    for (int m=0;m<7;++m) {
        assert(ld32(texture_slot(m))==TEX_NEXT);
        st32(texture_slot(m),TEX_ORIGINAL);
    }
    hook(0x027f50f8,3,MODEL,0); texture_tinted();
    /* Reproduce the observed late face overwrite: native animation runs after
     * refresh but before joint/view submission. Material 0 acts as the mouth. */
    hook(0x025e2e5c,2,MODEL,0);
    st32(texture_slot(0),TEX_NEXT); st32(texture_slot(1),TEX_NEXT);
    hook(0x027f583c,2,MODEL,DATA);
    texture_view_check(ld32(texture_slot(0)),TEX_NEXT,0);
    texture_view_check(ld32(texture_slot(1)),TEX_NEXT,1);
    hook(0x027f583c,3,MODEL,DATA);
    st32(texture_slot(2),TEX_NEXT);
    hook(0x027f55fc,2,MODEL,0);
    hook(0x027f55fc,3,MODEL,0);
    texture_view_check(ld32(texture_slot(2)),TEX_NEXT,1);
    hook(0x025e2e5c,3,MODEL,0);
    expect_hud("Kept=8  Lost=0  Rebound=3");
    hook(0x027f55fc,2,MODEL,0); hook(0x027f55fc,3,MODEL,0);
    expect_hud("Rebound=3"); /* stable owned views are never reinterpreted */
    assert(!memcmp(old_view,ppc_ptr(eye_view),sizeof old_view));
    /* Cancellation restores the new eye/mouth frames, not the pre-animation frame. */
    free_mode=1; ++revision; tick(); buttons(0); buttons(0x60000); tick();
    for (int m=0;m<3;++m) assert(ld32(texture_slot(m))==TEX_NEXT);
    for (int m=3;m<MATERIAL_COUNT;++m) assert(ld32(texture_slot(m))==TEX_ORIGINAL);
    for (int m=0;m<3;++m) st32(texture_slot(m),TEX_ORIGINAL);
    buttons(0); buttons(0x60000); tick(); texture_refresh(); texture_tinted();
    /* Never dereference/recolor an unknown replacement or stale material array. */
    st32(texture_slot(0),0xdeadbeef); st32(MODEL+0x34,TEX_MATS+0x1000);
    hook(0x027f55fc,2,MODEL,0); hook(0x027f55fc,3,MODEL,0);
    assert(ld32(texture_slot(0))==0xdeadbeef);
    st32(MODEL+0x34,TEX_MATS);
    hook(0x027f55fc,2,MODEL,0); hook(0x027f55fc,3,MODEL,0);
    assert(ld32(texture_slot(0))==0xdeadbeef);
    expect_hud("stale=8 rejected=1 ptr=DEADBEEF word=00000000");
    st32(texture_slot(0),TEX_PIXELS); st32(TEX_PIXELS,0x12345678);
    hook(0x027f55fc,2,MODEL,0); hook(0x027f55fc,3,MODEL,0);
    assert(ld32(texture_slot(0))==TEX_PIXELS);
    expect_hud("rejected=2 ptr=DEADBEEF word=00000000"); /* first rejection remains */
    st32(texture_slot(0),body_view);
    /* Cancel via actual input/config lifecycle; no further draw is needed. */
    free_mode=1; ++revision; tick();
    buttons(0); buttons(0x60000); tick(); assert(damage(3)==3); texture_originals();
    texture_refresh(); texture_originals();
    buttons(0); buttons(0x60000); tick(); assert(damage(3)==6);
    texture_refresh(); texture_tinted();
    /* Do not overwrite a more recent binding installed by native code/another mod. */
    st32(texture_slot(2),TEX_NEXT);
    enabled=0; ++revision; tick();
    assert(ld32(texture_slot(2))==TEX_NEXT);
    st32(texture_slot(2),TEX_ORIGINAL); texture_originals();
    enabled=1; ++revision; tick(); buttons(0); buttons(0x60000); tick(); assert(damage(3)==6);
    /* Missing albedo, non-FTEX input, mismatched native resource and bad index
     * refuse the affected material without writing elsewhere. */
    st8(TEX_RES+0xc5,'x');
    st32(texture_slot(1),TEX_PIXELS);
    st32(TEX_MATS+(MATERIAL_COUNT-1-2)*0x3c,TEX_RES);
    st16(TEX_RES+3*0x200+0xc,96);
    texture_refresh();
    assert(ld32(texture_slot(0))==TEX_ORIGINAL && ld32(texture_slot(1))==TEX_PIXELS);
    assert(ld32(texture_slot(2))==TEX_ORIGINAL && ld32(texture_slot(3))==TEX_ORIGINAL);
    texture_view_check(ld32(texture_slot(4)),TEX_ORIGINAL,1);
    expect_hud("Skip=NO_ALBEDO  Count=4");
    expect_hud("Body J/N=8/8  Albedo=7  Views=4  Eyes found/bound=6/3");
    hook(0x027f50f8,2,MODEL,0);
    st8(TEX_RES+0xc5,'a'); st32(texture_slot(1),TEX_ORIGINAL);
    st32(TEX_MATS+(MATERIAL_COUNT-1-2)*0x3c,TEX_RES+2*0x200);
    st16(TEX_RES+3*0x200+0xc,MATERIAL_COUNT-1-3);
    /* Hands/hair and selected sword grip/shield/scabbard/held sword stay black,
     * even if their synthetic material names match eyes. A held non-sword is excluded. */
    const uint32_t fields[]={0xcf4,0x44c,0xcf8,0xe80,0xe7c,0x4440};
    for (int part=0;part<6;++part) {
        uint32_t model=MODEL+0x400+part*0x400, field=LINK+fields[part];
        st32(field,model); st32(model+0xac,DATA); st16(model+0x2a,MATERIAL_COUNT); st32(model+0x34,TEX_MATS);
        if (part==5) {
            st16(LINK+0x69b0,0); hook(0x027f50f8,3,model,0); texture_originals();
            st16(LINK+0x69b0,0x103);
        }
        hook(0x027f50f8,3,model,0);
        for (int m=0;m<MATERIAL_COUNT;++m) texture_view_check(ld32(texture_slot(m)),TEX_ORIGINAL,0);
        st32(texture_slot(0),TEX_NEXT);
        hook(0x027f55fc,2,model,0); hook(0x027f55fc,3,model,0);
        texture_view_check(ld32(texture_slot(0)),TEX_NEXT,0);
        hook(0x027f50f8,2,model,0); assert(ld32(texture_slot(0))==TEX_NEXT);
        st32(texture_slot(0),TEX_ORIGINAL); texture_originals(); st32(field,0);
        st16(LINK+0x69b0,0);
    }
    st32(LINK+0x3bc,0x800); texture_refresh(); texture_originals(); st32(LINK+0x3bc,0);
    texture_refresh(); texture_tinted();
    st8(PLAY+0x514c,1); tick(); texture_originals(); st8(PLAY+0x514c,0);
    buttons(0); buttons(0x60000); tick(); texture_refresh(); texture_tinted();
    st16(SAVE+0x22,0); tick(); texture_originals(); st16(SAVE+0x22,80);
    buttons(0); buttons(0x60000); tick(); texture_refresh(); texture_tinted();
    hook(0x0240ee28,2,LINK,0); texture_originals();
    tick(); buttons(0); buttons(0x60000); tick(); assert(damage(3)==6);
    /* Changing the material-array identity means old pointers may be freed.
     * Reset must discard that ledger without writing its stale slot addresses. */
    texture_refresh(); st32(MODEL+0x34,TEX_MATS+0x1000);
    st32(texture_slot(0),0xdeadbeef);
    enabled=0; ++revision; tick(); assert(ld32(texture_slot(0))==0xdeadbeef);
    for (int m=0;m<MATERIAL_COUNT;++m) st32(texture_slot(m),TEX_ORIGINAL);
    st32(MODEL+0x34,TEX_MATS);
    enabled=1; ++revision; tick(); buttons(0); buttons(0x60000); tick(); assert(damage(3)==6);
    /* Diagnostic toggle only changes observations, never appearance bindings. */
    diagnostics=0; tick(); texture_refresh(); texture_tinted();
    st32(texture_slot(1),TEX_NEXT);
    hook(0x027f55fc,2,MODEL,0); hook(0x027f55fc,3,MODEL,0);
    texture_view_check(ld32(texture_slot(1)),TEX_NEXT,1);
    hook(0x027f50f8,2,MODEL,0); assert(ld32(texture_slot(1))==TEX_NEXT);
    st32(texture_slot(1),TEX_ORIGINAL);
    read_hud(); assert(!strstr(hud_text,"Skip=") && !strstr(hud_text,"Samplers body:"));
    diagnostics=1; tick(); expect_hud("Refresh B/H/Hair/Gear=0/0/0/0"); expect_hud("Skip=NO_REFRESH");
    texture_refresh(); texture_tinted(); expect_hud("Refresh B/H/Hair/Gear=1/0/0/0");
    hook(0x027f50f8,2,MODEL,0);
    st32(MODEL+0xac,0); hook(0x027f50f8,3,MODEL,0); expect_hud("Skip=MODEL_DATA");
    st32(MODEL+0xac,DATA); st16(MODEL+0x2a,0);
    hook(0x027f50f8,3,MODEL,0); expect_hud("Skip=MATERIALS");
    st16(MODEL+0x2a,MATERIAL_COUNT); texture_refresh(); texture_tinted();
    /* Turning off restores all original bindings and leaves an explicitly old sample. */
    buttons(0); buttons(0x60000); tick(); texture_originals(); expect_hud("OFF (last sample)");
    buttons(0); buttons(0x60000); tick(); expect_hud("Skip=NO_REFRESH");
    assert(!memcmp(asset,ppc_ptr(TEX_ORIGINAL),sizeof asset));
    for (int m=0;m<MATERIAL_COUNT;++m) st32(MATERIALS+m*0x39c,0);
    st32(MODEL+0x34,0); st16(MODEL+0x2a,0);
    free_mode=0; ++revision; tick(); restored();
    puts("Dark Link animated albedo binding tests passed");
}
