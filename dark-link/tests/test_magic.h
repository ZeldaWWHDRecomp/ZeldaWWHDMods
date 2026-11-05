/* Synthetic layout resources and native aura objects; no game artwork. */
enum { MAGIC_ARC=0x11400000, MAGIC_FLYT=MAGIC_ARC+0x100, MAGIC_CTRL=0x11410000,
       MAGIC_PANE=0x11411000, AURA=0x11420000, AURA_MAT=0x11421000,
       AURA_RES=0x11422000, AURA_RS=0x11423000, AURA_ANM=0x11424000,
       AURA_VALUE=0x11425000 };
static uint64_t magic_generation=1;
static int magic_images, magic_draws, magic_textures, magic_authored;
static float magic_width, magic_x, magic_y;
static void magic_release(Cpu* c) { c->r[3]=1; }
static void magic_epoch(Cpu* c) { c->r[3]=(uint32_t)(magic_generation>>32); c->r[4]=(uint32_t)magic_generation; }
static uint32_t magic_fill_handles[2],magic_border_handles[2],magic_base_handles[2];
static int magic_window;
static int magic_require_core_alpha;
static int magic_native_hidden;
static int magic_picture_layers,magic_distinct_fill;
static int magic_named_layout;
static int magic_named_outer;
static int magic_solid_frame,magic_opaque_frame,magic_layer_check;
static uint32_t magic_previous_layer;
static float magic_check_fraction;
static float magic_test_sx=1,magic_test_sy=1;
static uint32_t magic_detail_handle;
static unsigned magic_border_alpha=255;
static unsigned magic_frame_count=1;
static int magic_writes,magic_base_fill,magic_animated,magic_bad_animation,magic_long,magic_bad_geometry;
/* Host-side Data files survive a guest-memory savestate restore. */
static struct { char path[96]; unsigned size; unsigned char* data; } magic_cache[64];
static int magic_write_failure;
static int magic_cache_slot(const char* path) {
    for(int i=0;i<64;++i) if(!magic_cache[i].path[0] || !strcmp(magic_cache[i].path,path)) return i;
    abort();
}
static void magic_read(Cpu* c) {
    const char* path=(const char*)ppc_ptr(c->r[3]);
    if(strncmp(path,"magic-",6)) {c->r[3]=-1;return;}
    int i=magic_cache_slot(path);
    if(!magic_cache[i].path[0]) {c->r[3]=-1;return;}
    unsigned n=magic_cache[i].size<c->r[5]?magic_cache[i].size:c->r[5];
    memcpy(ppc_ptr(c->r[4]),magic_cache[i].data,n);c->r[3]=n;
}
static void magic_write(Cpu* c) {
    const char* path=(const char*)ppc_ptr(c->r[3]);
    const uint8_t* png=ppc_ptr(c->r[4]);
    assert(c->r[5]>100 && !memcmp(png,"\x89PNG\r\n\x1a\n",8));
    /* First pixel of the uncompressed zlib stream: filter byte then RGBA.
     * Validate output pixels, not only a red tint in an emitted HUD command. */
    assert(png[24]==8 && png[25]==6 && png[48]==0);
    if(strstr(path,"fill")) { for(int i=0;i<4;++i) assert(png[49+i*4]==255 && png[50+i*4]==99 && png[51+i*4]==99);assert(png[52]==255); }
    if(magic_base_fill && !magic_distinct_fill && strstr(path,"frame")) assert(png[49]==99&&png[50]==99&&png[51]==99);
    if(magic_named_layout && strstr(path,"frame")) assert(png[50]>png[49]); /* retain native green frame shading */
    if(magic_require_core_alpha && !strstr(path,"border")) assert(png[52]==255);
    if(strstr(path,"border")) {
        if(magic_named_layout) {
            unsigned w=(path[6]=='1'?386:196)+(magic_named_outer?14:0),h=magic_named_outer==2?33:magic_named_outer?30:18;
            assert(((unsigned)png[18]*256+png[19])==w && png[23]==h);
            assert(png[52]==255);
            if(!magic_opaque_frame) assert(png[49+(h/2)*(w*4+1)+(w/2)*4+3]==0);
            if(magic_named_outer==2) {
                /* Four padded 16x16 corner textures form a 210/400x33 frame.
                 * Nominal opening is only W-32 by 1, but actual transparent
                 * opening is W-16 by 17. All four edges must survive. */
                const unsigned points[4][2]={{w/2,1},{w-2,h/2},{1,h/2},{w/2,h-2}};
                for(unsigned k=0;k<4;++k) {
                    unsigned at=49+points[k][1]*(w*4+1)+points[k][0]*4;
                    assert(png[at]==71 && png[at+1]==110 && png[at+2]==71 && png[at+3]==255);
                }
                if(!magic_opaque_frame) for(unsigned y=11;y<22;++y)for(unsigned x=11;x<w-11;++x)
                    assert(png[49+y*(w*4+1)+x*4+3]==0);
                if(magic_opaque_frame) assert(png[49+11*(w*4+1)+11*4+3]==255);
            }
            if(magic_solid_frame) {
                /* Native linear white colour 10/28/10 becomes sRGB 71/110/71.
                 * Test actual frame pixels, not just a submitted rectangle. */
                assert(png[49]==71 && png[50]==110 && png[51]==71);
                unsigned edge=49+(h/2)*(w*4+1)+5*4;
                assert(png[edge]==71 && png[edge+1]==110 && png[edge+3]==255);
            }
        } else {
            assert(png[19]==108 && png[23]==24);
            assert(png[49]<png[49+3*4] && png[52]==magic_border_alpha); /* contrast and pane coverage retained */
            assert(png[49+12*(108*4+1)+54*4+3]==0); /* frame never fills the centre */
        }
    }
    if(strstr(path,"detail")) {
        assert(png[49]==png[50] && png[50]==png[51] && png[52]==255);
        assert(png[49+1*17+1*4+3]==0); /* native picture's open centre survives */
    }
    ++magic_writes;
    if(magic_write_failure) {c->r[3]=-1;return;}
    int i=magic_cache_slot(path);strcpy(magic_cache[i].path,path);
    magic_cache[i].size=c->r[5];magic_cache[i].data=realloc(magic_cache[i].data,c->r[5]);
    assert(magic_cache[i].data);memcpy(magic_cache[i].data,png,c->r[5]);c->r[3]=c->r[5];
}
static void magic_texture(Cpu* c) {
    if(c->r[3]==0) { assert(!strcmp((const char*)ppc_ptr(c->r[4]),"assets/dark_link.png")); c->r[3]=0; return; } /* fallback tested here; combined test loads the icon */
    assert(c->r[3]==1); /* owned, recoloured native artwork in mod Data */
    const char* path=(const char*)ppc_ptr(c->r[4]);
    assert(!strncmp(path,"magic-",6));
    ++magic_textures; c->r[3]=++magic_images;
    if(strstr(path,"border")) magic_border_handles[path[6]=='1']=c->r[3];
    if(strstr(path,"fill")) magic_fill_handles[path[6]=='1']=c->r[3];
    if(strstr(path,"frame")) magic_base_handles[path[6]=='1']=c->r[3];
    if(strstr(path,"detail")) magic_detail_handle=c->r[3];
}
static void magic_setting(Cpu* c) {
    const char* key=(const char*)ppc_ptr(c->r[3]);
    if (magic_authored && !strcmp(key,"layout-pane:Main_00/L_MagicBar_00")) {
        uint32_t p=c->r[5]; memset(ppc_ptr(p),0,64); st32(p,1);
        stf32(p+8,-530); stf32(p+12,280); c->r[3]=64; return;
    }
    if (strncmp(key,"layout-pane:MagicBar_00/",24)) { c->r[3]=0; return; }
    assert(c->r[4]==6 && c->r[6]==64);
    const char* name=key+24;
    if(magic_named_layout) {
        int base=!strcmp(name,"P_MagicBase_00") || !strcmp(name,"P_MagicBase_01");
        int fill=!strcmp(name,"P_Magic_00") || !strcmp(name,"P_Magic_01");
        int wnd=!strcmp(name,"W_MagicBase_00") || !strcmp(name,"W_MagicBase_01");
        if(!base && !fill && !wnd) {c->r[3]=0;return;}
        unsigned v=name[strlen(name)-1]=='1';uint32_t p=c->r[5];memset(ppc_ptr(p),0,64);
        st32(p,1);st32(p+4,wnd?0:2); /* hidden native panes still have valid artwork */
        stf32(p+16,(v?386:196)-(fill?6:0)+(wnd && magic_named_outer?14:0));
        stf32(p+20,fill?12:wnd && magic_named_outer?(magic_named_outer==2?33:30):18);
        strcpy((char*)ppc_ptr(p+32),fill?"fill":"frame");c->r[3]=64;return;
    }
    if(magic_picture_layers && (!strcmp(name,"P_Outline_00") || !strcmp(name,"P_White_00"))) {
        uint32_t p=c->r[5];memset(ppc_ptr(p),0,64);st32(p,1);st32(p+4,3);
        stf32(p+16,108);stf32(p+20,24);st32(p+28,255);
        strcpy((char*)ppc_ptr(p+32),!strcmp(name,"P_White_00")?"fill":"frame");c->r[3]=64;return;
    }
    if(magic_window && !strcmp(name,"W_Frame_00")) {
        uint32_t p=c->r[5];memset(ppc_ptr(p),0,64);st32(p,1);st32(p+4,1);
        stf32(p+16,108);stf32(p+20,24);st32(p+28,255);c->r[3]=64;return;
    }
    int fill=!strcmp(name,"P_Fill_00");
    if (!fill && strcmp(name,"P_MagicBase_00") && strcmp(name,"P_MagicBase_01")) { c->r[3]=0; return; }
    uint32_t p=c->r[5]; memset(ppc_ptr(p),0,64);
    st32(p,1); st32(p+4,magic_native_hidden?2:3); stf32(p+8,0); stf32(p+12,0);
    stf32(p+16,fill ? (magic_animated?48:96):(magic_long&&!strcmp(name,"P_MagicBase_01")?200:100)); stf32(p+20,fill ? 12:16); st32(p+28,magic_native_hidden?0:255);
    if(fill && magic_bad_geometry) stf32(p+8,100000);
    strcpy((char*)ppc_ptr(p+32),fill ? "fill":"frame"); c->r[3]=64;
}
static void magic_emit(uint32_t e) {
    ++magic_draws;
    assert(ld32(e+4)==(magic_authored ? 1u:0u)); /* follows native, already aspect-adjusted coordinates */
    if(magic_named_layout) {
        unsigned v=ld8(SAVE+0x33)>16;
        if(magic_layer_check) {
            uint32_t image=ld32(e+60);
            if(image==magic_base_handles[v]) assert(magic_previous_layer==magic_border_handles[v]);
            if(image==magic_fill_handles[v]) {
                assert(magic_previous_layer==magic_base_handles[v]);
                assert(ldf32(e+48)==magic_check_fraction);
            }
            magic_previous_layer=image;
        }
        if(ld32(e+60)==magic_fill_handles[v]) {
            magic_width=ldf32(e+20);magic_x=ldf32(e+12);magic_y=ldf32(e+16);
            assert(ldf32(e+24)==12*magic_test_sy && magic_x==10+3*magic_test_sx);
            assert(magic_y==28+(magic_named_outer==2?27:magic_named_outer?24:12)*magic_test_sy);
            assert(magic_width==(v?380:190)*magic_test_sx*ldf32(e+48));
        }
        if(magic_named_outer && ld32(e+60)==magic_base_handles[v]) {
            /* Background must stay at its INNER size; 0.1.30 enlarged this
             * green rectangle instead of supplying the missing frame. */
            assert(ldf32(e+12)==10 && ldf32(e+16)==28+(magic_named_outer==2?24:21)*magic_test_sy);
            assert(ldf32(e+20)==(v?386:196)*magic_test_sx && ldf32(e+24)==18*magic_test_sy);
        }
        if(magic_named_outer && ld32(e+60)==magic_border_handles[v]) {
            assert(ldf32(e+12)==10-7*magic_test_sx);
            assert(ldf32(e+16)==28+(magic_named_outer==2?16.5f:15)*magic_test_sy);
            assert(ldf32(e+20)==(v?400:210)*magic_test_sx && ldf32(e+24)==(magic_named_outer==2?33:30)*magic_test_sy);
            /* Native outer bottom is 20 + 15*sy: retain an eight-unit gap. */
            assert(ldf32(e+16)-(20+(magic_named_outer==2?16.5f:15)*magic_test_sy)==8);
        }
        return;
    }
    if(ld32(e+60)==magic_border_handles[0] || ld32(e+60)==magic_border_handles[1]) {
        assert(ldf32(e+20)==108 && ldf32(e+24)==24);assert(ld32(e+56)==0xffffffff);return;
    }
    if (ld32(e+60)==magic_fill_handles[0] || ld32(e+60)==magic_fill_handles[1]) {
        magic_width=ldf32(e+20); magic_x=ldf32(e+12); magic_y=ldf32(e+16);
        assert(ld32(e+56)==0xffffffff); /* red pixels must not be tinted a second time */
        float end=magic_width/(magic_base_fill&&!magic_distinct_fill?100.0f:magic_animated&&magic_bad_animation?48.0f:magic_long?196.0f:96.0f);
        assert(ldf32(e+40)==0 && ldf32(e+44)==0 && ldf32(e+52)==1);
        float delta=(float)ldf32(e+48)-end; assert(delta>-0.00001f && delta<0.00001f);
    } else assert(ld32(e+56)==0xffffffff);
}
static void fixture_picture(uint32_t p,const char* name,unsigned mat) {
    st32(p,0x70696331); st32(p+4,0x88); strcpy((char*)ppc_ptr(p+12),name);
    st8(p+8,1);st8(p+10,255);
    st16(p+0x64,mat); st8(p+0x66,1);
    stf32(p+0x44,1);stf32(p+0x48,1);
    stf32(p+0x4c,mat?(magic_animated?48:96):100);stf32(p+0x50,mat?12:16);
    for(unsigned i=0;i<4;++i) st32(p+0x54+4*i,0xffffffff);
    const float uv[8]={0,0,1,0,0,1,1,1};
    for(unsigned i=0;i<8;++i) stf32(p+0x68+4*i,uv[i]);
}
static void fixture_magic(void) {
    uint32_t f=MAGIC_FLYT;
    st32(MAGIC_ARC,0x53415243); st16(MAGIC_ARC+4,0x14);
    st32(MAGIC_ARC+8,0xc00); st32(MAGIC_ARC+12,0x100);
    st32(MAGIC_ARC+0x14,0x53464154); st16(MAGIC_ARC+0x18,12); st16(MAGIC_ARC+0x1a,3);
    st32(MAGIC_ARC+0x24,0x01000000); st32(MAGIC_ARC+0x28,0); st32(MAGIC_ARC+0x2c,0x1f0);
    st32(MAGIC_ARC+0x34,0x01000006);st32(MAGIC_ARC+0x38,0x300);st32(MAGIC_ARC+0x3c,0x728);
    st32(MAGIC_ARC+0x44,0x0100000b);st32(MAGIC_ARC+0x48,0x800);st32(MAGIC_ARC+0x4c,0xa28);
    st32(MAGIC_ARC+0x50,0x53464e54); st16(MAGIC_ARC+0x54,8);
    strcpy((char*)ppc_ptr(MAGIC_ARC+0x58),"blyt/MagicBar_00.bflyt");
    strcpy((char*)ppc_ptr(MAGIC_ARC+0x70),"timg/frame.bflim");
    strcpy((char*)ppc_ptr(MAGIC_ARC+0x84),"timg/fill.bflim");
    /* RGBA frame and BC4 alpha fill, both native linear tiling (pitch 64). */
    uint32_t frame=MAGIC_ARC+0x400,fill=MAGIC_ARC+0x900;
    memset(ppc_ptr(frame),255,1024);memset(ppc_ptr(fill),0,512);st8(fill,255);
    uint32_t trailers[2]={frame+1024,fill+512};
    for(int i=0;i<2;++i) {
        uint32_t t=trailers[i];st32(t,0x464c494d);st32(t+20,0x696d6167);
        st16(t+28,4);st16(t+30,4);st8(t+34,i?0x10:9);st8(t+35,1);st32(t+36,i?512:1024);
    }
    st32(f,0x464c5954); st16(f+4,0xfeff); st16(f+6,20); st16(f+16,4);
    uint32_t mat=f+20; st32(mat,0x6d617431); st32(mat+4,100); st16(mat+8,2);
    st32(mat+12,20); st32(mat+16,60);
    strcpy((char*)ppc_ptr(mat+20),"Frame"); st32(mat+20+32,0xffffffff);
    strcpy((char*)ppc_ptr(mat+60),"Fill"); st32(mat+60+32,0x20ff20ff);
    fixture_picture(f+120,"P_MagicBase_00",0);
    fixture_picture(f+256,"P_MagicBase_01",0);
    fixture_picture(f+392,"P_Fill_00",1);
    st32(MAGIC_ARC+0x2c,528); st32(MAGIC_ARC+8,0xc00);
    st32(0x101f7274,0x11430000); magic_archive=MAGIC_ARC;
    st32(MAGIC_CTRL+0x5c,MAGIC_PANE); st32(MAGIC_CTRL+0x60,MAGIC_PANE);
    stf32(MAGIC_PANE+0x3c,100); stf32(MAGIC_PANE+0x40,16);
    stf32(MAGIC_PANE+0x48,1); stf32(MAGIC_PANE+0x5c,1);
    stf32(MAGIC_PANE+0x54,-530); stf32(MAGIC_PANE+0x64,280);
}
static void fixture_window(void) {
    /* Separate archive location avoids overwriting either source texture. */
    uint32_t f=MAGIC_ARC+0x3000,mat=f+20,txl=f+576,wnd=f+608;
    memset(ppc_ptr(f),0,800);memcpy(ppc_ptr(f),ppc_ptr(MAGIC_FLYT),20);st16(f+16,6);
    st32(mat,0x6d617431);st32(mat+4,148);st16(mat+8,3);
    st32(mat+12,24);st32(mat+16,64);st32(mat+20,104);
    memcpy(ppc_ptr(mat+24),ppc_ptr(MAGIC_FLYT+40),40);
    memcpy(ppc_ptr(mat+64),ppc_ptr(MAGIC_FLYT+80),40);
    strcpy((char*)ppc_ptr(mat+104),"Border");st32(mat+136,0xffffffff);st32(mat+140,1);
    memcpy(ppc_ptr(f+168),ppc_ptr(MAGIC_FLYT+120),408);
    st32(txl,0x74786c31);st32(txl+4,32);st16(txl+8,1);st32(txl+12,4);
    strcpy((char*)ppc_ptr(txl+16),"frame.bflim");
    st32(wnd,0x776e6431);st32(wnd+4,112+8*magic_frame_count);strcpy((char*)ppc_ptr(wnd+12),"W_Frame_00");
    st8(wnd+8,1);st8(wnd+10,255);
    stf32(wnd+0x4c,108);stf32(wnd+0x50,24);
    for(int i=0;i<4;++i) st16(wnd+0x5c+i*2,4);
    st8(wnd+0x64,magic_frame_count);st8(wnd+0x65,0x11);st32(wnd+0x6c,112);
    for(unsigned i=0;i<magic_frame_count;++i) {
        unsigned e=112+4*magic_frame_count+4*i;st32(wnd+112+4*i,e);st16(wnd+e,2);
        const unsigned flips[8]={0,1,2,4,0,1,0,2};st8(wnd+e+2,flips[i]);
    }
    for(unsigned y=0;y<4;++y) for(unsigned x=0;x<4;++x) {
        uint32_t pixel=MAGIC_ARC+0x400+(y*64+x)*4;
        st8(pixel,32+x*32);st8(pixel+1,32+y*32);st8(pixel+2,64);st8(pixel+3,255);
    }
    st32(MAGIC_ARC+0x28,0x2f00);st32(MAGIC_ARC+0x2c,0x2f00+608+112+8*magic_frame_count);st32(MAGIC_ARC+8,0x4000);
}
static void fixture_animation(unsigned frames) {
    /* Fourth SARC node, preserving existing names after the SFNT moves. */
    memmove(ppc_ptr(MAGIC_ARC+0x60),ppc_ptr(MAGIC_ARC+0x50),0x48);
    st16(MAGIC_ARC+0x1a,4);st32(MAGIC_ARC+8,0x2000);
    st32(MAGIC_ARC+0x54,0);st32(MAGIC_ARC+0x58,0xb00);st32(MAGIC_ARC+0x5c,0xb00+280);
    uint32_t f=MAGIC_ARC+0xc00,pat=f+20,pai=f+84;
    memset(ppc_ptr(f),0,280);st32(f,0x464c414e);st16(f+4,0xfeff);st16(f+6,20);st16(f+16,2);
    st32(pat,0x70617431);st32(pat+4,64);st16(pat+10,1);st32(pat+12,28);st32(pat+16,36);
    st16(pat+22,frames);strcpy((char*)ppc_ptr(pat+28),"Full");strcpy((char*)ppc_ptr(pat+36),"G_Bar_00");
    st32(pai,0x70616931);st32(pai+4,196);st16(pai+8,frames);st16(pai+14,2);st32(pai+16,20);
    st32(pai+20,28);st32(pai+24,112);
    for(int i=0;i<2;++i) {
        uint32_t c=pai+28+i*84,t=c+36,e=t+12,k=e+12;
        strcpy((char*)ppc_ptr(c),i?"Fill":"P_Fill_00");st8(c+28,1);st8(c+29,i);st32(c+32,36);
        st32(t,i?0x464c5453:0x464c5041);st8(t+4,1);st32(t+8,12);
        st8(e+1,i?3:8);st8(e+2,2);st16(e+4,2);st32(e+8,12);
        stf32(k,0);stf32(k+4,i?1:48);stf32(k+8,i?-0.5f/frames:((frames==32?196.0f:96.0f)-48)/frames);
        stf32(k+12,frames);stf32(k+16,i?0.5f:frames==32?196:96);stf32(k+20,i?-0.5f/frames:((frames==32?196.0f:96.0f)-48)/frames);
    }
}
static uint32_t fixture_style_animation(const char* tag,const char* group,const char* name,
        unsigned kind,uint32_t type,unsigned target,unsigned channels,float first,float last) {
    unsigned nodes=ld16(MAGIC_ARC+0x1a);assert(nodes<8);
    uint32_t node=MAGIC_ARC+0x20+nodes*16;
    memmove(ppc_ptr(node+16),ppc_ptr(node),0x48);memset(ppc_ptr(node),0,16);
    st16(MAGIC_ARC+0x1a,nodes+1);
    uint32_t f=MAGIC_ARC+0x5000+(nodes-3)*0x400,pat=f+20,pai=f+116,c=pai+24,t=c+36;
    unsigned bytes=116+24+36+8+channels*40;
    memset(ppc_ptr(f),0,bytes);st32(f,0x464c414e);st16(f+4,0xfeff);st16(f+6,20);st16(f+16,2);
    st32(pat,0x70617431);st32(pat+4,96);st16(pat+10,1);st32(pat+12,28);st32(pat+16,64);
    st16(pat+22,20);strcpy((char*)ppc_ptr(pat+28),tag);strcpy((char*)ppc_ptr(pat+64),group);
    st32(pai,0x70616931);st32(pai+4,bytes-116);st16(pai+8,20);st16(pai+14,1);st32(pai+16,20);st32(pai+20,24);
    strcpy((char*)ppc_ptr(c),name);st8(c+28,1);st8(c+29,kind);st32(c+32,36);
    st32(t,type);st8(t+4,channels);
    for(unsigned i=0;i<channels;++i) {
        uint32_t e=t+8+channels*4+i*36,k=e+12;st32(t+8+i*4,e-t);
        st8(e+1,target+i);st8(e+2,2);st16(e+4,2);st32(e+8,12);
        stf32(k,0);stf32(k+4,first);stf32(k+8,(last-first)/20);
        stf32(k+12,20);stf32(k+16,last);stf32(k+20,(last-first)/20);
    }
    st32(node+8,f-MAGIC_ARC-0x100);st32(node+12,f-MAGIC_ARC-0x100+bytes);st32(MAGIC_ARC+8,0x7000);
    return t;
}
static void magic_expect_draws(unsigned count) {
    ++magic_generation;read_hud();if(!magic_authored) hook(0x02675414,3,MAGIC_CTRL,0);
    magic_draws=0;read_hud();assert((unsigned)magic_draws==count);
}
static void test_magic_hidden_native(void) {
    /* Dark Link must remain visible with no native magic gauge. The PNG
     * assertion checks actual alpha, not only submitted image handles. */
    magic_window=0;magic_require_core_alpha=1;fixture_magic();
    const unsigned pictures[3]={120,256,392};
    for(unsigned i=0;i<3;++i) {
        st8(MAGIC_FLYT+pictures[i]+8,0);st8(MAGIC_FLYT+pictures[i]+10,0);
    }
    st8(SAVE+0x33,0);magic_authored=magic_native_hidden=1;magic_expect_draws(2);assert(magic_width>0);
    /* When base == fill, 0.1.22 submits handles but bakes alpha zero. */
    magic_base_fill=1;st32(MAGIC_FLYT+20+20+32,0x20ff20ff);magic_expect_draws(2);
    magic_base_fill=0;
    /* Shared native fade parent, without an InAll resource to override it. */
    fixture_magic();
    uint32_t f=MAGIC_ARC+0x3000,parent=f+120;
    memset(ppc_ptr(f),0,628);memcpy(ppc_ptr(f),ppc_ptr(MAGIC_FLYT),120);
    st16(f+16,7);st32(parent,0x70616e31);st32(parent+4,84);
    strcpy((char*)ppc_ptr(parent+12),"N_All_00");st32(parent+84,0x70617331);st32(parent+88,8);
    memcpy(ppc_ptr(parent+92),ppc_ptr(MAGIC_FLYT+120),408);
    st32(f+620,0x70616531);st32(f+624,8);
    st32(MAGIC_ARC+0x28,0x2f00);st32(MAGIC_ARC+0x2c,0x2f00+628);st32(MAGIC_ARC+8,0x4000);
    const unsigned capacities[3]={0,16,32};
    for(unsigned i=0;i<3;++i) {
        st8(SAVE+0x33,capacities[i]);magic_authored=magic_native_hidden=i==0;
        magic_expect_draws(2);assert(magic_width>0);
    }
    int writes=magic_writes;read_hud();read_hud();assert(magic_writes==writes);
    /* Optional hierarchy metadata must not veto independently valid images. */
    st32(f+620,0x70617331);magic_expect_draws(2);
    magic_authored=0;magic_require_core_alpha=0;st8(SAVE+0x33,0);fixture_magic();
    puts("Dark Link hidden native HUD: visible cached pixels at 0/16/32 magic passed");
}
static void test_magic_picture_layers(void) {
    magic_window=0;magic_picture_layers=1;magic_frame_count=1;
    fixture_magic();fixture_window();
    uint32_t f=MAGIC_ARC+0x3000,pic=f+728;
    fixture_picture(pic,"P_Outline_00",2);fixture_picture(pic+136,"P_White_00",2);
    for(unsigned i=0;i<2;++i) { stf32(pic+i*136+0x4c,108);stf32(pic+i*136+0x50,24); }
    st16(f+16,8);st32(MAGIC_ARC+0x2c,0x2f00+1000);
    for(unsigned y=0;y<4;++y) for(unsigned x=0;x<4;++x) {
        uint32_t pixel=MAGIC_ARC+0x400+(y*64+x)*4;
        st8(pixel,128);st8(pixel+1,128);st8(pixel+2,128);
        st8(pixel+3,(x==0 || x==3 || y==0 || y==3)?255:0);
    }
    /* Copy a real pic1 outline; reject the similarly sized opaque white pane. */
    magic_detail_handle=0;magic_expect_draws(3);assert(magic_detail_handle);
    int writes=magic_writes;read_hud();assert(magic_writes==writes);
    magic_picture_layers=0;fixture_magic();
    /* A green background must not override the separate animated fill. */
    magic_animated=magic_base_fill=magic_distinct_fill=1;free_mode=1;++revision;tick();
    fixture_magic();fixture_animation(16);st32(MAGIC_FLYT+20+20+32,0x20ff20ff);
    magic_expect_draws(2);assert(magic_width==96 && magic_x==62);
    magic_animated=magic_base_fill=magic_distinct_fill=0;free_mode=0;++revision;tick();fixture_magic();
    puts("Dark Link separate animated fill and native picture outline tests passed");
}
static void test_magic_style(void) {
    magic_frame_count=1;fixture_magic();fixture_window();
    uint32_t f=MAGIC_ARC+0x3000,wnd=f+608;
    /* Published 0.1.21 draws this invisible white window as an opaque frame. */
    st8(wnd+10,0);magic_expect_draws(2);
    st8(wnd+10,255);st8(wnd+8,0);magic_expect_draws(2);
    st8(wnd+8,1);
    /* Outer box alone matches, but thick slices would hide almost all fill. */
    st16(wnd+0x5c,50);st16(wnd+0x5e,50);st16(wnd+0x60,11);st16(wnd+0x62,11);
    magic_expect_draws(2);
    fixture_window();st8(wnd+10,128);magic_border_alpha=128;magic_expect_draws(3);
    magic_border_alpha=255;
    /* InAll at end reveals it; Flash at zero hides it again. SARC order is
     * deliberately reversed so the loader must use native animation order. */
    fixture_magic();fixture_window();
    fixture_style_animation("Flash","G_Flash_00","W_Frame_00",0,0x464c5643,16,1,0,255);
    fixture_style_animation("InAll","G_All_00","W_Frame_00",0,0x464c5643,16,1,0,255);
    magic_expect_draws(2);
    /* Warning-group panes are excluded even if Flash resources are missing. */
    fixture_magic();fixture_window();
    uint32_t group=wnd+120;st32(group,0x67727031);st32(group+4,60);
    strcpy((char*)ppc_ptr(group+8),"G_Flash_00");st16(group+32,1);
    strcpy((char*)ppc_ptr(group+36),"W_Frame_00");st16(f+16,7);st32(MAGIC_ARC+0x2c,0x2f00+788);
    magic_expect_draws(2);
    /* Hidden parent propagates to its window child. */
    fixture_magic();fixture_window();
    memmove(ppc_ptr(wnd+92),ppc_ptr(wnd),120);memset(ppc_ptr(wnd),0,92);
    st32(wnd,0x70616e31);st32(wnd+4,84);st8(wnd+8,3);st8(wnd+10,0);
    strcpy((char*)ppc_ptr(wnd+12),"N_Flash");st32(wnd+84,0x70617331);st32(wnd+88,8);
    st32(wnd+212,0x70616531);st32(wnd+216,8);st16(f+16,9);st32(MAGIC_ARC+0x2c,0x2f00+828);
    magic_expect_draws(2);
    /* Copy the displayed style, not the white authoring colour or an initial
     * alpha of zero. ColorChange frame zero and InAll end both matter. */
    fixture_magic();st32(MAGIC_FLYT+20+60+32,0xffffffff);st8(MAGIC_FLYT+392+10,0);
    uint32_t tracks=fixture_style_animation("ColorChange","G_Change_00","Fill",1,0x464c4d43,4,3,32,255);
    uint32_t green=tracks+8+12+36+12;stf32(green+4,255);stf32(green+8,0);
    fixture_style_animation("InAll","G_All_00","P_Fill_00",0,0x464c5643,16,1,0,255);
    magic_expect_draws(2);assert(magic_width>0);
    fixture_magic();fixture_window();
    puts("Dark Link displayed style, flash exclusion, frame opening and alpha tests passed");
}
static void test_magic_named_layout(void) {
    /* Shape captured in the user's 0.1.28 report: green 196/386x18 bases,
     * separate 190/380x12 two-map fills with no first-stage progress track,
     * and hidden named windows. Synthetic textures, never game assets. */
    fixture_magic();magic_named_layout=1;free_mode=1;++revision;tick();
    /* Textured frame extents come from the actual texture, not wnd1's
     * fallback fields. Match this first fixture's three-unit frame. */
    st16(MAGIC_ARC+0x400+1024+28,3);st16(MAGIC_ARC+0x400+1024+30,3);
    uint32_t f=MAGIC_ARC+0x3000,mat=f+20,txl=f+724,wnd=f+768;
    memset(ppc_ptr(f),0,1100);memcpy(ppc_ptr(f),ppc_ptr(MAGIC_FLYT),20);st16(f+16,8);
    st32(mat,0x6d617431);st32(mat+4,160);st16(mat+8,3);
    st32(mat+12,24);st32(mat+16,68);st32(mat+20,116);
    const unsigned offsets[]={24,68,116};
    for(unsigned i=0;i<3;++i) {
        uint32_t m=mat+offsets[i];strcpy((char*)ppc_ptr(m),i==1?"Fill":i==2?"Border":"Base");
        st32(m+32,i==0?0x001103ff:i==1?0x20ff20ff:0xffffffff);
        st32(m+36,i==1?2:1);st16(m+40,i==1?1:0);
    }
    for(unsigned v=0;v<2;++v) {
        uint32_t b=f+180+v*272,p=b+136,w=wnd+v*120;
        fixture_picture(b,v?"P_MagicBase_01":"P_MagicBase_00",0);
        fixture_picture(p,v?"P_Magic_01":"P_Magic_00",1);
        stf32(b+0x4c,v?386:196);stf32(b+0x50,18);st8(b+10,0);
        stf32(p+0x4c,v?380:190);st8(p+10,v?0:255);
        st32(w,0x776e6431);st32(w+4,120);strcpy((char*)ppc_ptr(w+12),v?"W_MagicBase_01":"W_MagicBase_00");
        stf32(w+0x4c,v?386:196);stf32(w+0x50,18); /* hidden, alpha zero */
        for(unsigned k=0;k<4;++k)st16(w+0x5c+2*k,3);
        st8(w+0x64,1);st8(w+0x65,0x11);st32(w+0x6c,112);st32(w+112,116);st16(w+116,2);
    }
    st32(txl,0x74786c31);st32(txl+4,44);st16(txl+8,2);st32(txl+12,8);st32(txl+16,20);
    strcpy((char*)ppc_ptr(txl+20),"frame.bflim");strcpy((char*)ppc_ptr(txl+32),"fill.bflim");
    st32(MAGIC_ARC+0x28,0x2f00);st32(MAGIC_ARC+0x2c,0x2f00+1008);st32(MAGIC_ARC+8,0x4000);
    uint8_t original[1100];memcpy(original,ppc_ptr(f),sizeof original);
    const unsigned capacities[]={0,16,32,16};
    for(unsigned i=0;i<4;++i) {
        unsigned v=capacities[i]>16;st8(SAVE+0x33,capacities[i]);st8(SAVE+0x34,0);
        stf32(MAGIC_PANE+0x3c,v?386:196);stf32(MAGIC_PANE+0x40,18);
        stf32(MAGIC_PANE+0x54,-640+10+(v?193:98));stf32(MAGIC_PANE+0x64,340);
        magic_expect_draws(3);assert(magic_width==(v?380:190));
        expect_hud(v?"P_Magic_01 380x12 2/0/0 FILL":"P_Magic_00 190x12 2/0/255 FILL");
        assert(!strstr(hud_text,"BASE+FILL"));expect_hud("BORDER");expect_hud("Dark Link");
    }
    assert(!memcmp(original,ppc_ptr(f),sizeof original));
    /* A wider native outer rectangle must not resize the red fill. Verify
     * both capacities, nonuniform UI scaling, row spacing and cache stability. */
    magic_named_outer=1;
    for(unsigned v=0;v<2;++v) {
        stf32(wnd+v*120+0x4c,v?400:210);stf32(wnd+v*120+0x50,30);
        st16(wnd+v*120+0x5c,10);st16(wnd+v*120+0x5e,10);
        st16(wnd+v*120+0x60,9);st16(wnd+v*120+0x62,9);
    }
    for(unsigned i=0;i<4;++i) {
        unsigned v=i&1;st8(SAVE+0x33,v?32:16);
        magic_test_sx=i<2?1:0.75f;magic_test_sy=i<2?1:1.25f;
        stf32(MAGIC_PANE+0x3c,v?386:196);stf32(MAGIC_PANE+0x40,18);
        stf32(MAGIC_PANE+0x48,magic_test_sx);stf32(MAGIC_PANE+0x5c,magic_test_sy);
        stf32(MAGIC_PANE+0x54,-640+10+(v?193:98)*magic_test_sx);
        magic_expect_draws(3);
        int writes=magic_writes;for(unsigned n=0;n<5;++n)read_hud();assert(magic_writes==writes);
    }
    /* Zero-map frame materials need no TXL resource. Native window content
     * is deliberately unreadable: only the frame slices may be accessed. */
    magic_solid_frame=1;st32(mat+116+36,0);st32(mat+116+32,0x102810ff);
    st32(txl,0x70616431); /* remove the texture-list section */
    for(unsigned v=0;v<2;++v) {st8(wnd+v*120+0x65,1);st32(wnd+v*120+0x68,0xfffffff0);}
    magic_expect_draws(3);expect_hud("BORDER");
    magic_solid_frame=0;
    /* Reproduce the 0.1.31 device report: four frames, 210x33 outer bounds.
     * Padded slices are synthetic; their nominal bounds overlap the fill,
     * while their alpha does not. The old opening gate rejects this frame. */
    memcpy(ppc_ptr(f),original,sizeof original);magic_named_outer=2;
    const unsigned flips[4]={0,1,2,4};
    uint8_t old_windows[240];memcpy(old_windows,ppc_ptr(wnd),sizeof old_windows);
    for(unsigned v=0;v<2;++v) {
        uint32_t w=wnd+v*144;memcpy(ppc_ptr(w),old_windows+v*120,112);
        st32(w+4,144);stf32(w+0x4c,v?400:210);stf32(w+0x50,33);
        for(unsigned k=0;k<4;++k) {
            st16(w+0x5c+2*k,16);st32(w+112+4*k,128+4*k);
            st16(w+128+4*k,2);st8(w+130+4*k,flips[k]);
        }
        st8(w+0x64,4);st8(w+10,255);
    }
    st32(MAGIC_ARC+0x2c,0x2f00+1056);st32(MAGIC_ARC+8,0x8000);
    uint32_t tex=MAGIC_ARC+0x6000,trailer=tex+4096;
    st32(MAGIC_ARC+0x38,0x5f00);st32(MAGIC_ARC+0x3c,0x6f28);
    memset(ppc_ptr(tex),0,4096+40);
    for(unsigned y=0;y<16;++y)for(unsigned x=0;x<16;++x) {
        uint32_t pixel=tex+(y*64+x)*4;
        st8(pixel,16);st8(pixel+1,40);st8(pixel+2,16);st8(pixel+3,x<8 || y<8?255:0);
    }
    st32(trailer,0x464c494d);st32(trailer+20,0x696d6167);
    st16(trailer+28,16);st16(trailer+30,16);st8(trailer+34,9);st8(trailer+35,1);st32(trailer+36,4096);
    for(unsigned i=0;i<4;++i) {
        unsigned v=i&1;st8(SAVE+0x33,v?32:0);st8(SAVE+0x34,0);
        magic_test_sx=i<2?1:0.75f;magic_test_sy=i<2?1:1.25f;
        stf32(MAGIC_PANE+0x3c,v?386:196);stf32(MAGIC_PANE+0x48,magic_test_sx);stf32(MAGIC_PANE+0x5c,magic_test_sy);
        stf32(MAGIC_PANE+0x54,-640+10+(v?193:98)*magic_test_sx);
        magic_expect_draws(3);expect_hud("4/0/255 BORDER");
    }
    /* Native textured frames use the texture dimensions even with zero or
     * oversized fallback fields. Previously zero aborted at wnd-zero-slice
     * and oversized fields aborted before decoding any texture. */
    for(unsigned mode=0;mode<2;++mode) {
        for(unsigned v=0;v<2;++v)for(unsigned k=0;k<4;++k)st16(wnd+v*144+0x5c+2*k,mode?65535:0);
        for(unsigned v=0;v<2;++v) {
            st8(SAVE+0x33,v?32:0);stf32(MAGIC_PANE+0x3c,v?386:196);
            stf32(MAGIC_PANE+0x54,-640+10+(v?193:98)*magic_test_sx);
            magic_expect_draws(3);expect_hud("BORDER");
            expect_hud(mode?"65535/16,65535/16,65535/16,65535/16":"0/16,0/16,0/16,0/16");
        }
    }
    /* Failures retain precise, opt-in diagnostics; the core gauge survives. */
    strcpy((char*)ppc_ptr(txl+20),"lost.bflim");
    magic_expect_draws(2);expect_hud("wnd-texture-missing");expect_hud("file=lost.bflim");
    strcpy((char*)ppc_ptr(txl+20),"frame.bflim");
    st32(mat+116+36,2);magic_expect_draws(2);expect_hud("wnd-material-maps");expect_hud("maps=2");
    st32(mat+116+36,1);
    /* Reproduce the device's 0.1.33 report: 94x16 frame slices, outer
     * 210x33, flags 3, with opaque pixels inside the gauge. These valid
     * frame images must render behind the base/fill rather than be rejected.
     * The synthetic texture is deliberately opaque throughout. */
    magic_opaque_frame=magic_layer_check=1;magic_check_fraction=1;
    trailer=tex+8192;memset(ppc_ptr(tex),0,8192+40);
    for(unsigned y=0;y<16;++y)for(unsigned x=0;x<94;++x) {
        uint32_t pixel=tex+(y*128+x)*4;
        st8(pixel,16);st8(pixel+1,40);st8(pixel+2,16);st8(pixel+3,255);
    }
    st32(trailer,0x464c494d);st32(trailer+20,0x696d6167);
    st16(trailer+28,94);st16(trailer+30,16);st8(trailer+34,9);st8(trailer+35,1);st32(trailer+36,8192);
    st32(MAGIC_ARC+0x3c,0x7f28);st32(MAGIC_ARC+8,0x9000);
    for(unsigned v=0;v<2;++v) {
        uint32_t w=wnd+v*144;st8(w+0x65,3);
        st16(w+0x5c,94);st16(w+0x5e,94);st16(w+0x60,16);st16(w+0x62,16);
    }
    for(unsigned i=0;i<4;++i) {
        unsigned v=i&1;st8(SAVE+0x33,v?32:0);
        magic_test_sx=i<2?1:0.75f;magic_test_sy=i<2?1:1.25f;
        stf32(MAGIC_PANE+0x3c,v?386:196);stf32(MAGIC_PANE+0x48,magic_test_sx);stf32(MAGIC_PANE+0x5c,magic_test_sy);
        stf32(MAGIC_PANE+0x54,-640+10+(v?193:98)*magic_test_sx);
        magic_expect_draws(3);expect_hud("BORDER");expect_hud("94/94,94/94,16/16,16/16");
        /* Empty, half and full charge retain frame/base. A cropped fill must
         * always follow the base, including after a cached HUD redraw. */
        free_mode=0;enabled=0;++revision;tick();enabled=1;++revision;tick();
        magic_expect_draws(2);
        for(unsigned k=0;k<10;++k)hit(1);
        magic_check_fraction=0.5f;magic_expect_draws(3);
        for(unsigned k=0;k<10;++k)hit(1);
        magic_check_fraction=1;magic_expect_draws(3);
        int writes=magic_writes;read_hud();read_hud();assert(magic_writes==writes);
        free_mode=1;++revision;tick();
    }
    magic_opaque_frame=magic_layer_check=0;
    st32(MAGIC_ARC+0x38,0x300);st32(MAGIC_ARC+0x3c,0x728);
    magic_named_outer=0;magic_test_sx=magic_test_sy=1;
    memcpy(ppc_ptr(f),original,sizeof original);st8(SAVE+0x33,16);
    stf32(MAGIC_PANE+0x48,1);stf32(MAGIC_PANE+0x5c,1);stf32(MAGIC_PANE+0x3c,196);
    stf32(MAGIC_PANE+0x54,-640+10+98);
    /* Flash animation membership does not discard the selected frame's
     * static slices. Unrelated flash panes remain excluded in test_magic_style. */
    uint32_t group=f+1008;st32(group,0x67727031);st32(group+4,60);
    strcpy((char*)ppc_ptr(group+8),"G_Flash_00");st16(group+32,1);
    strcpy((char*)ppc_ptr(group+36),"W_MagicBase_00");st16(f+16,9);st32(MAGIC_ARC+0x2c,0x2f00+1068);
    magic_expect_draws(3);expect_hud("BORDER");
    memcpy(ppc_ptr(f),original,sizeof original);st32(MAGIC_ARC+0x2c,0x2f00+1008);
    /* Solid frame material may also extend inside: layering keeps the
     * native background and red fill above it. */
    st16(wnd+0x5c,90);st16(wnd+0x5e,90);magic_expect_draws(3);
    magic_opaque_frame=1;st32(mat+116+36,0);magic_expect_draws(3);expect_hud("BORDER");
    magic_opaque_frame=0;
    magic_named_layout=0;free_mode=0;++revision;tick();fixture_magic();st8(SAVE+0x33,0);
    puts("Dark Link real pane names: independent capacity, preserved base colour, hidden frame, flash and opening checks passed");
}
static void test_magic(void) {
    fixture_magic();
    /* Invalid resource is rejected with no texture handles or fake rectangle. */
    st32(MAGIC_FLYT+24,0xfffffffc); ++magic_generation;
    magic_draws=0; read_hud(); assert(!magic_draws);
    fixture_magic(); ++magic_generation; read_hud();
    hook(0x02675414,3,MAGIC_CTRL,0);
    magic_draws=0; read_hud(); assert(magic_draws==2);
    assert(magic_width>90 && magic_width<=96 && magic_x==62 && magic_y==98);
    int prior=magic_textures; read_hud(); assert(magic_textures==prior); /* reuse handles */
    int cached_writes=magic_writes;
    for(int load=0;load<3;++load) {++magic_generation;read_hud();hook(0x02675414,3,MAGIC_CTRL,0);read_hud();}
    assert(magic_writes==cached_writes && magic_draws>0);
    /* A failed write is attempted once per epoch, never every 60 HUD frames. */
    for(int i=0;i<64;++i) {free(magic_cache[i].data);memset(&magic_cache[i],0,sizeof(magic_cache[i]));}
    magic_write_failure=1;++magic_generation;read_hud();cached_writes=magic_writes;
    for(int frame=0;frame<181;++frame) read_hud();
    assert(magic_writes==cached_writes);
    magic_write_failure=0;++magic_generation;read_hud();hook(0x02675414,3,MAGIC_CTRL,0);read_hud();
    puts("Magic PNG cache: state-load reuse and no repeated failed writes passed");
    /* A coloured MagicBase is the full-width fill, not the smaller highlight. */
    magic_base_fill=1;st32(MAGIC_FLYT+20+20+32,0x20ff20ff);++magic_generation;
    read_hud();hook(0x02675414,3,MAGIC_CTRL,0);read_hud();
    assert(magic_width>95 && magic_width<=100 && magic_x==60);
    magic_base_fill=0;fixture_magic();++magic_generation;
    read_hud();hook(0x02675414,3,MAGIC_CTRL,0);read_hud();
    /* Maximum magic doubles duration; current magic never drains Dark Link. */
    st8(SAVE+0x33,32); st8(SAVE+0x34,0); read_hud(); assert(magic_width>90);
    for(int i=0;i<300;++i) tick();
    read_hud(); assert(magic_width>64 && magic_width<73);
    float remaining_width=magic_width;st8(SAVE+0x34,32);read_hud();assert(magic_width==remaining_width);
    st8(SAVE+0x34,0);
    st8(SAVE+0x33,0);
    ++magic_generation; magic_draws=0; read_hud(); assert(!magic_draws); /* fresh native position required */
    hook(0x02675414,3,MAGIC_CTRL,0); read_hud(); assert(magic_draws==2);
    magic_authored=1; ++magic_generation; magic_draws=0; read_hud();
    assert(magic_draws==2 && magic_x==62 && magic_y==98); /* before magic unlock */
    magic_authored=0; hook(0x02675414,3,MAGIC_CTRL,0);
    st8(MAGIC_PANE+0x47,5); /* native origin top-left */
    hook(0x02675414,3,MAGIC_CTRL,0); read_hud(); assert(magic_x==112 && magic_y==106);
    st8(MAGIC_PANE+0x47,0); hook(0x02675414,3,MAGIC_CTRL,0);
    /* Native authored half-width fill becomes full-width at frame 16;
     * its texture scale changes too. Free mode must cover the complete bar. */
    magic_animated=1;free_mode=1;++revision;tick();fixture_magic();fixture_animation(16);++magic_generation;
    read_hud();hook(0x02675414,3,MAGIC_CTRL,0);read_hud();assert(magic_width==96 && magic_x==62);
    expect_hud("Full=1/0");expect_hud("Fill=100%");
    /* Native FLTS scales may reverse UV order or extend past [0,1].
     * Replay these actual commands through the real HUD Store after this test. */
    uint32_t scale_key=MAGIC_ARC+0xc00+84+112+36+12+12+16;
    const float scales[]={-2,4,0};
    for(unsigned i=0;i<sizeof scales/sizeof scales[0];++i) {
        stf32(scale_key,scales[i]);++magic_generation;
        read_hud();hook(0x02675414,3,MAGIC_CTRL,0);magic_draws=0;read_hud();
        assert(magic_draws==2 && magic_width==96);expect_hud("Dark Link");
    }
    /* Unsupported rotation and non-finite UVs omit only the gauge. */
    uint32_t scale_entry=MAGIC_ARC+0xc00+84+112+36+12;
    st8(scale_entry+1,2);stf32(scale_key,45);++magic_generation;
    read_hud();hook(0x02675414,3,MAGIC_CTRL,0);magic_draws=0;read_hud();
    assert(!magic_draws);expect_hud("Dark Link");
    fixture_magic();fixture_animation(16);
    st32(MAGIC_FLYT+392+0x68,0x7fc00000);++magic_generation;
    magic_draws=0;read_hud();assert(!magic_draws);expect_hud("Dark Link");
    fixture_magic();fixture_animation(16);
    /* Malformed keys cannot partially apply an animation or read outside it. */
    magic_bad_animation=1;st32(MAGIC_ARC+0xc00+84+28+36+12+8,0xfffffff0);++magic_generation;
    read_hud();hook(0x02675414,3,MAGIC_CTRL,0);read_hud();assert(magic_width==48);
    expect_hud("Full=0/0");
    magic_bad_animation=0;magic_long=1;st8(SAVE+0x33,32);fixture_magic();fixture_animation(32);
    stf32(MAGIC_FLYT+256+0x4c,200);stf32(MAGIC_PANE+0x3c,200);++magic_generation;
    read_hud();hook(0x02675414,3,MAGIC_CTRL,0);read_hud();assert(magic_width==196);
    expect_hud("Full=0/1");
    magic_long=0;st8(SAVE+0x33,0);
    magic_animated=magic_bad_animation=0;free_mode=0;++revision;tick();fixture_magic();++magic_generation;
    read_hud();hook(0x02675414,3,MAGIC_CTRL,0);read_hud();
    magic_bad_geometry=1;++magic_generation;read_hud();hook(0x02675414,3,MAGIC_CTRL,0);
    magic_draws=0;read_hud();assert(magic_draws==1);expect_hud("Dark Link");
    magic_bad_geometry=0;++magic_generation;read_hud();hook(0x02675414,3,MAGIC_CTRL,0);read_hud();
    magic_window=1;fixture_window();++magic_generation;read_hud();hook(0x02675414,3,MAGIC_CTRL,0);
    magic_draws=0;read_hud();assert(magic_draws==3);
    expect_hud("pic/wnd=3/1");expect_hud("BORDER");
    /* Diagnostic toggling does not rebuild images or modify source resources. */
    uint8_t source_before[0x5000];memcpy(source_before,ppc_ptr(MAGIC_ARC),sizeof source_before);
    int writes_before=magic_writes;diagnostics=0;++revision;tick();read_hud();
    assert(!strstr(hud_text,"Gauge ") && magic_writes==writes_before);
    diagnostics=1;++revision;tick();read_hud();
    assert(strstr(hud_text,"Gauge ") && magic_writes==writes_before);
    assert(!memcmp(source_before,ppc_ptr(MAGIC_ARC),sizeof source_before));
    for(unsigned count=4;count<=8;count+=4) {
        magic_frame_count=count;fixture_window();++magic_generation;read_hud();hook(0x02675414,3,MAGIC_CTRL,0);
        magic_draws=0;read_hud();assert(magic_draws==3);
    }
    /* A malformed frame is omitted without invalidating the gauge/button. */
    st32(MAGIC_ARC+0x3000+608+0x6c,0xfffffff0);++magic_generation;
    read_hud();hook(0x02675414,3,MAGIC_CTRL,0);magic_draws=0;read_hud();assert(magic_draws==2);
    expect_hud("wnd-frame-table");
    test_magic_style();
    test_magic_hidden_native();
    test_magic_picture_layers();
    test_magic_named_layout();
    magic_window=0;fixture_magic();++magic_generation;read_hud();
    /* Reset the charge/state exercised above, then return a full duration
     * for the subsequent gameplay regression. */
    enabled=0;++revision;tick();enabled=1;++revision;tick();buttons(0);
    for(int i=0;i<20;++i) hit(1);
    buttons(0x60000); tick(); assert(damage(3)==6);
}
static void test_aura(void) {
    st32(LINK+0x5700,AURA); st16(AURA+0x2a,1); st32(AURA+0x34,AURA_MAT);
    st32(AURA_MAT,AURA_RES); st32(AURA_RES,0x464d4154);
    st32(AURA_RES+0x20,AURA_RS-(AURA_RES+0x20));
    st32(AURA_RS,7); st32(AURA_RS+0x14,0xcc0100); st32(AURA_RS+0x1c,0x20010101);
    for(int i=0;i<2;++i) {
        uint32_t b=LINK+(i ? 0x5704:0x5614),a=AURA_ANM+i*0x100,v=AURA_VALUE+i*0x100;
        st32(b+0x10,a); st32(b+0x20,v); st16(b+0xa,0); /* native init(play=FALSE): no playback limit */
        st32(a+4,AURA_ANM+0x300);st32(AURA_ANM+0x314,0x02000008);
        stf32(a,0); stf32(b+4,0); stf32(v,0); st32(v+0x10,0x02000004);
    }
    uint8_t original_save[0x40]; memcpy(original_save,ppc_ptr(SAVE),sizeof original_save);
    uint32_t flags=ld32(LINK+0x3bc); unsigned timer=ld16(LINK+0x699e);
    hook(0x0240b7f4,2,LINK,0);
    assert((float)ldf32(AURA_ANM)>30 && ld32(AURA_RS+0x1c)==0x20010101);
    assert(ld32(LINK+0x3bc)==flags && ld16(LINK+0x699e)==timer);
    hook(0x0240b7f4,3,LINK,0); assert((float)ldf32(AURA_ANM)==0);
    hook(0x023dbbe4,2,LINK,0); assert((float)ldf32(AURA_ANM)>29);
    assert((float)ldf32(AURA_ANM+0x100)==15);
    assert((float)ldf32(LINK+0x5704+4)==15 && (float)ldf32(AURA_VALUE+0x100)==15);
    hook(0x023dbbe4,3,LINK,0); assert((float)ldf32(AURA_ANM)==0 && (float)ldf32(AURA_VALUE)==0);
    assert((float)ldf32(AURA_ANM+0x100)==0 && (float)ldf32(AURA_VALUE+0x100)==0);
    assert((float)ldf32(LINK+0x5704+4)==0);
    assert(aura_notifications>=6 && !memcmp(original_save,ppc_ptr(SAVE),sizeof original_save));
    shape_begin(AURA); assert((ld32(AURA_RS+0x1c)&0x1fff)==0x500);
    shape_end(); assert(ld32(AURA_RS+0x1c)==0x20010101);
    st32(AURA_RS+0x1c,0x504); /* native non-separate alpha uses RGB equation */
    shape_begin(AURA);
    assert((ld32(AURA_RS+0x1c)&0x1fff)==0x500);
    assert((ld32(AURA_RS+0x1c)&(1u<<29)) && ((ld32(AURA_RS+0x1c)>>16)&0x1fff)==0x504);
    shape_end(); assert(ld32(AURA_RS+0x1c)==0x504);
    /* Return native separate-alpha fixture before restoration assertions. */
    st32(AURA_RS+0x1c,0x20010101);
    st32(LINK+0x3bc,flags|1); stf32(AURA_ANM,12);
    hook(0x0240b7f4,2,LINK,0); assert((float)ldf32(AURA_ANM)==12);
    hook(0x0240b7f4,3,LINK,0); assert((float)ldf32(AURA_ANM)==12);
    /* Real armor keeps all three distinct pre-draw frame values. Repeated
     * draws sample the resource midpoint, never halve the previous result. */
    stf32(AURA_ANM+0x100,18); stf32(LINK+0x5704+4,17); stf32(AURA_VALUE+0x100,16);
    for(int draw=0;draw<3;++draw) {
        hook(0x023dbbe4,2,LINK,0);
        assert((float)ldf32(AURA_ANM)>29 && (float)ldf32(AURA_ANM+0x100)==15);
        assert(ld32(LINK+0x3bc)==(flags|1) && ld16(LINK+0x699e)==timer);
        hook(0x023dbbe4,3,LINK,0);
        assert((float)ldf32(AURA_ANM)==12 && (float)ldf32(AURA_ANM+0x100)==18);
        assert((float)ldf32(LINK+0x5704+4)==17 && (float)ldf32(AURA_VALUE+0x100)==16);
    }
    st32(LINK+0x3bc,flags);
    st32(AURA_RS,ld32(AURA_RS)|0x400); /* another mod's unrelated flag */
    st32(AURA_RS+0x1c,ld32(AURA_RS+0x1c)|0x80000000u);
    enabled=0; ++revision; tick();
    assert(ld32(AURA_RS)==0x407 && ld32(AURA_RS+0x14)==0xcc0100 && ld32(AURA_RS+0x1c)==0xa0010101);
    hook(0x023dbbe4,2,LINK,0); assert((float)ldf32(AURA_ANM)==12);
    assert((float)ldf32(AURA_ANM+0x100)==18 && (float)ldf32(AURA_VALUE+0x100)==16);
    hook(0x023dbbe4,3,LINK,0);
    st32(LINK+0x5700,0); enabled=1; ++revision; tick();
    buttons(0); for(int i=0;i<20;++i) hit(1); buttons(0x60000); tick(); assert(damage(3)==6);
}
