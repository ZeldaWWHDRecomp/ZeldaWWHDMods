// MOD_GC_MINIMAP: read-only guest snapshot, no calls or writes to game state.
#include "gc_minimap.h"
#include "gc_minimap_math.h"
#include "ppc.h"
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <mutex>
#include <cstdio>
namespace gc_minimap {
namespace { std::mutex lock; Snapshot current; }
bool enabled() {
 static bool on=[] {
  auto e=std::getenv("WWHD_MOD_GC_MINIMAP");bool v=e && !std::strcmp(e,"1");
  if(v)fprintf(stderr,"[gc-minimap] mod enabled (WWHD_MOD_GC_MINIMAP=1)\n");
  return v;
 }();
 return on;
}
void capture(uint32_t p) {
 if(!enabled()) return;
 Snapshot s;
 constexpr uint32_t play=0x1046F0B0;
 static bool logged=false;if(!logged){logged=true;fprintf(stderr,"[gc-minimap] first capture player %08x current %08x stage %.8s layer %u event %u\n",p,ld32(play+0x5B2C),ppc_ptr(play+0x5134),ld8(play+0x514C),ld8(play+0x5292));}
 if(p && p==ld32(play+0x5B2C) && (!std::strcmp((const char*)ppc_ptr(play+0x5134),"sea") || !std::strcmp((const char*)ppc_ptr(play+0x5134),"sea_T") || !std::strcmp((const char*)ppc_ptr(play+0x5134),"sea_E")) && !ld8(play+0x514C)) {
  s.x=ldf32(p+0x314);s.z=ldf32(p+0x31C);s.yaw=(int16_t)ld16(p+0x32A);
  s.room=sea_room(s.x,s.z);s.visible=s.room!=0 && !ld8(play+0x5292);
 }
 std::lock_guard guard(lock);current=s;
}
Snapshot snapshot() { std::lock_guard guard(lock);return current; }
}
