#include "gc_minimap_math.h"
#include <cassert>
#include <limits>
using namespace gc_minimap;
int main() {
 assert(sea_room(-200000,300000)==44); // Outset
 assert(sea_room(200000,-200000)==13); // Dragon Roost
 assert(sea_room(49999,0)==25 && sea_room(50000,0)==26);
 assert(sea_room(-350000,-350000)==1 && sea_room(350000,0)==0);
 assert(sea_room(std::numeric_limits<float>::quiet_NaN(),0)==0);
 float bounds[]={-100,-200,100,200};
 auto center=project(0,0,bounds);assert(center.valid && !center.clipped && center.x==.5f && center.z==.5f);
 auto edge=project(1000,-1000,bounds);assert(edge.valid && edge.clipped && edge.x==.975f && edge.z==.025f);
 bounds[2]=bounds[0];assert(!project(0,0,bounds).valid);
}
