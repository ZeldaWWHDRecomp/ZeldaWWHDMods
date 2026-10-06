// Optional Valoo's Gratitude. Native assets; original quest/save flags are never changed.
#include "dragon_quest.h"
#include "dragon.h"
#include "runtime.h"
#include "input.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
namespace dragon::quest {
namespace {
constexpr uint32_t play=0x1046F0B0, medliTag=0x44510000;
struct Vec { float x,y,z; };
constexpr Vec lookout{202100,2562,-199900};
constexpr Vec chimes[]={{197720,94,-199470},{201800,2562,-199900},{209430,1900,-202600}};
struct Actor { uint32_t id=~0u,address=0; unsigned hit=0; };
Actor medli, gongs[3];
// 0 unavailable, 1 invitation, 2 restoration, 3 ready to learn, 4 learned.
unsigned phase=0, mask=0, cooldown=0, toast=0;
int slot=-1, dialog=0;
thread_local bool creatingMedli=false;
std::mutex uiMutex; Hud ui;
uint32_t call(Cpu* c,uint32_t fn,std::initializer_list<uint32_t> args,std::initializer_list<float> floats={}) {
 Cpu sub=*c; sub.r[1]-=0x100; st32(sub.r[1],c->r[1]); unsigned r=3,f=1;
 for(auto a:args) sub.r[r++]=a; for(auto a:floats) sub.f[f++].ps0=a;
 sub.pc=fn; ppc_dispatch(&sub); return sub.r[3];
}
Vec pos(uint32_t a) { return {(float)ldf32(a+0x314),(float)ldf32(a+0x318),(float)ldf32(a+0x31C)}; }
void vec(uint32_t a,Vec v) { stf32(a,v.x);stf32(a+4,v.y);stf32(a+8,v.z); }
float distance(Vec a,Vec b) { return std::sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z)); }
bool sea() { return !std::memcmp(ppc_ptr(play+0x5134),"sea\0",4) && !ld8(play+0x514C); }
bool roost(uint32_t p) { return sea() && (int8_t)ld8(p+0x326)==13; }
bool tagMd(uint32_t a) { return a && ld32(a+0xB0)==medliTag; }
int tagGong(uint32_t a) { uint32_t v=a?ld32(a+0xB0):0; return v>medliTag && v<=medliTag+3 ? (int)(v-medliTag-1) : -1; }
std::filesystem::path path() { return std::filesystem::path(config::save_dir)/("dragon-quest-slot"+std::to_string(slot)+".txt"); }
void persist() {
 if(slot<0 || slot>2) return;
 auto target=path(), temporary=target; temporary+=".tmp";
 std::ofstream out(temporary); out<<"WWHD_DRAGON_QUEST 1\n"<<phase<<' '<<mask<<'\n'; out.close();
 std::error_code error;
 if(out) std::filesystem::rename(temporary,target,error);
 if(!out || error) LOG("[dragon-quest] progress write failed for slot %d",slot);
}
void load(int number) {
 slot=number; phase=mask=0; dialog=0; toast=0;
 std::ifstream in(path()); std::string magic; unsigned version=0,p=0,m=0;
 if(in>>magic>>version>>p>>m && magic=="WWHD_DRAGON_QUEST" && version==1 && p<=4 && m<=7 && (p<2 ? m==0 : p==2 || m==7)) { phase=p==2 && m==7 ? 3 : p;mask=m; }
 LOG("[dragon-quest] loaded slot %d phase %u chimes %u",slot,phase,mask);
}
void publish(Hud next) {
 std::lock_guard<std::mutex> lock(uiMutex);
 if(next.visible==ui.visible && next.title==ui.title && next.line1==ui.line1 && next.line2==ui.line2 && next.line3==ui.line3 && next.action==ui.action) return;
 next.revision=ui.revision+1; ui=std::move(next);
}
int profile(uint32_t create) {
 for(int i=0;i<0x200;++i) { uint32_t p=ld32(0x101F3EE0+i*4); if(p<0x10000000||p>0x11000000) continue;
  uint32_t m=ld32(p+0x24);if(m>=0x10000000&&m<0x11000000&&ld32(m)==create)return i; }
 return -1;
}
void spawn(Cpu* c,Actor& a,uint32_t create,uint32_t parameter,Vec position) {
 if(a.id!=~0u) return; int p=profile(create);if(p<0)return;
 uint32_t t=c->r[1]-0x80;vec(t,position);st16(t+16,0);st16(t+18,0);st16(t+20,0);vec(t+32,{1,1,1});
 a.id=call(c,0x025D5834,{(uint32_t)p,parameter,t,13,t+16,t+32,0xFF,0});
 LOG("[dragon-quest] spawn %X id %u",parameter,a.id);
}
std::string notes() {
 return std::string(mask&1?"↑ →":"? ?")+"   "+(mask&2?"↑ ←":"? ?")+"   "+(mask&4?"↓ →":"? ?");
}
}
Hud hud() { std::lock_guard<std::mutex> lock(uiMutex);return ui; }
void collect(Cpu* c,PpcFunc original) {
 if(creatingMedli && c->r[4]==0 && c->r[5]==2) {c->r[3]=0;return;}
 original(c);
}
void card_load(Cpu* c,PpcFunc original) {
 int number=c->r[5]; original(c);
 if(enabled() && c->r[3]==0 && number>=0 && number<3) load(number);
}
void card_new(Cpu* c,PpcFunc original) {
 int number=c->r[5];original(c);
 if(enabled() && number>=0 && number<3) { int previous=slot;unsigned p=phase,m=mask;slot=number;phase=mask=0;persist();slot=previous;phase=p;mask=m; }
}
bool song_allowed(uint32_t player) { return phase==4 || (phase==3 && !dialog && roost(player) && medli.address && distance(pos(player),lookout)<650); }
void learned() {
 if(phase!=3)return;phase=4;mask=7;persist();toast=150;
 LOG("[dragon-quest] Call of the Sky learned permanently in slot %d",slot);
}
bool tick(Cpu* c,uint32_t p,uint32_t pressed,bool idle) {
 uint32_t save=ld32(0x101F84DC);
 if(!save || !sea()) { publish({});dialog=0;return false; }
 bool leaf=false;for(unsigned i=0;i<21;++i)leaf|=ld8(save+0x5C+i)==0x34;
 if(phase==0 && slot>=0 && leaf && call(c,0x02520A84,{3})) { phase=1;persist();LOG("[dragon-quest] Rito invitation arrived"); }
 if(toast) --toast;
 if(cooldown) --cooldown;
 if(phase>=1 && roost(p) && !cooldown) {
  spawn(c,medli,0x02286B58,medliTag,lookout);
  if(phase>=2) for(int i=0;i<3;++i)spawn(c,gongs[i],0x0234C308,medliTag+1+i,chimes[i]);
  cooldown=30;
 }
 bool consumed=dialog!=0;
 if(idle && !ld8(play+0x5292)) {
  if(!dialog && phase==1 && (pressed&input::kLeft)) dialog=1;
  if(!dialog && roost(p) && medli.address && distance(pos(p),lookout)<450 && (pressed&input::kA)) dialog=phase==1?2:phase==2?3:phase==3?4:5;
  if(dialog && (pressed&input::kB)) dialog=0;
  else if(consumed && (pressed&input::kA)) {
   if(dialog==2) {phase=2;persist();LOG("[dragon-quest] restoration accepted");}
   dialog=0;
  }
  // A genuine sword-cut animation, in reach and facing the visible gong.
  if(phase==2 && !dialog) {
   uint32_t proc=ld32(p+0x65F0);float frame=(float)ldf32(p+0x589C);
   if(proc>=0x41 && proc<=0x4A && frame>=4 && frame<=15) {
    Vec root=pos(p); float yaw=(int16_t)ld16(p+0x32A)*3.14159265358979323846f/32768;
    for(int i=0;i<3;++i) {
     Vec v=chimes[i];float dx=v.x-root.x,dz=v.z-root.z;
     if(!(mask&(1<<i)) && gongs[i].address && std::fabs(v.y-root.y)<220 && dx*dx+dz*dz<400*400 && std::sin(yaw)*dx+std::cos(yaw)*dz>0) {
      mask|=1<<i;gongs[i].hit=35;uint32_t morf=ld32(gongs[i].address+0x3B4);if(morf)stf32(morf+0x9C,0);
      call(c,0x0206A590,{gongs[i].address,0x69D3,0});toast=120;
      LOG("[dragon-quest] chime %d restored; mask %u",i+1,mask);
      if(mask==7) {phase=3;LOG("[dragon-quest] full melody found; return to Medli");}
      persist(); // Commit the last chime and ready-to-learn phase together.
     }
    }
   }
  }
 }
 consumed|=dialog!=0;
 Hud h;h.visible=idle;h.title="VALOO’S GRATITUDE";
 if(dialog==1) {h.title="A LETTER FROM MEDLI";h.line1="Valoo has not forgotten your kindness.";h.line2="Come to the high entrance on Dragon Roost.";h.line3="There is something he wishes to give you.";h.action="A  Close letter     B  Close";}
 else if(dialog==2) {h.title="MEDLI";h.line1="Three ancient wind chimes have fallen silent.";h.line2="Find them by the harbor, cliff and offshore lookout.";h.line3="Strike each with your sword to reveal its two notes.";h.action="A  Accept side quest     B  Leave";}
 else if(dialog==3) {h.title="MEDLI";h.line1="Listen to the wind chimes around the island.";h.line2="Climb, grapple and glide to reach all three.";h.line3=notes();h.action="A  Continue     B  Close";}
 else if(dialog==4) {h.title="MEDLI · CALL OF THE SKY";h.line1="Those notes are Valoo’s promise to you.";h.line2="Conduct them here, and he will answer.";h.line3="↑ → ↑ ← ↓ → · Six beats (left stick →)";h.action="A  Continue, then open your Wind Waker";}
 else if(dialog==5) {h.title="MEDLI";h.line1="If you need his wings, let the wind carry your song.";h.line2="Valoo will answer your call over the Great Sea.";h.action="A  Continue     B  Close";}
 else if(phase==0) {h.line1="Help Valoo and obtain the Deku Leaf.";h.line2="An optional adventure awaits on Dragon Roost.";}
 else if(phase==1) {h.line1="A Rito invitation has arrived.";h.line2="Medli awaits at Dragon Roost’s high entrance.";h.action="D-pad Left  Read letter · Near Medli: A  Talk";}
 else if(phase==2) {h.line1="Restore the harbor, cliff and lookout chimes.";h.line2=notes();h.action="Strike a chime with your sword · Return to Medli";}
 else if(phase==3) {h.line1="Return to Medli at Dragon Roost’s high entrance.";h.line2="Play ↑ → ↑ ← ↓ → there to learn the song.";h.action="Near Medli: A  Talk · Wind Waker: six beats";}
 else { h.visible=idle && roost(p) && medli.address && distance(pos(p),lookout)<450;h.line1="Call of the Sky learned!";h.action="A  Talk to Medli"; }
 publish(h); return consumed;
}
void gong_create(Cpu* c,PpcFunc original) {
 uint32_t a=c->r[3];int i=enabled()?tagGong(a):-1;original(c);
 if(i<0)return;
 if(c->r[3]==4) {gongs[i].address=a;st32(a+0x39C,0);uint32_t m=ld32(a+0x3B4);stf32(m+0x98,0);stf32(m+0x9C,0);LOG("[dragon-quest] chime %d ready",i+1);}
 else if(c->r[3]==5) {gongs[i]=Actor{};LOG("[dragon-quest] chime %d load failed",i+1);}
}
void gong_execute(Cpu* c,PpcFunc original) {
 uint32_t a=c->r[3];int i=enabled()?tagGong(a):-1;if(i<0){original(c);return;}
 uint32_t m=ld32(a+0x3B4);if(m)stf32(m+0x98,gongs[i].hit?1:0);
 if(gongs[i].hit)--gongs[i].hit;
 Cpu saved=*c;call(c,0x025E535C,{m,0,0,0});call(c,0x0234BFC4,{a});*c=saved;c->r[3]=1;
}
void gong_delete(Cpu* c,PpcFunc original) {int i=enabled()?tagGong(c->r[3]):-1;original(c);if(i>=0)gongs[i]=Actor{};}
void medli_create(Cpu* c,PpcFunc original) {
 uint32_t a=c->r[3];if(!enabled()||!tagMd(a)){original(c);return;}
 // The extra actor has its own idle behavior; bypass story-stage availability only for it.
 char stage[8];std::memcpy(stage,ppc_ptr(play+0x5134),8);std::memcpy(ppc_ptr(play+0x5134),"DQuest\0\0",8);
 uint32_t old=ld32(0x101CEF74);uint8_t flags[4];std::memcpy(flags,ppc_ptr(0x101D5F3E),4);creatingMedli=true;original(c);creatingMedli=false;std::memcpy(ppc_ptr(play+0x5134),stage,8);st32(0x101CEF74,old);std::memcpy(ppc_ptr(0x101D5F3E),flags,4);
 if(c->r[3]==4) {medli.address=a;st32(a+0x39C,0);LOG("[dragon-quest] extra Medli ready");}
 else if(c->r[3]==5){medli=Actor{};LOG("[dragon-quest] extra Medli load failed");}
}
void medli_execute(Cpu* c,PpcFunc original) {
 uint32_t a=c->r[3];if(!enabled()||!tagMd(a)){original(c);return;}
 Cpu saved=*c;call(c,0x025E65FC,{ld32(a+0x618),a+0x37C,0,0});call(c,0x02285A6C,{a});*c=saved;c->r[3]=1;
}
void medli_delete(Cpu* c,PpcFunc original) {
 bool tagged=enabled()&&tagMd(c->r[3]);uint32_t old=tagged?ld32(0x101CEF74):0;uint8_t flags[4];if(tagged)std::memcpy(flags,ppc_ptr(0x101D5F3E),4);original(c);
 if(tagged){st32(0x101CEF74,old);std::memcpy(ppc_ptr(0x101D5F3E),flags,4);medli=Actor{};}
}
}
