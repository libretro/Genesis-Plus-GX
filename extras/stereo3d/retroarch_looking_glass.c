/* Experimental macOS libretro adapter for GPU Looking Glass output. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <wchar.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>
#include "../../libretro/libretro-common/include/libretro.h"

extern void *looking_glass_context_current(void);
extern void *looking_glass_context_create(void);
extern void looking_glass_context_make_current(void *);
extern void looking_glass_context_restore(void *, CGLContextObj);
extern void looking_glass_context_destroy(void *);

#define CORE_FUNCTIONS(X) \
 X(void, set_environment, (retro_environment_t)) \
 X(void, set_video_refresh, (retro_video_refresh_t)) \
 X(void, set_audio_sample, (retro_audio_sample_t)) \
 X(void, set_audio_sample_batch, (retro_audio_sample_batch_t)) \
 X(void, set_input_poll, (retro_input_poll_t)) \
 X(void, set_input_state, (retro_input_state_t)) \
 X(void, init, (void)) X(void, deinit, (void)) \
 X(void, get_system_info, (struct retro_system_info *)) \
 X(void, get_system_av_info, (struct retro_system_av_info *)) \
 X(void, set_controller_port_device, (unsigned, unsigned)) \
 X(void, reset, (void)) X(void, run, (void)) \
 X(size_t, serialize_size, (void)) \
 X(bool, serialize, (void *, size_t)) \
 X(bool, unserialize, (const void *, size_t)) \
 X(void, cheat_reset, (void)) X(void, cheat_set, (unsigned, bool, const char *)) \
 X(bool, load_game, (const struct retro_game_info *)) \
 X(bool, load_game_special, (unsigned, const struct retro_game_info *, size_t)) \
 X(void, unload_game, (void)) X(unsigned, get_region, (void)) \
 X(void *, get_memory_data, (unsigned)) X(size_t, get_memory_size, (unsigned))
#define DECLARE_CORE(result, name, args) static result (*core_##name) args;
CORE_FUNCTIONS(DECLARE_CORE)

static void *core_library;
static retro_environment_t frontend_environment;
static retro_video_refresh_t frontend_video;
static retro_log_printf_t frontend_log;
static unsigned view_width = 320, view_height = 224;
static bool atlas_mode;
static uint16_t preview[348 * 576];

static struct
{
   uint32_t *stream;
   int stream_fd;
   void *context;
   void *library;
   bool initialized;
   bool failed;
   uint32_t window;
   int columns, rows;
   GLuint program, vao, atlas, quilt, fbo;
   unsigned width, height;
   bool (*initialize)(const wchar_t *);
   bool (*uninitialize)(void);
   bool (*instance)(uint32_t *, uint32_t);
   bool (*settings)(uint32_t, float *, int *, int *, int *, int *);
   bool (*show)(uint32_t, bool);
   bool (*draw)(uint32_t, uint64_t, unsigned, unsigned long, unsigned long,
         unsigned long, unsigned long, float, float);
} glass;

static void report_error(const char *text)
{
   struct retro_message message;
   fprintf(stderr, "[Looking Glass] %s\n", text);
   if (frontend_log) frontend_log(RETRO_LOG_ERROR, "[Looking Glass] %s\n", text);
   message.msg = text;
   message.frames = 300;
   if (frontend_environment)
      frontend_environment(RETRO_ENVIRONMENT_SET_MESSAGE, &message);
}

static bool load_core(void)
{
   const char *path;
   if (core_library)
      return true;
   path = getenv("GENESIS_LOOKING_GLASS_CORE");
   if (!path || !(core_library = dlopen(path, RTLD_NOW | RTLD_LOCAL)))
   {
      report_error("Cannot load Genesis core; use the Looking Glass launcher.");
      return false;
   }
#define LOAD_CORE(result, name, args) \
   *(void **)(&core_##name) = dlsym(core_library, "retro_" #name); \
   if (!core_##name) { dlclose(core_library); core_library = NULL; return false; }
   CORE_FUNCTIONS(LOAD_CORE)
#undef LOAD_CORE
   return true;
}

static void geometry(struct retro_game_geometry *geometry)
{
   if (geometry->base_width != 2048 || geometry->base_height != 704)
      return;
   geometry->base_width = view_width;
   geometry->base_height = view_height;
   geometry->max_width = 348;
   geometry->max_height = 576;
   geometry->aspect_ratio = 4.0f / 3.0f;
}

static bool environment(unsigned command, void *data)
{
   if (!frontend_environment)
      return false;
   if (command == RETRO_ENVIRONMENT_SET_GEOMETRY)
   {
      struct retro_game_geometry value = *(struct retro_game_geometry *)data;
      geometry(&value);
      return frontend_environment(command, &value);
   }
   if (command == RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO)
   {
      struct retro_system_av_info value = *(struct retro_system_av_info *)data;
      geometry(&value.geometry);
      return frontend_environment(command, &value);
   }
   if (command == RETRO_ENVIRONMENT_GET_VARIABLE)
   {
      struct retro_variable *variable = (struct retro_variable *)data;
      bool result = frontend_environment(command, data);
      if (result && variable->value &&
            !strcmp(variable->key, "genesis_plus_gx_stereo_3d") &&
            strcmp(variable->value, "disabled"))
         variable->value = "layers";
      return result;
   }
   return frontend_environment(command, data);
}

static GLuint shader(GLenum type, const char *source)
{
   GLint success;
   char message[1024];
   GLuint object = glCreateShader(type);
   glShaderSource(object, 1, &source, NULL);
   glCompileShader(object);
   glGetShaderiv(object, GL_COMPILE_STATUS, &success);
   if (success)
      return object;
   glGetShaderInfoLog(object, sizeof(message), NULL, message);
   report_error(message);
   glDeleteShader(object);
   return 0;
}

static void destroy_glass(void)
{
   CGLContextObj previous = CGLGetCurrentContext();
   void *previous_native = looking_glass_context_current();
   if (glass.context)
   {
      looking_glass_context_make_current(glass.context);
      if (glass.initialized)
         glass.uninitialize();
      looking_glass_context_make_current(glass.context);
      glDeleteTextures(1, &glass.atlas);
      glDeleteTextures(1, &glass.quilt);
      glDeleteFramebuffers(1, &glass.fbo);
      glDeleteVertexArrays(1, &glass.vao);
      if (glass.program)
         glDeleteProgram(glass.program);
      looking_glass_context_restore(previous_native, previous);
      looking_glass_context_destroy(glass.context);
   }
   if (glass.stream)
   {
      __atomic_store_n(&glass.stream[5], 0, __ATOMIC_RELEASE);
      munmap(glass.stream, 64 + 2048 * 704 * 2);
      close(glass.stream_fd);
   }
   if (glass.library)
      dlclose(glass.library);
   memset(&glass, 0, sizeof(glass));
}

static bool initialize_glass(void)
{
   static const char vertex[] =
      "#version 330 core\n"
      "void main() { vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);"
      "gl_Position=vec4(p*2.0-1.0,0.0,1.0); }\n";
   GLint success;
   GLuint vs = 0, fs = 0;
   const char *path;
   const char *stage = "shader file";
   char failure[256];
   FILE *file;
   char source[16384];
   size_t length;
   float aspect;
   int qw, qh;
   CGLContextObj previous = CGLGetCurrentContext();
   void *previous_native = looking_glass_context_current();
   if (glass.failed)
      return false;
   if (glass.context)
      return true;
   path = getenv("GENESIS_LOOKING_GLASS_SHADER");
   file = path ? fopen(path, "rb") : NULL;
   if (!file)
      goto error;
   length = fread(source, 1, sizeof(source) - 1, file);
   fclose(file);
   source[length] = '\0';
   stage = "native OpenGL context";
   glass.context = looking_glass_context_create();
   if (!glass.context)
      goto error;
   looking_glass_context_make_current(glass.context);
   path = getenv("GENESIS_LOOKING_GLASS_STREAM");
   if (path)
   {
      struct stat status;
      stage = "Bridge worker stream";
      glass.stream_fd = open(path, O_RDWR);
      if (glass.stream_fd < 0)
         goto error;
      if (fstat(glass.stream_fd, &status) || status.st_size != 64 + 2048 * 704 * 2)
      {
         close(glass.stream_fd);
         goto error;
      }
      glass.stream = (uint32_t *)mmap(NULL, status.st_size,
            PROT_READ | PROT_WRITE, MAP_SHARED, glass.stream_fd, 0);
      if (glass.stream == MAP_FAILED)
      {
         glass.stream = NULL;
         close(glass.stream_fd);
         goto error;
      }
      if (glass.stream[0] != 0x474c4741 || glass.stream[1] != 1)
         goto error;
      glass.columns = glass.stream[3];
      glass.rows = glass.stream[4];
   }
   else
   {
      path = getenv("GENESIS_LOOKING_GLASS_BRIDGE");
      stage = "Bridge library";
      glass.library = path ? dlopen(path, RTLD_NOW | RTLD_LOCAL) : NULL;
      if (!glass.library)
      {
         const char *detail = dlerror();
         fprintf(stderr, "[Looking Glass] Bridge path: %s\n", path ? path : "(unset)");
         fprintf(stderr, "[Looking Glass] dlopen: %s\n", detail ? detail : "unknown error");
         goto error;
      }
#define LOAD_BRIDGE(member, symbol) \
      *(void **)(&glass.member) = dlsym(glass.library, symbol); \
      if (!glass.member) goto error
      LOAD_BRIDGE(initialize, "initialize_bridge");
      LOAD_BRIDGE(uninitialize, "uninitialize_bridge");
      LOAD_BRIDGE(instance, "instance_window_gl");
      LOAD_BRIDGE(settings, "get_default_quilt_settings");
      LOAD_BRIDGE(show, "show_window");
      LOAD_BRIDGE(draw, "draw_interop_quilt_texture_gl");
#undef LOAD_BRIDGE
      stage = "Bridge initialization";
      if (!glass.initialize(L"RetroArchGenesisLookingGlass"))
         goto error;
      glass.initialized = true;
      stage = "Bridge window";
      if (!glass.instance(&glass.window, UINT32_MAX))
         goto error;
      stage = "show Bridge window";
      if (!glass.show(glass.window, true))
         goto error;
      stage = "device profile";
      if (!glass.settings(glass.window, &aspect, &qw, &qh,
               &glass.columns, &glass.rows))
         goto error;
   }
   if (glass.columns < 1 || glass.rows < 1 ||
         glass.columns > 128 || glass.rows > 128 ||
         glass.columns * glass.rows < 2 || glass.columns * glass.rows > 128)
      goto error;
   looking_glass_context_make_current(glass.context);
   glGenVertexArrays(1, &glass.vao);
   glBindVertexArray(glass.vao);
   stage = "GPU shaders";
   vs = shader(GL_VERTEX_SHADER, vertex);
   fs = shader(GL_FRAGMENT_SHADER, source);
   if (!vs || !fs)
      goto error;
   glass.program = glCreateProgram();
   glAttachShader(glass.program, vs);
   glAttachShader(glass.program, fs);
   glLinkProgram(glass.program);
   glDeleteShader(vs);
   glDeleteShader(fs);
   vs = fs = 0;
   glGetProgramiv(glass.program, GL_LINK_STATUS, &success);
   if (!success)
      goto error;
   glGenTextures(1, &glass.atlas);
   glBindTexture(GL_TEXTURE_2D, glass.atlas);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_R16UI, 2048, 704, 0,
         GL_RED_INTEGER, GL_UNSIGNED_SHORT, NULL);
   glGenTextures(1, &glass.quilt);
   glGenFramebuffers(1, &glass.fbo);
   looking_glass_context_restore(previous_native, previous);
   if (frontend_log) frontend_log(RETRO_LOG_INFO, "[Looking Glass] Live GPU grid %dx%d initialized.\n",
         glass.columns, glass.rows);
   fprintf(stderr, "[Looking Glass] Live GPU grid %dx%d initialized.\n",
         glass.columns, glass.rows);
   return true;
error:
   if (vs) glDeleteShader(vs);
   if (fs) glDeleteShader(fs);
   looking_glass_context_restore(previous_native, previous);
   destroy_glass();
   glass.failed = true;
   snprintf(failure, sizeof(failure), "Looking Glass setup failed at %s.", stage);
   report_error(failure);
   return false;
}

static bool compose(const uint16_t *frame)
{
   CGLContextObj previous = CGLGetCurrentContext();
   void *previous_native = looking_glass_context_current();
   unsigned width = frame[1664], height = frame[1665];
   unsigned qw, qh, view;
   GLint limit;
   bool result = false;
   if (width < 256 || width > 348 || height < 192 || height > 576)
      return false;
   if (!initialize_glass())
      return false;
   looking_glass_context_make_current(glass.context);
   qw = width * glass.columns;
   qh = height * glass.rows;
   glGetIntegerv(GL_MAX_TEXTURE_SIZE, &limit);
   if (qw > (unsigned)limit || qh > (unsigned)limit)
   {
      glass.failed = true;
      report_error("Looking Glass quilt exceeds the OpenGL texture limit.");
      goto end;
   }
   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D, glass.atlas);
   glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
   glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
   glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
   glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2048, 704,
         GL_RED_INTEGER, GL_UNSIGNED_SHORT, frame);
   glBindFramebuffer(GL_FRAMEBUFFER, glass.fbo);
   if (width != glass.width || height != glass.height)
   {
      glBindTexture(GL_TEXTURE_2D, glass.quilt);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, qw, qh, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, NULL);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, glass.quilt, 0);
      if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
         goto end;
      glass.width = width;
      glass.height = height;
   }
   glBindTexture(GL_TEXTURE_2D, glass.atlas);
   glViewport(0, 0, qw, qh);
   glDisable(GL_DEPTH_TEST);
   glDisable(GL_BLEND);
   glDisable(GL_SCISSOR_TEST);
   glUseProgram(glass.program);
   glUniform1i(glGetUniformLocation(glass.program, "Atlas"), 0);
   glUniform2i(glGetUniformLocation(glass.program, "ViewSize"), width, height);
   glUniform2i(glGetUniformLocation(glass.program, "Grid"), glass.columns, glass.rows);
   glBindVertexArray(glass.vao);
   glDrawArrays(GL_TRIANGLES, 0, 3);
   view = glass.columns * glass.rows / 2;
   glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
   glPixelStorei(GL_PACK_ALIGNMENT, 2);
   glPixelStorei(GL_PACK_ROW_LENGTH, 0);
   glReadPixels((view % glass.columns) * width,
         (glass.rows - 1 - view / glass.columns) * height,
         width, height, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, preview);
   glBindFramebuffer(GL_FRAMEBUFFER, 0);
   glFlush();
   if (glass.stream)
   {
      uint32_t sequence = __atomic_load_n(&glass.stream[2], __ATOMIC_ACQUIRE);
      __atomic_store_n(&glass.stream[2], sequence + 1, __ATOMIC_SEQ_CST);
      memcpy((char *)glass.stream + 64, frame, 2048 * 704 * 2);
      __atomic_store_n(&glass.stream[2], sequence + 2, __ATOMIC_RELEASE);
      __atomic_store_n(&glass.stream[5], 1, __ATOMIC_RELEASE);
      result = glass.stream[1] == 1;
   }
   else
      result = glass.draw(glass.window, glass.quilt, GL_RGBA, qw, qh,
            glass.columns, glass.rows, 4.0f / 3.0f, 1.0f);
   looking_glass_context_make_current(glass.context);
   if (!result)
   {
      glass.failed = true;
      report_error("Looking Glass Bridge could not draw the quilt.");
   }
end:
   looking_glass_context_restore(previous_native, previous);
   return result;
}

static void video(const void *frame, unsigned width, unsigned height, size_t pitch)
{
   if (width == 2048 && height == 704 && pitch == 4096)
   {
      if (frame)
      {
         const uint16_t *atlas = (const uint16_t *)frame;
         unsigned next_width = atlas[1664], next_height = atlas[1665];
         bool success = compose(atlas);
         if (next_width >= 256 && next_width <= 348 &&
               next_height >= 192 && next_height <= 576)
         {
            if (!atlas_mode || next_width != view_width || next_height != view_height)
            {
               struct retro_game_geometry value;
               value.base_width = next_width;
               value.base_height = next_height;
               value.max_width = 348;
               value.max_height = 576;
               value.aspect_ratio = 4.0f / 3.0f;
               frontend_environment(RETRO_ENVIRONMENT_SET_GEOMETRY, &value);
            }
            view_width = next_width;
            view_height = next_height;
         }
         if (!success)
            memset(preview, 0, sizeof(preview));
         atlas_mode = true;
      }
      frontend_video(frame ? preview : NULL, view_width, view_height, view_width * 2);
      return;
   }
   if (atlas_mode)
   {
      destroy_glass();
      atlas_mode = false;
   }
   frontend_video(frame, width, height, pitch);
}

void retro_set_environment(retro_environment_t cb)
{
   struct retro_log_callback logger;
   logger.log = NULL;
   frontend_environment = cb;
   if (cb && cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logger))
      frontend_log = logger.log;
   if (load_core()) core_set_environment(environment);
}
void retro_set_video_refresh(retro_video_refresh_t cb)
{
   frontend_video = cb;
   if (load_core()) core_set_video_refresh(video);
}
#define FORWARD_CALLBACK(name, type) \
 void retro_set_##name(type cb) { if (load_core()) core_set_##name(cb); }
FORWARD_CALLBACK(audio_sample, retro_audio_sample_t)
FORWARD_CALLBACK(audio_sample_batch, retro_audio_sample_batch_t)
FORWARD_CALLBACK(input_poll, retro_input_poll_t)
FORWARD_CALLBACK(input_state, retro_input_state_t)

unsigned retro_api_version(void) { return RETRO_API_VERSION; }
void retro_init(void) { if (load_core()) core_init(); }
void retro_deinit(void)
{
   destroy_glass();
   if (core_library)
   {
      core_deinit();
      dlclose(core_library);
      core_library = NULL;
   }
}
void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   if (load_core()) core_get_system_info(info);
   info->library_name = "Genesis Plus GX Looking Glass";
   if (!info->library_version) info->library_version = "experimental";
   if (!info->valid_extensions) info->valid_extensions = "md|bin|smd";
}
void retro_get_system_av_info(struct retro_system_av_info *info)
{
   memset(info, 0, sizeof(*info));
   if (load_core()) core_get_system_av_info(info);
   geometry(&info->geometry);
}
void retro_set_controller_port_device(unsigned port, unsigned device)
{ if (core_library) core_set_controller_port_device(port, device); }
void retro_reset(void) { if (core_library) core_reset(); }
void retro_run(void) { if (core_library) core_run(); }
size_t retro_serialize_size(void) { return core_library ? core_serialize_size() : 0; }
bool retro_serialize(void *data, size_t size)
{ return core_library && core_serialize(data, size); }
bool retro_unserialize(const void *data, size_t size)
{ return core_library && core_unserialize(data, size); }
void retro_cheat_reset(void) { if (core_library) core_cheat_reset(); }
void retro_cheat_set(unsigned index, bool enabled, const char *code)
{ if (core_library) core_cheat_set(index, enabled, code); }
bool retro_load_game(const struct retro_game_info *game)
{ atlas_mode = false; return load_core() && core_load_game(game); }
bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t count)
{ atlas_mode = false; return load_core() && core_load_game_special(type, info, count); }
void retro_unload_game(void)
{
   destroy_glass();
   atlas_mode = false;
   if (core_library) core_unload_game();
}
unsigned retro_get_region(void) { return core_library ? core_get_region() : 0; }
void *retro_get_memory_data(unsigned id) { return core_library ? core_get_memory_data(id) : NULL; }
size_t retro_get_memory_size(unsigned id) { return core_library ? core_get_memory_size(id) : 0; }
