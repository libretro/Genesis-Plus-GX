/* Native context bookkeeping for the experimental libretro adapter. */
#import <Cocoa/Cocoa.h>
#import <OpenGL/OpenGL.h>

void *looking_glass_context_current(void)
{
   return [NSOpenGLContext currentContext];
}

void *looking_glass_context_create(void)
{
   NSOpenGLPixelFormatAttribute attributes[] = {
      NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion4_1Core,
      NSOpenGLPFAAccelerated, NSOpenGLPFAAllowOfflineRenderers,
      NSOpenGLPFAColorSize, 24, 0
   };
   NSOpenGLPixelFormat *format;
   NSOpenGLContext *context;
   NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
   format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
   context = format ? [[NSOpenGLContext alloc] initWithFormat:format
      shareContext:nil] : nil;
   [format release];
   [pool drain];
   return context;
}

void looking_glass_context_make_current(void *context)
{
   [(NSOpenGLContext *)context makeCurrentContext];
}

void looking_glass_context_restore(void *native, CGLContextObj context)
{
   if (native)
      [(NSOpenGLContext *)native makeCurrentContext];
   else
   {
      [NSOpenGLContext clearCurrentContext];
      CGLSetCurrentContext(context);
   }
}

void looking_glass_context_destroy(void *context)
{
   [(NSOpenGLContext *)context release];
}
