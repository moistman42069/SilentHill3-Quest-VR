/* OpenGL ES 3.2 entry points, loaded through SDL (eglGetProcAddress on the Quest, WGL's ES profile on the PC test
 * build), so the PC runs exactly the renderer the headset runs.
 * SH3 Quest VR: sh3-nx drew with desktop GL 4.5 core (glClipControl for D3D's conventions); here the vertex stage
 * does that work (d3d8_shader.c) and every texture format is one ES takes (d3d8_res.c). The Khronos headers come
 * from the NDK (third_party/khronos on the PC). Extensions are loaded when present (gl_has). */
#pragma once
#include <SDL2/SDL.h>
#define GL_GLES_PROTOTYPES 0
#include <GLES3/gl32.h>
#include <GLES2/gl2ext.h>

#define GL_FUNCS(X) \
    X(PFNGLBINDTEXTUREPROC, glBindTexture) \
    X(PFNGLCLEARPROC, glClear) \
    X(PFNGLCLEARCOLORPROC, glClearColor) \
    X(PFNGLCLEARDEPTHFPROC, glClearDepthf) \
    X(PFNGLCLEARSTENCILPROC, glClearStencil) \
    X(PFNGLCOLORMASKPROC, glColorMask) \
    X(PFNGLCULLFACEPROC, glCullFace) \
    X(PFNGLDELETETEXTURESPROC, glDeleteTextures) \
    X(PFNGLDEPTHFUNCPROC, glDepthFunc) \
    X(PFNGLDEPTHMASKPROC, glDepthMask) \
    X(PFNGLDEPTHRANGEFPROC, glDepthRangef) \
    X(PFNGLDISABLEPROC, glDisable) \
    X(PFNGLDRAWARRAYSPROC, glDrawArrays) \
    X(PFNGLDRAWELEMENTSPROC, glDrawElements) \
    X(PFNGLENABLEPROC, glEnable) \
    X(PFNGLFRONTFACEPROC, glFrontFace) \
    X(PFNGLGENTEXTURESPROC, glGenTextures) \
    X(PFNGLGETSTRINGPROC, glGetString) \
    X(PFNGLGETINTEGERVPROC, glGetIntegerv) \
    X(PFNGLGETBOOLEANVPROC, glGetBooleanv) \
    X(PFNGLISENABLEDPROC, glIsEnabled) \
    X(PFNGLGETERRORPROC, glGetError) \
    X(PFNGLGETSTRINGIPROC, glGetStringi) \
    X(PFNGLPIXELSTOREIPROC, glPixelStorei) \
    X(PFNGLPOLYGONOFFSETPROC, glPolygonOffset) \
    X(PFNGLREADPIXELSPROC, glReadPixels) \
    X(PFNGLSCISSORPROC, glScissor) \
    X(PFNGLSTENCILFUNCPROC, glStencilFunc) \
    X(PFNGLSTENCILMASKPROC, glStencilMask) \
    X(PFNGLTEXIMAGE2DPROC, glTexImage2D) \
    X(PFNGLTEXIMAGE3DPROC, glTexImage3D) \
    X(PFNGLTEXPARAMETERIPROC, glTexParameteri) \
    X(PFNGLTEXSUBIMAGE2DPROC, glTexSubImage2D) \
    X(PFNGLTEXSUBIMAGE3DPROC, glTexSubImage3D) \
    X(PFNGLVIEWPORTPROC, glViewport) \
    X(PFNGLGENBUFFERSPROC, glGenBuffers) \
    X(PFNGLBINDBUFFERPROC, glBindBuffer) \
    X(PFNGLBUFFERDATAPROC, glBufferData) \
    X(PFNGLBUFFERSUBDATAPROC, glBufferSubData) \
    X(PFNGLDELETEBUFFERSPROC, glDeleteBuffers) \
    X(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays) \
    X(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray) \
    X(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer) \
    X(PFNGLVERTEXATTRIBIPOINTERPROC, glVertexAttribIPointer) \
    X(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray) \
    X(PFNGLDISABLEVERTEXATTRIBARRAYPROC, glDisableVertexAttribArray) \
    X(PFNGLVERTEXATTRIB4FPROC, glVertexAttrib4f) \
    X(PFNGLCREATESHADERPROC, glCreateShader) \
    X(PFNGLSHADERSOURCEPROC, glShaderSource) \
    X(PFNGLCOMPILESHADERPROC, glCompileShader) \
    X(PFNGLGETSHADERIVPROC, glGetShaderiv) \
    X(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog) \
    X(PFNGLCREATEPROGRAMPROC, glCreateProgram) \
    X(PFNGLATTACHSHADERPROC, glAttachShader) \
    X(PFNGLBINDATTRIBLOCATIONPROC, glBindAttribLocation) \
    X(PFNGLLINKPROGRAMPROC, glLinkProgram) \
    X(PFNGLGETPROGRAMIVPROC, glGetProgramiv) \
    X(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog) \
    X(PFNGLUSEPROGRAMPROC, glUseProgram) \
    X(PFNGLDELETESHADERPROC, glDeleteShader) \
    X(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation) \
    X(PFNGLUNIFORM1IPROC, glUniform1i) \
    X(PFNGLUNIFORM1FPROC, glUniform1f) \
    X(PFNGLUNIFORM2FPROC, glUniform2f) \
    X(PFNGLUNIFORM4FVPROC, glUniform4fv) \
    X(PFNGLUNIFORM1IVPROC, glUniform1iv) \
    X(PFNGLUNIFORMMATRIX4FVPROC, glUniformMatrix4fv) \
    X(PFNGLUNIFORMMATRIX3FVPROC, glUniformMatrix3fv) \
    X(PFNGLACTIVETEXTUREPROC, glActiveTexture) \
    X(PFNGLCOMPRESSEDTEXIMAGE2DPROC, glCompressedTexImage2D) \
    X(PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC, glCompressedTexSubImage2D) \
    X(PFNGLGENERATEMIPMAPPROC, glGenerateMipmap) \
    X(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers) \
    X(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer) \
    X(PFNGLFRAMEBUFFERTEXTURE2DPROC, glFramebufferTexture2D) \
    X(PFNGLFRAMEBUFFERTEXTURELAYERPROC, glFramebufferTextureLayer) \
    X(PFNGLCHECKFRAMEBUFFERSTATUSPROC, glCheckFramebufferStatus) \
    X(PFNGLDELETEFRAMEBUFFERSPROC, glDeleteFramebuffers) \
    X(PFNGLBLITFRAMEBUFFERPROC, glBlitFramebuffer) \
    X(PFNGLINVALIDATEFRAMEBUFFERPROC, glInvalidateFramebuffer) \
    X(PFNGLBLENDFUNCSEPARATEPROC, glBlendFuncSeparate) \
    X(PFNGLBLENDEQUATIONPROC, glBlendEquation) \
    X(PFNGLBLENDCOLORPROC, glBlendColor) \
    X(PFNGLSTENCILOPSEPARATEPROC, glStencilOpSeparate) \
    X(PFNGLDRAWELEMENTSBASEVERTEXPROC, glDrawElementsBaseVertex) \
    X(PFNGLGENSAMPLERSPROC, glGenSamplers) \
    X(PFNGLBINDSAMPLERPROC, glBindSampler) \
    X(PFNGLSAMPLERPARAMETERIPROC, glSamplerParameteri) \
    X(PFNGLSAMPLERPARAMETERFPROC, glSamplerParameterf) \
    X(PFNGLSAMPLERPARAMETERFVPROC, glSamplerParameterfv) \
    X(PFNGLFLUSHPROC, glFlush) \
    X(PFNGLFINISHPROC, glFinish) \
    X(PFNGLMAPBUFFERRANGEPROC, glMapBufferRange) \
    X(PFNGLUNMAPBUFFERPROC, glUnmapBuffer) \
    X(PFNGLFENCESYNCPROC, glFenceSync) \
    X(PFNGLCLIENTWAITSYNCPROC, glClientWaitSync) \
    X(PFNGLDELETESYNCPROC, glDeleteSync)

/* Extension entry points: NULL when the driver lacks them (gl_has says which extensions are there). */
#define GL_EXT_FUNCS(X) \
    X(PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC, glFramebufferTextureMultiviewOVR) \
    X(PFNGLGENQUERIESEXTPROC, glGenQueriesEXT) \
    X(PFNGLBEGINQUERYEXTPROC, glBeginQueryEXT) \
    X(PFNGLENDQUERYEXTPROC, glEndQueryEXT) \
    X(PFNGLGETQUERYOBJECTUIVEXTPROC, glGetQueryObjectuivEXT) \
    X(PFNGLGETQUERYOBJECTUI64VEXTPROC, glGetQueryObjectui64vEXT)
/* (0.1.5: the queries are GL_EXT_disjoint_timer_query's, the GPU time of a frame: sh3vr.cpp gpu_timer_begin/end) */

#define GL_DECLARE(type, name) extern type p_##name;
GL_FUNCS(GL_DECLARE)
GL_EXT_FUNCS(GL_DECLARE)
#undef GL_DECLARE
#ifndef GL_LOADER_IMPL
#define glBindTexture p_glBindTexture
#define glClear p_glClear
#define glClearColor p_glClearColor
#define glClearDepthf p_glClearDepthf
#define glClearStencil p_glClearStencil
#define glColorMask p_glColorMask
#define glCullFace p_glCullFace
#define glDeleteTextures p_glDeleteTextures
#define glDepthFunc p_glDepthFunc
#define glDepthMask p_glDepthMask
#define glDepthRangef p_glDepthRangef
#define glDisable p_glDisable
#define glDrawArrays p_glDrawArrays
#define glDrawElements p_glDrawElements
#define glEnable p_glEnable
#define glFrontFace p_glFrontFace
#define glGenTextures p_glGenTextures
#define glGetString p_glGetString
#define glGetIntegerv p_glGetIntegerv
#define glGetBooleanv p_glGetBooleanv  /* 0.1.4 smoother frames (sh3vr.cpp motion_draw), and glIsEnabled */
#define glIsEnabled p_glIsEnabled
#define glGetError p_glGetError
#define glGetStringi p_glGetStringi
#define glPixelStorei p_glPixelStorei
#define glPolygonOffset p_glPolygonOffset
#define glReadPixels p_glReadPixels
#define glScissor p_glScissor
#define glStencilFunc p_glStencilFunc
#define glStencilMask p_glStencilMask
#define glTexImage2D p_glTexImage2D
#define glTexImage3D p_glTexImage3D
#define glTexParameteri p_glTexParameteri
#define glTexSubImage2D p_glTexSubImage2D
#define glTexSubImage3D p_glTexSubImage3D
#define glViewport p_glViewport
#define glGenBuffers p_glGenBuffers
#define glBindBuffer p_glBindBuffer
#define glBufferData p_glBufferData
#define glBufferSubData p_glBufferSubData
#define glDeleteBuffers p_glDeleteBuffers
#define glGenVertexArrays p_glGenVertexArrays
#define glBindVertexArray p_glBindVertexArray
#define glVertexAttribPointer p_glVertexAttribPointer
#define glVertexAttribIPointer p_glVertexAttribIPointer
#define glEnableVertexAttribArray p_glEnableVertexAttribArray
#define glDisableVertexAttribArray p_glDisableVertexAttribArray
#define glVertexAttrib4f p_glVertexAttrib4f
#define glCreateShader p_glCreateShader
#define glShaderSource p_glShaderSource
#define glCompileShader p_glCompileShader
#define glGetShaderiv p_glGetShaderiv
#define glGetShaderInfoLog p_glGetShaderInfoLog
#define glCreateProgram p_glCreateProgram
#define glAttachShader p_glAttachShader
#define glBindAttribLocation p_glBindAttribLocation
#define glLinkProgram p_glLinkProgram
#define glGetProgramiv p_glGetProgramiv
#define glGetProgramInfoLog p_glGetProgramInfoLog
#define glUseProgram p_glUseProgram
#define glDeleteShader p_glDeleteShader
#define glGetUniformLocation p_glGetUniformLocation
#define glUniform1i p_glUniform1i
#define glUniform1f p_glUniform1f
#define glUniform2f p_glUniform2f
#define glUniform4fv p_glUniform4fv
#define glUniform1iv p_glUniform1iv
#define glUniformMatrix4fv p_glUniformMatrix4fv
#define glUniformMatrix3fv p_glUniformMatrix3fv
#define glActiveTexture p_glActiveTexture
#define glCompressedTexImage2D p_glCompressedTexImage2D
#define glCompressedTexSubImage2D p_glCompressedTexSubImage2D
#define glGenerateMipmap p_glGenerateMipmap
#define glGenFramebuffers p_glGenFramebuffers
#define glBindFramebuffer p_glBindFramebuffer
#define glFramebufferTexture2D p_glFramebufferTexture2D
#define glFramebufferTextureLayer p_glFramebufferTextureLayer
#define glCheckFramebufferStatus p_glCheckFramebufferStatus
#define glDeleteFramebuffers p_glDeleteFramebuffers
#define glBlitFramebuffer p_glBlitFramebuffer
#define glInvalidateFramebuffer p_glInvalidateFramebuffer
#define glBlendFuncSeparate p_glBlendFuncSeparate
#define glBlendEquation p_glBlendEquation
#define glBlendColor p_glBlendColor
#define glStencilOpSeparate p_glStencilOpSeparate
#define glDrawElementsBaseVertex p_glDrawElementsBaseVertex
#define glGenSamplers p_glGenSamplers
#define glBindSampler p_glBindSampler
#define glSamplerParameteri p_glSamplerParameteri
#define glSamplerParameterf p_glSamplerParameterf
#define glSamplerParameterfv p_glSamplerParameterfv
#define glFlush p_glFlush
#define glFinish p_glFinish
/* SH3 Quest VR: the hands' depth probes read back a frame later (sh3vr_body.cpp) */
#define glMapBufferRange p_glMapBufferRange
#define glUnmapBuffer p_glUnmapBuffer
#define glFenceSync p_glFenceSync
#define glClientWaitSync p_glClientWaitSync
#define glDeleteSync p_glDeleteSync
#define glFramebufferTextureMultiviewOVR p_glFramebufferTextureMultiviewOVR
#define glGenQueriesEXT p_glGenQueriesEXT
#define glBeginQueryEXT p_glBeginQueryEXT
#define glEndQueryEXT p_glEndQueryEXT
#define glGetQueryObjectuivEXT p_glGetQueryObjectuivEXT
#define glGetQueryObjectui64vEXT p_glGetQueryObjectui64vEXT
#endif

/* What the driver offers beyond ES 3.2 (filled by gl_load). */
typedef struct {
    int s3tc;          /* GL_EXT_texture_compression_s3tc: DXT textures stay compressed (else decoded to RGBA8) */
    int mirror_clamp;  /* GL_EXT_texture_mirror_clamp_to_edge: D3D's MIRRORONCE (else mirrored repeat) */
    int anisotropic;   /* GL_EXT_texture_filter_anisotropic */
    int multiview;     /* GL_OVR_multiview2: both eyes in one pass */
    int max_views;
    int timer;         /* GL_EXT_disjoint_timer_query: the GPU time of each frame in the log (0.1.5) */
} GlCaps;
extern GlCaps gl_caps;

void gl_load(void);
