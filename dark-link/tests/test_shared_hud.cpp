#include "mods/guest_hud.h"
#include <cassert>
#include <cstring>
using namespace guestmods::hud;
static Store hud;
static const std::string owners[]={"dark-link","enhanced-controls"};
static uint32_t word(const unsigned char* p) {return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|p[3];}
static float real(const unsigned char* p) {uint32_t n=word(p);float f;std::memcpy(&f,&n,4);return f;}
extern "C" {
void shared_reset(){hud.reset();}
uint32_t shared_begin(int owner,uint64_t step){hud.set_step(step);return hud.begin(owners[owner],0);}
uint32_t shared_texture(int owner){const uint8_t pixel[]={255,255,255,255};return hud.create_image(owners[owner],1,1,4,pixel,4);}
void shared_release(int owner,uint32_t image){assert(hud.release(owners[owner],image));}
int shared_emit(int owner,uint32_t list,const unsigned char* b,const char* text){
    Command c;c.kind=Command::Kind(word(b));c.anchor=Anchor(word(b+4));c.blend=Blend(word(b+8));
    c.x=real(b+12);c.y=real(b+16);c.w=real(b+20);c.h=real(b+24);c.size=real(b+28);
    c.thickness=real(b+32);c.rotation=real(b+36);c.u0=real(b+40);c.v0=real(b+44);c.u1=real(b+48);c.v1=real(b+52);c.rgba=word(b+56);
    if(c.kind==Command::Text)c.text.assign(text,word(b+68));
    return c.kind==Command::Picture?hud.picture(owners[owner],list,word(b+60),c):hud.append(owners[owner],list,c);
}
void shared_end(int owner,uint32_t list){assert(hud.commit(owners[owner],list));}
}
