/* Textures, surfaces and buffers. The game locks a guest memory copy; GL work happens lazily on the
 * render thread (at bind or draw time), so a loader thread may create and fill resources. */
#include "d3d8.h"
#include "../vr/sh3vr.h"

static Res *table[1 << 16];
static uint32_t next_slot = 1;

/* NULL for NULL, and for anything that is not a live resource object (a released texture the game
 * still has bound, say), which is logged once. */
Res *res_of(uint32_t obj) {
    if (!obj) return NULL;
    uint32_t slot = MEM32(obj + 4);
    Res *r = slot < (1u << 16) ? table[slot] : NULL;
    if (r && r->obj == obj) return r;
    static uint32_t warned;
    if (warned != obj) rt_log("D3D: %08X is not a live resource (slot %08X)", obj, slot), warned = obj;
    return NULL;
}

/* ---- formats ----
 * SH3 Quest VR: OpenGL ES 3.2 has no BGRA uploads and no 1_5_5_5_REV / 4_4_4_4_REV types (sh3-nx used them on
 * desktop GL). So: 32-bit BGRA textures upload their bytes unchanged as RGBA and the sampler swizzles red and blue
 * back (no CPU work); render targets hold real RGBA (what the shaders write) and swap on the CPU only when the
 * game locks or copies into one; 1555 and 4444 texels are rotated into ES's 5551 and 4444 order; DXT stays
 * compressed where the driver has S3TC (the PC) and is decoded to RGBA8 where not (the Quest). */
enum { CONV_NONE, CONV_1555, CONV_4444 };
typedef struct { uint32_t fmt; int bpp, block; GLenum ifmt, gfmt, type; GLint swz[4]; int bgra, conv; } Fmt;
#define RGBA {GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA}
#define RGB1 {GL_RED, GL_GREEN, GL_BLUE, GL_ONE}
static const Fmt fmts[] = {
    {FMT_A8R8G8B8, 4, 0, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, {GL_BLUE, GL_GREEN, GL_RED, GL_ALPHA}, 1, 0},
    {FMT_X8R8G8B8, 4, 0, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, {GL_BLUE, GL_GREEN, GL_RED, GL_ONE}, 1, 0},
    {FMT_R5G6B5, 2, 0, GL_RGB565, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, RGB1, 0, 0},
    {FMT_X1R5G5B5, 2, 0, GL_RGB5_A1, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, RGB1, 0, CONV_1555},
    {FMT_A1R5G5B5, 2, 0, GL_RGB5_A1, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, RGBA, 0, CONV_1555},
    {FMT_A4R4G4B4, 2, 0, GL_RGBA4, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, RGBA, 0, CONV_4444},
    {FMT_X4R4G4B4, 2, 0, GL_RGBA4, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, RGB1, 0, CONV_4444},
    {FMT_A8, 1, 0, GL_R8, GL_RED, GL_UNSIGNED_BYTE, {GL_ZERO, GL_ZERO, GL_ZERO, GL_RED}, 0, 0},
    {FMT_L8, 1, 0, GL_R8, GL_RED, GL_UNSIGNED_BYTE, {GL_RED, GL_RED, GL_RED, GL_ONE}, 0, 0},
    {FMT_A8L8, 2, 0, GL_RG8, GL_RG, GL_UNSIGNED_BYTE, {GL_RED, GL_RED, GL_RED, GL_GREEN}, 0, 0},
    {FMT_V8U8, 2, 0, GL_RG8_SNORM, GL_RG, GL_BYTE, {GL_RED, GL_GREEN, GL_ONE, GL_ONE}, 0, 0},
    {FMT_P8, 1, 0, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, RGBA, 0, 0},  /* expanded through the palette */
    {FMT_DXT1, 0, 8, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 0, 0, RGBA, 0, 0},
    {FMT_DXT2, 0, 16, GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, 0, 0, RGBA, 0, 0},
    {FMT_DXT3, 0, 16, GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, 0, 0, RGBA, 0, 0},
    {FMT_DXT4, 0, 16, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, 0, 0, RGBA, 0, 0},
    {FMT_DXT5, 0, 16, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, 0, 0, RGBA, 0, 0},
    {FMT_D16, 2, 0, GL_DEPTH24_STENCIL8, 0, 0, RGBA}, {FMT_D16_LOCKABLE, 2, 0, GL_DEPTH24_STENCIL8, 0, 0, RGBA},
    {FMT_D24S8, 4, 0, GL_DEPTH24_STENCIL8, 0, 0, RGBA}, {FMT_D24X8, 4, 0, GL_DEPTH24_STENCIL8, 0, 0, RGBA},
    {FMT_D24X4S4, 4, 0, GL_DEPTH24_STENCIL8, 0, 0, RGBA}, {FMT_D32, 4, 0, GL_DEPTH24_STENCIL8, 0, 0, RGBA},
    {FMT_D15S1, 2, 0, GL_DEPTH24_STENCIL8, 0, 0, RGBA},
};
static const Fmt *fmt_info(uint32_t f) {
    for (size_t i = 0; i < sizeof fmts / sizeof *fmts; i++)
        if (fmts[i].fmt == f) return &fmts[i];
    rt_fatal("unsupported D3D format %u (0x%08X)", f, f);
}
static int is_depth_fmt(uint32_t f) { return f >= FMT_D16_LOCKABLE && f <= FMT_D16; }

/* ---- texel conversions (SH3 Quest VR) ---- */

/* DXT blocks to RGBA8 rows (bw x bh texels, the size rounded up to whole blocks), D3D's palette rules: DXT1's
 * second mode (c0 <= c1) has black-transparent as its fourth color; DXT2-5 always use four colors. */
static void rgb565(uint16_t c, uint8_t *o) {
    o[0] = (uint8_t)((c >> 11 & 31) * 255 / 31);
    o[1] = (uint8_t)((c >> 5 & 63) * 255 / 63);
    o[2] = (uint8_t)((c & 31) * 255 / 31);
    o[3] = 255;
}
static void dxt_color(const uint8_t *b, int dxt1, uint8_t pal[4][4]) {
    uint16_t c0 = (uint16_t)(b[0] | b[1] << 8), c1 = (uint16_t)(b[2] | b[3] << 8);
    rgb565(c0, pal[0]);
    rgb565(c1, pal[1]);
    if (!dxt1 || c0 > c1) {
        for (int i = 0; i < 3; i++) {
            pal[2][i] = (uint8_t)((2 * pal[0][i] + pal[1][i] + 1) / 3);
            pal[3][i] = (uint8_t)((pal[0][i] + 2 * pal[1][i] + 1) / 3);
        }
        pal[2][3] = pal[3][3] = 255;
    } else {
        for (int i = 0; i < 3; i++) pal[2][i] = (uint8_t)((pal[0][i] + pal[1][i]) / 2);
        pal[2][3] = 255;
        pal[3][0] = pal[3][1] = pal[3][2] = pal[3][3] = 0;
    }
}
static void dxt_decode(uint32_t fmt, const uint8_t *src, uint32_t w, uint32_t h, uint8_t *dst) {
    uint32_t bw = (w + 3) / 4, bh = (h + 3) / 4, stride = bw * 4 * 4;
    int dxt1 = fmt == FMT_DXT1, bsize = dxt1 ? 8 : 16;
    for (uint32_t by = 0; by < bh; by++)
        for (uint32_t bx = 0; bx < bw; bx++, src += bsize) {
            const uint8_t *cb = dxt1 ? src : src + 8;
            uint8_t pal[4][4], alpha[16];
            dxt_color(cb, dxt1, pal);
            if (fmt == FMT_DXT2 || fmt == FMT_DXT3)
                for (int i = 0; i < 16; i++) alpha[i] = (uint8_t)((src[i / 2] >> (4 * (i & 1)) & 15) * 17);
            else if (fmt == FMT_DXT4 || fmt == FMT_DXT5) {
                uint8_t a[8] = {src[0], src[1]};
                if (a[0] > a[1]) for (int i = 1; i < 7; i++) a[i + 1] = (uint8_t)(((7 - i) * a[0] + i * a[1] + 3) / 7);
                else {
                    for (int i = 1; i < 5; i++) a[i + 1] = (uint8_t)(((5 - i) * a[0] + i * a[1] + 2) / 5);
                    a[6] = 0;
                    a[7] = 255;
                }
                uint64_t bits = 0;
                for (int i = 0; i < 6; i++) bits |= (uint64_t)src[2 + i] << (8 * i);
                for (int i = 0; i < 16; i++) alpha[i] = a[bits >> (3 * i) & 7];
            }
            uint32_t idx = (uint32_t)(cb[4] | cb[5] << 8 | cb[6] << 16 | (uint32_t)cb[7] << 24);
            for (int i = 0; i < 16; i++) {
                uint8_t *o = dst + (by * 4 + i / 4) * stride + (bx * 4 + i % 4) * 4;
                memcpy(o, pal[idx >> (2 * i) & 3], 4);
                if (!dxt1) o[3] = alpha[i];
            }
        }
}

/* The guest's texels as the GL texture takes them, or NULL when they go up unchanged (free with free()).
 * to_rt: the GL texture is a render target (real RGBA, no sampler swizzle). */
static void *to_gl(const Fmt *f, const void *src, uint32_t n_texels, int to_rt) {
    if (f->bgra && to_rt) {
        uint8_t *o = malloc(n_texels * 4);
        const uint8_t *s = src;
        for (uint32_t i = 0; i < n_texels; i++) {
            o[4 * i] = s[4 * i + 2]; o[4 * i + 1] = s[4 * i + 1]; o[4 * i + 2] = s[4 * i]; o[4 * i + 3] = s[4 * i + 3];
        }
        return o;
    }
    if (f->conv == CONV_1555 || f->conv == CONV_4444) {
        uint16_t *o = malloc(n_texels * 2);
        const uint16_t *s = src;
        int r = f->conv == CONV_1555 ? 1 : 4;  /* ARGB -> RGBA: rotate the alpha bits to the bottom */
        for (uint32_t i = 0; i < n_texels; i++) o[i] = (uint16_t)(s[i] << r | s[i] >> (16 - r));
        return o;
    }
    return NULL;
}

/* RGBA8 rows read back from a render target into the surface's D3D format. */
static void from_rgba8(uint32_t fmt, const uint8_t *s, uint8_t *dst, uint32_t n_texels) {
    for (uint32_t i = 0; i < n_texels; i++, s += 4) {
        uint32_t r = s[0], g = s[1], b = s[2], a = s[3];
        switch (fmt) {
        case FMT_A8R8G8B8: case FMT_X8R8G8B8:
            dst[4 * i] = (uint8_t)b; dst[4 * i + 1] = (uint8_t)g; dst[4 * i + 2] = (uint8_t)r; dst[4 * i + 3] = (uint8_t)a;
            break;
        case FMT_R5G6B5: ((uint16_t *)dst)[i] = (uint16_t)(r >> 3 << 11 | g >> 2 << 5 | b >> 3); break;
        case FMT_X1R5G5B5: case FMT_A1R5G5B5: ((uint16_t *)dst)[i] = (uint16_t)((a >= 128) << 15 | r >> 3 << 10 | g >> 3 << 5 | b >> 3); break;
        case FMT_A4R4G4B4: case FMT_X4R4G4B4: ((uint16_t *)dst)[i] = (uint16_t)(a >> 4 << 12 | r >> 4 << 8 | g >> 4 << 4 | b >> 4); break;
        default: break;
        }
    }
}

static void set_swizzle(GLenum target, const Fmt *f, int is_rt) {
    static const GLenum names[4] = {GL_TEXTURE_SWIZZLE_R, GL_TEXTURE_SWIZZLE_G, GL_TEXTURE_SWIZZLE_B, GL_TEXTURE_SWIZZLE_A};
    for (int i = 0; i < 4; i++) {
        GLint s = f->swz[i];
        if (f->bgra && is_rt) s = s == GL_RED ? GL_BLUE : s == GL_BLUE ? GL_RED : s;  /* real RGBA in targets */
        glTexParameteri(target, names[i], s);
    }
}

static void level_size(const Fmt *f, uint32_t w, uint32_t h, uint32_t *pitch, uint32_t *size) {
    if (f->block) {
        *pitch = (w + 3) / 4 * f->block;
        *size = *pitch * ((h + 3) / 4);
    } else {
        *pitch = w * f->bpp;
        *size = *pitch * h;
    }
}

/* ---- objects ---- */
static uint32_t vt_tex, vt_cube, vt_surf, vt_vb, vt_ib;

static Res *new_res(int kind, uint32_t vtable) {
    uint32_t slot = 0;
    for (uint32_t i = 0; i < (1u << 16) && !slot; i++) {
        uint32_t s = (next_slot + i) & 0xFFFF;
        if (s && !table[s]) slot = s;
    }
    if (!slot) rt_fatal("too many D3D resources");
    next_slot = slot + 1;
    Res *r = calloc(1, sizeof *r);
    r->kind = kind;
    r->refs = 1;
    r->obj = com_new(vtable, 16);
    MEM32(r->obj + 4) = slot;
    table[slot] = r;
    return r;
}

static void free_res(Res *r) {
    if (r->tex && !r->parent) glDeleteTextures(1, &r->tex);
    if (r->buf) glDeleteBuffers(1, &r->buf);
    if (r->mem) gfree(r->mem);
    for (int f = 0; f < 6; f++)
        for (int l = 0; l < 14; l++)
            if (r->surf[f][l]) free_res(res_of(r->surf[f][l]));
    table[MEM32(r->obj + 4)] = NULL;
    gfree(r->obj);
    free(r);
}

void res_addref(uint32_t obj) {
    Res *r = res_of(obj);
    if (!r) return;
    if (r->parent) r = r->parent;  /* a texture level shares its texture's count */
    r->refs++;
}
void res_release(uint32_t obj) {
    Res *r = res_of(obj);
    if (!r) return;
    if (r->parent) r = r->parent;
    /* ponytail: objects are freed at zero, but GL names deleted off the render thread would leak;
     * all game releases seen so far come from the render thread. */
    if (--r->refs == 0) free_res(r);
}

uint32_t res_surface(uint32_t w, uint32_t h, uint32_t fmt, int is_rt, int is_depth) {
    Res *r = new_res(RES_SURFACE, vt_surf);
    r->fmt = fmt;
    r->w = w;
    r->h = h;
    r->is_rt = is_rt;
    r->is_depth = is_depth || is_depth_fmt(fmt);
    level_size(fmt_info(fmt), w, h, &r->pitch, &r->size);
    if (!r->is_rt && !r->is_depth) r->mem = galloc(r->size);  /* image surfaces live in guest memory only */
    return r->obj;
}

uint32_t res_texture(uint32_t w, uint32_t h, uint32_t levels, uint32_t usage, uint32_t fmt, uint32_t pool, int cube) {
    Res *t = new_res(cube ? RES_CUBE : RES_TEXTURE, cube ? vt_cube : vt_tex);
    uint32_t max = 1;
    while ((w >> max) || (h >> max)) max++;
    if (!levels || levels > max) levels = max;
    if (levels > 14) levels = 14;
    *t = (Res){t->kind, t->obj, 1, fmt, usage, pool, w, h, levels};
    t->is_rt = usage & 1;
    t->is_depth = (usage & 2) || is_depth_fmt(fmt);
    const Fmt *f = fmt_info(fmt);
    for (int face = 0; face < (cube ? 6 : 1); face++)
        for (uint32_t l = 0; l < levels; l++) {
            Res *s = new_res(RES_SURFACE, vt_surf);
            s->fmt = fmt;
            s->w = w >> l ? w >> l : 1;
            s->h = h >> l ? h >> l : 1;
            s->parent = t;
            s->level = l;
            s->face = face;
            s->is_rt = t->is_rt;
            s->is_depth = t->is_depth;
            level_size(f, s->w, s->h, &s->pitch, &s->size);
            if (!s->is_rt && !s->is_depth) s->mem = galloc(s->size);
            t->surf[face][l] = s->obj;
        }
    return t->obj;
}

uint32_t res_buffer(int kind, uint32_t size, uint32_t usage, uint32_t fmt_or_fvf, uint32_t pool) {
    Res *b = new_res(kind, kind == RES_VB ? vt_vb : vt_ib);
    b->size = size;
    b->usage = usage;
    b->fmt = fmt_or_fvf;
    b->pool = pool;
    b->mem = galloc(size);
    b->dirty = 1;
    return b->obj;
}

/* ---- GL side ---- */

static GLenum face_target(Res *t, int face) {
    return t->kind == RES_CUBE ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + face : GL_TEXTURE_2D;
}

/* Creates the GL texture of a texture or standalone surface, with every level allocated. */
static void gl_ensure(Res *t) {
    if (t->tex) return;
    const Fmt *f = fmt_info(t->fmt);
    /* SH3 Quest VR: with the headset on, every render target and depth surface the game makes is a 2-layer array, a
     * layer an eye, drawn into with multiview (res_bind_targets) and sampled per eye (d3d8_shader.c, Key.arr): the
     * scene, its post effects, reflections and shadows all pass through them. A cube's faces stay single. */
    t->layers = sh3vr_active() && (t->is_rt || t->is_depth) && t->kind != RES_CUBE ? 2 : 1;
    if (t->layers == 2) {
        uint32_t levels = t->kind == RES_SURFACE ? 1 : t->levels;
        glGenTextures(1, &t->tex);
        glBindTexture(GL_TEXTURE_2D_ARRAY, t->tex);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, levels - 1);
        set_swizzle(GL_TEXTURE_2D_ARRAY, f, t->is_rt);
        for (uint32_t l = 0; l < levels; l++) {
            uint32_t w = t->w >> l ? t->w >> l : 1, h = t->h >> l ? t->h >> l : 1;
            if (t->is_depth)
                glTexImage3D(GL_TEXTURE_2D_ARRAY, l, GL_DEPTH24_STENCIL8, w, h, 2, 0, GL_DEPTH_STENCIL,
                             GL_UNSIGNED_INT_24_8, NULL);
            else glTexImage3D(GL_TEXTURE_2D_ARRAY, l, f->ifmt, w, h, 2, 0, f->gfmt, f->type, NULL);
        }
        return;
    }
    GLenum target = t->kind == RES_CUBE ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;
    glGenTextures(1, &t->tex);
    glBindTexture(target, t->tex);
    uint32_t levels = t->kind == RES_SURFACE ? 1 : t->levels;
    glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, levels - 1);
    set_swizzle(target, f, t->is_rt);
    for (int face = 0; face < (t->kind == RES_CUBE ? 6 : 1); face++)
        for (uint32_t l = 0; l < levels; l++) {
            uint32_t w = t->w >> l ? t->w >> l : 1, h = t->h >> l ? t->h >> l : 1, pitch, size;
            if (t->is_depth)
                glTexImage2D(face_target(t, face), l, GL_DEPTH24_STENCIL8, w, h, 0, GL_DEPTH_STENCIL,
                             GL_UNSIGNED_INT_24_8, NULL);
            else if (f->block && !gl_caps.s3tc)  /* decoded at upload */
                glTexImage2D(face_target(t, face), l, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
            else if (f->block) {
                level_size(f, w, h, &pitch, &size);
                glCompressedTexImage2D(face_target(t, face), l, f->ifmt, w, h, 0, size, NULL);
            } else glTexImage2D(face_target(t, face), l, f->ifmt, w, h, 0, f->gfmt, f->type, NULL);
        }
}

static GLuint surface_tex(Res *s) { if (s->parent) { gl_ensure(s->parent); return s->parent->tex; } gl_ensure(s); return s->tex; }
/* 2 when the surface's texture is per eye (after surface_tex). */
static int surface_layers(Res *s) { return (s->parent ? s->parent : s)->layers == 2 ? 2 : 1; }
int res_layers(Res *t) { return t && t->layers == 2 ? 2 : 1; }

/* Owner 84 (session 18: "theres still this weird fuzz over the screen, which i want completely off by default"): a
 * texture's level 0 averaged into a 1x1 texture, as the shader would sample it (the format's channels and swizzle
 * applied), from the game's own copy of it. The game's second grain (d3d8.c bind_textures, 0x429E3E) subtracts a
 * 128x128 noise tile from the picture; its average instead keeps the game's look without the grain. 0 when the texture
 * has no guest copy (a render target). The texture is redone every call: one small texture once a frame. */
GLuint res_flat_texture(Res *t, uint8_t out_rgba[4]) {
    static GLuint flat;
    Res *s = t && t->kind == RES_TEXTURE ? res_of(t->surf[0][0]) : NULL;
    if (!s || !s->mem || !s->w || !s->h) return 0;
    const Fmt *f = fmt_info(s->fmt);
    const uint32_t w = s->w, h = s->h;
    uint8_t *rgba = NULL;
    if (f->block) {
        uint32_t pw = (w + 3) & ~3u, ph = (h + 3) & ~3u;
        uint8_t *full = malloc((size_t)pw * ph * 4);
        dxt_decode(s->fmt, GPTR(s->mem), w, h, full);
        rgba = malloc((size_t)w * h * 4);
        for (uint32_t y = 0; y < h; y++) memcpy(rgba + (size_t)y * w * 4, full + (size_t)y * pw * 4, (size_t)w * 4);
        free(full);
    } else {
        rgba = malloc((size_t)w * h * 4);
        static const uint32_t none[256];
        const uint32_t *pal = dev->palette[dev->cur_palette] ? dev->palette[dev->cur_palette] : none;
        for (uint32_t y = 0; y < h; y++) {
            const uint8_t *row = GPTR(s->mem + y * s->pitch);
            for (uint32_t x = 0; x < w; x++) {
                uint8_t *o = rgba + ((size_t)y * w + x) * 4;
                uint32_t r = 0, g = 0, b = 0, a = 255, v;
                switch (s->fmt) {
                case FMT_A8R8G8B8: b = row[4 * x]; g = row[4 * x + 1]; r = row[4 * x + 2]; a = row[4 * x + 3]; break;
                case FMT_X8R8G8B8: b = row[4 * x]; g = row[4 * x + 1]; r = row[4 * x + 2]; break;
                case FMT_R5G6B5:
                    v = ((const uint16_t *)row)[x];
                    r = (v >> 11 & 31) * 255 / 31; g = (v >> 5 & 63) * 255 / 63; b = (v & 31) * 255 / 31;
                    break;
                case FMT_X1R5G5B5: case FMT_A1R5G5B5:
                    v = ((const uint16_t *)row)[x];
                    r = (v >> 10 & 31) * 255 / 31; g = (v >> 5 & 31) * 255 / 31; b = (v & 31) * 255 / 31;
                    if (s->fmt == FMT_A1R5G5B5) a = v >> 15 ? 255 : 0;
                    break;
                case FMT_A4R4G4B4: case FMT_X4R4G4B4:
                    v = ((const uint16_t *)row)[x];
                    r = (v >> 8 & 15) * 17; g = (v >> 4 & 15) * 17; b = (v & 15) * 17;
                    if (s->fmt == FMT_A4R4G4B4) a = (v >> 12 & 15) * 17;
                    break;
                case FMT_A8: a = row[x]; break;  /* as its swizzle: no colour */
                case FMT_L8: r = g = b = row[x]; break;
                case FMT_A8L8: r = g = b = row[2 * x]; a = row[2 * x + 1]; break;
                case FMT_P8: {
                    const uint32_t p = pal[row[x]];
                    r = p & 255; g = p >> 8 & 255; b = p >> 16 & 255; a = p >> 24;
                    break;
                }
                default: free(rgba); return 0;
                }
                o[0] = (uint8_t)r, o[1] = (uint8_t)g, o[2] = (uint8_t)b, o[3] = (uint8_t)a;
            }
        }
    }
    uint64_t sum[4] = {0, 0, 0, 0};
    for (size_t i = 0; i < (size_t)w * h; i++)
        for (int c = 0; c < 4; c++) sum[c] += rgba[i * 4 + c];
    free(rgba);
    for (int c = 0; c < 4; c++) out_rgba[c] = (uint8_t)((sum[c] + (uint64_t)w * h / 2) / ((uint64_t)w * h));
    if (!flat) {
        glGenTextures(1, &flat);
        glBindTexture(GL_TEXTURE_2D, flat);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);  /* complete under any min filter */
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, out_rgba);
    } else {
        glBindTexture(GL_TEXTURE_2D, flat);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, out_rgba);
    }
    return flat;
}

/* Attaches a surface (one layer of it when per eye) to the bound read or draw framebuffer. */
static void attach_layer(GLenum fb, Res *s, int layer) {
    GLuint tex = surface_tex(s);
    if (surface_layers(s) == 2) glFramebufferTextureLayer(fb, GL_COLOR_ATTACHMENT0, tex, s->level, layer);
    else glFramebufferTexture2D(fb, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, s->level);
}

/* A rectangle of a render target from client memory, into every eye's layer. */
static void target_sub_image(Res *dst, int x, int y, int w, int h, GLenum fmt, GLenum type, const void *px) {
    GLuint tex = surface_tex(dst);
    if (surface_layers(dst) == 2) {
        glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
        for (int layer = 0; layer < 2; layer++)
            glTexSubImage3D(GL_TEXTURE_2D_ARRAY, dst->level, x, y, layer, w, h, 1, fmt, type, px);
    } else {
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexSubImage2D(GL_TEXTURE_2D, dst->level, x, y, w, h, fmt, type, px);
    }
}

/* Uploads one level from its guest copy. */
static void upload_level(Res *t, Res *s) {
    const Fmt *f = fmt_info(s->fmt);
    GLenum target = face_target(t, s->face);
    glBindTexture(t->kind == RES_CUBE ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D, t->tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (f->block && !gl_caps.s3tc) {
        uint32_t pw = (s->w + 3) & ~3u, ph = (s->h + 3) & ~3u;
        uint8_t *rgba = malloc(pw * ph * 4);
        dxt_decode(s->fmt, GPTR(s->mem), s->w, s->h, rgba);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, (GLint)pw);
        glTexSubImage2D(target, s->level, 0, 0, s->w, s->h, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        free(rgba);
    } else if (f->block) glCompressedTexSubImage2D(target, s->level, 0, 0, s->w, s->h, f->ifmt, s->size, GPTR(s->mem));
    else if (s->fmt == FMT_P8) {
        /* The indices through the current palette. PALETTEENTRY is R,G,B,flags in memory, and flags
         * is the alpha (D3DPTEXTURECAPS_ALPHAPALETTE, as on the GeForce-class cards the game was made
         * for); it used to be forced opaque, which turns any alpha-masked P8 texture into solid blocks
         * (issue #1, Heather's hair in the real-time cutscenes). A palette with no alpha set at all is
         * opaque, not invisible. */
        uint32_t *rgba = malloc(4u * s->w * s->h);
        const uint8_t *src = GPTR(s->mem);
        static const uint32_t none[256];
        const uint32_t *pal = dev->palette[dev->cur_palette] ? dev->palette[dev->cur_palette] : none;
        uint32_t has_alpha = 0;
        for (int i = 0; i < 256; i++) has_alpha |= pal[i] & 0xFF000000u;
        uint32_t opaque = has_alpha ? 0 : 0xFF000000u;
        for (uint32_t i = 0; i < s->w * s->h; i++) rgba[i] = pal[src[i]] | opaque;
        glTexSubImage2D(target, s->level, 0, 0, s->w, s->h, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        free(rgba);
    } else {
        void *conv = to_gl(f, GPTR(s->mem), s->w * s->h, t->is_rt);
        glTexSubImage2D(target, s->level, 0, 0, s->w, s->h, f->gfmt, f->type, conv ? conv : GPTR(s->mem));
        free(conv);
    }
    s->dirty = 0;
}

void res_upload_dirty(Res *t) {
    gl_ensure(t);
    if (t->fmt == FMT_P8) {
        /* D3D resolves a paletted texture at draw time, with the palette then current: a GL copy
         * expanded under another palette, or one SetPaletteEntries has changed since, is redone. */
        uint32_t ver = dev->pal_version[dev->cur_palette];
        if (t->pal_idx != dev->cur_palette || t->pal_ver != ver) {
            t->pal_idx = dev->cur_palette;
            t->pal_ver = ver;
            t->dirty = 1;
            for (int face = 0; face < (t->kind == RES_CUBE ? 6 : 1); face++)
                for (uint32_t l = 0; l < t->levels; l++) res_of(t->surf[face][l])->dirty = 1;
        }
    }
    if (!t->dirty) return;
    t->dirty = 0;
    if (t->layers == 2) return;  /* a per-eye target: rendered, never uploaded level by level */
    for (int face = 0; face < (t->kind == RES_CUBE ? 6 : 1); face++)
        for (uint32_t l = 0; l < t->levels; l++) {
            Res *s = res_of(t->surf[face][l]);
            if (s->dirty) upload_level(t, s);
        }
}

static void mark_dirty(Res *s) {
    s->dirty = 1;
    if (s->parent) s->parent->dirty = 1;
}

/* A depth surface smaller than the render target clips a GL framebuffer to their intersection, where D3D8
 * drivers drew the whole target: SH3 renders its scene into 512x512 with the 640x480 auto depth, which left
 * the bottom 32 rows black. Such a pass gets a depth texture as big as both instead.
 * ponytail: one spare, re-made when the size changes. */
/* SH3 Quest VR: one spare of each kind, single and per eye (2 layers), for targets with a smaller depth surface or a
 * depth surface of the other kind (a cube face drawn with the per-eye auto depth). */
static GLuint spare_depth(uint32_t w, uint32_t h, int layers) {
    static GLuint tex[3];
    static uint32_t sw[3], sh[3];
    int k = layers == 2 ? 2 : 1;
    if (!tex[k] || sw[k] != w || sh[k] != h) {
        if (!tex[k]) glGenTextures(1, &tex[k]);
        if (k == 2) {
            glBindTexture(GL_TEXTURE_2D_ARRAY, tex[k]);
            glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH24_STENCIL8, w, h, 2, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
        } else {
            glBindTexture(GL_TEXTURE_2D, tex[k]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, w, h, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
        }
        sw[k] = w;
        sh[k] = h;
    }
    return tex[k];
}

static void res_report_framebuffer(GLenum st, Res *rt, int views);
void res_bind_targets(void) {
    Res *rt = res_of(dev->rt), *ds = res_of(dev->ds);
    GLuint color = surface_tex(rt);
    int views = surface_layers(rt);
    GLuint depth = ds ? surface_tex(ds) : 0;
    int depth_level = ds ? ds->level : 0;
    if (ds && (ds->w < rt->w || ds->h < rt->h || surface_layers(ds) != views)) {
        depth = spare_depth(ds->w > rt->w ? ds->w : rt->w, ds->h > rt->h ? ds->h : rt->h, views);
        depth_level = 0;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, dev->fbo);
    if (views == 2) {  /* both eyes at once (GL_OVR_multiview2) */
        glFramebufferTextureMultiviewOVR(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, color, rt->level, 0, 2);
        if (depth) glFramebufferTextureMultiviewOVR(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, depth, depth_level, 0, 2);
        else glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
    } else {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, rt->parent ? face_target(rt->parent, rt->face) : GL_TEXTURE_2D,
                               color, rt->level);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, depth, depth_level);
    }
    dev->views = views;
    /* 0.1.5 (performance): a set of attachments found complete once is not asked about again (the driver's check runs
     * its whole validation; the game binds the same few targets many times a frame) */
    {
        static struct { GLuint color, depth; int level, depth_level, views; } ok[32];
        static int n_ok;
        const int level = rt->level;
        for (int i = 0; i < n_ok; i++)
            if (ok[i].color == color && ok[i].depth == depth && ok[i].level == level &&
                ok[i].depth_level == depth_level && ok[i].views == views)
                return;
        GLenum st0 = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (st0 == GL_FRAMEBUFFER_COMPLETE) {
            const int slot = n_ok < 32 ? n_ok++ : (int)(((unsigned)color * 7u + (unsigned)depth) % 32u);
            ok[slot].color = color, ok[slot].depth = depth, ok[slot].level = level;
            ok[slot].depth_level = depth_level, ok[slot].views = views;
            return;
        }
        res_report_framebuffer(st0, rt, views);
    }
}

static void res_report_framebuffer(GLenum st, Res *rt, int views) {
    if (st == 0) {
        /* 0.1.5: 0 is no answer at all (GL has no context: after the headset's session stopped, at the end of a run):
         * told once, not a line a frame */
        static int told_none;
        if (!told_none++)
            rt_log("framebuffer status not known (GL gave 0: its context gone, as after the headset's session stopped); "
                   "not told again");
    } else if (st != GL_FRAMEBUFFER_COMPLETE) {
        static int told;
        if (told < 20 && ++told)
            rt_log("framebuffer incomplete: %04X (rt %ux%u fmt %u, %d views)%s", st, rt->w, rt->h, rt->fmt, views,
                   told == 20 ? " (not told again)" : "");
    }
}

/* Reads a render target back into the surface's guest copy, for locks of rendered images. */
static void readback(Res *s) {
    if (!s->mem) s->mem = galloc(s->size);
    if (s->is_depth) {  /* ES cannot read depth back; no SH3 path seen locking one */
        static int once;
        if (!once++) rt_log("D3D: a depth surface was locked; ES cannot read depth, it reads as cleared");
        memset(GPTR(s->mem), 0xFF, s->size);
        return;
    }
    GLuint fb;
    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fb);
    attach_layer(GL_READ_FRAMEBUFFER, s, 0);  /* per eye: the left eye's */
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    uint8_t *rgba = malloc((size_t)s->w * s->h * 4);
    glReadPixels(0, 0, s->w, s->h, GL_RGBA, GL_UNSIGNED_BYTE, rgba);  /* the one ES-guaranteed read format */
    for (uint32_t y = 0; y < s->h; y++)
        from_rgba8(s->fmt, rgba + (size_t)y * s->w * 4, GPTR(s->mem + y * s->pitch), s->w);
    free(rgba);
    glDeleteFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, dev->fbo);
}

/* The last presented frame. Direct3D's front buffer is what was last shown, not the back buffer the game has
 * already cleared for the next frame; the game uses it for the transition cross-fades and the save thumbnail. */
static GLuint front_tex, front_fbo;
static uint32_t front_w, front_h;

void res_snapshot_front(void) {  /* called by Present with the back buffer bound for reading */
    Res *bb = res_of(dev->backbuffer);
    if (!bb) return;
    if (!front_tex || front_w != bb->w || front_h != bb->h) {
        if (front_tex) { glDeleteTextures(1, &front_tex); glDeleteFramebuffers(1, &front_fbo); }
        front_w = bb->w;
        front_h = bb->h;
        glGenTextures(1, &front_tex);
        glBindTexture(GL_TEXTURE_2D, front_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, front_w, front_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glGenFramebuffers(1, &front_fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, front_fbo);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, front_tex, 0);
    }
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, front_fbo);
    glBlitFramebuffer(0, 0, front_w, front_h, 0, 0, front_w, front_h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

/* GetFrontBuffer: the last presented image into an A8R8G8B8 image surface. */
uint32_t res_read_front(uint32_t dst_o) {
    Res *src = res_of(dev->backbuffer), *dst = res_of(dst_o);
    if (!dst || !dst->mem || dst->fmt != FMT_A8R8G8B8) return D3DERR_INVALIDCALL;
    GLuint fb;
    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fb);
    /* the presented frame when there is one, else the back buffer */
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           front_tex && front_w == src->w && front_h == src->h ? front_tex : surface_tex(src), 0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    uint32_t w = src->w < dst->w ? src->w : dst->w, h = src->h < dst->h ? src->h : dst->h;
    uint8_t *rgba = malloc((size_t)w * h * 4);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba);  /* ES: RGBA, swapped to BGRA below */
    for (uint32_t y = 0; y < h; y++) from_rgba8(FMT_A8R8G8B8, rgba + (size_t)y * w * 4, GPTR(dst->mem + y * dst->pitch), w);
    free(rgba);
    glDeleteFramebuffers(1, &fb);
    for (uint32_t i = 0; i < dst->size; i += 4) *(uint8_t *)GPTR(dst->mem + i + 3) = 0xFF;
    mark_dirty(dst);
    res_bind_targets();
    return 0;
}

uint32_t res_copy_rects(uint32_t src_o, uint32_t rects, uint32_t n, uint32_t dst_o, uint32_t points) {
    Res *src = res_of(src_o), *dst = res_of(dst_o);
    if (src->fmt != dst->fmt && !(src->is_rt && dst->is_rt)) return D3DERR_INVALIDCALL;
    int32_t whole[4] = {0, 0, (int32_t)src->w, (int32_t)src->h};
    if (!n) n = 1;
    for (uint32_t i = 0; i < n; i++) {
        const int32_t *r = rects ? (const int32_t *)GPTR(rects + 16 * i) : whole;  /* RECT l t r b */
        int32_t dx = points ? ((int32_t *)GPTR(points + 8 * i))[0] : r[0];
        int32_t dy = points ? ((int32_t *)GPTR(points + 8 * i))[1] : r[1];
        int w = r[2] - r[0], h = r[3] - r[1];
        if (src->is_rt && dst->is_rt) {  /* GPU to GPU */
            GLuint fb[2];
            glGenFramebuffers(2, fb);
            glDisable(GL_SCISSOR_TEST);
            /* SH3 Quest VR: per eye, layer to layer (a single source into both eyes' layers) */
            int src_layers = (gl_ensure(src->parent ? src->parent : src), surface_layers(src));
            int dst_layers = (gl_ensure(dst->parent ? dst->parent : dst), surface_layers(dst));
            for (int layer = 0; layer < dst_layers; layer++) {
                glBindFramebuffer(GL_READ_FRAMEBUFFER, fb[0]);
                attach_layer(GL_READ_FRAMEBUFFER, src, src_layers == 2 ? layer : 0);
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb[1]);
                attach_layer(GL_DRAW_FRAMEBUFFER, dst, layer);
                glBlitFramebuffer(r[0], r[1], r[2], r[3], dx, dy, dx + w, dy + h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
            }
            glDeleteFramebuffers(2, fb);
            res_bind_targets();
            continue;
        }
        if (src->is_rt) readback(src);
        const Fmt *f = fmt_info(src->fmt);
        if (dst->mem) {  /* row copies between guest copies */
            int bpp = f->block ? f->block : f->bpp, div = f->block ? 4 : 1;
            for (int y = 0; y < h / div; y++)
                memcpy(GPTR(dst->mem + (dy / div + y) * dst->pitch + dx / div * bpp),
                       GPTR(src->mem + (r[1] / div + y) * src->pitch + r[0] / div * bpp), w / div * bpp);
            mark_dirty(dst);
        } else {         /* guest copy into a render target (SH3 Quest VR: every eye's layer of it) */
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            if (f->block) {  /* a compressed image into a target: decode it whole, upload the rect */
                uint32_t pw = (src->w + 3) & ~3u, ph = (src->h + 3) & ~3u;
                uint8_t *rgba = malloc((size_t)pw * ph * 4);
                dxt_decode(src->fmt, GPTR(src->mem), src->w, src->h, rgba);
                glPixelStorei(GL_UNPACK_ROW_LENGTH, (GLint)pw);
                target_sub_image(dst, dx, dy, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba + ((size_t)r[1] * pw + r[0]) * 4);
                free(rgba);
            } else {         /* rows gathered, converted to what the target texture holds */
                uint8_t *rows = malloc((size_t)w * h * f->bpp);
                for (int y = 0; y < h; y++)
                    memcpy(rows + (size_t)y * w * f->bpp, GPTR(src->mem + (r[1] + y) * src->pitch + r[0] * f->bpp), (size_t)w * f->bpp);
                const Fmt *df = fmt_info(dst->fmt);
                void *conv = to_gl(df, rows, (uint32_t)(w * h), 1);
                glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
                target_sub_image(dst, dx, dy, w, h, df->gfmt, df->type, conv ? conv : rows);
                free(conv);
                free(rows);
            }
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        }
    }
    return 0;
}

/* ---- COM methods ---- */

#define RES ((Res *)res_of(THIS))
METHOD(r_QueryInterface, 3) { return E_NOINTERFACE; }
METHOD(r_AddRef, 1) { res_addref(THIS); return RES->parent ? RES->parent->refs : RES->refs; }
METHOD(r_Release, 1) {
    Res *r = RES->parent ? RES->parent : RES;
    uint32_t left = r->refs - 1;
    res_release(THIS);
    return left;
}
METHOD(r_GetDevice, 2) { MEM32(ARG(1)) = dev->obj; return 0; }
METHOD(r_SetPrivateData, 5) { return 0; }
METHOD(r_GetPrivateData, 4) { return D3DERR_NOTAVAILABLE; }
METHOD(r_FreePrivateData, 2) { return 0; }
METHOD(r_SetPriority, 2) { return 0; }
METHOD(r_GetPriority, 1) { return 0; }
METHOD(r_PreLoad, 1) { return 0; }
METHOD(r_GetType, 1) {
    static const uint32_t type[] = {0, 3, 5, 1, 6, 7};
    return type[RES->kind];
}
METHOD(t_SetLOD, 2) { return 0; }
METHOD(t_GetLOD, 1) { return 0; }
METHOD(t_GetLevelCount, 1) { return RES->levels; }

static void surface_desc(Res *s, uint32_t d) {
    uint32_t usage = s->is_rt ? 1 : s->is_depth ? 2 : 0;
    uint32_t v[8] = {s->fmt, 1, usage, s->parent ? s->parent->pool : 0, s->size, 0, s->w, s->h};
    memcpy(GPTR(d), v, 32);
}
static Res *level_of(Res *t, int face, uint32_t l) { return l < t->levels ? res_of(t->surf[face][l]) : NULL; }

static uint32_t lock_surface(Res *s, uint32_t locked, uint32_t rect) {
    if (!s) return D3DERR_INVALIDCALL;
    if (!s->mem) readback(s);
    uint32_t off = 0;
    if (rect) {
        const int32_t *r = GPTR(rect);
        const Fmt *f = fmt_info(s->fmt);
        off = f->block ? r[1] / 4 * s->pitch + r[0] / 4 * f->block : r[1] * s->pitch + r[0] * f->bpp;
    }
    MEM32(locked) = s->pitch;
    MEM32(locked + 4) = s->mem + off;
    return 0;
}
static uint32_t unlock_surface(Res *s) {
    if (!s) return D3DERR_INVALIDCALL;
    if (s->parent || !s->is_rt) mark_dirty(s);
    else {  /* a locked render target: push the guest copy back */
        const Fmt *f = fmt_info(s->fmt);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        void *conv = to_gl(f, GPTR(s->mem), s->w * s->h, 1);
        target_sub_image(s, 0, 0, s->w, s->h, f->gfmt, f->type, conv ? conv : GPTR(s->mem));  /* every eye's layer */
        free(conv);
    }
    return 0;
}

METHOD(t_GetLevelDesc, 3) { Res *s = level_of(RES, 0, ARG(1)); if (!s) return D3DERR_INVALIDCALL; surface_desc(s, ARG(2)); return 0; }
METHOD(t_GetSurfaceLevel, 3) {
    Res *s = level_of(RES, 0, ARG(1));
    if (!s) return D3DERR_INVALIDCALL;
    res_addref(s->obj);
    MEM32(ARG(2)) = s->obj;
    return 0;
}
METHOD(t_LockRect, 5) { return lock_surface(level_of(RES, 0, ARG(1)), ARG(2), ARG(3)); }
METHOD(t_UnlockRect, 2) { return unlock_surface(level_of(RES, 0, ARG(1))); }
METHOD(t_AddDirtyRect, 2) { RES->dirty = 1; return 0; }

METHOD(c_GetCubeMapSurface, 4) {
    Res *s = level_of(RES, ARG(1) % 6, ARG(2));
    if (!s) return D3DERR_INVALIDCALL;
    res_addref(s->obj);
    MEM32(ARG(3)) = s->obj;
    return 0;
}
METHOD(c_LockRect, 6) { return lock_surface(level_of(RES, ARG(1) % 6, ARG(2)), ARG(3), ARG(4)); }
METHOD(c_UnlockRect, 3) { return unlock_surface(level_of(RES, ARG(1) % 6, ARG(2))); }
METHOD(c_AddDirtyRect, 3) { RES->dirty = 1; return 0; }

METHOD(s_GetContainer, 3) {
    Res *p = RES->parent;
    if (!p) return E_NOINTERFACE;
    res_addref(p->obj);
    MEM32(ARG(2)) = p->obj;
    return 0;
}
METHOD(s_GetDesc, 2) { surface_desc(RES, ARG(1)); return 0; }
METHOD(s_LockRect, 4) { return lock_surface(RES, ARG(1), ARG(2)); }
METHOD(s_UnlockRect, 1) { return unlock_surface(RES); }

METHOD(b_Lock, 5) {
    Res *b = RES;
    MEM32(ARG(3)) = b->mem + (ARG(1) < b->size ? ARG(1) : 0);
    return 0;
}
METHOD(b_Unlock, 1) { RES->dirty = 1; return 0; }
METHOD(vb_GetDesc, 2) {
    Res *b = RES;
    uint32_t v[6] = {FMT_VERTEXDATA, 6, b->usage, b->pool, b->size, b->fmt};
    memcpy(GPTR(ARG(1)), v, 24);
    return 0;
}
METHOD(ib_GetDesc, 2) {
    Res *b = RES;
    uint32_t v[5] = {b->fmt, 7, b->usage, b->pool, b->size};
    memcpy(GPTR(ARG(1)), v, 20);
    return 0;
}

#define RESOURCE(i) \
    {i "::QueryInterface", r_QueryInterface}, {i "::AddRef", r_AddRef}, {i "::Release", r_Release}, \
    {i "::GetDevice", r_GetDevice}, {i "::SetPrivateData", r_SetPrivateData}, {i "::GetPrivateData", r_GetPrivateData}, \
    {i "::FreePrivateData", r_FreePrivateData}, {i "::SetPriority", r_SetPriority}, {i "::GetPriority", r_GetPriority}, \
    {i "::PreLoad", r_PreLoad}, {i "::GetType", r_GetType}
static const ComEntry tex_vt[] = {
    RESOURCE("IDirect3DTexture8"), {"IDirect3DTexture8::SetLOD", t_SetLOD}, {"IDirect3DTexture8::GetLOD", t_GetLOD},
    {"IDirect3DTexture8::GetLevelCount", t_GetLevelCount}, {"IDirect3DTexture8::GetLevelDesc", t_GetLevelDesc},
    {"IDirect3DTexture8::GetSurfaceLevel", t_GetSurfaceLevel}, {"IDirect3DTexture8::LockRect", t_LockRect},
    {"IDirect3DTexture8::UnlockRect", t_UnlockRect}, {"IDirect3DTexture8::AddDirtyRect", t_AddDirtyRect},
};
static const ComEntry cube_vt[] = {
    RESOURCE("IDirect3DCubeTexture8"), {"IDirect3DCubeTexture8::SetLOD", t_SetLOD},
    {"IDirect3DCubeTexture8::GetLOD", t_GetLOD}, {"IDirect3DCubeTexture8::GetLevelCount", t_GetLevelCount},
    {"IDirect3DCubeTexture8::GetLevelDesc", t_GetLevelDesc},
    {"IDirect3DCubeTexture8::GetCubeMapSurface", c_GetCubeMapSurface},
    {"IDirect3DCubeTexture8::LockRect", c_LockRect}, {"IDirect3DCubeTexture8::UnlockRect", c_UnlockRect},
    {"IDirect3DCubeTexture8::AddDirtyRect", c_AddDirtyRect},
};
static const ComEntry surf_vt[] = {
    {"IDirect3DSurface8::QueryInterface", r_QueryInterface}, {"IDirect3DSurface8::AddRef", r_AddRef},
    {"IDirect3DSurface8::Release", r_Release}, {"IDirect3DSurface8::GetDevice", r_GetDevice},
    {"IDirect3DSurface8::SetPrivateData", r_SetPrivateData}, {"IDirect3DSurface8::GetPrivateData", r_GetPrivateData},
    {"IDirect3DSurface8::FreePrivateData", r_FreePrivateData}, {"IDirect3DSurface8::GetContainer", s_GetContainer},
    {"IDirect3DSurface8::GetDesc", s_GetDesc}, {"IDirect3DSurface8::LockRect", s_LockRect},
    {"IDirect3DSurface8::UnlockRect", s_UnlockRect},
};
static const ComEntry vb_vt[] = {
    RESOURCE("IDirect3DVertexBuffer8"), {"IDirect3DVertexBuffer8::Lock", b_Lock},
    {"IDirect3DVertexBuffer8::Unlock", b_Unlock}, {"IDirect3DVertexBuffer8::GetDesc", vb_GetDesc},
};
static const ComEntry ib_vt[] = {
    RESOURCE("IDirect3DIndexBuffer8"), {"IDirect3DIndexBuffer8::Lock", b_Lock},
    {"IDirect3DIndexBuffer8::Unlock", b_Unlock}, {"IDirect3DIndexBuffer8::GetDesc", ib_GetDesc},
};

void res_init_vtables(void) {
    vt_tex = com_vtable(tex_vt, sizeof tex_vt / sizeof *tex_vt);
    vt_cube = com_vtable(cube_vt, sizeof cube_vt / sizeof *cube_vt);
    vt_surf = com_vtable(surf_vt, sizeof surf_vt / sizeof *surf_vt);
    vt_vb = com_vtable(vb_vt, sizeof vb_vt / sizeof *vb_vt);
    vt_ib = com_vtable(ib_vt, sizeof ib_vt / sizeof *ib_vt);
}
