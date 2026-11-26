/* tools/movie_check.c movie.000 seconds [out.ppm]: the port's movie path off the game (src/game/movie.c's key, table and
 * stream reading), decoding `seconds` of the movie, writing the last picture and counting pictures and sound.
 * Build: tools/movie_check.bat (clang, from the repo root); session 11 checked op.000 and huxx.000 with it. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define PL_MPEG_IMPLEMENTATION
#define PLM_NO_STDIO
#include "../third_party/pl_mpeg/pl_mpeg.h"

static FILE *f;
static uint64_t pos;
static uint8_t table[0x1000];
static int pictures, sound_frames, w, h;
static uint8_t *rgba;
static double first_sound = -1, last_sound;

static int make_table(const uint8_t *blk) {
    uint32_t x = (uint32_t)blk[0x55E2] << 24 | (uint32_t)blk[0x2A5C] << 16 | (uint32_t)blk[0x3D40] << 8 | blk[0x2DDE];
    int bad = 0;
    for (uint32_t i = 0; i < 0x2000; i++) {
        x = x * 0x5D588B65u + 1;
        uint32_t v;
        memcpy(&v, blk + 4 * i, 4);
        v ^= x;
        for (int b = 0; b < 4; b++) {
            uint32_t at = 4 * i + b;
            if (at == 0x55E2 || at == 0x2A5C || at == 0x3D40 || at == 0x2DDE) v &= ~(0xFFu << (8 * b));
        }
        if (v) bad++;
    }
    for (int i = 0; i < 0x400; i++) { x = x * 0x5D588B65u + 1; memcpy(table + 4 * i, &x, 4); }
    return bad == 0;
}
static void load_more(plm_buffer_t *b, void *u) {
    static uint8_t chunk[0x10000];
    size_t n = fread(chunk, 1, sizeof chunk, f);
    for (size_t i = 0; i < n; i++) chunk[i] ^= table[(pos + i) & 0xFFF];
    pos += n;
    if (n) plm_buffer_write(b, chunk, n); else plm_buffer_signal_end(b);
}
static void on_video(plm_t *p, plm_frame_t *fr, void *u) {
    w = fr->width; h = fr->height;
    if (!rgba) rgba = malloc(w * h * 4);
    plm_frame_to_rgba(fr, rgba, w * 4);
    pictures++;
}
static void on_audio(plm_t *p, plm_samples_t *s, void *u) {
    if (first_sound < 0) first_sound = s->time;
    last_sound = s->time;
    sound_frames += s->count;
}
int main(int argc, char **argv) {
    f = fopen(argv[1], "rb");
    static uint8_t blk[0x8000];
    fread(blk, 1, 0x8000, f);
    printf("key block checks: %d\n", make_table(blk));
    plm_buffer_t *buf = plm_buffer_create_with_capacity(PLM_BUFFER_DEFAULT_SIZE);
    plm_buffer_set_load_callback(buf, load_more, NULL);
    plm_t *plm = plm_create_with_buffer(buf, 1);
    printf("headers %d, video streams %d, audio streams %d, %dx%d %.3f fps, pixel aspect %.3f, %d Hz\n",
           plm_has_headers(plm), plm_get_num_video_streams(plm), plm_get_num_audio_streams(plm), plm_get_width(plm),
           plm_get_height(plm), plm_get_framerate(plm), plm_get_pixel_aspect_ratio(plm), plm_get_samplerate(plm));
    plm_set_video_decode_callback(plm, on_video, NULL);
    plm_set_audio_decode_callback(plm, on_audio, NULL);
    plm_set_loop(plm, 0);
    double secs = atof(argv[2]);
    for (double t = 0; t < secs && !plm_has_ended(plm); t += 1.0 / 72) plm_decode(plm, 1.0 / 72);
    printf("after %.1f s: %d pictures, %.2f s of sound (first at %.2f, last at %.2f), ended %d, %llu bytes read\n", secs,
           pictures, sound_frames / 48000.0, first_sound, last_sound, plm_has_ended(plm), (unsigned long long)pos);
    if (rgba && argc > 3) {
        FILE *o = fopen(argv[3], "wb");
        fprintf(o, "P6 %d %d 255\n", w, h);
        for (int i = 0; i < w * h; i++) fwrite(rgba + 4 * i, 1, 3, o);
        fclose(o);
    }
    return 0;
}
