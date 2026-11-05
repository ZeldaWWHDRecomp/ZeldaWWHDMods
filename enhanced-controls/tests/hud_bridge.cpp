/* SPDX-License-Identifier: MPL-2.0
 * Validate the translated mod's actual commands with upstream's real HUD store.
 * Tiny placeholder textures avoid requiring a renderer or PNG decoder here. */
#include "mods/guest_hud.h"
#include <cassert>
#include <cstdio>
using namespace guestmods::hud;
static Store hud;
static uint64_t current_epoch;
static const std::string owner="enhanced-controls";
extern "C" {
void test_hud_reset(void) { hud.reset(); current_epoch=0; }
uint32_t test_hud_begin(uint64_t step,uint64_t epoch) {
    if(epoch!=current_epoch) { hud.reset(); current_epoch=epoch; }
    hud.set_step(step);
    uint32_t list=hud.begin(owner,0); assert(list); return list;
}
uint32_t test_hud_texture(void) {
    const uint8_t pixel[]={255,255,255,255};
    uint32_t image=hud.create_image(owner,1,1,4,pixel,sizeof(pixel));
    assert(image); return image;
}
void test_hud_release(uint32_t image) { assert(hud.release(owner,image)); }
int test_hud_emit(uint32_t list,uint32_t kind,uint32_t anchor,uint32_t blend,
                  const float* f,uint32_t rgba,uint32_t image,const char* text,uint32_t bytes) {
    Command c;
    c.kind=Command::Kind(kind); c.anchor=Anchor(anchor); c.blend=Blend(blend);
    c.x=f[0]; c.y=f[1]; c.w=f[2]; c.h=f[3]; c.size=f[4]; c.thickness=f[5];
    c.rotation=f[6]; c.u0=f[7]; c.v0=f[8]; c.u1=f[9]; c.v1=f[10]; c.rgba=rgba;
    if(kind==Command::Text) c.text.assign(text,bytes);
    bool ok=kind==Command::Picture?hud.picture(owner,list,image,std::move(c)):hud.append(owner,list,std::move(c));
    if(!ok) std::fprintf(stderr,"%s (kind=%u, thickness=%g)\n",hud.error(owner).c_str(),kind,f[5]);
    return ok;
}
void test_hud_end(uint32_t list,unsigned commands) {
    bool ok=hud.commit(owner,list);
    if(!ok) std::fprintf(stderr,"%s\n",hud.error(owner).c_str());
    assert(ok);
    auto published=hud.snapshot(0);
    assert(commands ? published.size()==1 && published[0]->commands.size()==commands : published.empty());
}
}
