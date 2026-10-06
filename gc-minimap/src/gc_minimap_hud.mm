// MOD_GC_MINIMAP: native prototype backend; the console GX2 backend is separate work.
#import <Metal/Metal.h>
#include "gc_minimap.h"
#include "gc_minimap_math.h"
#include "cemu_shim.h"
#include "Cafe/HW/Latte/LatteAddrLib/LatteAddrLib.h"
#include <cstdlib>
#include <fstream>
#include <string>
#include <iterator>
#include <unordered_map>
namespace {
const char* shader=R"(
#include <metal_stdlib>
using namespace metal;
struct U { float4 rect; float2 marker; float yaw; float pad; };
struct V { float4 pos [[position]]; float2 uv; };
vertex V map_vs(uint id [[vertex_id]],constant U& u [[buffer(0)]]) {
 float2 q=float2(id&1,id>>1);V v;v.pos=float4(mix(u.rect.xy,u.rect.zw,q),0,1);v.uv=float2(q.x,1-q.y);return v;
}
fragment float4 map_fs(V v [[stage_in]],constant U& u [[buffer(0)]],texture2d<float> a [[texture(0)]],texture2d<float> b [[texture(1)]],texture2d<float> c [[texture(2)]],texture2d<float> d [[texture(3)]]) {
 constexpr sampler s(filter::linear,address::clamp_to_edge);
 float2 uv=(v.uv-0.10)/0.80;
 float4 out=float4(0);
 if(all(uv>=0) && all(uv<=1)) {
  if(u.pad>.5) {
   float4 tex=a.sample(s,uv);float opacity=tex.a*.80;
   out=float4(tex.rgb*opacity,opacity);
  } else {
  // HD BC5 layers store luminance in R and coverage in G. Treating
  // both channels as masks loses the inked terrain edges.
  float2 layers[4]={a.sample(s,uv).rg,b.sample(s,uv).rg,c.sample(s,uv).rg,d.sample(s,uv).rg};
  float3 light[4]={float3(.51,1,.51),float3(.36,.76,.29),float3(.24,.58,.17),float3(.11,.40,.08)};
  float3 ink=float3(.02,.23,.03);
  float3 col=float3(.06,.12,.66);
  float coverage=0;
  for(int i=0;i<4;i++) {
   float alpha=clamp(layers[i].y,0.0,1.0);
   col=mix(col,mix(ink,light[i],layers[i].x),alpha);
   coverage=alpha+coverage*(1-alpha);
  }
  // Convert the GC-inspired display palette to linear light for Metal's
  // sRGB scan target. Sea remains translucent; terrain stays legible.
  col=select(col/12.92,pow((col+.055)/1.055,float3(2.4)),col>.04045);
  float opacity=mix(.56,.84,coverage);
  out=float4(col*opacity,opacity);
  }
  float border=min(min(uv.x,1-uv.x),min(uv.y,1-uv.y));
  if(border<.015) out=float4(.64,.72,.83,.92);
  float2 p=(uv-u.marker)*float2(1,-1);
  float cs=cos(u.yaw),sn=sin(u.yaw);p=float2(cs*p.x-sn*p.y,sn*p.x+cs*p.y);
  if(p.y>-.023 && p.y<.039 && abs(p.x)<(.039-p.y)*.47) out=float4(p.y>0?float3(1,.88,.05):float3(.95,.18,.05),1);
 }
 // Outlined edge arrows, positioned like the GC frame.
 float2 p=v.uv-float2(.50,.045);
 bool arrow=p.y>-.025 && p.y<.025 && abs(p.x)<(.025+p.y);
 float2 p2=v.uv-float2(.045,.50);p2=float2(p2.y,p2.x);
 bool left=p2.y>-.025 && p2.y<.025 && abs(p2.x)<(.025+p2.y);
 float2 p3=v.uv-float2(.955,.50);p3=float2(p3.y,-p3.x);
 bool right=p3.y>-.025 && p3.y<.025 && abs(p3.x)<(.025+p3.y);
 if(arrow||left||right) {
  float2 q=arrow?p:(left?p2:p3);
  float edge=min(.025-q.y,(.025+q.y-abs(q.x))*.7071);
  out=edge<.005?float4(.78,.84,.91,.9):float4(.03,.06,.20,.6);
 }
 return out;
}
)";
uint16_t be16(const std::vector<uint8_t>& b,size_t i){return (b.at(i)<<8)|b.at(i+1);}
uint32_t be32(const std::vector<uint8_t>& b,size_t i){return (uint32_t(be16(b,i))<<16)|be16(b,i+2);}
// Local chart cache made by tools/gc-minimap-assets.py from the player's own game files.
// WWHD_GC_MINIMAP_CACHE selects it; the default is ~/Library/Application Support/wwhd/gc-minimap.
std::string cache_dir() {
 if(const char* env=std::getenv("WWHD_GC_MINIMAP_CACHE"))return env;
 const char* home=std::getenv("HOME");
 return std::string(home?home:".")+"/Library/Application Support/wwhd/gc-minimap";
}
struct Chart { id<MTLTexture> layers[4]={nil,nil,nil,nil};float bounds[4]={};bool ready=false,rgba=false; };
struct State { id<MTLDevice> dev=nil;id<MTLLibrary> lib=nil;std::unordered_map<NSUInteger,id<MTLRenderPipelineState>> pipes;std::unordered_map<int,Chart> charts; } state;
Chart load(id<MTLDevice> dev,int room) {
 Chart result;std::string dir=cache_dir();
 char name[64];snprintf(name,sizeof name,"/room-%02d.bounds",room);std::ifstream bounds(dir+name);
 if(!(bounds>>result.bounds[0]>>result.bounds[1]>>result.bounds[2]>>result.bounds[3]) || result.bounds[2]<=result.bounds[0] || result.bounds[3]<=result.bounds[1])return result;
 // Prefer registered vector traces. Original GC is the faithful fallback;
 // HD chart layers remain available when no GC-derived texture is present.
 for(const char* suffix : {"vector-rgba", "gc-rgba"}) {
  snprintf(name,sizeof name,"/room-%02d.%s",room,suffix);
  std::ifstream file(dir+name,std::ios::binary);
  std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});
  if(bytes.size()<12 || std::memcmp(bytes.data(),"GCM1",4))continue;
  unsigned w=be32(bytes,4),h=be32(bytes,8);
  if(!w || !h || w>2048 || h>2048 || bytes.size()!=12+size_t(w)*h*4)continue;
  auto desc=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm_sRGB width:w height:h mipmapped:NO];desc.storageMode=MTLStorageModeShared;
  result.layers[0]=[dev newTextureWithDescriptor:desc];
  if(!result.layers[0])continue;
  [result.layers[0] replaceRegion:MTLRegionMake2D(0,0,w,h) mipmapLevel:0 withBytes:bytes.data()+12 bytesPerRow:w*4];
  for(int i=1;i<4;i++)result.layers[i]=result.layers[0];
  result.rgba=true;result.ready=true;
  fprintf(stderr,"[gc-minimap] %s chart ready: room %d (%ux%u)\n",suffix,room,w,h);return result;
 }
 for(int layer=0;layer<4;layer++) {
  snprintf(name,sizeof name,"/room-%02d-%d.bflim",room,layer);std::ifstream file(dir+name,std::ios::binary);
  std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),{});
  if(data.size()<40)return result;size_t f=data.size()-40;
  if(std::memcmp(data.data()+f,"FLIM",4) || std::memcmp(data.data()+f+20,"imag",4) || data[f+34]!=0x11)return result;
  unsigned w=be16(data,f+28),h=be16(data,f+30),packed=data[f+35],size=be32(data,f+36);
  if(!w || !h || w>1024 || h>1024 || w%4 || h%4 || size>f)return result;
  unsigned tile=packed&31,swizzle=((packed>>5)&7)<<8;
  if(tile<1 || tile>16)return result;
  LatteAddrLib::AddrSurfaceInfo_OUT info{};
  LatteAddrLib::GX2CalculateSurfaceInfo((Latte::E_GX2SURFFMT)0x35,w,h,1,Latte::E_DIM::DIM_2D,Latte::MakeGX2TileMode((Latte::E_HWTILEMODE)tile),0,0,&info);
  auto mode=info.hwTileMode;unsigned bw=w/4,bh=h/4;std::vector<uint8_t> blocks(bw*bh*16);
  for(unsigned y=0;y<bh;y++)for(unsigned x=0;x<bw;x++) {
   unsigned off;
   if(mode==Latte::E_HWTILEMODE::TM_LINEAR_GENERAL || mode==Latte::E_HWTILEMODE::TM_LINEAR_ALIGNED) off=LatteAddrLib::ComputeSurfaceAddrFromCoordLinear(x,y,0,0,128,info.pitch,info.height,1);
   else if(!Latte::TM_IsMacroTiled(mode))off=LatteAddrLib::ComputeSurfaceAddrFromCoordMicroTiled(x,y,0,128,info.pitch,info.height,mode,false);
   else off=LatteAddrLib::ComputeSurfaceAddrFromCoordMacroTiled(x,y,0,0,128,info.pitch,info.height,1,mode,false,(swizzle>>8)&1,(swizzle>>9)&3);
   if(off+16>size)return result;std::memcpy(blocks.data()+(y*bw+x)*16,data.data()+off,16);
  }
  auto desc=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBC5_RGUnorm width:w height:h mipmapped:NO];desc.storageMode=MTLStorageModeShared;
  result.layers[layer]=[dev newTextureWithDescriptor:desc];
  if(!result.layers[layer])return result;
  [result.layers[layer] replaceRegion:MTLRegionMake2D(0,0,w,h) mipmapLevel:0 withBytes:blocks.data() bytesPerRow:bw*16];
 }
 result.ready=true;fprintf(stderr,"[gc-minimap] HD terrain chart ready: room %d\n",room);return result;
}
}
namespace gc_minimap {
void draw_hud(id<MTLCommandBuffer> cmd,id<MTLTexture> target) {
 if(!enabled() || !target || !(target.usage&MTLTextureUsageRenderTarget))return;
 auto snap=snapshot();if(!snap.visible)return;
 if(state.dev!=cmd.device) {state=State{};state.dev=cmd.device;NSError* error=nil;state.lib=[cmd.device newLibraryWithSource:[NSString stringWithUTF8String:shader] options:nil error:&error];if(!state.lib)NSLog(@"[gc-minimap] shader: %@",error);}
 if(!state.lib)return;
 auto it=state.charts.find(snap.room);if(it==state.charts.end())it=state.charts.emplace(snap.room,load(cmd.device,snap.room)).first;
 auto& chart=it->second;if(!chart.ready)return;
 auto& pipe=state.pipes[target.pixelFormat];
 if(!pipe){auto desc=[MTLRenderPipelineDescriptor new];desc.vertexFunction=[state.lib newFunctionWithName:@"map_vs"];desc.fragmentFunction=[state.lib newFunctionWithName:@"map_fs"];auto a=desc.colorAttachments[0];a.pixelFormat=target.pixelFormat;a.blendingEnabled=YES;a.sourceRGBBlendFactor=MTLBlendFactorOne;a.destinationRGBBlendFactor=MTLBlendFactorOneMinusSourceAlpha;a.sourceAlphaBlendFactor=MTLBlendFactorZero;a.destinationAlphaBlendFactor=MTLBlendFactorOne;NSError* error=nil;pipe=[cmd.device newRenderPipelineStateWithDescriptor:desc error:&error];if(!pipe)NSLog(@"[gc-minimap] pipeline: %@",error);}
 if(!pipe)return;
 float w=target.width,h=target.height,side=150*h/480,x=4*h/480,y=h-side-12*h/480;
 auto marker=project(snap.x,snap.z,chart.bounds);if(!marker.valid)return;
 struct {float rect[4],marker[2],yaw,pad;} u={{2*x/w-1,1-2*(y+side)/h,2*(x+side)/w-1,1-2*y/h},{marker.x,marker.z},3.141592654f-snap.yaw*6.283185307f/65536,chart.rgba?1.0f:0.0f};
 auto pass=[MTLRenderPassDescriptor renderPassDescriptor];pass.colorAttachments[0].texture=target;pass.colorAttachments[0].loadAction=MTLLoadActionLoad;pass.colorAttachments[0].storeAction=MTLStoreActionStore;
 auto enc=[cmd renderCommandEncoderWithDescriptor:pass];[enc setRenderPipelineState:pipe];[enc setVertexBytes:&u length:sizeof u atIndex:0];[enc setFragmentBytes:&u length:sizeof u atIndex:0];for(int i=0;i<4;i++)[enc setFragmentTexture:chart.layers[i] atIndex:i];[enc drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];[enc endEncoding];
}
}
