/* Decode the player's resident BFLIM, apply its BFLYT black/white colours and
 * cache a red copy in this mod's Data folder. No art is bundled or uploaded.
 * Formats/tiling mirror runtime/src/mods/{game_layouts,lyt_build}.cpp. */
#pragma once
#define DL_ART_PIXELS 131072u
static u8 dl_art_pixels[DL_ART_PIXELS*4+4097];
static u8 dl_art_png[DL_ART_PIXELS*4+4096];
static u32 dl_art_width,dl_art_height;
static u32 dl_art_be16(const u8* p) { return ((u32)p[0]<<8)|p[1]; }
static u32 dl_art_be32(const u8* p) { return ((u32)p[0]<<24)|((u32)p[1]<<16)|((u32)p[2]<<8)|p[3]; }
static void dl_art_put(u8* p,u32 v) { p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v; }
static u32 dl_art_bit(u32 x,u32 i) { return (x>>i)&1; }
static u32 dl_art_address(u32 x,u32 y,u32 pitch,u32 mode,u32 swizzle,u32 bits) {
    u32 bytes=bits/8;
    if(mode==1) return (x+pitch*y)*bytes;
    u32 i;
    if(bits==64) i=dl_art_bit(x,0)|(dl_art_bit(y,0)<<1)|(dl_art_bit(x,1)<<2)|(dl_art_bit(x,2)<<3)|(dl_art_bit(y,1)<<4)|(dl_art_bit(y,2)<<5);
    else if(bits==128) i=dl_art_bit(y,0)|(dl_art_bit(x,0)<<1)|(dl_art_bit(x,1)<<2)|(dl_art_bit(x,2)<<3)|(dl_art_bit(y,1)<<4)|(dl_art_bit(y,2)<<5);
    else i=dl_art_bit(x,0)|(dl_art_bit(x,1)<<1)|(dl_art_bit(y,0)<<2)|(dl_art_bit(x,2)<<3)|(dl_art_bit(y,1)<<4)|(dl_art_bit(y,2)<<5);
    if(mode==2) return i*bytes+64*bytes*((x>>3)+(pitch>>3)*(y>>3));
    u32 pipe=((y>>3)^(x>>3))&1,bank=(y>>4)&3;
    bank=(((bank>>1)|(bank<<1))^(x>>3))&3;
    u32 bp=(pipe+2*bank)^swizzle;
    u32 off=(((x/32)+(pitch/32)*(y/16))*32*16*bytes>>3)+i*bytes;
    return ((off&~255u)<<3)|(off&255)|((bp&1)<<8)|(((bp>>1)&3)<<9);
}
static void dl_art_565(u32 c,u8* p) {
    u32 r=(c>>11)&31,g=(c>>5)&63,b=c&31;
    p[0]=(r<<3)|(r>>2);p[1]=(g<<2)|(g>>4);p[2]=(b<<3)|(b>>2);p[3]=255;
}
static void dl_art_colour(const u8* b,int four,u8 px[16][4]) {
    u32 a=b[0]|((u32)b[1]<<8),c=b[2]|((u32)b[3]<<8);
    u8 p[4][4]; dl_art_565(a,p[0]);dl_art_565(c,p[1]);
    for(u32 k=0;k<3;++k) {
        p[2][k]=(four||a>c) ? (2*p[0][k]+p[1][k])/3 : (p[0][k]+p[1][k])/2;
        p[3][k]=(four||a>c) ? (p[0][k]+2*p[1][k])/3 : 0;
    }
    p[2][3]=255;p[3][3]=(four||a>c) ? 255:0;
    u32 indices=b[4]|((u32)b[5]<<8)|((u32)b[6]<<16)|((u32)b[7]<<24);
    for(u32 j=0;j<16;++j) for(u32 k=0;k<4;++k) px[j][k]=p[(indices>>(j*2))&3][k];
}
static void dl_art_channel(const u8* b,u8 out[16]) {
    u32 p[8]={b[0],b[1]};
    if(p[0]>p[1]) for(u32 j=1;j<7;++j) p[j+1]=((7-j)*p[0]+j*p[1])/7;
    else { for(u32 j=1;j<5;++j) p[j+1]=((5-j)*p[0]+j*p[1])/5; p[6]=0;p[7]=255; }
    /* Each code touches at most two bytes: avoid unsupported 64-bit shifts. */
    for(u32 j=0;j<16;++j) {
        u32 bit=j*3,byte=2+bit/8,code=b[byte]>>(bit&7);
        if((bit&7)>5 && byte<7) code|=(u32)b[byte+1]<<(8-(bit&7));
        out[j]=p[code&7];
    }
}
static int dl_art_decode(const u8* data,u32 size,int* srgb) {
    if(size<40) return 0;
    const u8* f=data+size-40;
    if(dl_art_be32(f)!=0x464c494d || dl_art_be32(f+20)!=0x696d6167) return 0;
    u32 w=dl_art_be16(f+28),h=dl_art_be16(f+30),code=f[34],mode=f[35]&15,swizzle=f[35]>>5,n=dl_art_be32(f+36);
    if(!w||!h||w>2048||h>2048||w*h>DL_ART_PIXELS||n>size-40||(mode!=1&&mode!=2&&mode!=4)) return 0;
    u32 bc=0,use=0;
    switch(code) {
        case 9:case 0x14:break;
        case 0xc:case 0x15:bc=1;break;
        case 0xd:case 0x16:bc=2;break;
        case 0xe:case 0x17:bc=3;break;
        case 0xf:bc=4;use=1;break;
        case 0x10:bc=4;use=2;break;
        case 0x11:bc=5;use=3;break;
        default:return 0;
    }
    u32 bits=bc ? ((bc==1||bc==4)?64:128):32,bytes=bits/8;
    u32 ew=bc?(w+3)/4:w,eh=bc?(h+3)/4:h;
    u32 align=mode==1?64:mode==2?8:32,pitch=(ew+align-1)/align*align;
    for(u32 y=0;y<eh;++y) for(u32 x=0;x<ew;++x) {
        u32 at=dl_art_address(x,y,pitch,mode,swizzle,bits);
        if(at>n||bytes>n-at) return 0;
        const u8* b=data+at;
        u8 px[16][4],r[16],g[16];
        if(!bc) { for(u32 k=0;k<4;++k) dl_art_pixels[(y*w+x)*4+k]=b[k];continue; }
        if(bc<=3) dl_art_colour(b+(bc==1?0:8),bc!=1,px);
        if(bc==3) { dl_art_channel(b,r);for(u32 j=0;j<16;++j) px[j][3]=r[j]; }
        if(bc==2) for(u32 j=0;j<16;++j) px[j][3]=((b[j/2]>>(4*(j&1)))&15)*17;
        if(bc>=4) {
            dl_art_channel(b,r); if(bc==5) dl_art_channel(b+8,g);
            for(u32 j=0;j<16;++j) { px[j][0]=px[j][1]=px[j][2]=use==2?255:r[j];px[j][3]=use==2?r[j]:use==3?g[j]:255; }
        }
        for(u32 j=0;j<16;++j) if(x*4+j%4<w&&y*4+j/4<h)
            for(u32 k=0;k<4;++k) dl_art_pixels[((y*4+j/4)*w+x*4+j%4)*4+k]=px[j][k];
    }
    dl_art_width=w;dl_art_height=h;*srgb=code>=0x14&&code<=0x17;return 1;
}
/* Math lookup, not artwork: exact sRGB byte -> linear conversion. */
#include "srgb_table.h"
static u8 dl_art_srgb(f32 value) {
    u32 lo=0,hi=255;
    while(lo<hi) { u32 mid=(lo+hi)/2; if(dl_srgb_linear[mid]<value) lo=mid+1;else hi=mid; }
    if(lo && value-dl_srgb_linear[lo-1]<dl_srgb_linear[lo]-value) --lo;
    return (u8)lo;
}
static void dl_art_material(u32 black,u32 white,int srgb) {
    /* Reproduce native black/white interpolation before vertex tinting and
     * recolouring, including the coverage of luminance-mask textures. */
    f32 low[4],high[4];
    for(u32 k=0;k<4;++k) { low[k]=((black>>(24-8*k))&255)/255.0f;high[k]=((white>>(24-8*k))&255)/255.0f; }
    for(u32 i=0;i<dl_art_width*dl_art_height;++i) for(u32 k=0;k<4;++k) {
        u8* p=&dl_art_pixels[i*4+k];
        f32 t=srgb&&k<3?dl_srgb_linear[*p]:*p/255.0f;
        f32 v=low[k]+(high[k]-low[k])*t;
        *p=k<3?dl_art_srgb(v):(u8)(v*255+0.5f);
    }
}
/* Vertex gradients are in pane space, after texture UV/SRT sampling. Bake
 * all four corners and then swap final R/G, preserving texture contrast,
 * neutral outlines, highlights and coverage instead of flattening to red. */
static void dl_art_vertices(const u32 vertex[4],int red,int empty) {
    for(u32 y=0;y<dl_art_height;++y) for(u32 x=0;x<dl_art_width;++x) {
        f32 fx=((f32)x+0.5f)/dl_art_width,fy=((f32)y+0.5f)/dl_art_height;
        u8* p=dl_art_pixels+(y*dl_art_width+x)*4;
        for(u32 c=0;c<4;++c) {
            u32 shift=24-8*c;
            f32 a=(vertex[0]>>shift)&255,b=(vertex[1]>>shift)&255;
            f32 d=(vertex[2]>>shift)&255,e=(vertex[3]>>shift)&255;
            f32 top=a+(b-a)*fx,bottom=d+(e-d)*fx,v=(top+(bottom-top)*fy)/255;
            p[c]=c<3?dl_art_srgb(dl_srgb_linear[p[c]]*v):(u8)(p[c]*v+0.5f);
        }
        if(red) { u8 r=p[0];p[0]=p[1];p[1]=r; }
        else if(empty && p[1]>p[0] && p[1]>p[2]) {
            /* Remove only the coloured fill from its empty copy. Neutral
             * frame pixels remain intact, including when base == fill. */
            p[0]=p[1]=p[2]=p[0]<p[2]?p[0]:p[2];
        }
    }
}
static u32 dl_art_crc(const u8* p,u32 n) {
    u32 crc=~0u;
    for(u32 i=0;i<n;++i) { crc^=p[i];for(u32 k=0;k<8;++k) crc=(crc>>1)^(0xedb88320u& (0u-(crc&1))); }
    return ~crc;
}
static u32 dl_art_chunk(u32 at,u32 tag,u32 n) {
    dl_art_put(dl_art_png+at,n);dl_art_put(dl_art_png+at+4,tag);
    dl_art_put(dl_art_png+at+8+n,dl_art_crc(dl_art_png+at+4,n+4));return at+n+12;
}
/* Bake native UV/SRT into an owned image. HUD rectangles require ordered UVs
 * in [0,1]; native samplers also allow outside/reversed coordinates.
 * Reuse the PNG buffer as scratch before encoding, without another allocation.
 * Rotation remains unsupported: omit that gauge image, never emit bad UVs. */
/* BFLYT sampler low two bits: clamp=0, repeat=1, mirror=2. Reference:
 * Switch-Toolbox Layout/CAFE/Materials/TextureRef.cs and Layout/Common.cs.
 * Resolve EACH bilinear tap so repeat seams blend opposite texture edges. */
static u32 dl_art_tap(int i,u32 size,u32 wrap) {
    if(!wrap) return i<0?0:(u32)i>=size?size-1:(u32)i;
    int period=(int)(wrap==2?size*2:size),v=i%period;if(v<0)v+=period;
    return wrap==2 && v>=(int)size?2*size-1-(u32)v:(u32)v;
}
static int dl_art_bake_sampler(const f32 uv[4],const f32 srt[5],u32 ws,u32 wt,u32 width,u32 height) {
    for(u32 i=0;i<4;++i) if(!(uv[i]>=-1000000 && uv[i]<=1000000)) return 0;
    for(u32 i=0;i<5;++i) if(!(srt[i]>=-1000000 && srt[i]<=1000000)) return 0;
    if(srt[2]!=0 || !dl_art_width || !dl_art_height || ws>2 || wt>2 ||
       !width || !height || width>2048 || height>2048 || width*height>DL_ART_PIXELS) return 0;
    for(u32 y=0;y<height;++y) for(u32 x=0;x<width;++x) {
        f32 u=uv[0]+(uv[2]-uv[0])*((f32)x+0.5f)/width;
        f32 v=uv[1]+(uv[3]-uv[1])*((f32)y+0.5f)/height;
        f32 sx=((u-0.5f)*srt[3]+0.5f+srt[0])*dl_art_width-0.5f;
        f32 sy=((v-0.5f)*srt[4]+0.5f+srt[1])*dl_art_height-0.5f;
        if(!(sx>=-100000000 && sx<=100000000 && sy>=-100000000 && sy<=100000000)) return 0;
        if(!ws) sx=sx<0?0:sx>dl_art_width-1?dl_art_width-1:sx;
        if(!wt) sy=sy<0?0:sy>dl_art_height-1?dl_art_height-1:sy;
        int ix=(int)sx,iy=(int)sy;if(sx<ix)--ix;if(sy<iy)--iy;
        u32 x0=dl_art_tap(ix,dl_art_width,ws),x1=dl_art_tap(ix+1,dl_art_width,ws);
        u32 y0=dl_art_tap(iy,dl_art_height,wt),y1=dl_art_tap(iy+1,dl_art_height,wt);
        f32 fx=sx-ix,fy=sy-iy;
        for(u32 c=0;c<4;++c) {
            f32 a=dl_art_pixels[(y0*dl_art_width+x0)*4+c],b=dl_art_pixels[(y0*dl_art_width+x1)*4+c];
            f32 d=dl_art_pixels[(y1*dl_art_width+x0)*4+c],e=dl_art_pixels[(y1*dl_art_width+x1)*4+c];
            f32 top=a+(b-a)*fx,bottom=d+(e-d)*fx;
            dl_art_png[(y*width+x)*4+c]=(u8)(top+(bottom-top)*fy+0.5f);
        }
    }
    dl_art_width=width;dl_art_height=height;
    memcpy(dl_art_pixels,dl_art_png,width*height*4);
    return 1;
}
static u32 dl_art_encode(void) {
    static const u8 sig[8]={137,80,78,71,13,10,26,10};
    for(u32 i=0;i<8;++i) dl_art_png[i]=sig[i];
    dl_art_put(dl_art_png+16,dl_art_width);dl_art_put(dl_art_png+20,dl_art_height);
    dl_art_png[24]=8;dl_art_png[25]=6;dl_art_png[26]=dl_art_png[27]=dl_art_png[28]=0;
    u32 at=dl_art_chunk(8,0x49484452,13),start=at+8,pos=start;
    dl_art_png[pos++]=0x78;dl_art_png[pos++]=1;
    u32 stride=dl_art_width*4+1,total=stride*dl_art_height,read=0,a=1,b=0;
    if(total+total/65535*5+128>sizeof dl_art_png) return 0;
    while(read<total) {
        u32 n=total-read;if(n>65535)n=65535;
        dl_art_png[pos++]=(read+n==total);dl_art_png[pos++]=n;dl_art_png[pos++]=n>>8;
        dl_art_png[pos++]=~n;dl_art_png[pos++]=(~n)>>8;
        for(u32 j=0;j<n;++j,++read) {
            u32 col=read%stride,row=read/stride;
            u8 v=col?dl_art_pixels[row*(stride-1)+col-1]:0;
            dl_art_png[pos++]=v;a=(a+v)%65521;b=(b+a)%65521;
        }
    }
    dl_art_put(dl_art_png+pos,(b<<16)|a);pos+=4;
    at=dl_art_chunk(at,0x49444154,pos-start);
    return dl_art_chunk(at,0x49454e44,0);
}
