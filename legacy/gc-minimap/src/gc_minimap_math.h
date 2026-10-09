#pragma once
#include <algorithm>
#include <cmath>
namespace gc_minimap {
inline int sea_room(float x,float z) {
 if(!std::isfinite(x)||!std::isfinite(z)||x < -350000 || x>=350000 || z < -350000 || z>=350000)return 0;
 int gx=(int)std::floor(x/100000+.5f),gz=(int)std::floor(z/100000+.5f);
 return (gz+3)*7+gx+4;
}
struct Point { float x=0,z=0;bool valid=false,clipped=false; };
inline Point project(float x,float z,const float* b) {
 for(int i=0;i<4;i++)if(!std::isfinite(b[i]))return {};
 if(!std::isfinite(x)||!std::isfinite(z)||b[2]<=b[0]||b[3]<=b[1])return {};
 float u=(x-b[0])/(b[2]-b[0]),v=(z-b[1])/(b[3]-b[1]);
 return {std::clamp(u,.025f,.975f),std::clamp(v,.025f,.975f),true,u<.025f||u>.975f||v<.025f||v>.975f};
}
}
