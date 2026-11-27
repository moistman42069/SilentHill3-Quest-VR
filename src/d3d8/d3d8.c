/* IDirect3D8 and IDirect3DDevice8 on OpenGL. */
#include "d3d8.h"
#include "../vr/sh3vr.h"
#include <math.h>

Dev *dev;
extern uint32_t rt_res_w, rt_res_h;
void sh3_vr_frame(void);  /* src/game/sh3.c */
void sh3_vr_after_present(void);  /* src/game/sh3.c: LIST 51, the 3D pictures made again at dynamic resolution's size */
static int panel_has_picture;  /* SH3 Quest VR: the panel got a draw this frame (vr_route) */
/* Session 11: this frame's 3D draws with the frame camera (the world) and with others, its 2D sent whole to the panel
 * as a full screen; whether the last frame drew the world (vr_route) */
static int frame_world, frame_other, frame_screen, world_last = 1;
static int frame_fog;  /* owner 79 (session 16): the game's drifting fog's draws this frame (vr_route_pick) */
static uint32_t d3d_obj;

/* What a state block holds: everything for CreateStateBlock, only what was set while recording for
 * Begin/EndStateBlock (SH3 records a few render states; restoring its whole snapshot brought back the
 * backbuffer's viewport while drawing into a 512x512 target, and stale textures and shaders). */
typedef struct { uint8_t rs[RS_COUNT], tss[MAX_STAGES][TSS_COUNT], xf[14], vsc[96], psc[8], material, light[8],
                 light_on[8], tex[MAX_STAGES], stream[16], ib, vs, ps, vp; } Mask;
static Mask *rec_mask;  /* while recording: Set* calls go to a scratch device and are marked here */
#define MARK(f) do { if (rec_mask) rec_mask->f = 1; } while (0)

static float f32(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }
static uint32_t u32(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }

/* ---- IDirect3D8 ---- */

extern uint32_t rt_res_w, rt_res_h;  /* the resolution sh3.ini asks for (rt_core.c): always offered as the last mode */
static uint32_t modes[][2] = {{640, 480}, {800, 600}, {1024, 768}, {1280, 720}, {1280, 960},
                                    {1280, 1024}, {1600, 900}, {1920, 1080}, {960, 720}, {1440, 1080}, {0, 0}};
/* The last two are 4:3 at the Switch's 720p and 1080p heights. */
#define N_MODES (sizeof modes / sizeof *modes)
static void sync_modes(void) { modes[N_MODES - 1][0] = rt_res_w; modes[N_MODES - 1][1] = rt_res_h; }

static void put_mode(uint32_t m, int i, uint32_t fmt) {
    sync_modes();
    MEM32(m) = modes[i][0];
    MEM32(m + 4) = modes[i][1];
    MEM32(m + 8) = 60;
    MEM32(m + 12) = fmt;
}

METHOD(d3d_AddRef, 1) { return 1; }
METHOD(d3d_Release, 1) { return 0; }
METHOD(d3d_GetAdapterCount, 1) { return 1; }
METHOD(d3d_GetAdapterIdentifier, 4) {
    uint32_t id = ARG(3);
    memset(GPTR(id), 0, 1068);
    strcpy(GPTR(id), "sh3-nx");
    strcpy(GPTR(id + 512), "OpenGL");
    MEM32(id + 1024) = 0x00010001;  /* DriverVersion */
    MEM32(id + 1028) = 0x0006000E;
    MEM32(id + 1032) = 0x10DE;      /* an NVIDIA vendor id keeps vendor-specific paths on their safe side */
    MEM32(id + 1036) = 0x0200;
    return 0;
}
/* Two formats per resolution: X8R8G8B8 then R5G6B5. */
METHOD(d3d_GetAdapterModeCount, 2) { return N_MODES * 2; }
METHOD(d3d_EnumAdapterModes, 4) {
    uint32_t i = ARG(2);
    if (i >= N_MODES * 2) return D3DERR_INVALIDCALL;
    put_mode(ARG(3), i % N_MODES, i < N_MODES ? FMT_X8R8G8B8 : FMT_R5G6B5);
    return 0;
}
METHOD(d3d_GetAdapterDisplayMode, 3) { put_mode(ARG(2), 3, FMT_X8R8G8B8); return 0; }
METHOD(d3d_CheckDeviceType, 6) { return 0; }
METHOD(d3d_CheckDeviceFormat, 7) {
    uint32_t fmt = ARG(6);
    switch (fmt) {
    case FMT_A8R8G8B8: case FMT_X8R8G8B8: case FMT_R5G6B5: case FMT_X1R5G5B5: case FMT_A1R5G5B5:
    case FMT_A4R4G4B4: case FMT_A8: case FMT_L8: case FMT_A8L8: case FMT_DXT1: case FMT_DXT2:
    case FMT_DXT3: case FMT_DXT4: case FMT_DXT5: case FMT_D16: case FMT_D24S8: case FMT_D24X8:
    case FMT_D32: case FMT_D16_LOCKABLE: case FMT_P8:
        return 0;
    }
    return D3DERR_NOTAVAILABLE;
}
METHOD(d3d_CheckDeviceMultiSampleType, 6) { return ARG(5) ? D3DERR_NOTAVAILABLE : 0; }
METHOD(d3d_CheckDepthStencilMatch, 6) { return 0; }

/* A GeForce3/4-class card with vs/ps 1.1, the hardware the PC version was made for. */
static void fill_caps(uint32_t c) {
    memset(GPTR(c), 0, 212);
    uint32_t v[53] = {0};
    v[0] = 1;                     /* D3DDEVTYPE_HAL */
    v[2] = 0x20000;               /* Caps: D3DCAPS_READ_SCANLINE */
    v[3] = 0x20000000 | 0x40000000;
    v[5] = 0x80000000u | 1 | 2;   /* PresentationIntervals: IMMEDIATE, ONE, TWO */
    v[7] = 0x10 | 0x40 | 0x80 | 0x200 | 0x400 | 0x2000 | 0x4000 | 0x10000 | 0x80000 | 0x100000;  /* DevCaps */
    v[8] = 0x2 | 0x4 | 0x8 | 0x10 | 0x20 | 0x40 | 0x80 | 0x100 | 0x400 | 0x800;              /* PrimitiveMiscCaps */
    v[9] = 0x1 | 0x10 | 0x80 | 0x100 | 0x2000 | 0x4000 | 0x8000 | 0x10000 | 0x20000 | 0x100000 | 0x200000;
    v[10] = v[13] = 0xFF;         /* ZCmpCaps, AlphaCmpCaps */
    v[11] = v[12] = 0x1FFF;       /* Src/DestBlendCaps */
    v[14] = 0x8 | 0x200 | 0x4000 | 0x80000;
    /* TextureCaps. No D3DPTEXTURECAPS_POW2 (0x2): GL takes any size, and with it the game rounds a
     * texture up to a power of two (the Enhanced Edition's 2732x2048 title became 4096x2048 with the
     * art misplaced). The modern GPUs EE runs on report no POW2 either. */
    /* D3DPTEXTURECAPS_ALPHAPALETTE (0x80): a P8 palette's flags byte is the alpha (d3d8_res.c). */
    v[15] = 0x1 | 0x4 | 0x8 | 0x40 | 0x80 | 0x400 | 0x800 | 0x4000 | 0x8000 | 0x10000;
    v[16] = v[17] = v[18] = 0x0100 | 0x0200 | 0x0400 | 0x010000 | 0x020000 | 0x01000000 | 0x02000000;
    v[19] = v[20] = 0x1F;         /* TextureAddressCaps */
    v[21] = 0x1F;
    v[22] = v[23] = 4096;
    v[24] = 512;
    v[25] = 8192;
    v[26] = 4096;
    v[27] = 8;
    v[28] = u32(1e10f);
    v[29] = u32(-1e8f); v[30] = u32(-1e8f); v[31] = u32(1e8f); v[32] = u32(1e8f);
    v[34] = 0xFF;                 /* StencilCaps */
    v[35] = 8 | 0x80000;          /* FVFCaps: 8 texcoords, PSIZE */
    v[36] = 0x3FFFFFF;            /* TextureOpCaps */
    v[37] = 8;                    /* MaxTextureBlendStages */
    v[38] = 4;                    /* MaxSimultaneousTextures */
    v[39] = 0x1 | 0x2 | 0x8 | 0x10 | 0x20 | 0x40;
    v[40] = 8;                    /* MaxActiveLights */
    v[41] = 6;
    v[42] = 4;
    v[43] = 0;
    v[44] = u32(64.0f);
    v[45] = 0xFFFFF;
    v[46] = 0xFFFFF;
    v[47] = 16;
    v[48] = 256;
    v[49] = 0xFFFE0101;           /* vs_1_1 */
    v[50] = 96;
    v[51] = 0xFFFF0101;           /* ps_1_1 */
    v[52] = u32(8.0f);
    memcpy(GPTR(c), v, 212);
}
METHOD(d3d_GetDeviceCaps, 4) { fill_caps(ARG(3)); return 0; }
METHOD(d3d_GetAdapterMonitor, 2) { return 1; }

static uint32_t device_vtable(void);
static void device_reset(uint32_t pp);

/* The GL context, made by the launcher or the first CreateDevice. */
void gl_context_open(void) {
    static SDL_GLContext ctx;
    if (ctx) return;
    /* SH3 Quest VR: OpenGL ES 3.2, the Quest's API, on every platform (the PC's driver makes an ES context too),
     * so the PC test build runs the headset's renderer. */
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    ctx = SDL_GL_CreateContext(rt_window);
    if (!ctx) rt_fatal("no OpenGL ES 3.2 context (the graphics driver must offer OpenGL ES 3.2): %s", SDL_GetError());
    SDL_GL_SetSwapInterval(1);
    gl_load();
    /* SH3 Quest VR: the headset, once there is GL: the game then draws each eye's size (its targets per eye). */
    if (sh3vr_start()) {
        SDL_GL_SetSwapInterval(0);  /* the headset paces the frames (xrWaitFrame), not the desktop window */
        sh3vr_eye_size(&rt_res_w, &rt_res_h);
        rt_log("VR: the game draws %ux%u an eye", rt_res_w, rt_res_h);
    }
}

METHOD(d3d_CreateDevice, 7) {
    uint32_t pp = ARG(5);
    rt_log("CreateDevice %ux%u fmt %u windowed %u depth %u/%u flags %08X", MEM32(pp), MEM32(pp + 4),
           MEM32(pp + 8), MEM32(pp + 28), MEM32(pp + 32), MEM32(pp + 36), ARG(4));
    if (!dev) {
        gl_context_open();
        /* SH3 Quest VR: no glClipControl in ES. The vertex stage flips y and maps D3D's [0,1] depth to GL's
         * [-1,1] (d3d8_shader.c), which stores render targets top row first as before. */
        dev = calloc(1, sizeof *dev);
        dev->obj = com_new(device_vtable(), 16);
        res_init_vtables();
        glGenFramebuffers(1, &dev->fbo);
        glGenVertexArrays(1, &dev->vao);
        glBindVertexArray(dev->vao);
        glGenBuffers(1, &dev->stream_buf);
        glGenBuffers(1, &dev->index_buf);
    }
    device_reset(pp);
    /* Like D3D8 on Windows: without D3DCREATE_FPU_PRESERVE the creating thread's x87 is switched to
     * single precision, and the whole game ran its float logic that way. */
    if (!(ARG(4) & 2)) g_fp_control_word &= ~0x300u;
    MEM32(ARG(6)) = dev->obj;
    return 0;
}

static const ComEntry d3d_vt[] = {
    {"IDirect3D8::QueryInterface", NULL}, {"IDirect3D8::AddRef", d3d_AddRef},
    {"IDirect3D8::Release", d3d_Release}, {"IDirect3D8::RegisterSoftwareDevice", NULL},
    {"IDirect3D8::GetAdapterCount", d3d_GetAdapterCount}, {"IDirect3D8::GetAdapterIdentifier", d3d_GetAdapterIdentifier},
    {"IDirect3D8::GetAdapterModeCount", d3d_GetAdapterModeCount}, {"IDirect3D8::EnumAdapterModes", d3d_EnumAdapterModes},
    {"IDirect3D8::GetAdapterDisplayMode", d3d_GetAdapterDisplayMode}, {"IDirect3D8::CheckDeviceType", d3d_CheckDeviceType},
    {"IDirect3D8::CheckDeviceFormat", d3d_CheckDeviceFormat},
    {"IDirect3D8::CheckDeviceMultiSampleType", d3d_CheckDeviceMultiSampleType},
    {"IDirect3D8::CheckDepthStencilMatch", d3d_CheckDepthStencilMatch}, {"IDirect3D8::GetDeviceCaps", d3d_GetDeviceCaps},
    {"IDirect3D8::GetAdapterMonitor", d3d_GetAdapterMonitor}, {"IDirect3D8::CreateDevice", d3d_CreateDevice},
};

WINAPI(Direct3DCreate8, "Direct3DCreate8", 1) {
    if (!d3d_obj) d3d_obj = com_new(com_vtable(d3d_vt, sizeof d3d_vt / sizeof *d3d_vt), 8);
    return d3d_obj;
}

/* ---- device state ---- */

static const float identity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

static void set_defaults(int autodepth) {
    memset(dev->rs, 0, sizeof dev->rs);
    uint32_t *rs = dev->rs;
    rs[7] = autodepth;            /* ZENABLE */
    rs[8] = 3;                    /* FILLMODE solid */
    rs[9] = 2;                    /* SHADEMODE gouraud */
    rs[14] = 1;                   /* ZWRITEENABLE */
    rs[16] = 1;                   /* LASTPIXEL */
    rs[19] = 2; rs[20] = 1;       /* SRCBLEND one, DESTBLEND zero */
    rs[22] = 3;                   /* CULLMODE ccw */
    rs[23] = 4;                   /* ZFUNC lessequal */
    rs[25] = 8;                   /* ALPHAFUNC always */
    rs[37] = u32(1.0f);           /* FOGEND */
    rs[38] = u32(1.0f);           /* FOGDENSITY */
    rs[53] = rs[54] = rs[55] = 1; /* stencil ops keep */
    rs[56] = 8;                   /* STENCILFUNC always */
    rs[58] = rs[59] = 0xFFFFFFFFu;
    rs[60] = 0xFFFFFFFFu;         /* TEXTUREFACTOR */
    rs[136] = 1;                  /* CLIPPING */
    rs[137] = 1;                  /* LIGHTING */
    rs[141] = 1;                  /* COLORVERTEX */
    rs[142] = 1;                  /* LOCALVIEWER */
    rs[145] = 1;                  /* DIFFUSEMATERIALSOURCE color1 */
    rs[146] = 2;                  /* SPECULARMATERIALSOURCE color2 */
    rs[154] = u32(1.0f);          /* POINTSIZE */
    rs[158] = u32(1.0f);          /* POINTSCALE_A */
    rs[162] = 0xFFFFFFFFu;        /* MULTISAMPLEMASK */
    rs[166] = u32(64.0f);         /* POINTSIZE_MAX */
    rs[168] = 0xF;                /* COLORWRITEENABLE */
    rs[171] = 1;                  /* BLENDOP add */
    for (int s = 0; s < MAX_STAGES; s++) {
        uint32_t *t = dev->tss[s];
        memset(t, 0, TSS_COUNT * 4);
        t[1] = s ? 1 : 4;         /* COLOROP: modulate on stage 0, disable after */
        t[2] = 2; t[3] = 1;       /* COLORARG1 texture, COLORARG2 current */
        t[4] = s ? 1 : 2;         /* ALPHAOP: selectarg1 on stage 0 */
        t[5] = 2; t[6] = 1;
        t[11] = s;                /* TEXCOORDINDEX */
        t[13] = t[14] = t[25] = 1;/* ADDRESS wrap */
        t[16] = t[17] = 1;        /* MAG/MINFILTER point */
        t[21] = 1;                /* MAXANISOTROPY */
        t[26] = t[27] = 1;        /* ARG0 current */
        t[28] = 1;                /* RESULTARG current */
        memcpy(dev->texm[s], identity, 64);
    }
    for (int i = 0; i < 4; i++) memcpy(dev->world[i], identity, 64);
    memcpy(dev->view, identity, 64);
    memcpy(dev->proj, identity, 64);
    memset(dev->material, 0, sizeof dev->material);
    memset(dev->light_on, 0, sizeof dev->light_on);
    memset(dev->tex, 0, sizeof dev->tex);
    memset(dev->stream, 0, sizeof dev->stream);
    dev->ib = dev->vs = dev->ps = 0;
}

static void device_reset(uint32_t pp) {
    uint32_t w = MEM32(pp), h = MEM32(pp + 4);
    if (!w || !h) { w = 1280; h = 720; }
    dev->bb_w = w;
    dev->bb_h = h;
    if (dev->rt) res_release(dev->rt);
    if (dev->ds) res_release(dev->ds);
    if (dev->backbuffer) res_release(dev->backbuffer);
    if (dev->autodepth) res_release(dev->autodepth);
    dev->backbuffer = res_surface(w, h, FMT_A8R8G8B8, 1, 0);
    dev->autodepth = MEM32(pp + 32) ? res_surface(w, h, MEM32(pp + 36), 0, 1) : 0;
    /* Bound as the render target and depth surface too: each binding holds its own reference. */
    dev->rt = dev->backbuffer;
    res_addref(dev->rt);
    dev->ds = dev->autodepth;
    if (dev->ds) res_addref(dev->ds);
    set_defaults(dev->autodepth != 0);
    dev->vp_x = dev->vp_y = 0;
    dev->vp_w = w;
    dev->vp_h = h;
    dev->vp_minz = 0;
    dev->vp_maxz = 1;
    res_bind_targets();
}

/* ---- IDirect3DDevice8 ---- */

METHOD(dev_QueryInterface, 3) { return E_NOINTERFACE; }
METHOD(dev_AddRef, 1) { return 1; }
METHOD(dev_Release, 1) { return 0; }
METHOD(dev_TestCooperativeLevel, 1) { return 0; }
METHOD(dev_GetAvailableTextureMem, 1) { return 256u << 20; }
METHOD(dev_ResourceManagerDiscardBytes, 2) { return 0; }
METHOD(dev_GetDirect3D, 2) { MEM32(ARG(1)) = d3d_obj; return 0; }
METHOD(dev_GetDeviceCaps, 2) { fill_caps(ARG(1)); return 0; }
METHOD(dev_GetDisplayMode, 2) {
    uint32_t m = ARG(1);
    MEM32(m) = dev->bb_w; MEM32(m + 4) = dev->bb_h; MEM32(m + 8) = 60; MEM32(m + 12) = FMT_X8R8G8B8;
    return 0;
}
METHOD(dev_GetCreationParameters, 2) {
    uint32_t p = ARG(1);
    MEM32(p) = 0; MEM32(p + 4) = 1; MEM32(p + 8) = 0x00010001; MEM32(p + 12) = 0x40;
    return 0;
}
METHOD(dev_SetCursorProperties, 4) { return 0; }
METHOD(dev_SetCursorPosition, 4) { return 0; }
METHOD(dev_ShowCursor, 2) { return 0; }
METHOD(dev_Reset, 2) { device_reset(ARG(1)); return 0; }

/* SH3 Quest VR: the frame camera's draws counted by depth surface; at Present the one most drawn into is the scene's
 * depth (run vr28: the device's own depth held one flat value, a full-screen pass's, so VR's probes found no walls). */
void note_scene_depth(void) {
    dev->scene_rt = dev->rt;
    if (!dev->ds) return;
    for (int i = 0; i < 4; i++) {
        if (dev->scene_ds[i] == dev->ds) { dev->scene_draws[i]++; return; }
        if (!dev->scene_ds[i]) { dev->scene_ds[i] = dev->ds; dev->scene_draws[i] = 1; return; }
    }
}
static Res *scene_depth(void) {
    int best = -1;
    for (int i = 0; i < 4; i++)
        if (dev->scene_ds[i] && (best < 0 || dev->scene_draws[i] > dev->scene_draws[best])) best = i;
    uint32_t pick = best >= 0 ? dev->scene_ds[best] : dev->autodepth;
    int draws = best >= 0 ? dev->scene_draws[best] : 0;
    memset(dev->scene_ds, 0, sizeof dev->scene_ds);
    memset(dev->scene_draws, 0, sizeof dev->scene_draws);
    Res *d = res_of(pick);
    if (!d || res_layers(d) != 2) d = res_of(dev->autodepth), pick = dev->autodepth;
    static uint32_t logged;
    if (d && pick != logged) {
        logged = pick;
        Res *c = res_of(dev->scene_rt);
        rt_log("VR: the scene's depth for the probes: surface %08x (%dx%d, %d draws of the frame camera, last into a "
               "%dx%d picture)%s", pick,
               (int)d->w, (int)d->h, draws, c ? (int)c->w : 0, c ? (int)c->h : 0,
               pick == dev->autodepth ? ", the device's own" : "");
    }
    return d;
}

/* LIST 5 (movies, src/game/movie.c): the playing movie's latest picture over the frame, its own shape kept inside the
 * frame with black round it, in each eye's layer (VR shows the frame on the cinema screen while a movie plays). The
 * game's own drawing of it went through DirectShow, which the port does not have. */
const uint8_t *sh3_movie_picture(int *w, int *h, double *aspect, int *serial);
static void movie_draw(Res *bb) {
    int mw, mh, serial;
    double aspect;
    const uint8_t *px = sh3_movie_picture(&mw, &mh, &aspect, &serial);
    if (!px || !bb || mw <= 0 || mh <= 0) return;
    static GLuint tex, fbo[2];
    static int tw, th, shown = -1;
    if (!tex) {
        glGenTextures(1, &tex);
        glGenFramebuffers(2, fbo);
    }
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    if (tw != mw || th != mh) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, mw, mh, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        tw = mw;
        th = mh;
        rt_log("movie: pictures %dx%d drawn into the %ux%u frame (%d layer(s))", mw, mh, bb->w, bb->h, res_layers(bb));
    } else if (serial != shown) glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, mw, mh, GL_RGBA, GL_UNSIGNED_BYTE, px);
    shown = serial;
    glBindTexture(GL_TEXTURE_2D, 0);
    /* both are top row first (D3D's way), so the picture goes across unflipped */
    const float pw = (float)(mh * aspect), s = fminf((float)bb->w / pw, (float)bb->h / mh);
    const int dw = (int)(pw * s), dh = (int)(mh * s), dx = ((int)bb->w - dw) / 2, dy = ((int)bb->h - dh) / 2;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo[0]);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo[1]);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(1, 1, 1, 1);
    glClearColor(0, 0, 0, 1);
    const int layers = res_layers(bb) == 2 ? 2 : 1;
    for (int l = 0; l < layers; l++) {
        if (layers == 2) glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, bb->tex, 0, l);
        else glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bb->tex, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        glBlitFramebuffer(0, 0, mw, mh, dx, dy, dx + dw, dy + dh, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, dev->fbo);
}

METHOD(dev_Present, 5) {
    extern __thread int rt_render_thread;  /* kernel32.c: the slow-call log watches this thread (0.1.5) */
    rt_render_thread = 1;
    int ww, wh;
    SDL_GL_GetDrawableSize(rt_window, &ww, &wh);
    Res *bb = res_of(dev->backbuffer);
    movie_draw(bb);
    /* SH3 Quest VR: the frame to the headset (each eye's layer, the panel), the next frame's poses and pad; the
     * desktop window then shows the left eye. The read framebuffer is a spare: dev->fbo keeps its multiview targets. */
    static GLuint present_fbo;
    if (!present_fbo) glGenFramebuffers(1, &present_fbo);
    if (sh3vr_active()) {
        sh3_vr_frame();
        Res *depth = scene_depth();  /* the eyes' depth: VR's hands, probes and the head-in-wall fade */
        {
            /* session 11: the world drawn or not (a full screen next frame out of play), told when it changes */
            static int was_screen = -1;
            const int screen_now = frame_screen > 0;
            if (screen_now != was_screen)
                rt_log("VR: frame %d: %s (this frame: %d draws with the frame camera, %d with others, %d of the 2D)",
                       rt_frame, screen_now ? "a full screen: the game's picture whole on the panel"
                                            : "the world's frames: the 2D on the panel, the world in the eyes",
                       frame_world, frame_other, frame_screen);
            was_screen = screen_now;
            {
                /* owner 79 (session 16): the drifting fog seen or gone, told when it changes (how many, where) */
                static int had_fog = -1;
                if ((frame_fog > 0) != had_fog) {
                    had_fog = frame_fog > 0;
                    rt_log("VR: frame %d: %s", rt_frame, !had_fog ? "no drifting fog"
                           : !sh3vr_drifting_fog() ? "the game's drifting fog left out (the VR option Drifting fog off)"
                           : sh3vr_world_effects() ? "the game's drifting fog on a far layer in the world, each eye's view"
                                                   : "the game's drifting fog as drawn (no eye corrections yet)");
                }
                frame_fog = 0;
            }
            sh3vr_frame_world(frame_world > 0);
            world_last = frame_world > 0;
            frame_world = frame_other = frame_screen = 0;
        }
        sh3vr_present(bb->tex, depth && res_layers(depth) == 2 ? depth->tex : 0, (int)bb->w, (int)bb->h);
        panel_has_picture = 0;
        if (sh3vr_wants_exit()) { rt_log("VR: the headset ended the session: leaving"); rt_leave(0); }
        sh3_vr_after_present();
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER, present_fbo);
    if (res_layers(bb) == 2) glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, bb->tex, 0, 0);
    else glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bb->tex, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    /* Keep the game's aspect ratio inside the window; rows flip because the window's row 0 is its bottom. */
    float s = fminf((float)ww / bb->w, (float)wh / bb->h);
    int dw = bb->w * s, dh = bb->h * s, dx = (ww - dw) / 2, dy = (wh - dh) / 2;
    glBlitFramebuffer(0, 0, bb->w, bb->h, dx, dy + dh, dx + dw, dy, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    res_snapshot_front();
    static int frame;
    rt_frame++;
    /* debugging: SH3_SHOT=dir writes frames 0-9, then every SH3_SHOT_EVERY-th (30), as PPM */
    const char *shot = RT_ENV("SH3_SHOT"), *every = RT_ENV("SH3_SHOT_EVERY");
    if (shot && (frame++ < 10 || frame % (every ? atoi(every) : 30) == 1)) {
        char path[512];
        snprintf(path, sizeof path, "%s/frame%05d.ppm", shot, frame - 1);
        uint8_t *px = malloc(bb->w * bb->h * 4);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, bb->w, bb->h, GL_RGBA, GL_UNSIGNED_BYTE, px);  /* ES reads RGBA only */
        for (uint32_t i = 0; i < bb->w * bb->h; i++) memmove(px + 3 * i, px + 4 * i, 3);
        FILE *f = fopen(path, "wb");
        if (f) { fprintf(f, "P6 %u %u 255\n", bb->w, bb->h); fwrite(px, 3, bb->w * bb->h, f); fclose(f); }
        free(px);
    }
    SDL_GL_SwapWindow(rt_window);
    static Uint64 stat_t0;
    static int stat_frames;
    if (RT_ENV("SH3_STATS") && ++stat_frames == 120) {  /* debugging: FPS and guest heap */
        Uint64 now = SDL_GetTicks64();
        if (stat_t0) rt_log("stats: %.1f fps, guest heap %u MB", 120000.0 / (now - stat_t0), rt_heap_used() >> 20);
        stat_t0 = now;
        stat_frames = 0;
    }
    /* debugging: SH3_POKE=frame:va=value writes a guest dword at that frame */
    const char *poke = RT_ENV("SH3_POKE");
    unsigned pf, pva, pval;
    if (poke && sscanf(poke, "%u:%x=%x", &pf, &pva, &pval) == 3 && rt_frame >= (int)pf && rt_frame < (int)pf + 30) {
        MEM32(pva) = pval;
        if (rt_frame == (int)pf) rt_log("poke %08X = %08X", pva, pval);
    }
    /* debugging: SH3_PEEK=va,va,... logs those guest dwords every 60 frames */
    const char *peek = RT_ENV("SH3_PEEK");
    if (peek && rt_frame % 60 == 0) {
        char line[512];
        int n = snprintf(line, sizeof line, "peek frame %d:", rt_frame);
        for (const char *p = peek; *p && n < (int)sizeof line - 24; p += strcspn(p, ","), p += *p == ',') {
            uint32_t va = (uint32_t)strtoul(p, NULL, 16);
            n += snprintf(line + n, sizeof line - n, " %08X=%08X", va, MEM32(va));
        }
        rt_log("%s", line);
    }
    rt_pump_events();
    res_bind_targets();
    return 0;
}
METHOD(dev_GetBackBuffer, 4) { res_addref(dev->backbuffer); MEM32(ARG(3)) = dev->backbuffer; return 0; }
METHOD(dev_GetRasterStatus, 2) { MEM32(ARG(1)) = 1; MEM32(ARG(1) + 4) = 0; return 0; }
METHOD(dev_SetGammaRamp, 3) { return 0; }
METHOD(dev_GetGammaRamp, 2) {
    for (int c = 0; c < 3; c++)
        for (int i = 0; i < 256; i++) *(uint16_t *)GPTR(ARG(1) + c * 512 + i * 2) = i * 257;
    return 0;
}
METHOD(dev_CreateTexture, 8) {
    /* The game's render targets in the log with the code that made them (each eye's picture sizes, LIST 6/33). */
    if (ARG(4) & 3) rt_log("d3d8: the game made a %ux%u %s (format %u) from %08x", ARG(1), ARG(2),
                           ARG(4) & 1 ? "render target texture" : "depth texture", ARG(5), MEM32(g_esp));
    MEM32(ARG(7)) = res_texture(ARG(1), ARG(2), ARG(3), ARG(4), ARG(5), ARG(6), 0);
    return MEM32(ARG(7)) ? 0 : D3DERR_INVALIDCALL;
}
METHOD(dev_CreateCubeTexture, 7) {
    MEM32(ARG(6)) = res_texture(ARG(1), ARG(1), ARG(2), ARG(3), ARG(4), ARG(5), 1);
    return MEM32(ARG(6)) ? 0 : D3DERR_INVALIDCALL;
}
METHOD(dev_CreateVertexBuffer, 6) { MEM32(ARG(5)) = res_buffer(RES_VB, ARG(1), ARG(2), ARG(3), ARG(4)); return 0; }
METHOD(dev_CreateIndexBuffer, 6) { MEM32(ARG(5)) = res_buffer(RES_IB, ARG(1), ARG(2), ARG(3), ARG(4)); return 0; }
METHOD(dev_CreateRenderTarget, 7) {
    rt_log("d3d8: the game made a %ux%u render target surface (format %u) from %08x", ARG(1), ARG(2), ARG(3), MEM32(g_esp));
    MEM32(ARG(6)) = res_surface(ARG(1), ARG(2), ARG(3), 1, 0);
    return 0;
}
METHOD(dev_CreateDepthStencilSurface, 6) {
    rt_log("d3d8: the game made a %ux%u depth surface (format %u) from %08x", ARG(1), ARG(2), ARG(3), MEM32(g_esp));
    MEM32(ARG(5)) = res_surface(ARG(1), ARG(2), ARG(3), 0, 1);
    return 0;
}
METHOD(dev_CreateImageSurface, 5) { MEM32(ARG(4)) = res_surface(ARG(1), ARG(2), ARG(3), 0, 0); return 0; }
METHOD(dev_SetRenderTarget, 3) {
    if (ARG(1)) { res_addref(ARG(1)); if (dev->rt) res_release(dev->rt); dev->rt = ARG(1); }
    if (ARG(2)) res_addref(ARG(2));
    if (dev->ds) res_release(dev->ds);
    dev->ds = ARG(2);
    res_bind_targets();
    Res *r = res_of(dev->rt);  /* D3D8 resets the viewport to the new target */
    {
        /* testing (session 10, LIST 56: where the flashlight's shadow is drawn): in SH3_VSC_DUMP's frame, each change of
         * target and the game's code that made it */
        static int at = -2;
        if (at == -2) at = getenv("SH3_VSC_DUMP") ? atoi(getenv("SH3_VSC_DUMP")) : -1;
        if (at >= 0 && rt_frame == at)
            rt_log("vsc target: frame %d: %08X (%ux%u), depth %08X, from %08x", rt_frame, dev->rt, r ? r->w : 0,
                   r ? r->h : 0, dev->ds, MEM32(g_esp));
    }
    dev->vp_x = dev->vp_y = 0;
    dev->vp_w = r->w;
    dev->vp_h = r->h;
    return 0;
}
METHOD(dev_GetRenderTarget, 2) { res_addref(dev->rt); MEM32(ARG(1)) = dev->rt; return 0; }
METHOD(dev_GetDepthStencilSurface, 2) {
    if (dev->ds) res_addref(dev->ds);
    MEM32(ARG(1)) = dev->ds;
    return dev->ds ? 0 : 0x88760870u;  /* D3DERR_NOTFOUND */
}
METHOD(dev_BeginScene, 1) { return 0; }
METHOD(dev_EndScene, 1) { return 0; }

static void apply_viewport_scissor(int scissor);

METHOD(dev_Clear, 7) {
    uint32_t flags = ARG(3), c = ARG(4);
    GLbitfield mask = 0;
    if (flags & 1) {
        glClearColor((c >> 16 & 255) / 255.f, (c >> 8 & 255) / 255.f, (c & 255) / 255.f, (c >> 24) / 255.f);
        glColorMask(1, 1, 1, 1);
        mask |= GL_COLOR_BUFFER_BIT;
    }
    if ((flags & 2) && dev->ds) { glClearDepthf(f32(ARG(5))); glDepthMask(1); mask |= GL_DEPTH_BUFFER_BIT; }
    if ((flags & 4) && dev->ds) { glClearStencil(ARG(6)); glStencilMask(0xFF); mask |= GL_STENCIL_BUFFER_BIT; }
    apply_viewport_scissor(1);
    if (ARG(1) && ARG(2))
        for (uint32_t i = 0; i < ARG(1); i++) {
            int32_t *r = GPTR(ARG(2) + 16 * i);  /* D3DRECT x1 y1 x2 y2 */
            glScissor(r[0], r[1], r[2] - r[0], r[3] - r[1]);
            glClear(mask);
        }
    else glClear(mask);
    return 0;
}

/* Transforms in state block masks: view, projection, texture 0-7, world 0-3. */
static int transform_id(uint32_t t) {
    return t == 2 ? 0 : t == 3 ? 1 : t >= 16 && t < 24 ? 2 + (int)(t - 16) : t >= 256 && t < 260 ? 10 + (int)(t - 256) : -1;
}
static float *transform_slot(uint32_t t) {
    if (t == 2) return dev->view;
    if (t == 3) return dev->proj;
    if (t >= 16 && t < 24) return dev->texm[t - 16];
    if (t >= 256 && t < 260) return dev->world[t - 256];
    return NULL;
}
METHOD(dev_SetTransform, 3) {
    float *m = transform_slot(ARG(1));
    if (m) { memcpy(m, GPTR(ARG(2)), 64); MARK(xf[transform_id(ARG(1))]); }
    return 0;
}
METHOD(dev_GetTransform, 3) {
    float *m = transform_slot(ARG(1));
    memcpy(GPTR(ARG(2)), m ? m : identity, 64);
    return 0;
}
METHOD(dev_MultiplyTransform, 3) {
    float *m = transform_slot(ARG(1)), r[16];
    const float *b = GPTR(ARG(2));
    if (!m) return 0;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            r[i * 4 + j] = m[i * 4] * b[j] + m[i * 4 + 1] * b[4 + j] + m[i * 4 + 2] * b[8 + j] + m[i * 4 + 3] * b[12 + j];
    memcpy(m, r, 64);
    MARK(xf[transform_id(ARG(1))]);
    return 0;
}
METHOD(dev_SetViewport, 2) {
    uint32_t v = ARG(1);
    dev->vp_x = MEM32(v); dev->vp_y = MEM32(v + 4); dev->vp_w = MEM32(v + 8); dev->vp_h = MEM32(v + 12);
    dev->vp_minz = MEMF(v + 16); dev->vp_maxz = MEMF(v + 20);
    MARK(vp);
    return 0;
}
METHOD(dev_GetViewport, 2) {
    uint32_t v = ARG(1);
    MEM32(v) = dev->vp_x; MEM32(v + 4) = dev->vp_y; MEM32(v + 8) = dev->vp_w; MEM32(v + 12) = dev->vp_h;
    MEMF(v + 16) = dev->vp_minz; MEMF(v + 20) = dev->vp_maxz;
    return 0;
}
METHOD(dev_SetMaterial, 2) { memcpy(dev->material, GPTR(ARG(1)), 68); MARK(material); return 0; }
METHOD(dev_GetMaterial, 2) { memcpy(GPTR(ARG(1)), dev->material, 68); return 0; }
/* D3DLIGHT8: Type, Diffuse, Specular, Ambient, Position, Direction, Range, Falloff, Att0-2, Theta, Phi. */
METHOD(dev_SetLight, 3) {
    if (ARG(1) < 8) {
        MARK(light[ARG(1)]);
        Light *l = &dev->light[ARG(1)];
        uint32_t p = ARG(2);
        l->type = MEM32(p);
        memcpy(l->diffuse, GPTR(p + 4), 16);
        memcpy(l->specular, GPTR(p + 20), 16);
        memcpy(l->ambient, GPTR(p + 36), 16);
        memcpy(l->pos, GPTR(p + 52), 12);
        memcpy(l->dir, GPTR(p + 64), 12);
        memcpy(&l->range, GPTR(p + 76), 28);
    }
    return 0;
}
METHOD(dev_LightEnable, 3) { if (ARG(1) < 8) { dev->light_on[ARG(1)] = ARG(2) != 0; MARK(light_on[ARG(1)]); } return 0; }
METHOD(dev_GetLightEnable, 3) { MEM32(ARG(2)) = ARG(1) < 8 && dev->light_on[ARG(1)]; return 0; }
METHOD(dev_SetClipPlane, 3) { return 0; }
/* Film grain (session 7; the owner: "sh3's built in noise/film grain effect should be disabled by default. can be
 * re-enabled via in game settings as needed"). The game's noise is an alpha test: with its Noise Effect switch on
 * (0x72C839, set for play) the screen pass 0x41C090 has 0x427ED0 turn the alpha test on (ALPHAREF from 0x72C83B), so
 * its stencil pass keeps only the pixels whose alpha passes, and the scene's colour pass (the game's grade, the texture
 * times its factor, doubled) lands on those alone: the grain. Holding that one alpha test off (MEM32(g_esp), the
 * return address, is 0x427ED0's call) lets the stencil take the whole screen and the grade cover every pixel, as the
 * game's own look without the grain (run vr108: the switch itself held off took the grade away too, the picture greyed
 * from a mean of 14 to 36). The option "Film grain" puts it back. */
/* LIST 56 (session 12): the game camera its shadow volumes were last drawn with, and in which frame (draw_setup) */
static float vol_view[16], vol_proj[16];
static int vol_frame = -1;
static void vr_her_shadow(void);
static void her_shadow_ready(void);
/* LIST 56 (session 12): her part draws under way (sh3.c's 0x45ED40 wrapper, first person): their pixels marked */
static int her_mark;
void d3d8_her_mark(int on) { her_mark = on; }
METHOD(dev_SetRenderState, 3) {
    uint32_t v = ARG(2);
    if (ARG(1) == 0xF /* D3DRS_ALPHATESTENABLE */ && v && MEM32(g_esp) == 0x00427EF0u && !sh3vr_film_grain()) v = 0;
    {
        /* testing (session 12, LIST 56: which of the game's code draws with the stencil on, the shadow leads in
         * GAME_FACTS "Shadows"): in SH3_VSC_DUMP's frame, each time the stencil is turned on (state 52) and the code
         * that did it (its return address) */
        static int at = -2;
        if (at == -2) at = getenv("SH3_VSC_DUMP") ? atoi(getenv("SH3_VSC_DUMP")) : -1;
        if (at >= 0 && rt_frame == at && ARG(1) == 52 && v && !dev->rs[52]) {
            rt_log("vsc stencil: frame %d: on from %08x (func %u ref %u pass %u zfail %u)", rt_frame, MEM32(g_esp),
                   dev->rs[56], dev->rs[57], dev->rs[55], dev->rs[54]);
            /* session 12 (owner message 51: "pretty sure theres body shadows for her in the normal game at SOME
             * points"): the shadow volumes' own list as 0x491320 hands it to 0x491030 (0x48B930's caller: its return
             * 0x49138E at [esp+16], 0x491320's list at [esp+24]): each 16-byte entry (pointer, type 0-8, ?, end) and,
             * for a pointer, its +0xC (a model's number: Heather's is 73) */
            if (MEM32(g_esp) == 0x0048B97Eu && MEM32(g_esp + 16) == 0x0049138Eu) {
                const uint32_t list = MEM32(g_esp + 24);
                rt_log("vsc shadow list: frame %d: %08x (0x491320 called from %08x), head %08x %08x %08x %08x", rt_frame,
                       list, MEM32(g_esp + 20), list ? MEM32(list) : 0, list ? MEM32(list + 4) : 0,
                       list ? MEM32(list + 8) : 0, list ? MEM32(list + 12) : 0);
                for (int i = 0; list && i <= 48; i++) {
                    const uint32_t e = list + 16u * (uint32_t)i, p = MEM32(e);
                    const int ok = p >= 0x400000u && p < 0x7FFF0000u && MEM32(e + 4) <= 8; /* as the walker takes it */
                    rt_log("  shadow entry %d: %08x type %u %08x end %08x; +0C %08x +1F8 %08x", i, p, MEM32(e + 4),
                           MEM32(e + 8), MEM32(e + 12), ok ? MEM32(p + 0xC) : 0, ok ? MEM32(p + 0x1F8) : 0);
                    if (ok && (MEM32(e + 4) == 0 || MEM32(e + 4) == 2))  /* its three rows (0x491030: +1C0, +1A0, +1B0) */
                        rt_log("    rows %.2f %.2f %.2f %.2f / %.2f %.2f %.2f %.2f / %.2f %.2f %.2f %.2f", MEMF(p + 0x1C0),
                               MEMF(p + 0x1C4), MEMF(p + 0x1C8), MEMF(p + 0x1CC), MEMF(p + 0x1A0), MEMF(p + 0x1A4),
                               MEMF(p + 0x1A8), MEMF(p + 0x1AC), MEMF(p + 0x1B0), MEMF(p + 0x1B4), MEMF(p + 0x1B8),
                               MEMF(p + 0x1BC));
                    if (MEM32(e + 12)) break;
                }
            }
        }
    }
    /* LIST 56 (session 12, owner message 51): the game's own character shadows (0x685C10 draws the list 0x4D1020 builds
     * from a room's shadow lights, 0x7F4C18/0x7F4C7C; the start room has none) told as they start and stop: Heather's
     * shadow from those lights is the game's (her animation's pose, her kg1 shadow model), for the playthrough to see */
    if (ARG(1) == 52 && v && !dev->rs[52] && MEM32(g_esp) == 0x0048B97Eu && MEM32(g_esp + 16) == 0x0049138Eu) {
        static int char_last = -1000;
        if (MEM32(g_esp + 20) == 0x00685C5Bu) {
            if (rt_frame - char_last > 30)
                rt_log("VR: frame %d: the game's own character shadows are on here (a room's shadow lights: list %08x)",
                       rt_frame, MEM32(g_esp + 24));
            char_last = rt_frame;
        }
    }
    /* LIST 56 (session 12): the game's shadow darkening turns the stencil test on here (0x48D469's call, after its
     * volumes were counted this frame). 0.1.3 (owner 84: "body ik shadows can be refined i feel, they seem a bit
     * overbearing as is"): Heather's volumes are made ready here and counted after the game's mask draw (her_mask_due,
     * the ROUTED draws), which then runs again at her own strength. */
    if (ARG(1) == 52 && v && !dev->rs[52] && MEM32(g_esp) == 0x0048D474u && vol_frame == rt_frame) her_shadow_ready();
    if (ARG(1) < RS_COUNT) { dev->rs[ARG(1)] = v; MARK(rs[ARG(1)]); }
    return 0;
}
METHOD(dev_GetRenderState, 3) { MEM32(ARG(2)) = ARG(1) < RS_COUNT ? dev->rs[ARG(1)] : 0; return 0; }
/* LIST 56 (session 12): whether the stencil test is on now (sh3.c's part draw tells whether her parts draw in a
 * stencil pass, a shadow's). */
int d3d8_stencil_on(void) { return dev && dev->rs[52] ? 1 : 0; }

/* ---- state blocks ---- */
typedef struct { uint32_t rs[RS_COUNT], tss[MAX_STAGES][TSS_COUNT]; float world[4][16], view[16], proj[16],
                 texm[MAX_STAGES][16], vsc[96][4], psc[8][4], material[17]; Light light[8]; int light_on[8];
                 uint32_t tex[MAX_STAGES], stream[16], stride[16], ib, base_vertex, vs, ps;
                 float vp[6]; Mask m; } Block;
static Block *blocks[1024];

/* Copies the block's marked states from the device (capture) or to it (apply). */
#define XFER(on, d, b) do { if (on) { if (apply) memcpy(&(d), &(b), sizeof(d)); else memcpy(&(b), &(d), sizeof(d)); } } while (0)
static void block_xfer(Block *b, int apply) {
    const Mask *m = &b->m;
    for (int i = 0; i < RS_COUNT; i++) XFER(m->rs[i], dev->rs[i], b->rs[i]);
    for (int s = 0; s < MAX_STAGES; s++) {
        for (int i = 0; i < TSS_COUNT; i++) XFER(m->tss[s][i], dev->tss[s][i], b->tss[s][i]);
        XFER(m->xf[2 + s], dev->texm[s], b->texm[s]);
        XFER(m->tex[s], dev->tex[s], b->tex[s]);
    }
    XFER(m->xf[0], dev->view, b->view);
    XFER(m->xf[1], dev->proj, b->proj);
    for (int i = 0; i < 4; i++) XFER(m->xf[10 + i], dev->world[i], b->world[i]);
    for (int i = 0; i < 96; i++) XFER(m->vsc[i], dev->vsc[i], b->vsc[i]);
    for (int i = 0; i < 8; i++) {
        XFER(m->psc[i], dev->psc[i], b->psc[i]);
        XFER(m->light[i], dev->light[i], b->light[i]);
        XFER(m->light_on[i], dev->light_on[i], b->light_on[i]);
    }
    XFER(m->material, dev->material, b->material);
    for (int i = 0; i < 16; i++) { XFER(m->stream[i], dev->stream[i], b->stream[i]); XFER(m->stream[i], dev->stride[i], b->stride[i]); }
    XFER(m->ib, dev->ib, b->ib);
    XFER(m->ib, dev->base_vertex, b->base_vertex);
    XFER(m->vs, dev->vs, b->vs);
    XFER(m->ps, dev->ps, b->ps);
    float vp[6] = {dev->vp_x, dev->vp_y, dev->vp_w, dev->vp_h, dev->vp_minz, dev->vp_maxz};
    XFER(m->vp, vp, b->vp);
    dev->vp_x = vp[0]; dev->vp_y = vp[1]; dev->vp_w = vp[2]; dev->vp_h = vp[3]; dev->vp_minz = vp[4]; dev->vp_maxz = vp[5];
}
#undef XFER

static uint32_t block_new(const Mask *m) {
    for (uint32_t i = 1; i < 1024; i++)
        if (!blocks[i]) {
            blocks[i] = calloc(1, sizeof(Block));
            if (m) blocks[i]->m = *m;
            else memset(&blocks[i]->m, 1, sizeof(Mask));  /* ponytail: every D3DSBT type captures everything */
            block_xfer(blocks[i], 0);
            return i;
        }
    rt_fatal("too many state blocks");
}
/* Recording leaves the device as it was: the Set* calls in between go to a scratch copy of it. */
static Dev rec_scratch, *rec_real;
static Mask rec_set;
METHOD(dev_BeginStateBlock, 1) {
    rec_scratch = *dev;
    rec_real = dev;
    dev = &rec_scratch;
    memset(&rec_set, 0, sizeof rec_set);
    rec_mask = &rec_set;
    return 0;
}
METHOD(dev_EndStateBlock, 2) {
    if (!rec_mask) return D3DERR_INVALIDCALL;
    MEM32(ARG(1)) = block_new(&rec_set);
    rec_mask = NULL;
    dev = rec_real;
    return 0;
}
METHOD(dev_CreateStateBlock, 3) { MEM32(ARG(2)) = block_new(NULL); return 0; }
METHOD(dev_ApplyStateBlock, 2) { if (ARG(1) < 1024 && blocks[ARG(1)]) block_xfer(blocks[ARG(1)], 1); return 0; }
METHOD(dev_CaptureStateBlock, 2) { if (ARG(1) < 1024 && blocks[ARG(1)]) block_xfer(blocks[ARG(1)], 0); return 0; }
METHOD(dev_DeleteStateBlock, 2) { if (ARG(1) < 1024) { free(blocks[ARG(1)]); blocks[ARG(1)] = NULL; } return 0; }

METHOD(dev_SetClipStatus, 2) { return 0; }
METHOD(dev_GetClipStatus, 2) { memset(GPTR(ARG(1)), 0, 8); return 0; }
METHOD(dev_GetTexture, 3) {
    uint32_t t = ARG(1) < MAX_STAGES ? dev->tex[ARG(1)] : 0;
    if (t) res_addref(t);
    MEM32(ARG(2)) = t;
    return 0;
}
METHOD(dev_SetTexture, 3) {
    if (ARG(1) < MAX_STAGES) MARK(tex[ARG(1)]);
    if (ARG(1) < MAX_STAGES && dev->tex[ARG(1)] != ARG(2)) {
        if (ARG(2) && !rec_mask) res_addref(ARG(2));  /* a recorded block holds no references */
        if (dev->tex[ARG(1)] && !rec_mask) res_release(dev->tex[ARG(1)]);
        dev->tex[ARG(1)] = ARG(2);
    }
    return 0;
}
METHOD(dev_GetTextureStageState, 4) {
    MEM32(ARG(3)) = ARG(1) < MAX_STAGES && ARG(2) < TSS_COUNT ? dev->tss[ARG(1)][ARG(2)] : 0;
    return 0;
}
METHOD(dev_SetTextureStageState, 4) {
    if (ARG(1) < MAX_STAGES && ARG(2) < TSS_COUNT) { dev->tss[ARG(1)][ARG(2)] = ARG(3); MARK(tss[ARG(1)][ARG(2)]); }
    return 0;
}
METHOD(dev_ValidateDevice, 2) { MEM32(ARG(1)) = 1; return 0; }
/* Palette numbers go up to 65535 in D3D8. The entries are device data, not state: they are kept on the
 * real device while a state block records. */
METHOD(dev_SetPaletteEntries, 3) {
    uint32_t n = ARG(1);
    Dev *d = rec_real ? rec_real : dev;
    if (n >= 65536) return D3DERR_INVALIDCALL;
    if (!d->palette[n]) d->palette[n] = malloc(1024);
    memcpy(d->palette[n], GPTR(ARG(2)), 1024);
    d->pal_version[n]++;
    return 0;
}
METHOD(dev_GetPaletteEntries, 3) {
    uint32_t n = ARG(1);
    if (n >= 65536) return D3DERR_INVALIDCALL;
    if (dev->palette[n]) memcpy(GPTR(ARG(2)), dev->palette[n], 1024);
    else memset(GPTR(ARG(2)), 0, 1024);
    return 0;
}
METHOD(dev_SetCurrentTexturePalette, 2) { if (ARG(1) >= 65536) return D3DERR_INVALIDCALL; dev->cur_palette = ARG(1); return 0; }
METHOD(dev_GetCurrentTexturePalette, 2) { MEM32(ARG(1)) = dev->cur_palette; return 0; }

/* ---- drawing ---- */

static GLenum blend_factor(uint32_t b) {
    static const GLenum t[] = {GL_ONE, GL_ZERO, GL_ONE, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR, GL_SRC_ALPHA,
                               GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA, GL_DST_COLOR,
                               GL_ONE_MINUS_DST_COLOR, GL_SRC_ALPHA_SATURATE};
    return b < 12 ? t[b] : GL_ONE;
}
static GLenum stencil_op(uint32_t o) {
    static const GLenum t[] = {GL_KEEP, GL_KEEP, GL_ZERO, GL_REPLACE, GL_INCR, GL_DECR, GL_INVERT, GL_INCR_WRAP, GL_DECR_WRAP};
    return o < 9 ? t[o] : GL_KEEP;
}
static GLenum cmp_func(uint32_t c) { return c >= 1 && c <= 8 ? GL_NEVER + c - 1 : GL_ALWAYS; }

static void apply_viewport_scissor(int scissor) {
    float x = dev->vp_x, y = dev->vp_y, w = dev->vp_w, h = dev->vp_h;
    if (dev->to_panel) {  /* SH3 Quest VR: the back buffer's 2D, scaled onto the 4:3 panel */
        int pw, ph;
        sh3vr_panel_fbo(&pw, &ph);
        float sx = (float)pw / dev->bb_w, sy = (float)ph / dev->bb_h;
        x *= sx; w *= sx; y *= sy; h *= sy;
    }
    glViewport(x, y, w, h);
    glDepthRangef(dev->vp_minz, dev->vp_maxz);
    if (scissor) { glEnable(GL_SCISSOR_TEST); glScissor(x, y, w, h); }
    else glDisable(GL_SCISSOR_TEST);
}

/* ---- SH3 Quest VR: where a draw goes ----
 * Into the eyes (the bound targets, both eyes at once) or onto the panel (the HUD, menus, the inventory, the map,
 * puzzle screens: the 2D that would stick to the face in the eyes). Only draws into the back buffer are routed:
 * - a draw sampling a per-eye texture is a pass over the eyes' pictures (the scene composited, post effects): eyes;
 * - screen-space (pre-transformed) draws: full-screen ones untextured (fades, the pause's dimming) to the eyes, and
 *   to the panel too when it already has a picture this frame (they darken what was drawn before them, as on a
 *   screen); full-screen textured ones blended (the noise, flashes) to the eyes, opaque (a menu's background) to the
 *   panel; everything smaller (text, icons, frames, the cursor) to the panel;
 * - 3D drawn with another camera than the frame's (the inventory's and the item view's models) among the panel's 2D:
 *   the panel. 0 eyes, 1 panel, 2 both, 3 neither (the Life Display cross while the wrist shows health). */
static int vr_route_pick(uint32_t va, uint32_t stride, uint32_t count);
/* testing (session 11, the full screens): SH3_ROUTE_DUMP=N logs each draw of frames N to N+2 with where it went */
static int vr_route(uint32_t va, uint32_t stride, uint32_t count) {
    const int r = vr_route_pick(va, stride, count);
    /* (session 15: SH3_ROUTE_DUMP2=M a second frame, M only; each line with the game code that drew it and the
     * draw's nv / np, to tell the beam saber's draws by a lit frame against an unlit one) */
    static int at = -2, at2 = -1;
    if (at == -2) {
        at = getenv("SH3_ROUTE_DUMP") ? atoi(getenv("SH3_ROUTE_DUMP")) : -1;
        at2 = getenv("SH3_ROUTE_DUMP2") ? atoi(getenv("SH3_ROUTE_DUMP2")) : -1;
    }
    if ((at >= 0 && rt_frame >= at && rt_frame <= at + 2) || (at2 >= 0 && rt_frame == at2)) {
        Res *t0 = res_of(dev->tex[0]), *rt = res_of(dev->rt);
        rt_log("route: frame %d: %d (views %d, into the back buffer %d, target %ux%u layers %d; vs %08X count %u, "
               "blend %u, texture 0 %ux%u layers %d, z %u, world %d; from %08X, nv %u np %u, beam mesh %d)", rt_frame, r,
               dev->views, dev->rt == dev->backbuffer, rt ? rt->w : 0, rt ? rt->h : 0, rt ? res_layers(rt) : 0, dev->vs,
               count, dev->rs[27], t0 ? t0->w : 0, t0 ? t0->h : 0, t0 ? res_layers(t0) : 0, dev->rs[7], world_last,
               MEM32(g_esp), dev->draw_nv, dev->draw_np, dev->beam_mesh);
        /* (session 16, owner 79: a screen-space draw's place: its box and depths, and whether it went in the world) */
        const int rhw = dev->vs < 0xF0000000u && (dev->vs & 0xE) == 4;
        if (rhw && va && count) {
            float b[6] = {1e30f, -1e30f, 1e30f, -1e30f, 1e30f, -1e30f};
            for (uint32_t i = 0; i < count && i < 4096; i++) {
                const float *p = (const float *)GPTR(va + i * stride);
                for (int k = 0; k < 3; k++) {
                    if (p[k] < b[2 * k]) b[2 * k] = p[k];
                    if (p[k] > b[2 * k + 1]) b[2 * k + 1] = p[k];
                }
            }
            rt_log("  its place: x %.0f to %.0f, y %.0f to %.0f, z %.5f to %.5f (viewport %.0f %.0f %.0f %.0f); in the world %d, "
                   "moved %d", b[0], b[1], b[2], b[3], b[4], b[5], dev->vp_x, dev->vp_y, dev->vp_w, dev->vp_h,
                   dev->rhw_world, dev->rhw_mapped);
        }
    }
    {
        /* testing (session 15, owner 61: the beam saber's strip): SH3_BEAM_VERTS=N logs the first N screen-space draws
         * with a texture of 128 texels or less while the beam saber (weapon 9) is lit (0x712C66B): the game code that drew
         * it, its route, whether it went to the eyes as a world effect and was moved with the saber, and each vertex
         * (x y z rhw), so the strip's depths tell whether moving it rigidly can work */
        /* (session 15: from frame SH3_BEAM_VERTS_FROM on, so a steady beam is seen, not only its lighting burst; the
         * vertices as the game drew them, before sh3vr_beam_sprite) */
        static int left = -2, from = 0, tex_max = 128;
        if (left == -2) {
            left = getenv("SH3_BEAM_VERTS") ? atoi(getenv("SH3_BEAM_VERTS")) : 0;
            from = getenv("SH3_BEAM_VERTS_FROM") ? atoi(getenv("SH3_BEAM_VERTS_FROM")) : 0;
            /* (vr207: the lights' 128x128 halos used up the count; SH3_BEAM_VERTS_TEX=64 leaves them out) */
            if (getenv("SH3_BEAM_VERTS_TEX")) tex_max = atoi(getenv("SH3_BEAM_VERTS_TEX"));
        }
        const int rhw = dev->vs < 0xF0000000u && (dev->vs & 0xE) == 4;
        Res *t0 = res_of(dev->tex[0]);
        if (left > 0 && rt_frame >= from && rhw && va && MEM8(0x712C656u) == 9 && MEM8(0x712C66Bu) && t0 &&
            (int)t0->w <= tex_max) {
            left--;
            float z0 = 1e30f, z1 = -1e30f;
            for (uint32_t i = 0; i < count; i++) {
                const float z = ((const float *)GPTR(va + i * stride))[2];
                if (z < z0) z0 = z;
                if (z > z1) z1 = z;
            }
            rt_log("beam verts: frame %d: from %08X, %u vertices of %u bytes, texture %ux%u, blend %u (%u %u), z %.5f "
                   "to %.5f, route %d, world effect %d, moved with the saber %d, viewport %.0f %.0f %.0f %.0f", rt_frame,
                   MEM32(g_esp), count, stride, t0->w, t0->h, dev->rs[27], dev->rs[19], dev->rs[20], z0, z1, r,
                   dev->rhw_world, dev->rhw_mapped, dev->vp_x, dev->vp_y, dev->vp_w, dev->vp_h);
            for (uint32_t i = 0; i < count && i < 24; i++) {
                const float *v = (const float *)GPTR(va + i * stride);
                if (stride >= 28)
                    rt_log("  beam vertex %u: %.2f %.2f %.5f rhw %.6f, colour %08X, texture %.3f %.3f", i, v[0], v[1], v[2],
                           v[3], MEM32(va + i * stride + 16), v[5], v[6]);
                else rt_log("  beam vertex %u: %.2f %.2f %.5f rhw %.6f", i, v[0], v[1], v[2], v[3]);
            }
            {
                /* where the frame's camera puts the saber's line, as the game holds it and as the VR hand does */
                const float vpf[4] = {dev->vp_x, dev->vp_y, dev->vp_w, dev->vp_h};
                float s[12];
                if (sh3vr_beam_screen(vpf, s))
                    rt_log("  the game's saber: palm %.0f %.0f (z %.4f), tip %.0f %.0f (z %.4f); the VR hand's: palm %.0f %.0f "
                           "(z %.4f), tip %.0f %.0f (z %.4f)", s[0], s[1], s[2], s[3], s[4], s[5], s[6], s[7], s[8], s[9],
                           s[10], s[11]);
            }
        }
    }
    return r;
}
static int vr_route_pick(uint32_t va, uint32_t stride, uint32_t count) {
    dev->rhw_world = dev->rhw_mapped = dev->beam_mesh = dev->no_ztest = 0;
    dev->rhw_fade = 1.0f;
    dev->saber_out = MEM8(0x712C656u) == 9;  /* the beam saber out (its light: d3d8_shader.c; 0x712C66B drops while the blade stays out, vr218) */
    {
        /* testing (session 6, the muzzle flash): SH3_VSC_DUMP_SHOT=N logs each draw of N frames after Heather's first
         * shot with its vertices' middle (their first three floats, world space for the effects' sprites) */
        extern int sh3_first_shot_frame;
        static int after_shot = -2;
        if (after_shot == -2) after_shot = getenv("SH3_VSC_DUMP_SHOT") ? atoi(getenv("SH3_VSC_DUMP_SHOT")) : -1;
        if (after_shot >= 0 && sh3_first_shot_frame >= 0 && rt_frame == sh3_first_shot_frame + after_shot && va) {
            double s[3] = {0, 0, 0};
            const uint32_t n = count < 256 ? count : 256;
            for (uint32_t i = 0; i < n; i++)
                for (int k = 0; k < 3; k++) s[k] += ((const float *)GPTR(va + i * stride))[k];
            rt_log("draw vertices: vs %08X stride %u count %u blend %u tex %08X middle %.1f %.1f %.1f", dev->vs, stride,
                   count, dev->rs[27], dev->tex[0], n ? s[0] / n : 0.0, n ? s[1] / n : 0.0, n ? s[2] / n : 0.0);
            if (dev->vs == 0x144 && count <= 8) {
                /* the screen-space quads: each vertex whole (x, y, z, rhw), the viewport and the scene's camera
                 * (0x72C460, view x projection) the game placed them with */
                static int told;
                if (told++ < 6) {
                    for (uint32_t i = 0; i < count; i++) {
                        const float *v = (const float *)GPTR(va + i * stride);
                        rt_log("  quad vertex %u: %.2f %.2f %.4f rhw %.6f", i, v[0], v[1], v[2], v[3]);
                    }
                    rt_log("  viewport %.0f %.0f %.0f %.0f; 0x72C460: %g %g %g %g / %g %g %g %g / %g %g %g %g / %g %g %g %g; "
                           "0x72C420: %g %g %g %g / %g %g %g %g / %g %g %g %g / %g %g %g %g",
                           dev->vp_x, dev->vp_y, dev->vp_w, dev->vp_h, MEMF(0x72C460), MEMF(0x72C464), MEMF(0x72C468),
                           MEMF(0x72C46C), MEMF(0x72C470), MEMF(0x72C474), MEMF(0x72C478), MEMF(0x72C47C), MEMF(0x72C480),
                           MEMF(0x72C484), MEMF(0x72C488), MEMF(0x72C48C), MEMF(0x72C490), MEMF(0x72C494), MEMF(0x72C498),
                           MEMF(0x72C49C), MEMF(0x72C420), MEMF(0x72C424), MEMF(0x72C428), MEMF(0x72C42C), MEMF(0x72C430),
                           MEMF(0x72C434), MEMF(0x72C438), MEMF(0x72C43C), MEMF(0x72C440), MEMF(0x72C444), MEMF(0x72C448),
                           MEMF(0x72C44C), MEMF(0x72C450), MEMF(0x72C454), MEMF(0x72C458), MEMF(0x72C45C));
                }
            }
        }
    }
    if (!sh3vr_active() || dev->views != 2 || sh3vr_screen_mode()) return 0;
    /* The game's Life Display cross (0x5F1C40: its 12 vertices at 0x71FAE0; z between 0 and 1, so it was taken for a
     * world effect and stamped into each eye's corner, vr148): left out while the wrist's bar shows her health in its
     * place (3), else on the panel with the game's other 2D */
    if (va == 0x71FAE0u && count == 12) return sh3vr_wrist_health_on() ? 3 : 1;
    /* (Only draws into the back buffer are routed; the world effects below go into the eyes' targets too: the scene's
     * own target is where the game draws its muzzle flash, run vr72.) */
    const int to_backbuffer = dev->rt == dev->backbuffer;
    /* Session 11 (owner message 44: the inventory and menus "projected in front of the screen"): out of play, a frame
     * after one without the frame camera's 3D (the world) is one of the game's full screens (the inventory, the map,
     * memos, menus without the world, loading): all of it onto the panel, which stands in front of the eyes. Its
     * backdrop and the inventory's models went to the eyes, stretched across the whole view (vr174); a pass over a
     * per-eye picture goes too (the shader samples its left eye into a single target, d3d8_shader.c). */
    extern int sh3_playable_now(void);
    const int full_screen = to_backbuffer && !world_last && !sh3_playable_now() && sh3vr_full_screens();
    int any_tex = 0;
    for (int s = 0; s < 4; s++) {
        Res *t = res_of(dev->tex[s]);
        if (!t) continue;
        if (res_layers(t) == 2) {
            if (full_screen) frame_screen++;
            return full_screen ? 1 : 0;
        }
        any_tex = 1;
    }
    int rhw = dev->vs < 0xF0000000u && (dev->vs & 0xE) == 4;
    if (!rhw) {
        int main = dev->vs < 0xF0000000u ? sh3vr_is_main_proj(dev->proj) : sh3vr_is_main_vp_constants(&dev->vsc[2][0]);
        if (main) frame_world++;
        else frame_other++;
        /* Owner 61 (session 15): the beam saber's glow as it lights is a 3D mesh of the game's (vr203: vs F0000007,
         * 6 vertices, added light, two draws a frame) placed by its own constants at the saber in her animated hand.
         * (The blade itself is a skinned draw, moved in sh3vr_combat.cpp saber_blade_draw: left at her animated hand
         * it was the green bar of vr205-215.) A small blended mesh of the frame camera's while the saber is lit moves
         * with the saber to the VR
         * hand (sh3vr_beam_mesh: its clip -> the world -> the weapon's map -> its clip, before the eyes' corrections,
         * d3d8_shader.c) and writes no depth (the hands' probes took the beam for a wall, vr205). */
        if (main && dev->rs[27] && dev->vs >= 0xF0000000u && count && count <= 16 && MEM8(0x712C656u) == 9 &&
            MEM8(0x712C66Bu)) {
            /* testing (session 15): SH3_HIDE_BEAM_MESH=F leaves these draws out from frame F on (is it the bar?) */
            static int hide_from = -2;
            if (hide_from == -2) hide_from = getenv("SH3_HIDE_BEAM_MESH") ? atoi(getenv("SH3_HIDE_BEAM_MESH")) : -1;
            if (hide_from >= 0 && rt_frame >= hide_from) return 3;
            dev->beam_mesh = sh3vr_beam_mesh(dev->beam_map, count);
        }
        if (!to_backbuffer) return 0;
        return !main && (panel_has_picture || full_screen) ? 1 : 0;
    }
    if (full_screen) {
        frame_screen++;
        return 1;
    }
    {
        /* Owner 79 (session 16; a player on 0.1.0: "theres like a weird overlay that looks like fog. unplayable"): the
         * game's drifting fog is 0x5F572E's fans of four (6 vertices, 0x144, a 512x512 texture, blended, the z test
         * off; vr243: 171 a frame). Their z is no real depth (vr244: 0.60 to 0.98 here; the PC VR mod saw "z slightly
         * above 1.0"), and the world effects' path below put each eye's view of them at it: patches of fog floating
         * close in front of the eyes. Now on a far layer fixed in the world (their depth set to the far plane, then
         * halfway in: 10 m), each eye's own view of it (sh3vr_far_layer_map), as the PC VR mod replays them; the option
         * "Drifting fog" off leaves them out. (0x5F572E is the game's draw helper: other draws come from it too, the
         * light composite's strip among them; only these fans are the fog.) */
        Res *t0 = res_of(dev->tex[0]);
        if (dev->vs == 0x144u && count == 6 && dev->rs[27] && MEM32(g_esp) == 0x5F572Eu && t0 && t0->w == 512 &&
            t0->h == 512) {
            frame_fog++;
            if (!sh3vr_drifting_fog()) return 3;
            if (sh3vr_far_layer_map(dev->rhw_map)) {
                dev->rhw_world = dev->rhw_mapped = 1;
                return 0;
            }
        }
    }
    {
        /* testing (session 17): SH3_HALO_DUMP=1 logs the lamps' halos (0x468CDA, 0x468F6E: 128x128 sprites at z 0.50003,
         * which the world effects' path below puts 0.40 m from the eyes) with what the game placed them from: the record
         * at 0x898420 (0x4681F0 fills it through 0x678450 from the light array at 0x8983E0) and the scene's camera */
        static int dump = -1, told;
        if (dump < 0) dump = getenv("SH3_HALO_DUMP") ? 1 : 0;
        const uint32_t ret = MEM32(g_esp);
        if (dump && (ret == 0x468CDAu || ret == 0x468F6Eu) && va && (told < 8 || rt_frame % 300 == 0) && told < 40) {
            told++;
            char line[1600];
            int n = snprintf(line, sizeof line, "halo dump: frame %d from %06X, %u vertices:", rt_frame, ret, count);
            for (uint32_t i = 0; i < count && i < 6 && n < (int)sizeof line - 80; i++) {
                const float *v = (const float *)GPTR(va + i * stride);
                n += snprintf(line + n, sizeof line - n, " (%.1f %.1f %.5f %.4f)", v[0], v[1], v[2], v[3]);
            }
            n += snprintf(line + n, sizeof line - n, "; 0x8983E0..0x898480 as floats:");
            for (uint32_t a = 0x8983E0u; a < 0x898480u && n < (int)sizeof line - 24; a += 4)
                n += snprintf(line + n, sizeof line - n, " %g", MEMF(a));
            rt_log("%s", line);
            n = snprintf(line, sizeof line, "halo dump: as ints:");
            for (uint32_t a = 0x8983E0u; a < 0x898480u && n < (int)sizeof line - 16; a += 4)
                n += snprintf(line + n, sizeof line - n, " %d", (int)MEM32(a));
            n += snprintf(line + n, sizeof line - n, "; 0x72C460:");
            for (uint32_t a = 0x72C460u; a < 0x72C4A0u && n < (int)sizeof line - 16; a += 4)
                n += snprintf(line + n, sizeof line - n, " %g", MEMF(a));
            n += snprintf(line + n, sizeof line - n, "; viewport %.0f %.0f %.0f %.0f", dev->vp_x, dev->vp_y, dev->vp_w,
                          dev->vp_h);
            rt_log("%s", line);
        }
    }
    float x0 = 1e30f, y0 = 1e30f, x1 = -1e30f, y1 = -1e30f, z0 = 1e30f, z1 = -1e30f;
    double sx = 0, sy = 0, sz = 0;
    uint32_t nv = 0;
    for (uint32_t i = 0; va && i < count && i < 4096; i++, nv++) {
        const float *p = (const float *)GPTR(va + i * stride);
        if (p[0] < x0) x0 = p[0];
        if (p[0] > x1) x1 = p[0];
        if (p[1] < y0) y0 = p[1];
        if (p[1] > y1) y1 = p[1];
        if (p[2] < z0) z0 = p[2];
        if (p[2] > z1) z1 = p[2];
        sx += p[0], sy += p[1], sz += p[2];
    }
    int full = x1 - x0 >= 0.95f * dev->vp_w && y1 - y0 >= 0.95f * dev->vp_h;
    /* Session 17 (the handoff's item 2): the game's lens flare round a lamp, 0x468CDA's streaks and 0x468F6E's rings
     * (128x128 sprites, the z test on), is drawn at a fixed depth, z 0.50003, which the world effects' path below put
     * 0.40 m from the eyes: a double image in VR, nearer than the wall the lamp is on (vr245, vr246: about 60 px more
     * between the eyes than the wall's bench). The lamp's glow itself the game draws at the lamp's depth (vr246: z
     * 0.90321 for a lamp 936 units off), which is right as it is. The game's own view depth of the lamp is in its flare
     * record (0x898420 + 0x18, filled with its screen place through 0x678450; vr246: the record's view point projects
     * to the streaks' middle exactly), so the flare goes at that depth (sh3vr_depth_layer_map), each eye's own view,
     * and over everything (no z test), as on the flat screen, where 0.40 m was in front of it all. */
    /* 0.1.5 (the owner, message 101: "lens flares ... seem placed on the screen, not actually following the world. i dont
     * want ANY issues like that"; a player's 0.1.4 report: "strange light reflections, both with and without the
     * flashlight", their video: soft white rings hanging in the air round the park's lamps): a lens flare is the
     * camera's own, its streaks and rings laid along the line from the lamp through the middle of the game's picture, so
     * at the lamp's depth they still slid about the room as the head turned. In VR they are left out, as the PC VR mod
     * leaves them (it puts only the world's draws in the eyes).
     * The flare's own glow too (its sprite at the lamp's depth; 0x4690A0 draws the flare of one lamp at a time, from its
     * record at 0x898420): the owner, on the rig's pictures, "should follow whatever theyre coming from in view" and
     * "check pcvr build for visual fixes that have already been done". The PC VR mod puts none of these screen-space
     * sprites in the eyes; the lamps' own glowing bulbs (3D) and their halos from the game's effects (below, put at
     * their place in the world) stay. */
    if (!full && nv && (MEM32(g_esp) == 0x468CDAu || MEM32(g_esp) == 0x468F6Eu) && sh3vr_world_effects()) {
        static int told;
        if (!told++)
            rt_log("VR: frame %d: the game's lens flare left out (its %s from %06X: the camera's, not the world's), as "
                   "the PC VR mod leaves it", rt_frame, MEM32(g_esp) == 0x468CDAu ? "streaks" : "rings and glow",
                   MEM32(g_esp));
        return 3;
    }
    /* The game's effects it projected itself (session 6: the muzzle flash, its smoke, dust, sparks): their depth is
     * real, strictly between 0 and 1 (its 2D sits at exactly 0 or 1). In the eyes, put right per eye as the frame
     * camera's draws are, so they stand in the world (they were on the panel, flat: the flash in the left palm). The
     * gun's own, just after a shot, move with the gun to the hand (sh3vr_effect_map). */
    {
        /* testing (session 15): SH3_HIDE_RIBBON=F leaves out, from frame F on, the beam saber's one draw that only a lit
         * frame has (vr212: 0x5F6937's 8 vertices, a 64x64 texture): is it the green bar? */
        static int hide_from = -2;
        if (hide_from == -2) hide_from = getenv("SH3_HIDE_RIBBON") ? atoi(getenv("SH3_HIDE_RIBBON")) : -1;
        Res *t0 = res_of(dev->tex[0]);
        if (hide_from >= 0 && rt_frame >= hide_from && count == 8 && t0 && t0->w == 64 && MEM32(g_esp) == 0x5F6937u &&
            MEM8(0x712C656u) == 9)
            return 3;
    }
    if (!full && nv && z0 > 1e-4f && z1 < 0.9999f && dev->vp_w > 0 && dev->vp_h > 0 && sh3vr_world_effects()) {
        const float ndc[3] = {(float)((sx / nv - dev->vp_x) / dev->vp_w * 2.0 - 1.0),
                              (float)(1.0 - (sy / nv - dev->vp_y) / dev->vp_h * 2.0), (float)(sz / nv)};
        dev->rhw_world = 1;
        {
            /* 0.1.5: through the game's own camera, which it projected the effect with, to its place in the world
             * (0x43AD30 keeps that view at 0x743C34, which the game's effects read through 0x43AD50), faded out toward
             * the edge of the game's own picture (session 22: it draws only what its camera sees) */
            Res *t0 = res_of(dev->tex[0]);
            float game_view[16];
            const int have_view = sh3_game_view(game_view);
            const float half[2] = {(x1 - x0) / dev->vp_w, (y1 - y0) / dev->vp_h};  /* the draw's half size, clip units */
            dev->rhw_mapped = sh3vr_effect_map(ndc, half, t0 ? (int)t0->w : 0, have_view ? game_view : NULL,
                                               dev->rhw_map, &dev->rhw_fade);
            if (dev->rhw_mapped && dev->rhw_fade < 0.004f) return 3;  /* nothing of it shows */
        }
        return 0;
    }
    if (!to_backbuffer) return 0;
    if (full && !any_tex) return panel_has_picture ? 2 : 0;
    if (full) return dev->rs[27] ? 0 : 1;  /* ALPHABLENDENABLE */
    /* session 11 (owners 42-44): where the game's 2D lands on the panel (its text's box: a message by its thing) */
    if (nv && dev->bb_w > 0 && dev->bb_h > 0)
        sh3vr_panel_box(x0 / dev->bb_w, y0 / dev->bb_h, x1 / dev->bb_w, y1 / dev->bb_h);
    return 1;
}
static void panel_target(int on) {
    if (on) {
        int pw, ph;
        glBindFramebuffer(GL_FRAMEBUFFER, sh3vr_panel_fbo(&pw, &ph));
        dev->to_panel = 1;
        panel_has_picture = 1;
        sh3vr_panel_drawn();
    } else {
        dev->to_panel = 0;
        glBindFramebuffer(GL_FRAMEBUFFER, dev->fbo);
    }
}
static int her_mask_due(void);
static void her_mask_second(void);
static void her_mask_blend(void);
/* A draw, routed: `setup` then `draw` once for each place it goes. 0.1.3: after the game's shadow mask draw, Heather's
 * own shadow (her_mask_due): her volumes counted, then the same draw again at her strength. */
#define ROUTED(route, setup, draw) do { \
        int r_ = (route); /* 0 the eyes, 1 the panel, 2 both, 3 neither */ \
        if (r_ == 0 || r_ == 2) { setup; draw; if (her_mask_due()) { her_mask_second(); setup; her_mask_blend(); draw; } } \
        if (r_ == 1 || r_ == 2) { panel_target(1); setup; draw; panel_target(0); } \
    } while (0)

static void apply_render_states(void) {
    uint32_t *rs = dev->rs;
    apply_viewport_scissor(0);
    if (rs[7] && dev->ds && !dev->no_ztest) { glEnable(GL_DEPTH_TEST); glDepthFunc(cmp_func(rs[23])); }
    else glDisable(GL_DEPTH_TEST);
    /* session 14 (vr204): the game's blended screen-space effects put in the world (its fog, the beam saber's beam, the
     * muzzle flash) never write the eyes' depth: the hands' and the weapon's depth probes took the beam for a wall */
    glDepthMask(rs[14] && dev->ds && !((dev->rhw_world || dev->beam_mesh) && rs[27]));
    if (rs[27]) {
        glEnable(GL_BLEND);
        uint32_t s = rs[19], d = rs[20];
        if (s == 12) { s = 5; d = 6; }        /* BOTHSRCALPHA */
        else if (s == 13) { s = 6; d = 5; }   /* BOTHINVSRCALPHA */
        glBlendFuncSeparate(blend_factor(s), blend_factor(d), blend_factor(s), blend_factor(d));
        static const GLenum op[] = {GL_FUNC_ADD, GL_FUNC_ADD, GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT, GL_MIN, GL_MAX};
        glBlendEquation(rs[171] < 6 ? op[rs[171]] : GL_FUNC_ADD);
    } else glDisable(GL_BLEND);
    if (rs[22] == 1) glDisable(GL_CULL_FACE);
    else {
        glEnable(GL_CULL_FACE);
        /* SH3 Quest VR: the vertex stage negates y (ES has no clip control), which mirrors window-space winding:
         * D3D's clockwise front faces arrive counter-clockwise. sh3-nx had GL_CW under an upper-left clip origin,
         * which negates the facing area itself. SH3_FRONTFACE_CW=1 (testing) puts the old winding back. */
        static int cw = -1;
        if (cw < 0) cw = getenv("SH3_FRONTFACE_CW") != NULL;
        glFrontFace(cw ? GL_CW : GL_CCW);
        glCullFace(rs[22] == 2 ? GL_FRONT : GL_BACK);  /* CULL_CW drops clockwise (front) faces */
    }
    if (rs[52] && dev->ds) {
        glEnable(GL_STENCIL_TEST);
        glStencilFunc(cmp_func(rs[56]), rs[57], rs[58]);
        glStencilOpSeparate(GL_FRONT_AND_BACK, stencil_op(rs[53]), stencil_op(rs[54]), stencil_op(rs[55]));
        glStencilMask(rs[59]);
        {
            /* testing (session 10, LIST 56: shadow volumes?): in SH3_VSC_DUMP's frame, each new stencil state a draw
             * uses, with the colour writes and culling (a volume's passes write no colour, one face each) */
            static int at = -2;
            static uint32_t last[8];
            if (at == -2) at = getenv("SH3_VSC_DUMP") ? atoi(getenv("SH3_VSC_DUMP")) : -1;
            const uint32_t now[8] = {rs[53], rs[54], rs[55], rs[56], rs[57], rs[58], rs[168], rs[22]};
            if (at >= 0 && rt_frame == at && memcmp(now, last, sizeof now)) {
                memcpy(last, now, sizeof now);
                rt_log("vsc stencil: frame %d: fail %u zfail %u pass %u, func %u ref %u mask %X, colour writes %X, cull %u",
                       rt_frame, now[0], now[1], now[2], now[3], now[4], now[5], now[6], now[7]);
            }
        }
    } else glDisable(GL_STENCIL_TEST);
    uint32_t cw = rs[168];
    glColorMask(cw & 1, (cw >> 1) & 1, (cw >> 2) & 1, (cw >> 3) & 1);
    /* SH3 Quest VR: ES has no polygon mode; FILLMODE point/wireframe draw solid (logged once). */
    static int fill_logged;
    if (rs[8] != 3 && !fill_logged++) rt_log("D3D: FILLMODE %u drawn solid (no polygon mode in GL ES)", rs[8]);
    if (rs[47]) { glEnable(GL_POLYGON_OFFSET_FILL); glPolygonOffset(0, -(float)rs[47]); }  /* ZBIAS */
    else glDisable(GL_POLYGON_OFFSET_FILL);
}

static GLenum prim_mode(uint32_t t) {
    static const GLenum m[] = {GL_POINTS, GL_POINTS, GL_LINES, GL_LINE_STRIP, GL_TRIANGLES, GL_TRIANGLE_STRIP, GL_TRIANGLE_FAN};
    return t < 7 ? m[t] : GL_TRIANGLES;
}
static uint32_t prim_verts(uint32_t t, uint32_t n) {
    switch (t) {
    case 1: return n;
    case 2: return n * 2;
    case 3: return n + 1;
    case 4: return n * 3;
    default: return n + 2;
    }
}

/* Vertex attribute locations: D3DVSDE numbering for the fixed pipeline (0 position, 1 blend weights,
 * 2 blend indices, 3 normal, 4 point size, 5 diffuse, 6 specular, 7-14 texcoords); v# for shaders. */
static void attrib(int loc, int type, GLsizei stride, uintptr_t off) {
    glEnableVertexAttribArray(loc);
    switch (type) {  /* the caller marks D3DCOLOR (type 4) attributes present = 2 for the shader's swizzle */
    case 0: case 1: case 2: case 3: glVertexAttribPointer(loc, type + 1, GL_FLOAT, 0, stride, (void *)off); break;
    /* D3DCOLOR: bytes B, G, R, A; ES has no GL_BGRA size, so the vertex stage swizzles (.zyxw, d3d8_shader.c) */
    case 4: glVertexAttribPointer(loc, 4, GL_UNSIGNED_BYTE, 1, stride, (void *)off); break;
    case 5: glVertexAttribPointer(loc, 4, GL_UNSIGNED_BYTE, 0, stride, (void *)off); break;        /* UBYTE4 */
    case 6: glVertexAttribPointer(loc, 2, GL_SHORT, 0, stride, (void *)off); break;
    case 7: glVertexAttribPointer(loc, 4, GL_SHORT, 0, stride, (void *)off); break;
    }
}

/* Sets up attributes for stream 0 (FVF) or the declaration's streams. base: GL buffer offsets of each
 * stream's vertex 0; up_stride: stride for DrawPrimitiveUP. Returns 1 when vertices are pretransformed. */
static int setup_attribs(const uintptr_t *base, const GLuint *bufs, uint32_t up_stride, int *present) {
    for (int i = 0; i < 16; i++) { glDisableVertexAttribArray(i); present[i] = 0; }
    if (dev->vs < 0xF0000000u) {
        uint32_t fvf = dev->vs, stride = up_stride ? up_stride : dev->stride[0], off = 0;
        if (!bufs[0]) return 0;
        glBindBuffer(GL_ARRAY_BUFFER, bufs[0]);
        uint32_t pos = fvf & 0xE;
        int rhw = pos == 4;
        attrib(0, rhw ? 3 : 2, stride, base[0]);
        present[0] = 1;
        off = rhw ? 16 : 12;
        if (pos >= 6 && pos <= 0xE) {
            int nb = (pos - 4) / 2;  /* XYZB1..B5 */
            if (fvf & 0x1000) nb--;  /* last beta is UBYTE4 indices */
            if (nb > 0) { attrib(1, nb - 1, stride, base[0] + off); present[1] = 1; off += 4 * nb; }
            if (fvf & 0x1000) { attrib(2, 5, stride, base[0] + off); present[2] = 1; off += 4; }
        }
        if (fvf & 0x10) { attrib(3, 2, stride, base[0] + off); present[3] = 1; off += 12; }
        if (fvf & 0x20) { attrib(4, 0, stride, base[0] + off); present[4] = 1; off += 4; }
        if (fvf & 0x40) { attrib(5, 4, stride, base[0] + off); present[5] = 2; off += 4; }
        if (fvf & 0x80) { attrib(6, 4, stride, base[0] + off); present[6] = 2; off += 4; }
        int ntex = (fvf >> 8) & 0xF;
        for (int t = 0; t < ntex && t < 8; t++) {
            static const int size[4] = {2, 3, 4, 1};
            int n = size[(fvf >> (16 + 2 * t)) & 3];
            attrib(7 + t, n - 1, stride, base[0] + off);
            present[7 + t] = 1;
            off += 4 * n;
        }
        return rhw;
    }
    const VsInput *in = shader_vs_inputs(dev->vs);
    for (int r = 0; r < 16; r++) {
        if (in[r].stream < 0 || !bufs[in[r].stream]) continue;
        int s = in[r].stream;
        glBindBuffer(GL_ARRAY_BUFFER, bufs[s]);
        attrib(r, in[r].type, up_stride ? up_stride : dev->stride[s], base[s] + in[r].offset);
        present[r] = in[r].type == 4 ? 2 : 1;
    }
    return 0;
}

/* The game's second grain (0.1.3, owner 84: "theres still this weird fuzz over the screen, which i want completely off
 * by default. no screen filters or ANYTHING like that shit on by default"). Besides the Noise Effect's alpha test
 * (dev_SetRenderState above), 0x429A60 (a screen effect's mode 2, every frame of play, vr256) draws two full-screen
 * quads: the picture times (1 + L) (DESTCOLOR, ONE: a grey TFACTOR, 0x429C6F), then a 128x128 noise tile repeated
 * four times across the screen, at a new offset each frame, times L taken off the picture (BLENDOP REVSUBTRACT,
 * SRCALPHA, ONE: 0x429E38, return 0x429E3E). The result: a grey grain of the same strength over everything lit, none
 * on black (vr253-vr255: 4-5 levels frame to frame, about 2 pixels across, the Film grain option no help). Unless
 * "Film grain" is on, the tile is drawn as its own average (res_flat_texture): the same brightness and contrast as the
 * game's (the grade's (1 + L) and the tile's mean taken off), no grain. */
static int grain_tile_draw(int stage, Res *t) {
    return stage == 0 && t && MEM32(g_esp) == 0x00429E3Eu && !sh3vr_film_grain();
}
static void bind_textures(void) {
    for (int s = 0; s < MAX_STAGES; s++) {
        glActiveTexture(GL_TEXTURE0 + s);
        Res *t = res_of(dev->tex[s]);
        if (t) res_upload_dirty(t);
        if (grain_tile_draw(s, t)) {
            uint8_t avg[4] = {0, 0, 0, 0};
            const GLuint flat = res_flat_texture(t, avg);  /* binds it */
            static int told;
            if (!told) {
                told = 1;
                rt_log("VR: frame %d: the game's second grain (0x429A60's noise tile, %ux%u, format %u) drawn as its "
                       "average %u %u %u %u: no grain (the VR option Film grain off)%s", rt_frame, t->w, t->h, t->fmt,
                       avg[0], avg[1], avg[2], avg[3], flat ? "" : "; could not: the tile has no copy, the grain stays");
            }
            if (flat) continue;
        }
        glBindTexture(t && t->kind == RES_CUBE ? GL_TEXTURE_CUBE_MAP : res_layers(t) == 2 ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D,
                      t ? t->tex : 0);
    }
}

static void draw_setup(const uintptr_t *base, const GLuint *bufs, uint32_t up_stride) {
    int present[16];
    /* LIST 56 (session 12): a shadow volume of the game's (the stencil counting up on its front faces, 0x48B930's
     * states): its camera kept for Heather's own volumes (vr_her_shadow) */
    if (dev->rs[52] && dev->rs[55] == 7 && dev->rs[22] == 3) {
        memcpy(vol_view, dev->view, sizeof vol_view);
        memcpy(vol_proj, dev->proj, sizeof vol_proj);
        vol_frame = rt_frame;
    }
    apply_render_states();
    bind_textures();
    int rhw = setup_attribs(base, bufs, up_stride, present);
    /* Screen-space vertices are never clipped by depth in D3D; UI quads sit at z = 0 or 1 exactly, which a
     * driver's rounding can push just outside the clip volume. SH3 Quest VR: ES has no depth clamp, so the
     * vertex stage clamps their depth (d3d8_shader.c). */
    shader_bind_for_draw(rhw, dev->vs, present);
    dev->pal_new = 1;
    /* LIST 56 (session 12; vr184: her arms darkened whole by their own volumes, each limb half in its own shadow): her
     * own pixels, and her weapon's in the hand, marked in stencil bit 0x20 (outside the game's count, 0x1F) where they
     * pass the depth test, so her volumes leave her own body alone (vr_her_shadow counts only where the bit is clear);
     * the room's shadows still fall on her. vr185: the game draws her itself with the stencil on (0x461462: ALWAYS,
     * REPLACE 64, its count bits kept), so the bit goes in with its own replace; without the stencil, a replace of the
     * bit alone. */
    if ((her_mark || shader_held_draw) && dev->ds && sh3vr_active()) {
        if (!dev->rs[52]) {
            glEnable(GL_STENCIL_TEST);
            glStencilFunc(GL_ALWAYS, 0x20, 0xFF);
            glStencilOpSeparate(GL_FRONT_AND_BACK, GL_KEEP, GL_KEEP, GL_REPLACE);
            glStencilMask(0x20);
        } else if (dev->rs[56] == 8 /* ALWAYS */ && dev->rs[55] == 3 /* REPLACE */) {
            glStencilFunc(GL_ALWAYS, (GLint)(dev->rs[57] | 0x20), dev->rs[58]);
            glStencilMask(dev->rs[59] | 0x20);
        }
    }
}

/* LIST 56, Heather's shadow (session 12; owner messages 49-50): the game's stencil shadows count only the room's own
 * volumes (0x491320 -> 0x48B930's states -> 0x491030), never her. Her volumes (sh3vr_her_shadow: her IK body, the weapon
 * in her hand, a reload part held; triangles in the world wound so the game camera takes their outsides as front faces)
 * are counted the game's way just before its darkening step reads the count: no colour, the depth tested and not
 * written, the stencil (the game's 1F mask) up one on front faces, down one on back faces, through the game's own
 * transforms (its volume pass's camera, so each eye gets its correction as the game's volumes do). The device's state
 * is put back after. */
/* Session 14 (owners 62-64): her volumes are closed (sh3vr_ik.cpp shadow_mesh: her own mesh's lit faces, both caps and
 * the sides) and counted the z-fail way, behind what the eyes see: up one on back faces, down one on front faces where
 * the depth test fails. That count is right with the eyes inside a volume (the z-pass count of session 12 turned over
 * there, so such casters were left out whole: her shadow came and went by pieces). The game's darkening reads the same
 * count (17 and over), the room's own volumes counted their way before. */
/* 0.1.3 (owner 84: "body ik shadows can be refined i feel, they seem a bit overbearing as is"): her shadow at half the
 * game's darkening. The darkening is one draw (0x48D474's stencil test, LESSEQUAL 17 on the count, into the mask target
 * set from 0x48D3D2), which the game then blurs through two 256x256 targets and lays over the scene (vr185's log). So
 * the room's volumes alone make the mask first; after that draw the count goes back to 16 (its bits only: her mark 0x20
 * and the game's 0x40 stay), her volumes are counted, and the same draw runs again blended at her strength: where only
 * she shades, the mask goes that far towards the game's shadow; where the room's shadow already is, it stays as it is
 * (a shadow in a shadow is no darker). The game's blur still softens her edges as its own. A mask draw that blends
 * itself (none seen) keeps the full strength. */
#define HER_SHADOW_STRENGTH 0.5f
enum { kHerMaxTris = 24576 };
static float her_tri[kHerMaxTris * 9];
static int her_tris_n, her_mask_frame = -1;
static float her_mask_strength = 1.0f;
static GLuint her_buf;  /* her volumes' own buffer: the mask draw's vertices (the stream buffer) stay for its second run */
/* 0.1.7 (owner 123: "heathers normal shadow seems to be rendered weirdly"): in a lit place the game casts Heather's
 * shadow itself: a record of type 0 for her in its map list (0x491030 keeps the record at 0x8E43A8 and its rows'
 * pointers at 0x8E43AC.., the first row the lamp's place) whose volumes 0x490810 draws from her kg1 shadow model in her
 * ANIMATION's pose, z-pass (vr291/vr294: a shadow of walking legs under the view, the eyes inside her own volume). In
 * first person those draws are left out here (her_game_volume: sh3vr tells hers by her place) and her body as drawn is
 * counted from the same lamp at the darkening's start (her_shadow_ready), into the game's own count: its full
 * strength, its blur. Her own pixels (bit 0x20) stay out of it, as for the flashlight's. */
enum { kRoomLamps = 4 };
static float room_lamp[kRoomLamps][3];
static int room_lamps, room_lamp_frame = -1;
static int her_game_volume(uint32_t va, uint32_t stride, uint32_t n) {
    if (!dev->rs[52] || dev->rs[56] != 8 /* ALWAYS */ || dev->rs[59] != 0x1F || (dev->rs[55] != 7 && dev->rs[55] != 8) ||
        !va || stride < 12 || n < 3 || !dev->ds || !sh3vr_active())
        return 0;
    {
        /* testing (the PC rig): SH3_NO_ROOM_SHADOW=1 leaves the game's own volumes of her as they are */
        static int off = -1;
        if (off < 0) off = getenv("SH3_NO_ROOM_SHADOW") && atoi(getenv("SH3_NO_ROOM_SHADOW")) != 0;
        if (off) return 0;
    }
    /* its corners in the world as given (the game's volumes: the world matrix the identity) */
    const float *w = dev->world[0];
    if (fabsf(w[0] - 1.0f) > 1e-4f || fabsf(w[5] - 1.0f) > 1e-4f || fabsf(w[10] - 1.0f) > 1e-4f || fabsf(w[12]) > 1e-3f ||
        fabsf(w[13]) > 1e-3f || fabsf(w[14]) > 1e-3f || fabsf(w[1]) > 1e-4f || fabsf(w[2]) > 1e-4f || fabsf(w[4]) > 1e-4f)
        return 0;
    const uint32_t row = MEM32(0x8E43AC);
    if (!MEM32(0x8E43A8) || !row) return 0;
    const float lamp[3] = {MEMF(row), MEMF(row + 4), MEMF(row + 8)};
    if (!isfinite(lamp[0]) || !isfinite(lamp[1]) || !isfinite(lamp[2])) return 0;
    if (!sh3vr_her_game_volume((const uint8_t *)GPTR(va), stride, n, lamp)) return 0;
    if (dev->rs[55] == 7 && dev->rs[22] == 3) {  /* the pass's camera, as draw_setup keeps it from the game's volumes */
        memcpy(vol_view, dev->view, sizeof vol_view);
        memcpy(vol_proj, dev->proj, sizeof vol_proj);
        vol_frame = rt_frame;
    }
    if (room_lamp_frame != rt_frame) room_lamp_frame = rt_frame, room_lamps = 0;
    int k = 0;
    while (k < room_lamps && (fabsf(room_lamp[k][0] - lamp[0]) > 1.0f || fabsf(room_lamp[k][1] - lamp[1]) > 1.0f ||
                              fabsf(room_lamp[k][2] - lamp[2]) > 1.0f))
        k++;
    if (k == room_lamps && room_lamps < kRoomLamps) {
        memcpy(room_lamp[room_lamps], lamp, sizeof lamp);
        room_lamps++;
    }
    return 1;
}
static void her_shadow_ready(void) {
    if (room_lamp_frame == rt_frame && room_lamps > 0 && dev->ds) {
        /* her shadow from each room lamp whose volumes of her were left out, counted with the room's before the
         * darkening reads the count (so at the game's own strength) */
        for (int i = 0; i < room_lamps; i++) {
            her_tris_n = sh3vr_her_room_shadow(vol_view, vol_proj, room_lamp[i], her_tri, kHerMaxTris);
            vr_her_shadow();
        }
        her_tris_n = 0;
    }
    room_lamp_frame = -1;
    if (her_mask_frame >= 0 && her_mask_frame != rt_frame) {
        static int told = 0;
        if (!told++)
            rt_log("VR: frame %d: her shadow: the game's mask draw after its stencil test (frame %d) was not seen: her "
                   "shadow left out that frame", rt_frame, her_mask_frame);
    }
    her_tris_n = sh3vr_her_shadow(vol_view, vol_proj, her_tri, kHerMaxTris);
    if (her_tris_n > 0 && dev->ds) her_mask_frame = rt_frame;
}
static int her_mask_due(void) {
    if (her_mask_frame != rt_frame || !dev->rs[52] || dev->rs[56] != 4 /* LESSEQUAL */ || dev->rs[57] != 17) return 0;
    her_mask_frame = -1;
    return 1;
}
static void her_mask_second(void) {
    glStencilMask(0x1F);
    glClearStencil(16);
    glClear(GL_STENCIL_BUFFER_BIT);
    vr_her_shadow();
    her_mask_strength = dev->rs[27] ? 1.0f : HER_SHADOW_STRENGTH;
    static int told = 0;
    if (!told++)
        rt_log("VR: frame %d: her shadow counted after the room's, its mask drawn again at %.0f%% of the game's darkening%s",
               rt_frame, her_mask_strength * 100.0f, dev->rs[27] ? " (the mask draw blends itself: full)" : "");
}
static void her_mask_blend(void) {
    if (her_mask_strength >= 0.999f) return;
    glEnable(GL_BLEND);
    glBlendColor(0.0f, 0.0f, 0.0f, her_mask_strength);
    glBlendFuncSeparate(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA, GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);
    glBlendEquation(GL_FUNC_ADD);
}
static void vr_her_shadow(void) {
    const int n = her_tris_n;
    const float *tri = her_tri;
    if (n <= 0 || !dev->ds) return;
    if (!her_buf) glGenBuffers(1, &her_buf);
    static uint32_t rs_keep[RS_COUNT];
    memcpy(rs_keep, dev->rs, sizeof rs_keep);
    float world_keep[16], view_keep[16], proj_keep[16];
    memcpy(world_keep, dev->world[0], sizeof world_keep);
    memcpy(view_keep, dev->view, sizeof view_keep);
    memcpy(proj_keep, dev->proj, sizeof proj_keep);
    const uint32_t vs_keep = dev->vs, ps_keep = dev->ps, colorop_keep = dev->tss[0][1], alphaop_keep = dev->tss[0][4];
    const int rhw_world_keep = dev->rhw_world, rhw_mapped_keep = dev->rhw_mapped, beam_mesh_keep = dev->beam_mesh,
              no_ztest_keep = dev->no_ztest;
    dev->beam_mesh = dev->no_ztest = 0;
    static const float ident[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    memcpy(dev->world[0], ident, sizeof ident);
    memcpy(dev->view, vol_view, sizeof vol_view);
    memcpy(dev->proj, vol_proj, sizeof vol_proj);
    dev->vs = 0x002; /* D3DFVF_XYZ */
    dev->ps = 0;
    dev->tss[0][1] = 1; /* COLOROP DISABLE */
    dev->tss[0][4] = 1; /* ALPHAOP DISABLE */
    dev->rhw_world = dev->rhw_mapped = 0;
    uint32_t *rs = dev->rs;
    rs[7] = 1; rs[14] = 0; rs[23] = 4;            /* depth tested (LESSEQUAL), not written */
    rs[15] = 0; rs[27] = 0; rs[28] = 0; rs[29] = 0; rs[137] = 0;  /* no alpha test, blend, fog, specular, lighting */
    rs[168] = 0;                                  /* no colour */
    rs[47] = 0;                                   /* no depth bias */
    rs[52] = 1; rs[53] = 1; rs[55] = 1;           /* stencil on, fail and pass KEEP */
    rs[56] = 3; rs[57] = 0; rs[58] = 0x20; rs[59] = 0x1F;  /* only where her own mark (0x20) is clear; the game's count */
    glBindBuffer(GL_ARRAY_BUFFER, her_buf);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)n * 36, tri, GL_STREAM_DRAW);
    uintptr_t base[16] = {0};
    GLuint bufs[16] = {her_buf};
    for (int pass = 0; pass < 2; pass++) {
        rs[22] = pass == 0 ? 2 : 3;   /* clockwise culled: the back faces; then the front */
        rs[54] = pass == 0 ? 7 : 8;   /* depth-fail INCR, then DECR (wrapping in the 1F bits, as the game's volumes) */
        draw_setup(base, bufs, 12);
        glDrawArrays(GL_TRIANGLES, 0, n * 3);
    }
    memcpy(dev->rs, rs_keep, sizeof rs_keep);
    memcpy(dev->world[0], world_keep, sizeof world_keep);
    memcpy(dev->view, view_keep, sizeof view_keep);
    memcpy(dev->proj, proj_keep, sizeof proj_keep);
    dev->vs = vs_keep, dev->ps = ps_keep, dev->tss[0][1] = colorop_keep, dev->tss[0][4] = alphaop_keep;
    dev->rhw_world = rhw_world_keep, dev->rhw_mapped = rhw_mapped_keep, dev->beam_mesh = beam_mesh_keep;
    dev->no_ztest = no_ztest_keep;
}

/* The GL buffer of a vertex or index buffer, created and refreshed from the guest copy on the render
 * thread. */
static GLuint gl_buffer(Res *b, GLenum target) {
    if (!b->buf) { glGenBuffers(1, &b->buf); b->dirty = 1; }
    glBindBuffer(target, b->buf);
    if (b->dirty) {
        glBufferData(target, b->size, GPTR(b->mem), GL_DYNAMIC_DRAW);
        b->dirty = 0;
    }
    return b->buf;
}

static void stream_bases(uintptr_t *base, GLuint *bufs) {
    for (int s = 0; s < 16; s++) {
        Res *vb = res_of(dev->stream[s]);
        base[s] = 0;
        bufs[s] = vb ? gl_buffer(vb, GL_ARRAY_BUFFER) : 0;
    }
}

/* SH3 Quest VR (LIST 24, 46): Heather's weapon drawn in passes (sh3vr_weapon_passes): the draw's faces as a local
 * triangle list (a strip unrolled with its winding kept, degenerate triangles left out), each pass's faces then drawn
 * through the game's own shader, lights and textures with the palette VR gives that pass. Only draws of skinned
 * stride-48 meshes the eyes see (the inventory's own models stay as they are). 0: drawn as the game does. */
static GLuint vr_pass_ebo, vr_pass_vbo;
/* An indexed draw's faces as a local triangle list (a strip unrolled with its winding kept, degenerate triangles left
 * out): the number of faces, 0 for a draw of other primitives or over max. */
static int unroll_tris(const Res *ib, int is32, uint32_t type, uint32_t min_index, uint32_t nv, uint32_t start,
                       uint32_t np, uint16_t *tris, int max) {
    if (!ib->mem || (type != 4 && type != 5) || nv == 0 || nv > 65535 || np == 0 || np > (uint32_t)max) return 0;
    int n = 0;
    for (uint32_t i = 0; i < np; i++) {
        uint32_t k[3];
        for (int j = 0; j < 3; j++) {
            const uint32_t at = type == 4 ? start + i * 3 + j : start + i + j;
            k[j] = (is32 ? MEM32(ib->mem + at * 4) : MEM16(ib->mem + at * 2)) - min_index;
        }
        if (type == 5 && (i & 1)) { const uint32_t t = k[0]; k[0] = k[1]; k[1] = t; }
        if (k[0] == k[1] || k[1] == k[2] || k[0] == k[2] || k[0] >= nv || k[1] >= nv || k[2] >= nv) continue;
        tris[n * 3] = (uint16_t)k[0], tris[n * 3 + 1] = (uint16_t)k[1], tris[n * 3 + 2] = (uint16_t)k[2];
        n++;
    }
    return n;
}
static int vr_weapon_passes(Res *vb, Res *ib, int is32, uint32_t type, uint32_t min_index, uint32_t nv, uint32_t start,
                            uint32_t np, Sh3VrWeaponPass *out, int max) {
    enum { kMaxTris = 4096 };
    static uint16_t tris[kMaxTris * 3];
    if (!sh3vr_active() || dev->stride[0] != 48 || !vb || !vb->mem) return 0;
    const uint32_t first = dev->base_vertex + min_index;
    const int n = unroll_tris(ib, is32, type, min_index, nv, start, np, tris, kMaxTris);
    if (n <= 0) return 0;
    return sh3vr_weapon_passes(nv, np, (const uint8_t *)GPTR(vb->mem + first * 48u), tris, n, out, max);
}
/* LIST 56, Heather's shadow from her own mesh (session 14; the owner, 62-64: "the ratchet and clank shadow works fine
 * and shows the whole body", "dont take shortcuts and be proper"): each skinned draw of hers just drawn for the eyes
 * (her parts in sh3.c's 0x45ED40 wrapper, her_mark; the weapon or a part in her hand, shader_held_draw), with its
 * shader's bone palette, handed to VR as drawn: its vertices, faces and the constants it was given. VR skins it as the
 * game's shader does and casts her shadow from it (sh3vr_ik.cpp shadow_part, shadow_mesh). */
static void her_shadow_capture(const uint8_t *verts48, uint32_t nv, const uint16_t *tris, int ntris, uint32_t key) {
    if (!(her_mark || shader_held_draw == 1) || !shader_last_main || !shader_last_palette || !shader_last_consts ||
        dev->stride[0] != 48 || ntris <= 0 || !sh3vr_active())
        return;
    sh3vr_shadow_part(verts48, (int)nv, tris, ntris, shader_last_consts, key, shader_held_draw ? 1 : 0);
}
static void vr_draw_passes(const Sh3VrWeaponPass *passes, int npass, const uintptr_t *base, const GLuint *bufs,
                           GLint base_vertex, const uint8_t *game_verts, uint32_t game_nv, uint32_t key) {
    if (!vr_pass_ebo) { glGenBuffers(1, &vr_pass_ebo); glGenBuffers(1, &vr_pass_vbo); }
    for (int k = 0; k < npass; k++) {
        const Sh3VrWeaponPass *p = &passes[k];
        if (p->ntris <= 0 || !p->tris) continue;
        uintptr_t pb[16];
        GLuint pbufs[16];
        memcpy(pb, base, sizeof pb);
        memcpy(pbufs, bufs, sizeof pbufs);
        GLint bv = base_vertex;
        if (p->verts) {
            glBindBuffer(GL_ARRAY_BUFFER, vr_pass_vbo);
            glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)p->nverts * 48, p->verts, GL_STREAM_DRAW);
            pbufs[0] = vr_pass_vbo;
            pb[0] = 0;
            bv = 0;
        }
        sh3vr_weapon_pass(k);
        draw_setup(pb, pbufs, 0);
        if (p->texture) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, p->texture);
        }
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vr_pass_ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)p->ntris * 6, p->tris, GL_STREAM_DRAW);
        glDrawElementsBaseVertex(GL_TRIANGLES, p->ntris * 3, GL_UNSIGNED_SHORT, (void *)0, bv);
        /* this pass's faces in her shadow, with the palette this pass was drawn with */
        if (p->verts) her_shadow_capture((const uint8_t *)p->verts, (uint32_t)p->nverts, p->tris, p->ntris,
                                         key ^ (uint32_t)(k + 1) * 0x9E3779B9u);
        else if (game_verts) her_shadow_capture(game_verts, game_nv, p->tris, p->ntris, key ^ (uint32_t)(k + 1) * 0x9E3779B9u);
    }
    sh3vr_weapon_pass(0);
}

METHOD(dev_DrawPrimitive, 4) {
    uintptr_t base[16];
    GLuint bufs[16];
    stream_bases(base, bufs);
    Res *vb = res_of(dev->stream[0]);
    uint32_t n = prim_verts(ARG(1), ARG(3));
    if (vb && vb->mem && her_game_volume(vb->mem + ARG(2) * dev->stride[0], dev->stride[0], n)) return 0;  /* 0.1.7 */
    ROUTED(vr_route(vb && vb->mem ? vb->mem + ARG(2) * dev->stride[0] : 0, dev->stride[0], n),
           draw_setup(base, bufs, 0), glDrawArrays(prim_mode(ARG(1)), ARG(2), n));
    return 0;
}

METHOD(dev_DrawIndexedPrimitive, 6) {
    Res *ib = res_of(dev->ib);
    if (!ib) return D3DERR_INVALIDCALL;
    uintptr_t base[16];
    GLuint bufs[16];
    stream_bases(base, bufs);
    int is32 = ib->fmt == FMT_INDEX32;
    Res *vb = res_of(dev->stream[0]);
    uint32_t first = dev->base_vertex + ARG(2);  /* MinIndex, NumVertices */
    if (vb && vb->mem && her_game_volume(vb->mem + first * dev->stride[0], dev->stride[0], ARG(3))) return 0;  /* 0.1.7 */
    dev->draw_nv = ARG(3), dev->draw_np = ARG(5);  /* SH3 Quest VR: the weapon's mesh signature (d3d8_shader.c) */
    {
        /* testing (LIST 46): SH3_WEAPON_DUMP=N, in the SH3_VSC_DUMP frame: a draw of N vertices, each 4-byte column's
         * distinct values (the bone indices show as a column of few), and its first vertices. */
        static int want = -2, frame = -2;
        if (want == -2) {
            want = getenv("SH3_WEAPON_DUMP") ? atoi(getenv("SH3_WEAPON_DUMP")) : -1;
            frame = getenv("SH3_VSC_DUMP") ? atoi(getenv("SH3_VSC_DUMP")) : -1;
        }
        if (want > 0 && (int)ARG(3) == want && rt_frame == frame && vb && vb->mem && dev->stride[0] >= 4) {
            const uint32_t st = dev->stride[0], at = vb->mem + first * st;
            rt_log("weapon dump: %u vertices, stride %u", ARG(3), st);
            for (uint32_t c = 0; c + 4 <= st; c += 4) {
                uint32_t seen[16]; int n = 0, more = 0;
                for (uint32_t v = 0; v < ARG(3); v++) {
                    uint32_t x = MEM32(at + v * st + c); int k = 0;
                    while (k < n && seen[k] != x) k++;
                    if (k == n) { if (n < 16) seen[n++] = x; else more = 1; }
                }
                char line[300]; int o = snprintf(line, sizeof line, "  +%2u: %s%d values:", c, more ? "over " : "", n);
                for (int k = 0; k < n && o < 280; k++) o += snprintf(line + o, sizeof line - o, " %08x", seen[k]);
                rt_log("%s", line);
            }
        }
        /* testing (LIST 46, the guns' moving parts): SH3_MESH_DUMP=N1,N2,... writes the first skinned draw (stride 48) of
         * each vertex count, in any frame, to mesh_N.txt in the game folder: every vertex's 4-byte words (hex) and the
         * indices with the primitive type, for tools/mesh_parts.py. N/P takes only a draw of N vertices and P
         * primitives (a count Heather's own parts share: the maul's 208). N/P+ (session 5, Heather's gripping hands,
         * several of one count) writes each different mesh of that count, up to 4, to mesh_N_P_0.txt .. _3. W (session
         * 13, LIST 24's extras) writes each different draw of an extra weapon's own model (weapons 9, 10, 13, 14, 17, 18
         * in the hand: a palette of 1-4 bones from its model bone 0), up to 8, to mesh_N_P_wWEAPON.txt. */
        static char mesh_want[512];
        static int mesh_init, mesh_done[40], mesh_n;
        static int mesh_counts[40], mesh_prims[40], mesh_multi[40], mesh_variants[40];
        static uint32_t mesh_hash[40][8];
        if (!mesh_init) {
            mesh_init = 1;
            const char *e = getenv("SH3_MESH_DUMP");
            if (e) {
                snprintf(mesh_want, sizeof mesh_want, "%s", e);
                for (const char *s = mesh_want; *s && mesh_n < 40;) {
                    const char *slash = strchr(s, '/'), *comma = strchr(s, ','), *plus = strchr(s, '+');
                    mesh_prims[mesh_n] = slash && (!comma || slash < comma) ? atoi(slash + 1) : 0;
                    mesh_multi[mesh_n] = (plus && (!comma || plus < comma)) || *s == 'W';
                    mesh_counts[mesh_n++] = *s == 'W' ? -1 : atoi(s);
                    s = comma;
                    if (!s) break;
                    s++;
                }
            }
        }
        for (int k = 0; k < mesh_n; k++) {
            const unsigned held_w = MEM8(0x712C656u);
            const int extra = mesh_counts[k] == -1;
            if (extra ? !(dev->pal_count >= 1 && dev->pal_count <= 4 && dev->pal_bone[0] == 0 &&
                          (held_w == 9 || held_w == 10 || held_w == 13 || held_w == 14 || held_w == 17 || held_w == 18))
                      : (mesh_counts[k] != (int)ARG(3) || (mesh_prims[k] && mesh_prims[k] != (int)ARG(5))))
                continue;
            if (mesh_done[k] || dev->stride[0] != 48 || !vb || !vb->mem || !ib->mem) continue;
            char path[64];
            if (mesh_multi[k]) {
                /* this mesh's own: its first vertices' positions hashed (an extra's: with its counts and weapon) */
                uint32_t hsh = 2166136261u;
                const uint32_t hat = vb->mem + first * dev->stride[0];
                for (uint32_t v = 0; v < ARG(3) && v < 64; v++)
                    for (uint32_t c = 0; c < 12; c += 4) hsh = (hsh ^ MEM32(hat + v * dev->stride[0] + c)) * 16777619u;
                if (extra) hsh = (hsh ^ (ARG(3) * 131u + ARG(5) * 7u + held_w)) * 16777619u;
                int known = 0;
                for (int j = 0; j < mesh_variants[k]; j++) known |= mesh_hash[k][j] == hsh;
                if (known) continue;
                mesh_hash[k][mesh_variants[k]] = hsh;
                if (extra) {
                    snprintf(path, sizeof path, "mesh_%u_%u_w%u.txt", ARG(3), ARG(5), held_w);
                    rt_log("mesh dump: weapon %u's own draw %u/%u, %d bones (model bones %d %d %d %d)", held_w, ARG(3),
                           ARG(5), dev->pal_count, dev->pal_bone[0], dev->pal_bone[1], dev->pal_bone[2], dev->pal_bone[3]);
                } else
                    snprintf(path, sizeof path, "mesh_%u_%u_%d.txt", ARG(3), ARG(5), mesh_variants[k]);
                if (++mesh_variants[k] >= (extra ? 8 : 4)) mesh_done[k] = 1;
            } else {
                mesh_done[k] = 1;
                snprintf(path, sizeof path, "mesh_%u.txt", ARG(3));
            }
            FILE *mf = fopen(path, "w");
            if (!mf) break;
            const uint32_t st = dev->stride[0], at = vb->mem + first * st;
            fprintf(mf, "# frame %d, %u vertices from %u (base %u), primitive type %u, %u primitives from index %u, %s "
                        "indices\n", rt_frame, ARG(3), ARG(2), dev->base_vertex, ARG(1), ARG(5), ARG(4),
                    is32 ? "32-bit" : "16-bit");
            for (uint32_t v = 0; v < ARG(3); v++) {
                fprintf(mf, "v");
                for (uint32_t c = 0; c < st; c += 4) fprintf(mf, " %08x", MEM32(at + v * st + c));
                fprintf(mf, "\n");
            }
            const uint32_t ni = prim_verts(ARG(1), ARG(5));
            fprintf(mf, "i");
            for (uint32_t i = 0; i < ni; i++) {
                const uint32_t idx = is32 ? MEM32(ib->mem + (ARG(4) + i) * 4) : MEM16(ib->mem + (ARG(4) + i) * 2);
                fprintf(mf, " %u", idx - ARG(2));
            }
            fprintf(mf, "\n");
            fclose(mf);
            rt_log("mesh dump: %u vertices written to %s", ARG(3), path);
            /* ... and its stage-0 texture's top level as the game holds it (tools/tex_view.py): "SH3T", format, width,
             * height, pitch, then the bytes. */
            Res *t = res_of(dev->tex[0]);
            Res *s0 = t && t->surf[0][0] ? res_of(t->surf[0][0]) : NULL;
            if (s0 && s0->mem && s0->size) {
                snprintf(path, sizeof path, "mesh_%u_tex.bin", ARG(3));
                FILE *tf = fopen(path, "wb");
                if (tf) {
                    const uint32_t head[5] = {0x54334853u, t->fmt, t->w, t->h, s0->pitch};
                    fwrite(head, 4, 5, tf);
                    fwrite(GPTR(s0->mem), 1, s0->size, tf);
                    fclose(tf);
                    rt_log("mesh dump: its texture (format %08X, %ux%u) written to %s", t->fmt, t->w, t->h, path);
                }
            }
        }
    }
    {
        Sh3VrWeaponPass passes[8];
        const int route = vr_route(vb && vb->mem ? vb->mem + first * dev->stride[0] : 0, dev->stride[0], ARG(3));
        const int npass = route == 0 ? vr_weapon_passes(vb, ib, is32, ARG(1), ARG(2), ARG(3), ARG(4), ARG(5), passes, 8) : 0;
        /* LIST 56 (session 14): this draw told apart from the others across frames (her shadow's parts) */
        const uint32_t key = vb ? (vb->mem + first * 48u) * 2654435761u ^ ARG(4) * 40503u ^ ARG(5) * 97u : 0;
        const uint8_t *verts48 = vb && vb->mem && dev->stride[0] == 48 ? (const uint8_t *)GPTR(vb->mem + first * 48u) : NULL;
        if (npass > 0) vr_draw_passes(passes, npass, base, bufs, (GLint)first, verts48, ARG(3), key);
        else {
            ROUTED(route, (draw_setup(base, bufs, 0), gl_buffer(ib, GL_ELEMENT_ARRAY_BUFFER)),
                   glDrawElementsBaseVertex(prim_mode(ARG(1)), prim_verts(ARG(1), ARG(5)),
                                            is32 ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT,
                                            (void *)(uintptr_t)(ARG(4) * (is32 ? 4 : 2)), dev->base_vertex));
            if ((route == 0 || route == 2) && verts48 && (her_mark || shader_held_draw == 1) && shader_last_main) {
                enum { kMaxTris = 8192 };
                static uint16_t tris[kMaxTris * 3];
                const int nt = unroll_tris(ib, is32, ARG(1), ARG(2), ARG(3), ARG(4), ARG(5), tris, kMaxTris);
                her_shadow_capture(verts48, ARG(3), tris, nt, key);
            }
        }
    }
    dev->draw_nv = dev->draw_np = 0;
    return 0;
}

/* The UP draws stream vertices straight from guest memory; D3D8 then unbinds stream 0 and the indices. */
static void up_upload(uint32_t data, uint32_t bytes) {
    glBindBuffer(GL_ARRAY_BUFFER, dev->stream_buf);
    glBufferData(GL_ARRAY_BUFFER, bytes, GPTR(data), GL_STREAM_DRAW);
}
METHOD(dev_DrawPrimitiveUP, 5) {
    uint32_t n = prim_verts(ARG(1), ARG(2)), stride = ARG(4);
    if (her_game_volume(ARG(3), stride, n)) {  /* 0.1.7: her game shadow volume, left out (VR casts hers) */
        if (dev->stream[0]) res_release(dev->stream[0]);
        dev->stream[0] = 0;
        return 0;
    }
    up_upload(ARG(3), n * stride);
    uintptr_t base[16] = {0};
    GLuint bufs[16] = {dev->stream_buf};
    ROUTED(vr_route(ARG(3), stride, n), draw_setup(base, bufs, stride), glDrawArrays(prim_mode(ARG(1)), 0, n));
    if (dev->stream[0]) res_release(dev->stream[0]);
    dev->stream[0] = 0;
    return 0;
}
METHOD(dev_DrawIndexedPrimitiveUP, 9) {
    uint32_t nidx = prim_verts(ARG(1), ARG(4)), isz = ARG(6) == FMT_INDEX32 ? 4 : 2, stride = ARG(8);
    if (her_game_volume(ARG(7) + ARG(2) * stride, stride, ARG(3))) {  /* 0.1.7: her game shadow volume, left out */
        if (dev->stream[0]) res_release(dev->stream[0]);
        dev->stream[0] = 0;
        if (dev->ib) res_release(dev->ib);
        dev->ib = 0;
        return 0;
    }
    up_upload(ARG(7), (ARG(2) + ARG(3)) * stride);
    uintptr_t base[16] = {0};
    GLuint bufs[16] = {dev->stream_buf};
    ROUTED(vr_route(ARG(7) + ARG(2) * stride, stride, ARG(3)),
           (draw_setup(base, bufs, stride), glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dev->index_buf),
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, nidx * isz, GPTR(ARG(5)), GL_STREAM_DRAW)),
           glDrawElements(prim_mode(ARG(1)), nidx, isz == 4 ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT, 0));
    if (dev->stream[0]) res_release(dev->stream[0]);
    dev->stream[0] = 0;
    if (dev->ib) res_release(dev->ib);
    dev->ib = 0;
    return 0;
}

METHOD(dev_CreateVertexShader, 5) { MEM32(ARG(3)) = shader_create_vs(ARG(1), ARG(2)); return 0; }
METHOD(dev_SetVertexShader, 2) { dev->vs = ARG(1); MARK(vs); return 0; }
METHOD(dev_GetVertexShader, 2) { MEM32(ARG(1)) = dev->vs; return 0; }
METHOD(dev_DeleteVertexShader, 2) { shader_delete_vs(ARG(1)); return 0; }
METHOD(dev_SetVertexShaderConstant, 4) {
    uint32_t r = ARG(1), n = ARG(3);
    if (r < 96) memcpy(dev->vsc[r], GPTR(ARG(2)), 16 * (r + n > 96 ? 96 - r : n));
    for (uint32_t i = r; i < r + n && i < 96; i++) MARK(vsc[i]);
    /* testing (SH3_LIGHT_LOG=1): which game code uploads c80 (the flashlight's projection in the lit shaders), each
     * caller once */
    static int light_log = -1;
    if (light_log < 0) light_log = getenv("SH3_LIGHT_LOG") != NULL;
    if (light_log && r <= 80 && r + n > 80) {
        static uint32_t seen[16];
        const uint32_t from = MEM32(g_esp);
        int k = 0;
        while (k < 16 && seen[k] && seen[k] != from) k++;
        if (k < 16 && !seen[k]) {
            seen[k] = from;
            rt_log("testing: c%u-c%u uploaded from %08X (frame %d)", r, r + n - 1, from, rt_frame);
        }
    }
    /* SH3 Quest VR: how many palette bones the next draw has (the game uploads one bone, 3 registers, at a time from
     * c48 on; a draw's first upload after the last draw starts the count; sh3vr_body_cut matches only those). */
    if (r >= 48 && r < 96) {
        if (dev->pal_new) {
            dev->pal_count = 0, dev->pal_new = 0;
            for (int s = 0; s < 16; s++) dev->pal_bone[s] = -1;
        }
        const int top = (int)((r + n > 96 ? 96 : r + n) - 48 + 2) / 3;
        if (top > dev->pal_count) dev->pal_count = top;
        /* Which model bone each slot holds (SH3 Quest VR, the weapons: a part's palette lists only the bones it uses,
         * so a slot is not the model's bone number): the game uploads a bone from its bone array, [0x6B4540] + 64 bone,
         * or a blend from + 0x2000 + 64 n (docs/GAME_FACTS.md, Bones). */
        const uint32_t bones = MEM32(0x6B4540u);
        for (uint32_t k = 0; k + 3 <= n; k += 3) {
            const uint32_t reg = r + k;
            if (reg < 48 || reg >= 96 || (reg - 48) % 3 != 0) continue;
            const uint32_t src = ARG(2) + 16u * k, off = src - bones;
            dev->pal_bone[(reg - 48) / 3] = bones && off < 0x2000u && off % 64 == 0   ? (int)(off / 64)
                                            : bones && off - 0x2000u < 0x2000u && off % 64 == 0 ? 0x80 + (int)((off - 0x2000u) / 64)
                                                                                                : -1;
        }
    }
    /* testing (LIST 19): in the SH3_VSC_DUMP frame, where each bone palette upload (c48 on) comes from and the game's
     * own buffer it reads (the return address is the guest stack's top). */
    static int dump_at = -2;
    if (dump_at == -2) dump_at = getenv("SH3_VSC_DUMP") ? atoi(getenv("SH3_VSC_DUMP")) : -1;
    if (dump_at >= 0 && rt_frame == dump_at && r >= 48 && n >= 3)
        rt_log("vsc upload c%u x%u from %08X, data at %08X", r, n, MEM32(g_esp), ARG(2));
    return 0;
}
METHOD(dev_GetVertexShaderConstant, 4) {
    uint32_t r = ARG(1), n = ARG(3);
    if (r < 96) memcpy(GPTR(ARG(2)), dev->vsc[r], 16 * (r + n > 96 ? 96 - r : n));
    return 0;
}
METHOD(dev_SetStreamSource, 4) {
    uint32_t s = ARG(1);
    if (s >= 16) return D3DERR_INVALIDCALL;
    if (ARG(2) && !rec_mask) res_addref(ARG(2));
    if (dev->stream[s] && !rec_mask) res_release(dev->stream[s]);
    dev->stream[s] = ARG(2);
    MARK(stream[s]);
    dev->stride[s] = ARG(3);
    return 0;
}
METHOD(dev_GetStreamSource, 4) {
    uint32_t s = ARG(1) & 15;
    if (dev->stream[s]) res_addref(dev->stream[s]);
    MEM32(ARG(2)) = dev->stream[s];
    MEM32(ARG(3)) = dev->stride[s];
    return 0;
}
METHOD(dev_SetIndices, 3) {
    MARK(ib);
    if (ARG(1) && !rec_mask) res_addref(ARG(1));
    if (dev->ib && !rec_mask) res_release(dev->ib);
    dev->ib = ARG(1);
    dev->base_vertex = ARG(2);
    return 0;
}
METHOD(dev_GetIndices, 3) {
    if (dev->ib) res_addref(dev->ib);
    MEM32(ARG(1)) = dev->ib;
    MEM32(ARG(2)) = dev->base_vertex;
    return 0;
}
METHOD(dev_CreatePixelShader, 3) { MEM32(ARG(2)) = shader_create_ps(ARG(1)); return 0; }
METHOD(dev_SetPixelShader, 2) { dev->ps = ARG(1); MARK(ps); return 0; }
METHOD(dev_GetPixelShader, 2) { MEM32(ARG(1)) = dev->ps; return 0; }
METHOD(dev_DeletePixelShader, 2) { shader_delete_ps(ARG(1)); return 0; }
METHOD(dev_SetPixelShaderConstant, 4) {
    uint32_t r = ARG(1), n = ARG(3);
    if (r < 8) memcpy(dev->psc[r], GPTR(ARG(2)), 16 * (r + n > 8 ? 8 - r : n));
    for (uint32_t i = r; i < r + n && i < 8; i++) MARK(psc[i]);
    return 0;
}
METHOD(dev_GetPixelShaderConstant, 4) {
    uint32_t r = ARG(1), n = ARG(3);
    if (r < 8) memcpy(GPTR(ARG(2)), dev->psc[r], 16 * (r + n > 8 ? 8 - r : n));
    return 0;
}

METHOD(dev_GetFrontBuffer, 2) { return res_read_front(ARG(1)); }
METHOD(dev_CopyRects, 6) { return res_copy_rects(ARG(1), ARG(2), ARG(3), ARG(4), ARG(5)); }

static const ComEntry dev_vt[] = {
    {"IDirect3DDevice8::QueryInterface", dev_QueryInterface}, {"IDirect3DDevice8::AddRef", dev_AddRef},
    {"IDirect3DDevice8::Release", dev_Release}, {"IDirect3DDevice8::TestCooperativeLevel", dev_TestCooperativeLevel},
    {"IDirect3DDevice8::GetAvailableTextureMem", dev_GetAvailableTextureMem},
    {"IDirect3DDevice8::ResourceManagerDiscardBytes", dev_ResourceManagerDiscardBytes},
    {"IDirect3DDevice8::GetDirect3D", dev_GetDirect3D}, {"IDirect3DDevice8::GetDeviceCaps", dev_GetDeviceCaps},
    {"IDirect3DDevice8::GetDisplayMode", dev_GetDisplayMode},
    {"IDirect3DDevice8::GetCreationParameters", dev_GetCreationParameters},
    {"IDirect3DDevice8::SetCursorProperties", dev_SetCursorProperties},
    {"IDirect3DDevice8::SetCursorPosition", dev_SetCursorPosition}, {"IDirect3DDevice8::ShowCursor", dev_ShowCursor},
    {"IDirect3DDevice8::CreateAdditionalSwapChain", NULL}, {"IDirect3DDevice8::Reset", dev_Reset},
    {"IDirect3DDevice8::Present", dev_Present}, {"IDirect3DDevice8::GetBackBuffer", dev_GetBackBuffer},
    {"IDirect3DDevice8::GetRasterStatus", dev_GetRasterStatus}, {"IDirect3DDevice8::SetGammaRamp", dev_SetGammaRamp},
    {"IDirect3DDevice8::GetGammaRamp", dev_GetGammaRamp}, {"IDirect3DDevice8::CreateTexture", dev_CreateTexture},
    {"IDirect3DDevice8::CreateVolumeTexture", NULL}, {"IDirect3DDevice8::CreateCubeTexture", dev_CreateCubeTexture},
    {"IDirect3DDevice8::CreateVertexBuffer", dev_CreateVertexBuffer},
    {"IDirect3DDevice8::CreateIndexBuffer", dev_CreateIndexBuffer},
    {"IDirect3DDevice8::CreateRenderTarget", dev_CreateRenderTarget},
    {"IDirect3DDevice8::CreateDepthStencilSurface", dev_CreateDepthStencilSurface},
    {"IDirect3DDevice8::CreateImageSurface", dev_CreateImageSurface}, {"IDirect3DDevice8::CopyRects", dev_CopyRects},
    {"IDirect3DDevice8::UpdateTexture", NULL}, {"IDirect3DDevice8::GetFrontBuffer", dev_GetFrontBuffer},
    {"IDirect3DDevice8::SetRenderTarget", dev_SetRenderTarget}, {"IDirect3DDevice8::GetRenderTarget", dev_GetRenderTarget},
    {"IDirect3DDevice8::GetDepthStencilSurface", dev_GetDepthStencilSurface},
    {"IDirect3DDevice8::BeginScene", dev_BeginScene}, {"IDirect3DDevice8::EndScene", dev_EndScene},
    {"IDirect3DDevice8::Clear", dev_Clear}, {"IDirect3DDevice8::SetTransform", dev_SetTransform},
    {"IDirect3DDevice8::GetTransform", dev_GetTransform}, {"IDirect3DDevice8::MultiplyTransform", dev_MultiplyTransform},
    {"IDirect3DDevice8::SetViewport", dev_SetViewport}, {"IDirect3DDevice8::GetViewport", dev_GetViewport},
    {"IDirect3DDevice8::SetMaterial", dev_SetMaterial}, {"IDirect3DDevice8::GetMaterial", dev_GetMaterial},
    {"IDirect3DDevice8::SetLight", dev_SetLight}, {"IDirect3DDevice8::GetLight", NULL},
    {"IDirect3DDevice8::LightEnable", dev_LightEnable}, {"IDirect3DDevice8::GetLightEnable", dev_GetLightEnable},
    {"IDirect3DDevice8::SetClipPlane", dev_SetClipPlane}, {"IDirect3DDevice8::GetClipPlane", NULL},
    {"IDirect3DDevice8::SetRenderState", dev_SetRenderState}, {"IDirect3DDevice8::GetRenderState", dev_GetRenderState},
    {"IDirect3DDevice8::BeginStateBlock", dev_BeginStateBlock}, {"IDirect3DDevice8::EndStateBlock", dev_EndStateBlock},
    {"IDirect3DDevice8::ApplyStateBlock", dev_ApplyStateBlock},
    {"IDirect3DDevice8::CaptureStateBlock", dev_CaptureStateBlock},
    {"IDirect3DDevice8::DeleteStateBlock", dev_DeleteStateBlock},
    {"IDirect3DDevice8::CreateStateBlock", dev_CreateStateBlock},
    {"IDirect3DDevice8::SetClipStatus", dev_SetClipStatus}, {"IDirect3DDevice8::GetClipStatus", dev_GetClipStatus},
    {"IDirect3DDevice8::GetTexture", dev_GetTexture}, {"IDirect3DDevice8::SetTexture", dev_SetTexture},
    {"IDirect3DDevice8::GetTextureStageState", dev_GetTextureStageState},
    {"IDirect3DDevice8::SetTextureStageState", dev_SetTextureStageState},
    {"IDirect3DDevice8::ValidateDevice", dev_ValidateDevice}, {"IDirect3DDevice8::GetInfo", NULL},
    {"IDirect3DDevice8::SetPaletteEntries", dev_SetPaletteEntries},
    {"IDirect3DDevice8::GetPaletteEntries", dev_GetPaletteEntries},
    {"IDirect3DDevice8::SetCurrentTexturePalette", dev_SetCurrentTexturePalette},
    {"IDirect3DDevice8::GetCurrentTexturePalette", dev_GetCurrentTexturePalette},
    {"IDirect3DDevice8::DrawPrimitive", dev_DrawPrimitive},
    {"IDirect3DDevice8::DrawIndexedPrimitive", dev_DrawIndexedPrimitive},
    {"IDirect3DDevice8::DrawPrimitiveUP", dev_DrawPrimitiveUP},
    {"IDirect3DDevice8::DrawIndexedPrimitiveUP", dev_DrawIndexedPrimitiveUP},
    {"IDirect3DDevice8::ProcessVertices", NULL}, {"IDirect3DDevice8::CreateVertexShader", dev_CreateVertexShader},
    {"IDirect3DDevice8::SetVertexShader", dev_SetVertexShader}, {"IDirect3DDevice8::GetVertexShader", dev_GetVertexShader},
    {"IDirect3DDevice8::DeleteVertexShader", dev_DeleteVertexShader},
    {"IDirect3DDevice8::SetVertexShaderConstant", dev_SetVertexShaderConstant},
    {"IDirect3DDevice8::GetVertexShaderConstant", dev_GetVertexShaderConstant},
    {"IDirect3DDevice8::GetVertexShaderDeclaration", NULL}, {"IDirect3DDevice8::GetVertexShaderFunction", NULL},
    {"IDirect3DDevice8::SetStreamSource", dev_SetStreamSource}, {"IDirect3DDevice8::GetStreamSource", dev_GetStreamSource},
    {"IDirect3DDevice8::SetIndices", dev_SetIndices}, {"IDirect3DDevice8::GetIndices", dev_GetIndices},
    {"IDirect3DDevice8::CreatePixelShader", dev_CreatePixelShader},
    {"IDirect3DDevice8::SetPixelShader", dev_SetPixelShader}, {"IDirect3DDevice8::GetPixelShader", dev_GetPixelShader},
    {"IDirect3DDevice8::DeletePixelShader", dev_DeletePixelShader},
    {"IDirect3DDevice8::SetPixelShaderConstant", dev_SetPixelShaderConstant},
    {"IDirect3DDevice8::GetPixelShaderConstant", dev_GetPixelShaderConstant},
    {"IDirect3DDevice8::GetPixelShaderFunction", NULL}, {"IDirect3DDevice8::DrawRectPatch", NULL},
    {"IDirect3DDevice8::DrawTriPatch", NULL}, {"IDirect3DDevice8::DeletePatch", NULL},
};

static uint32_t device_vtable(void) { return com_vtable(dev_vt, sizeof dev_vt / sizeof *dev_vt); }
