/* SH3 Quest VR (LIST 5): the game's movies (data/movie/NAME.000: the opening, the endings, the transformations).
 *
 * sh3.exe plays them through DirectShow with a source filter of its own that unscrambles the file, and a renderer
 * filter that copies frames into a texture; sh3-nx has no DirectShow (CoCreateInstance answers "no such class"), so
 * the game skipped every movie. Here the port plays them itself (src/game/sh3.c replaces the player's start
 * 0x00404B40, its per-frame step 0x00404440 and its stop 0x00403DC0):
 *
 * - The scrambling (the game's 0x00405680 / 0x00405640 / 0x00405530; docs/GAME_FACTS.md, Movies): the first 0x8000
 *   bytes are a key block. Its bytes 0x55E2, 0x2A5C, 0x3D40, 0x2DDE make a seed (high byte first); x = x * 0x5D588B65 + 1
 *   per 32-bit word XORed over the block turns it into zeros (those 4 bytes aside: the game's check that the key is
 *   right); the next 0x400 values of x make a 0x1000-byte table, XORed over the rest of the file (offset & 0xFFF).
 * - What comes out is an MPEG-1 program stream: MPEG-1 video (512x384, 29.97 fps) and MPEG-1 Layer II audio (48 kHz
 *   stereo), decoded with pl_mpeg (third_party/pl_mpeg, MIT, Dominic Szablewski).
 * - The sound goes to dsound.c's movie soundtrack (44.1 kHz stereo, resampled here) at the game's music volume; the
 *   picture to d3d8.c, which draws it over the frame at Present (sh3_movie_picture), and VR shows that frame on the
 *   cinema screen while a movie plays (sh3vr_movie). */
#include "../runtime/rt.h"
#include "../vr/sh3vr.h"
#include <SDL2/SDL.h>
#include <math.h>

#define PL_MPEG_IMPLEMENTATION
#define PLM_NO_STDIO  /* the file is read (and unscrambled) here */
#include "../../third_party/pl_mpeg/pl_mpeg.h"

#define KEY_BLOCK 0x8000u
#define MUL 0x5D588B65u
#define OUT_RATE 44100

void ds_track_write(const float *lr, int frames);
void ds_track_clear(void);
void ds_track_pause(int on);

typedef struct {
    FILE *f;
    uint64_t pos;               /* bytes of the movie (after the key block) handed to the decoder so far */
    uint8_t table[0x1000];
    plm_t *plm;
    uint8_t *rgba;              /* the latest picture, top row first */
    int w, h, serial;
    double aspect;              /* the picture's shape, width / height */
    double rs_pos;              /* the resampler: where the next output sample falls, in input samples of this block */
    float prev[2];              /* the last input sample of the block before */
    int rate, frames, audio_frames;
    float gain;
    Uint64 last;
    char name[64];
} Movie;
static Movie *mv;

/* The key: the seed from the key block, the block's check, then the table. 0 if the block is not one. */
static int make_table(const uint8_t *blk, uint8_t *table) {
    uint32_t x = (uint32_t)blk[0x55E2] << 24 | (uint32_t)blk[0x2A5C] << 16 | (uint32_t)blk[0x3D40] << 8 | blk[0x2DDE];
    int bad = 0;
    for (uint32_t i = 0; i < KEY_BLOCK / 4; i++) {
        x = x * MUL + 1;
        uint32_t w;
        memcpy(&w, blk + 4 * i, 4);
        w ^= x;
        /* the 4 seed bytes are left out of the check */
        for (int b = 0; b < 4; b++) {
            const uint32_t at = 4 * i + b;
            if (at == 0x55E2 || at == 0x2A5C || at == 0x3D40 || at == 0x2DDE) w &= ~(0xFFu << (8 * b));
        }
        if (w) bad++;
    }
    for (int i = 0; i < 0x400; i++) {
        x = x * MUL + 1;
        memcpy(table + 4 * i, &x, 4);
    }
    return bad == 0;
}

/* pl_mpeg asks for more of the stream: the next 64 KB, unscrambled. */
static void load_more(plm_buffer_t *b, void *user) {
    Movie *m = (Movie *)user;
    static uint8_t chunk[0x10000];
    size_t n = fread(chunk, 1, sizeof chunk, m->f);
    for (size_t i = 0; i < n; i++) chunk[i] ^= m->table[(m->pos + i) & 0xFFF];
    m->pos += n;
    if (n) plm_buffer_write(b, chunk, n);
    else plm_buffer_signal_end(b);
}

static void on_video(plm_t *p, plm_frame_t *fr, void *user) {
    (void)p;
    Movie *m = (Movie *)user;
    if ((int)fr->width != m->w || (int)fr->height != m->h || !m->rgba) {
        free(m->rgba);
        m->w = (int)fr->width;
        m->h = (int)fr->height;
        m->rgba = (uint8_t *)malloc((size_t)m->w * m->h * 4);
        if (!m->rgba) return;
    }
    plm_frame_to_rgba(fr, m->rgba, m->w * 4);
    m->serial++;
    m->frames++;
}

/* Layer II at the movie's rate (48 kHz) to the soundtrack's 44.1 kHz stereo, linear between samples. */
static void on_audio(plm_t *p, plm_samples_t *s, void *user) {
    (void)p;
    Movie *m = (Movie *)user;
    static float out[2 * (PLM_AUDIO_SAMPLES_PER_FRAME * 2 + 4)];
    const double step = (double)m->rate / OUT_RATE;
    const int count = (int)s->count;
    int n = 0;
    while (m->rs_pos < count - 1 && n < PLM_AUDIO_SAMPLES_PER_FRAME * 2) {
        const int i0 = (int)floor(m->rs_pos);
        const float t = (float)(m->rs_pos - i0);
        for (int ch = 0; ch < 2; ch++) {
            const float a = i0 < 0 ? m->prev[ch] : s->interleaved[2 * i0 + ch];
            const float b = s->interleaved[2 * (i0 + 1) + ch];
            out[2 * n + ch] = (a + (b - a) * t) * m->gain;
        }
        n++;
        m->rs_pos += step;
    }
    m->rs_pos -= count;
    m->prev[0] = s->interleaved[2 * (count - 1)];
    m->prev[1] = s->interleaved[2 * (count - 1) + 1];
    m->audio_frames += n;
    if (n) ds_track_write(out, n);
}

void sh3_movie_stop(const char *why) {
    if (!mv) return;
    rt_log("movie: %s stopped (%s) after %d pictures, %.1f s of sound", mv->name, why, mv->frames,
           mv->audio_frames / (double)OUT_RATE);
    if (mv->plm) plm_destroy(mv->plm);
    if (mv->f) fclose(mv->f);
    free(mv->rgba);
    free(mv);
    mv = NULL;
    ds_track_clear();
    ds_track_pause(0);
    sh3vr_movie(0);
}

/* path: the file on this machine. volume: the game's music level for it (0..1). 1 when it plays. */
int sh3_movie_start(const char *path, float volume) {
    sh3_movie_stop("another started");
    /* testing: the PC rig's scripted runs (QUEST_SIM_SCRIPT) are timed on a game that skipped them (the opening's is
     * 2.6 minutes): skipped there unless SH3_MOVIES=1 (tools/sim/movie.txt) */
    const char *want = getenv("SH3_MOVIES");
    if (getenv("QUEST_SIM_SCRIPT") && !(want && atoi(want) == 1)) {
        rt_log("movie: %s skipped (a scripted run; SH3_MOVIES=1 plays it)", path);
        return 0;
    }
    const char *base = strrchr(path, '/');
    const char *base2 = strrchr(path, '\\');
    if (base2 > base) base = base2;
    base = base ? base + 1 : path;
    FILE *f = fopen(path, "rb");
    if (!f) {
        rt_log("movie: %s is not there: skipped", path);
        return 0;
    }
    static uint8_t blk[KEY_BLOCK];
    Movie *m = (Movie *)calloc(1, sizeof *m);
    if (!m || fread(blk, 1, KEY_BLOCK, f) != KEY_BLOCK || !make_table(blk, m->table)) {
        rt_log("movie: %s: its key block does not check out (not one of the game's movies?): skipped", path);
        fclose(f);
        free(m);
        return 0;
    }
    m->f = f;
    snprintf(m->name, sizeof m->name, "%s", base);
    plm_buffer_t *buf = plm_buffer_create_with_capacity(PLM_BUFFER_DEFAULT_SIZE);
    plm_buffer_set_load_callback(buf, load_more, m);
    m->plm = plm_create_with_buffer(buf, 1);
    /* the stream's own headers (its system header names the streams; plm_probe would seek back, which a stream
     * unscrambled as it is read cannot) */
    if (!m->plm || !plm_has_headers(m->plm) || !plm_get_num_video_streams(m->plm)) {
        rt_log("movie: %s: no MPEG-1 picture found after unscrambling: skipped", path);
        mv = m;
        sh3_movie_stop("unreadable");
        return 0;
    }
    m->rate = plm_get_samplerate(m->plm);
    if (m->rate <= 0) m->rate = 48000;
    m->aspect = (double)plm_get_width(m->plm) / (plm_get_height(m->plm) > 0 ? plm_get_height(m->plm) : 1) *
                plm_get_pixel_aspect_ratio(m->plm);
    if (!(m->aspect > 0.5 && m->aspect < 3.0)) m->aspect = 4.0 / 3.0;
    m->rs_pos = 0;
    m->gain = volume >= 0 && volume <= 1 ? volume : 1;
    plm_set_loop(m->plm, 0);
    plm_set_video_decode_callback(m->plm, on_video, m);
    plm_set_audio_decode_callback(m->plm, on_audio, m);
    plm_set_audio_enabled(m->plm, plm_get_num_audio_streams(m->plm) > 0);
    /* the soundtrack's device takes about 1024 samples before they are heard: the sound decoded that much ahead */
    plm_set_audio_lead_time(m->plm, 1024.0 / OUT_RATE);
    m->last = SDL_GetPerformanceCounter();
    mv = m;
    ds_track_clear();
    ds_track_pause(0);
    sh3vr_movie(1);
    rt_log("movie: %s: %dx%d at %.2f fps, shape %.3f, %d sound stream(s) at %d Hz, volume %.2f", m->name,
           plm_get_width(m->plm), plm_get_height(m->plm), plm_get_framerate(m->plm), m->aspect,
           plm_get_num_audio_streams(m->plm), m->rate, m->gain);
    return 1;
}

/* The game's per-frame step: the movie moves on by the time since the last one (a hitch longer than a tenth of a
 * second is not caught up, so the sound never jumps). 1 while it plays, 0 at its end. */
int sh3_movie_frame(void) {
    if (!mv) return 0;
    const Uint64 now = SDL_GetPerformanceCounter();
    double dt = (double)(now - mv->last) / (double)SDL_GetPerformanceFrequency();
    mv->last = now;
    if (dt > 0.1) dt = 0.1;
    if (dt < 0) dt = 0;
    plm_decode(mv->plm, dt);
    if (plm_has_ended(mv->plm)) {
        sh3_movie_stop("its end");
        return 0;
    }
    return 1;
}

int sh3_movie_playing(void) { return mv != NULL; }

/* d3d8.c, at Present: the latest picture (top row first, RGBA), its size, its shape and a number that changes with
 * each new picture. NULL while no movie plays or before its first picture. */
const uint8_t *sh3_movie_picture(int *w, int *h, double *aspect, int *serial) {
    if (!mv || !mv->rgba) return NULL;
    *w = mv->w;
    *h = mv->h;
    *aspect = mv->aspect;
    *serial = mv->serial;
    return mv->rgba;
}
