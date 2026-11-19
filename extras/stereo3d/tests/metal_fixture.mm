// Own test double for documented SDK entry points; not SDK implementation code.
#import <Metal/Metal.h>
#import <IOSurface/IOSurface.h>
#include <bridge.h>
#include <cstdlib>
#include <cstring>
#include <vector>
static id<MTLDevice> device;
static unsigned int counters[6]={}; // initialize/window/create/draw/release/shutdown
static bool copied_pattern=false;
static std::vector<uint8_t> last_pixels;
static bool failing(const char *step) { const char *s=std::getenv("LG_FIXTURE_FAIL"); return s && std::strcmp(s,step)==0; }
extern "C" bool get_bridge_version(unsigned long *major,unsigned long *minor,unsigned long *build,int *chars,wchar_t *postfix) {
    *major=failing("version")?3:2; *minor=6; *build=3; *chars=0;
    if (postfix) *postfix=0;
    return true;
}
extern "C" bool initialize_bridge(const char *) { ++counters[0]; return !failing("initialize"); }
extern "C" bool uninitialize_bridge() { ++counters[5]; if (failing("shutdown")) return false; device=nil; return true; }
extern "C" bool instance_window_metal(void *d,WINDOW_HANDLE *window,unsigned long index) {
    ++counters[1]; if (failing("window") || index!=7) return false;
    device=(__bridge id<MTLDevice>)d; *window=17; return true;
}
extern "C" bool create_metal_texture_with_iosurface(WINDOW_HANDLE window,void *d,void **texture) {
    ++counters[2]; if (failing("create") || window!=17) return false;
    MTLTextureDescriptor *desc=(__bridge MTLTextureDescriptor *)d;
    NSUInteger element=desc.pixelFormat==MTLPixelFormatRGBA8Unorm?4:8;
    NSDictionary *properties=@{(id)kIOSurfaceWidth:@(desc.width),(id)kIOSurfaceHeight:@(desc.height),(id)kIOSurfaceBytesPerElement:@(element),(id)kIOSurfaceBytesPerRow:@(desc.width*element),(id)kIOSurfaceAllocSize:@(desc.width*desc.height*element),(id)kIOSurfacePixelFormat:@(element==4?0x52474241:0x52476841)};
    IOSurfaceRef surface=IOSurfaceCreate((__bridge CFDictionaryRef)properties);
    if (!surface) return false;
    id<MTLTexture> result=[device newTextureWithDescriptor:desc iosurface:surface plane:0];
    CFRelease(surface);
    if (!result) return false;
    *texture=(__bridge_retained void *)result;
    return true;
}
extern "C" bool draw_interop_quilt_texture_metal(WINDOW_HANDLE window,void *t,unsigned long columns,unsigned long rows,float aspect,float zoom) {
    ++counters[3]; if (failing("draw")) return false;
    id<MTLTexture> texture=(__bridge id<MTLTexture>)t;
    if (window!=17 || columns!=3 || rows!=2 || aspect!=(4.0f/3.0f) || zoom!=1 || !texture.iosurface) return false;
    NSUInteger element=texture.pixelFormat==MTLPixelFormatRGBA8Unorm?4:8;
    std::vector<uint8_t> pixels(texture.width*texture.height*element);
    // Test-only readback. Production adapter never reads pixel data on CPU.
    [texture getBytes:pixels.data() bytesPerRow:texture.width*element fromRegion:MTLRegionMake2D(0,0,texture.width,texture.height) mipmapLevel:0];
    copied_pattern=element==8;
    if (element==8) {
        const auto *halves=reinterpret_cast<const uint16_t *>(pixels.data());
        for (size_t i=0;i<pixels.size()/2;++i) if (halves[i]!=(i%4==3?0x3c00:(i%3==0?0x3800:0x3c00))) copied_pattern=false;
    }
    last_pixels=pixels;
    return copied_pattern || std::getenv("LG_FIXTURE_ACCEPT_QUILT");
}
extern "C" bool release_metal_texture(WINDOW_HANDLE,void *texture) { ++counters[4]; if (failing("release")) return false; id owned=CFBridgingRelease(texture); return owned!=nil; }
extern "C" void fixture_reset() { std::memset(counters,0,sizeof(counters)); copied_pattern=false; }
extern "C" unsigned int fixture_count(unsigned int i) { return counters[i]; }
extern "C" bool fixture_pattern_ok() { return copied_pattern; }

extern "C" unsigned long fixture_output_size() { return last_pixels.size(); }
extern "C" bool fixture_copy_output(void *output,unsigned long bytes) {
    if (bytes!=last_pixels.size()) return false;
    std::memcpy(output,last_pixels.data(),bytes); return true;
}
extern "C" bool show_window(WINDOW_HANDLE window, bool) { return window==17; }

extern "C" bool get_displays(int *count,unsigned long *indices) { *count=1; if(indices) indices[0]=7; return true; }
extern "C" bool get_default_quilt_settings(WINDOW_HANDLE w,float *a,int *qw,int *qh,int *c,int *r) { if(w!=17) return false; *a=4.0f/3.0f; *qw=960; *qh=448; *c=3; *r=2; return true; }
