// Replay actual emitted guest HUD records with the renderer's transaction rules.
// Synthetic images stand in for handles; no game assets or GPU are needed.
#include "mods/guest_hud.h"
#include <cassert>
#include <cstdio>
#include <cstring>
using namespace guestmods::hud;
static uint32_t word(const uint8_t* p) {
    return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|p[3];
}
static float real(const uint8_t* p) { uint32_t n=word(p);float f;std::memcpy(&f,&n,4);return f; }
int main(int argc,char** argv) {
    assert(argc==2);FILE* f=std::fopen(argv[1],"rb");assert(f);
    Store store;const std::string owner="dark-link";const uint8_t pixel[4]={255,255,255,255};
    Handle image=store.create_image(owner,1,1,4,pixel,4);assert(image);
    // Negative control: one invalid UV really does discard a valid DARK command.
    Handle list=store.begin(owner,0);Command button;button.kind=Command::Text;button.text="DARK";
    assert(store.append(owner,list,button));Command bad;bad.u0=-0.5f;
    assert(!store.picture(owner,list,image,bad));assert(!store.commit(owner,list));assert(store.snapshot(0).empty());
    list=0;unsigned frames=0,commands=0,pictures=0,dark_frames=0,count=0;bool dark=false;
    for(int tag;(tag=std::fgetc(f))!=EOF;) {
        if(tag=='B') { assert(!list);list=store.begin(owner,0);assert(list);dark=false;count=0;continue; }
        assert(list);
        if(tag=='E') {
            assert(store.commit(owner,list));auto published=store.snapshot(0);
            assert(count ? published.size()==1 && published[0]->commands.size()==count : published.empty());
            if(dark) { bool found=false;for(const auto& c:published[0]->commands) found|=c.text=="Dark Link";assert(found);++dark_frames; }
            ++frames;list=0;continue;
        }
        assert(tag=='C');uint8_t b[72];assert(std::fread(b,1,sizeof b,f)==sizeof b);
        Command c;c.kind=Command::Kind(word(b));c.anchor=Anchor(word(b+4));c.blend=Blend(word(b+8));
        c.x=real(b+12);c.y=real(b+16);c.w=real(b+20);c.h=real(b+24);c.size=real(b+28);
        c.thickness=real(b+32);c.rotation=real(b+36);c.u0=real(b+40);c.v0=real(b+44);c.u1=real(b+48);c.v1=real(b+52);c.rgba=word(b+56);
        if(c.kind==Command::Text) {
            uint32_t n=word(b+68);assert(n<2048);c.text.resize(n);assert(std::fread(c.text.data(),1,n,f)==n);dark|=c.text=="Dark Link";
        }
        if(c.kind==Command::Picture) { assert(word(b+60));assert(store.picture(owner,list,image,c));++pictures; }
        else assert(store.append(owner,list,c));
        ++count;++commands;
    }
    assert(!list && !std::ferror(f));std::fclose(f);
    assert(frames>20 && pictures>20 && dark_frames>20);
    std::printf("Dark Link runtime HUD validation passed: %u frames, %u commands, %u images, %u DARK frames\n",frames,commands,pictures,dark_frames);
}
