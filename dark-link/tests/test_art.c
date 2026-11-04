/* Pixel/PNG regressions use synthetic data, never Nintendo textures. */
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
typedef uint8_t u8;typedef uint32_t u32;typedef float f32;
#include "magic_art.h"
static u8 texture[1100];
static void fixture(u32 code,u32 n) {
    memset(texture,0,sizeof texture);
    u8* f=texture+n;dl_art_put(f,0x464c494d);dl_art_put(f+20,0x696d6167);
    f[29]=4;f[31]=4;f[34]=code;f[35]=1;dl_art_put(f+36,n);
}
int main(int argc,char** argv) {
    assert(argc==2);int srgb=0;
    fixture(0x10,512);texture[0]=255;texture[1]=0;
    /* BC4-alpha: first alpha 255, next alpha 0, then six interpolated values. */
    const u8 codes[6]={0x88,0xc6,0xfa,0x88,0xc6,0xfa};memcpy(texture+2,codes,6);
    assert(dl_art_decode(texture,552,&srgb));
    const u8 expected[8]={255,0,218,182,145,109,72,36};
    for(u32 i=0;i<16;++i) { assert(dl_art_pixels[i*4]==255);assert(dl_art_pixels[i*4+3]==expected[i%8]); }
    dl_art_material(0x00000000,0xff2020ff,srgb);
    for(u32 i=0;i<16;++i) { assert(dl_art_pixels[i*4]==255);assert(dl_art_pixels[i*4+1]<100);assert(dl_art_pixels[i*4+3]==expected[i%8]); }
    u32 n=dl_art_encode();assert(n>100);
    FILE* f=fopen(argv[1],"wb");assert(f);assert(fwrite(dl_art_png,1,n,f)==n);fclose(f);
    assert(!dl_art_decode(texture,39,&srgb));
    texture[512+35]=3;assert(!dl_art_decode(texture,552,&srgb));
    fixture(0xc,512);texture[0]=0;texture[1]=0xf8; /* BC1 red, code 0 */
    assert(dl_art_decode(texture,552,&srgb));assert(dl_art_pixels[0]==255&&dl_art_pixels[1]==0&&dl_art_pixels[3]==255);
    fixture(0x11,1024);texture[0]=180;texture[8]=90; /* BC5 luminance+alpha */
    assert(dl_art_decode(texture,1064,&srgb));assert(dl_art_pixels[0]==180&&dl_art_pixels[1]==180&&dl_art_pixels[3]==90);
    fixture(9,1024);texture[0]=10;texture[1]=20;texture[2]=30;texture[3]=40;
    assert(dl_art_decode(texture,1064,&srgb));assert(dl_art_pixels[0]==10&&dl_art_pixels[3]==40);
    /* Known GX2 microtile addresses; test each ordering and macro bank bits. */
    assert(dl_art_address(1,0,8,2,0,64)==8);
    assert(dl_art_address(0,1,8,2,0,64)==16);
    assert(dl_art_address(1,0,8,2,0,128)==32);
    assert(dl_art_address(0,1,8,2,0,128)==16);
    assert(dl_art_address(8,0,32,4,0,32)==768);
    assert(dl_art_address(0,0,32,4,1,32)==256);
    /* Independent tiny ramp: identity, mirror, edge clamping and interpolation.
     * Channels include alpha so transformed coverage is checked as well. */
    dl_art_width=4;dl_art_height=1;
    const f32 uv[4]={0,0,1,1};f32 srt[5]={0,0,0,1,1};
    for(u32 i=0;i<4;++i) for(u32 c=0;c<4;++c) dl_art_pixels[i*4+c]=i*80;
    assert(dl_art_bake_sampler(uv,srt,0,0,dl_art_width,dl_art_height));
    for(u32 i=0;i<4;++i) assert(dl_art_pixels[i*4+3]==i*80);
    srt[3]=-1;assert(dl_art_bake_sampler(uv,srt,0,0,dl_art_width,dl_art_height));
    for(u32 i=0;i<4;++i) assert(dl_art_pixels[i*4+3]==(3-i)*80);
    srt[3]=-1;assert(dl_art_bake_sampler(uv,srt,0,0,dl_art_width,dl_art_height)); /* restore ramp */
    srt[3]=2;assert(dl_art_bake_sampler(uv,srt,0,0,dl_art_width,dl_art_height));
    const u8 stretched[4]={0,40,200,240};
    for(u32 i=0;i<4;++i) assert(dl_art_pixels[i*4+3]==stretched[i]);
    srt[0]=4;assert(dl_art_bake_sampler(uv,srt,0,0,dl_art_width,dl_art_height));
    for(u32 i=0;i<4;++i) assert(dl_art_pixels[i*4+3]==240);
    srt[2]=45;assert(!dl_art_bake_sampler(uv,srt,0,0,dl_art_width,dl_art_height));
    srt[2]=0;srt[0]=1e30f;assert(!dl_art_bake_sampler(uv,srt,0,0,dl_art_width,dl_art_height));
    u32 nanbits=0x7fc00000;memcpy(&srt[0],&nanbits,4);assert(!dl_art_bake_sampler(uv,srt,0,0,dl_art_width,dl_art_height));
    /* A four-texel native repeat becomes two visible segments, not a flat
     * edge colour or an aliased four-pixel output. Negative UVs wrap as well. */
    const f32 repeat_uv[4]={-1,0,1,1},mirror_uv[4]={0,0,2,1},identity[5]={0,0,0,1,1};
    for(u32 mode=1;mode<=2;++mode) {
        dl_art_width=4;dl_art_height=1;
        for(u32 i=0;i<4;++i) for(u32 c=0;c<4;++c) dl_art_pixels[i*4+c]=20+i*60;
        assert(dl_art_bake_sampler(mode==1?repeat_uv:mirror_uv,identity,mode,0,8,1));
        for(u32 i=0;i<8;++i) {
            u32 index=mode==1?i%4:i<4?i:7-i;
            assert(dl_art_pixels[i*4]==20+index*60 && dl_art_pixels[i*4+3]==20+index*60);
        }
    }
    dl_art_width=4;dl_art_height=1;
    for(u32 i=0;i<4;++i) for(u32 c=0;c<4;++c) dl_art_pixels[i*4+c]=i*80;
    const f32 seam_uv[4]={0,0,0,1};
    assert(dl_art_bake_sampler(seam_uv,identity,1,0,1,1));assert(dl_art_pixels[0]==120);
    assert(!dl_art_bake_sampler(uv,identity,3,0,4,1));
    /* Recolouring retains contrast, neutral frame pixels and alpha. */
    dl_art_width=4;dl_art_height=1;
    const u8 coloured[16]={20,80,20,255,40,180,40,200,120,120,120,128,255,255,255,0};
    const u32 white[4]={0xffffffff,0xffffffff,0xffffffff,0xffffffff};
    memcpy(dl_art_pixels,coloured,16);dl_art_vertices(white,1,0);
    const u8 red[16]={80,20,20,255,180,40,40,200,120,120,120,128,255,255,255,0};
    assert(!memcmp(dl_art_pixels,red,16));
    memcpy(dl_art_pixels,coloured,16);dl_art_vertices(white,0,1);
    assert(dl_art_pixels[0]==20 && dl_art_pixels[1]==20 && dl_art_pixels[8]==120 && dl_art_pixels[11]==128);
    const u32 gradient[4]={0xffffffff,0x40404040,0xffffffff,0x40404040};
    memset(dl_art_pixels,255,16);dl_art_vertices(gradient,1,0);
    assert(dl_art_pixels[0]>dl_art_pixels[12] && dl_art_pixels[3]>dl_art_pixels[15]);
    puts("Dark Link native artwork pixel tests passed");
}
