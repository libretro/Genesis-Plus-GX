#import <Cocoa/Cocoa.h>
#import <OpenGL/OpenGL.h>
#import <Metal/Metal.h>
#include <bridge.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char kernel_source[] = R"MSL(
#include <metal_stdlib>
using namespace metal;
struct Params { uint width, height, tw, th, columns, rows; };
uint raw(device const ushort *a, int x, int y) { return a[y*2048+x]; }
float3 palette(device const ushort *a,uint i,int y) {
 uint v=raw(a,1408+int(i),y);
 return float3(float((v>>11)&31)/31,float((v>>5)&63)/63,float(v&31)/31);
}
uint lut(device const ushort *a,int t,uint i) { return raw(a,int(i&2047),576+t*32+int(i>>11)); }
int offset(float direction,int depth) { float v=direction*float(depth); return v<0 ? -int(floor(-v+.5)) : int(floor(v+.5)); }
kernel void compose(device const ushort *a [[buffer(0)]],constant Params &c [[buffer(1)]],texture2d<float,access::write> out [[texture(0)]],uint2 p [[thread_position_in_grid]]) {
 if(p.x>=c.tw*c.columns || p.y>=c.th*c.rows) return;
 uint view=(c.rows-1-p.y/c.th)*c.columns+p.x/c.tw;
 int y=int((p.y%c.th)*c.height/c.th);
 int x=int((p.x%c.tw)*c.width/c.tw)-int(raw(a,1666,y));
 int width=int(raw(a,1664,y))-2*int(raw(a,1666,y));
 uint result;
 if(x<0 || x>=width || (raw(a,1673,y)!=0 && x<8)) result=64;
 else if(raw(a,1672,y)!=0) result=raw(a,x+16,y);
 else {
  float direction=2*float(view)/float(c.columns*c.rows-1)-1;
  if(raw(a,1670,y)!=0) direction=-direction;
  uint av=raw(a,x-offset(direction,int(raw(a,1667,y))-16)+16,y);
  uint bv=raw(a,352+x-offset(direction,int(raw(a,1668,y))-16)+16,y);
  uint sv=raw(a,704+x-offset(direction,int(raw(a,1669,y))-16)+16,y);
  uint window=raw(a,1056+x+16,y);
  if((window&128)!=0) av=window&127;
  bool sh=raw(a,1671,y)!=0;
  result=lut(a,sh?3:1,(lut(a,sh?2:0,(bv<<8)|av)<<8)|sv);
 }
 out.write(float4(palette(a,result,y),1),p);
}
)MSL";

struct MetalSession {
 void *library=nullptr;
 decltype(&initialize_bridge) initialize=nullptr;
 decltype(&uninitialize_bridge) shutdown=nullptr;
 decltype(&instance_window_metal) instance=nullptr;
 decltype(&create_metal_texture_with_iosurface) create=nullptr;
 decltype(&release_metal_texture) release=nullptr;
 decltype(&draw_interop_quilt_texture_metal) draw=nullptr;
 decltype(&get_default_quilt_settings) settings=nullptr;
 decltype(&get_displays) displays=nullptr;
 decltype(&show_window) show=nullptr;
 void (*pump)()=nullptr;
 WINDOW_HANDLE window=0;
 id<MTLDevice> device=nil;
 id<MTLCommandQueue> queue=nil;
 id<MTLBuffer> atlas=nil;
 id<MTLTexture> quilt=nil;
 id<MTLComputePipelineState> pipeline=nil;
 CGLContextObj bridge_context=nullptr;
 NSOpenGLContext *bridge_native=nil;
 bool initialized=false;
 unsigned frames=0;
 double interval=0, next_frame=0;
 bool profile=false;
 double gpu_seconds=0, sdk_seconds=0, worst_seconds=0;
 struct { uint32_t width,height,tw,th,columns,rows; } params={};
};
static MetalSession *active_session=nullptr;
static void use_bridge_context(MetalSession *s) {
 if(s->bridge_native) [s->bridge_native makeCurrentContext];
 else { [NSOpenGLContext clearCurrentContext]; CGLSetCurrentContext(s->bridge_context); }
}
static void capture_bridge_context(MetalSession *s) {
 s->bridge_context=CGLGetCurrentContext();
 s->bridge_native=[NSOpenGLContext currentContext];
}

extern "C" bool looking_glass_metal_close(void *opaque) {
 MetalSession *s=static_cast<MetalSession *>(opaque);
 if(!s) return true;
 if(s->initialized) use_bridge_context(s);
 if(s->quilt && !s->release(s->window,(__bridge void *)s->quilt)) return false;
 s->quilt=nil;
 if(s->initialized && !s->shutdown()) return false;
 s->initialized=false;
 s->bridge_native=nil; s->bridge_context=nullptr;
 fprintf(stderr,"[Looking Glass Metal] Closed after %u frames.\n",s->frames);
 if(s->library) dlclose(s->library);
 if(active_session==s) active_session=nullptr;
 delete s;
 return true;
}

extern "C" void *looking_glass_metal_open(const char *path) {
 if(active_session || ![NSThread isMainThread] || !path) return nullptr;
 MetalSession *s=new MetalSession;
 active_session=s;
 [NSOpenGLContext clearCurrentContext];
 CGLSetCurrentContext(nullptr);
 s->library=dlopen(path,RTLD_NOW|RTLD_LOCAL);
 if(!s->library) goto fail;
#define SYMBOL(field,name) s->field=reinterpret_cast<decltype(s->field)>(dlsym(s->library,name)); if(!s->field) goto fail
 SYMBOL(initialize,"initialize_bridge");
 SYMBOL(shutdown,"uninitialize_bridge");
 SYMBOL(instance,"instance_window_metal");
 SYMBOL(create,"create_metal_texture_with_iosurface");
 SYMBOL(release,"release_metal_texture");
 SYMBOL(draw,"draw_interop_quilt_texture_metal");
 SYMBOL(settings,"get_default_quilt_settings");
 SYMBOL(displays,"get_displays");
 SYMBOL(show,"show_window");
#undef SYMBOL
 s->pump=reinterpret_cast<void(*)()>(dlsym(s->library,"SDL_PumpEvents"));
 if(!s->initialize("RetroArch Genesis Metal")) goto fail;
 s->initialized=true;
 s->device=MTLCreateSystemDefaultDevice();
 s->queue=[s->device newCommandQueue];
 s->atlas=[s->device newBufferWithLength:2048*704*2 options:MTLResourceStorageModeShared];
 if(!s->device || !s->queue || !s->atlas) goto fail;
 {
  int count=0,qw=0,qh=0,columns=0,rows=0;
  float aspect=0;
  unsigned long indices[32]={};
  if(!s->displays(&count,nullptr) || count<1 || count>32 || !s->displays(&count,indices)) goto fail;
  if(!s->instance((__bridge void *)s->device,&s->window,indices[0])) goto fail;
  if(!s->settings(s->window,&aspect,&qw,&qh,&columns,&rows) || columns<1 || rows<1 || columns*rows<2 || columns*rows>128 || qw<1 || qh<1 || qw>8192 || qh>8192 || qw%columns || qh%rows) goto fail;
  s->params.tw=qw/columns; s->params.th=qh/rows;
  s->params.columns=columns; s->params.rows=rows;
  MTLTextureDescriptor *d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:qw height:qh mipmapped:NO];
  d.storageMode=MTLStorageModeShared;
  d.usage=MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite;
  void *native=nullptr;
  if(!s->create(s->window,(__bridge void *)d,&native) || !native) goto fail;
  s->quilt=(__bridge id<MTLTexture>)native;
  if(s->quilt.device!=s->device || s->quilt.width!=(NSUInteger)qw || s->quilt.height!=(NSUInteger)qh || s->quilt.pixelFormat!=MTLPixelFormatRGBA8Unorm) goto fail;
  NSError *error=nil;
  id<MTLLibrary> shaders=[s->device newLibraryWithSource:[NSString stringWithUTF8String:kernel_source] options:nil error:&error];
  if(!shaders) { fprintf(stderr,"[Looking Glass Metal] Shader: %s\n",error.description.UTF8String); goto fail; }
  s->pipeline=[s->device newComputePipelineStateWithFunction:[shaders newFunctionWithName:@"compose"] error:&error];
  if(!s->pipeline || !s->show(s->window,true)) goto fail;
  capture_bridge_context(s);
  fprintf(stderr,"[Looking Glass Metal] Persistent SDK texture %dx%d, grid %dx%d.\n",qw,qh,columns,rows);
 }
 {
  const char *fps=getenv("GENESIS_LOOKING_GLASS_MAX_FPS");
  if(fps && *fps) {
   char *end=nullptr;
   double value=strtod(fps,&end);
   if(*end || !(value>=1 && value<=120)) goto fail;
   s->interval=1/value;
  }
  s->profile=getenv("GENESIS_LOOKING_GLASS_PROFILE")!=nullptr;
 }
 return s;
fail:
 fprintf(stderr,"[Looking Glass Metal] Setup failed.\n");
 looking_glass_metal_close(s);
 return nullptr;
}

extern "C" bool looking_glass_metal_compose(void *opaque,const uint16_t *frame) {
 @autoreleasepool {
  MetalSession *s=static_cast<MetalSession *>(opaque);
  if(!s || !frame || ![NSThread isMainThread]) return false;
  double started=[NSProcessInfo processInfo].systemUptime;
  if(s->interval && started<s->next_frame) return true;
  s->params.width=frame[1664]; s->params.height=frame[1665];
  if(s->params.width<256 || s->params.width>348 || s->params.height<192 || s->params.height>576) return false;
  memcpy(s->atlas.contents,frame,2048*704*2);
  id<MTLCommandBuffer> command=[s->queue commandBuffer];
  id<MTLComputeCommandEncoder> compute=[command computeCommandEncoder];
  [compute setComputePipelineState:s->pipeline];
  [compute setBuffer:s->atlas offset:0 atIndex:0];
  [compute setBytes:&s->params length:sizeof(s->params) atIndex:1];
  [compute setTexture:s->quilt atIndex:0];
  [compute dispatchThreads:MTLSizeMake(s->quilt.width,s->quilt.height,1) threadsPerThreadgroup:MTLSizeMake(16,16,1)];
  [compute endEncoding]; [command commit]; [command waitUntilCompleted];
  if(command.status!=MTLCommandBufferStatusCompleted) return false;
  double gpu_done=[NSProcessInfo processInfo].systemUptime;
  use_bridge_context(s);
  if(!s->show(s->window,true)) return false;
  if(s->pump) s->pump();
  if(!s->draw(s->window,(__bridge void *)s->quilt,s->params.columns,s->params.rows,4.0f/3.0f,1)) return false;
  capture_bridge_context(s);
  if(!s->frames) fprintf(stderr,"[Looking Glass Metal] First frame submitted.\n");
  double ended=[NSProcessInfo processInfo].systemUptime;
  s->gpu_seconds+=gpu_done-started; s->sdk_seconds+=ended-gpu_done;
  if(ended-started>s->worst_seconds) s->worst_seconds=ended-started;
  s->next_frame=started+s->interval;
  s->frames++;
  if(s->profile && s->frames%60==0) fprintf(stderr,"[Looking Glass Metal timing] frames=%u gpu=%.2fms sdk=%.2fms max=%.2fms\n",s->frames,s->gpu_seconds*1000/s->frames,s->sdk_seconds*1000/s->frames,s->worst_seconds*1000);
  return true;
 }
}
