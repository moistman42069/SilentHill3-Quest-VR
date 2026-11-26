#define GL_LOADER_IMPL
#include "gl.h"
#include "../runtime/rt.h"
#include <stdlib.h>
#include <string.h>

#define GL_DEFINE(type, name) type p_##name;
GL_FUNCS(GL_DEFINE)
GL_EXT_FUNCS(GL_DEFINE)

GlCaps gl_caps;

static int has_ext(const char *name) {
    GLint n = 0;
    p_glGetIntegerv(GL_NUM_EXTENSIONS, &n);
    for (GLint i = 0; i < n; i++)
        if (!strcmp((const char *)p_glGetStringi(GL_EXTENSIONS, i), name)) return 1;
    return 0;
}

void gl_load(void) {
#define GL_LOAD_FN(type, name) if (!(p_##name = (type)SDL_GL_GetProcAddress(#name))) rt_fatal("missing GL function " #name);
    GL_FUNCS(GL_LOAD_FN)
#define GL_LOAD_EXT(type, name) p_##name = (type)SDL_GL_GetProcAddress(#name);
    GL_EXT_FUNCS(GL_LOAD_EXT)
    gl_caps.s3tc = has_ext("GL_EXT_texture_compression_s3tc");
    gl_caps.mirror_clamp = has_ext("GL_EXT_texture_mirror_clamp_to_edge");
    gl_caps.anisotropic = has_ext("GL_EXT_texture_filter_anisotropic");
    gl_caps.multiview = has_ext("GL_OVR_multiview2") && p_glFramebufferTextureMultiviewOVR;
    if (gl_caps.multiview) p_glGetIntegerv(GL_MAX_VIEWS_OVR, &gl_caps.max_views);
    if (getenv("SH3_NO_S3TC")) gl_caps.s3tc = 0;  /* testing: the Quest's path (DXT decoded) on the PC */
    rt_log("GL: %s / %s / %s", p_glGetString(GL_RENDERER), p_glGetString(GL_VERSION),
           p_glGetString(GL_SHADING_LANGUAGE_VERSION));
    rt_log("GL extensions: S3TC %s, mirror clamp %s, anisotropic %s, multiview %s (%d views)",
           gl_caps.s3tc ? "yes" : "no (DXT decoded)", gl_caps.mirror_clamp ? "yes" : "no",
           gl_caps.anisotropic ? "yes" : "no", gl_caps.multiview ? "yes" : "no", gl_caps.max_views);
}
