// MOD_DRAGON: generated text UI only; no game assets or guest-state writes.
#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#include "dragon.h"
#include "dragon_quest.h"
#include <unordered_map>
#include <algorithm>
#include <cmath>
namespace {
const char* shader=R"(
#include <metal_stdlib>
using namespace metal;
struct V { float4 pos [[position]]; float2 uv; };
vertex V dragon_vs(uint id [[vertex_id]], constant float4& rect [[buffer(0)]]) {
 float2 q=float2(id&1,id>>1); V v;
 v.pos=float4(mix(rect.xy,rect.zw,q),0,1); v.uv=float2(q.x,1-q.y); return v;
}
fragment float4 dragon_fs(V v [[stage_in]],texture2d<float> t [[texture(0)]]) {
 constexpr sampler s(filter::linear,address::clamp_to_edge); return t.sample(s,v.uv);
}
)";
struct Panel {
 id<MTLDevice> device=nil; id<MTLLibrary> library=nil; id<MTLTexture> texture=nil;
 std::unordered_map<NSUInteger,id<MTLRenderPipelineState>> pipelines;
 int phase=-99, altitude=-99999, minimum=-1, notes=-1; bool conducting=false; unsigned questRevision=~0u; int height=160;
};
Panel panel;
void update(id<MTLDevice> dev,dragon::Hud state) {
 auto quest=dragon::quest::hud(); bool showQuest=quest.visible && state.phase==0;
 int altitude=(int)std::lround(state.altitude), minimum=(int)std::ceil(state.minimum);
 if(panel.texture && panel.phase==state.phase && panel.altitude==altitude && panel.minimum==minimum && panel.notes==state.notes && panel.conducting==state.conducting && panel.questRevision==quest.revision) return;
 panel.questRevision=quest.revision; panel.height=showQuest?220:160; int ph=panel.height;
 panel.phase=state.phase; panel.altitude=altitude; panel.minimum=minimum; panel.notes=state.notes; panel.conducting=state.conducting;
 const char* titles[]={"VALOO · READY","VALOO · ARRIVING","VALOO · APPROACHING","VALOO · AUTO GRAPPLE","VALOO · LIFTING YOU","VALOO · DRAGON VIEW","VALOO · DEPARTING","CALL OF THE SKY"};
 NSString* title=[NSString stringWithUTF8String:titles[std::clamp(state.phase,0,7)]];
 NSString* controls=state.phase==5 ? @"Left stick  ← / →  Turn    ↑ / ↓  Climb / dive" : state.phase==0 ? @"Wind Waker   Call of the Sky: ↑ → ↑ ← ↓ →" : @"Automatic Grappling Hook pickup, then lift";
 NSString* action=state.phase==5 ? @"A   Release     Then press your Deku Leaf button" : @"Flight controls unlock after the lift";
 if(state.phase==0) action=state.conducting ? [NSString stringWithFormat:@"Hold left stick right for 6 beats · Notes %d / 6",state.notes] : @"L + D-pad Up opens the Wind Waker · Use right stick";
 if(state.phase==7) { controls=@"Call of the Sky · Valoo hears your call"; action=@"Conducting, then automatic hook pickup"; }
 NSString* height=[NSString stringWithFormat:@"Altitude %d    Flight min %d / max %.0f",std::max(0,altitude),minimum,state.maximum];
 NSString* line2=@"", *line3=@"";
 if(showQuest) {title=[NSString stringWithUTF8String:quest.title.c_str()];controls=[NSString stringWithUTF8String:quest.line1.c_str()];line2=[NSString stringWithUTF8String:quest.line2.c_str()];line3=[NSString stringWithUTF8String:quest.line3.c_str()];action=[NSString stringWithUTF8String:quest.action.c_str()];}
 NSBitmapImageRep* image=[[NSBitmapImageRep alloc] initWithBitmapDataPlanes:nil pixelsWide:560 pixelsHigh:ph bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace bytesPerRow:2240 bitsPerPixel:32];
 [NSGraphicsContext saveGraphicsState];
 [NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithBitmapImageRep:image]];
 [[NSColor colorWithCalibratedRed:0.025 green:0.055 blue:0.08 alpha:0.88] setFill];
 [[NSBezierPath bezierPathWithRoundedRect:NSMakeRect(0,0,560,ph) xRadius:12 yRadius:12] fill];
 NSDictionary* heading=@{NSFontAttributeName:[NSFont boldSystemFontOfSize:22],NSForegroundColorAttributeName:[NSColor colorWithCalibratedRed:0.55 green:0.94 blue:0.85 alpha:1]};
 NSDictionary* body=@{NSFontAttributeName:[NSFont systemFontOfSize:18],NSForegroundColorAttributeName:NSColor.whiteColor};
 [title drawAtPoint:NSMakePoint(18,ph-38) withAttributes:heading];
 [controls drawAtPoint:NSMakePoint(18,ph-72) withAttributes:body];
 if(showQuest) { [line2 drawAtPoint:NSMakePoint(18,ph-104) withAttributes:body]; [line3 drawAtPoint:NSMakePoint(18,ph-136) withAttributes:body]; [action drawAtPoint:NSMakePoint(18,20) withAttributes:@{NSFontAttributeName:[NSFont systemFontOfSize:16],NSForegroundColorAttributeName:NSColor.whiteColor}]; }
 else { [action drawAtPoint:NSMakePoint(18,54) withAttributes:body]; [height drawAtPoint:NSMakePoint(18,20) withAttributes:body]; }
 [NSGraphicsContext restoreGraphicsState];
 // Allocate a fresh texture: earlier command buffers can still be sampling the previous panel.
 auto desc=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm_sRGB width:560 height:ph mipmapped:NO];
 desc.usage=MTLTextureUsageShaderRead; desc.storageMode=MTLStorageModeShared;
 panel.texture=[dev newTextureWithDescriptor:desc];
 [panel.texture replaceRegion:MTLRegionMake2D(0,0,560,ph) mipmapLevel:0 withBytes:image.bitmapData bytesPerRow:2240];
}
}
namespace dragon {
void draw_hud(id<MTLCommandBuffer> cmd,id<MTLTexture> target) {
 if(!enabled() || !target || !(target.usage&MTLTextureUsageRenderTarget)) return;
 Hud state=hud(); if(state.phase<0) return;
 if(panel.device!=cmd.device) {
  panel=Panel{}; panel.device=cmd.device;
  NSError* error=nil;
  panel.library=[cmd.device newLibraryWithSource:[NSString stringWithUTF8String:shader] options:nil error:&error];
  if(!panel.library) { NSLog(@"[dragon] HUD shader: %@",error); return; }
 }
 if(!panel.library) return;
 auto& pipe=panel.pipelines[target.pixelFormat];
 if(!pipe) {
  auto desc=[MTLRenderPipelineDescriptor new];
  desc.vertexFunction=[panel.library newFunctionWithName:@"dragon_vs"]; desc.fragmentFunction=[panel.library newFunctionWithName:@"dragon_fs"];
  auto attachment=desc.colorAttachments[0]; attachment.pixelFormat=target.pixelFormat; attachment.blendingEnabled=YES;
  attachment.sourceRGBBlendFactor=MTLBlendFactorOne; attachment.destinationRGBBlendFactor=MTLBlendFactorOneMinusSourceAlpha;
  attachment.sourceAlphaBlendFactor=MTLBlendFactorZero; attachment.destinationAlphaBlendFactor=MTLBlendFactorOne;
  NSError* error=nil; pipe=[cmd.device newRenderPipelineStateWithDescriptor:desc error:&error];
  if(!pipe) { NSLog(@"[dragon] HUD pipeline: %@",error); return; }
 }
 update(cmd.device,state);
 float scale=target.height/720.0f, w=target.width,h=target.height;
 float x=24*scale,y=h-(panel.height+24)*scale,pw=560*scale,ph=panel.height*scale;
 float rect[]={2*x/w-1,1-2*(y+ph)/h,2*(x+pw)/w-1,1-2*y/h};
 auto pass=[MTLRenderPassDescriptor renderPassDescriptor]; pass.colorAttachments[0].texture=target;
 pass.colorAttachments[0].loadAction=MTLLoadActionLoad;pass.colorAttachments[0].storeAction=MTLStoreActionStore;
 auto encoder=[cmd renderCommandEncoderWithDescriptor:pass];
 [encoder setRenderPipelineState:pipe];[encoder setVertexBytes:rect length:sizeof rect atIndex:0];
 [encoder setFragmentTexture:panel.texture atIndex:0];[encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];[encoder endEncoding];
}
}
