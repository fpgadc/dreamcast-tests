/* Shared harness for the hardware-vs-emulator tests. A test's main.c defines NUM_PAGES and the three
   test_* functions below and then includes this file's main() by defining nothing else.

   The screen is a grid of 32x32 cells (one tile each), 20 across, numbered row by row. A test draws a known
   thing in each cell and says what it is with label().

   Builds (see ../run.sh):
       default            hardware under dc-load: draws every page, writes the displayed framebuffer of each
                          (640x480 words of 0RGB, read from video memory 32 bits at a time) to
                          DUMP_DIR "/page<n>.hw.raw" on the host, and exits
       -DPAGE=n           draws page n forever (for an emulator's screenshot)
       -DLABELS           prints the labels and idles (run in the emulator; its console is captured) */
#ifndef PVRTEST_H
#define PVRTEST_H

#include <kos.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define COLS 20
/* The framebuffer format. A test may define TEST_PIXEL_MODE as PM_RGB565 or PM_RGB555 before including this;
   the dump is then 640x480 16 bit words, named page<n>.hw16.raw. */
#ifndef TEST_PIXEL_MODE
#define TEST_PIXEL_MODE PM_RGB0888
#define FB_BYTES 4
#else
#define FB_BYTES 2
#endif
/* refsw2 used to point-sample half a texel before the pixel center, where hardware samples at the center
   (see ../README.md, "sample"). Against such an emulator, build with -DPOINT_FIX=0.5f in the emulator builds
   so that tests of something else sample the same texel on both sides. With refsw2 fixed this is 0. */
#ifndef POINT_FIX
#define POINT_FIX 0.0f
#endif
#define CELL 32.0f
#ifndef DUMP_DIR
#define DUMP_DIR "/pc/tmp/pvr_tests/out"
#endif

static void test_setup(void);           /* once, after pvr_init */
static void test_describe(void);        /* calls label() for everything drawn */
static void test_page(int page);        /* between pvr_scene_begin and pvr_scene_finish: submits the lists */

static inline float cell_x(int c) { return (c % COLS) * CELL; }
static inline float cell_y(int c) { return (c / COLS) * CELL; }

/* A labeled region: `cw` x `ch` cells starting at `cell`. */
static void label_span(int page, int cell, int cw, int ch, const char *fmt, ...) {
    va_list ap;
    printf("LABEL %d %d %d %d ", page, cell, cw, ch);
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
}
#define label(page, cell, ...) label_span(page, cell, 1, 1, __VA_ARGS__)

/* A context with nothing that could hide a polygon: no culling, depth always passes, no depth write. */
static void cxt_plain(pvr_poly_cxt_t *cxt, pvr_list_t list) {
    pvr_poly_cxt_col(cxt, list);
    cxt->gen.culling = PVR_CULLING_NONE;
    cxt->gen.alpha = PVR_ALPHA_ENABLE;
    cxt->depth.comparison = PVR_DEPTHCMP_ALWAYS;
    cxt->depth.write = PVR_DEPTHWRITE_DISABLE;
    cxt->blend.src = PVR_BLEND_ONE;
    cxt->blend.dst = PVR_BLEND_ZERO;
}

static void cxt_textured(pvr_poly_cxt_t *cxt, pvr_list_t list, int format, int w, int h, pvr_ptr_t txr, int filter) {
    pvr_poly_cxt_txr(cxt, list, format, w, h, txr, filter);
    cxt->gen.culling = PVR_CULLING_NONE;
    cxt->gen.alpha = PVR_ALPHA_ENABLE;
    cxt->txr.alpha = PVR_TXRALPHA_ENABLE;
    cxt->txr.env = PVR_TXRENV_REPLACE;
    cxt->depth.comparison = PVR_DEPTHCMP_ALWAYS;
    cxt->depth.write = PVR_DEPTHWRITE_DISABLE;
    cxt->blend.src = PVR_BLEND_ONE;
    cxt->blend.dst = PVR_BLEND_ZERO;
}

/* Corner order everywhere: top left, top right, bottom left, bottom right (a strip). */
static void quad_full(const pvr_poly_hdr_t *hdr, float x, float y, float w, float h, float z,
                      float u0, float v0, float u1, float v1, const uint32_t argb[4], const uint32_t oargb[4]) {
    pvr_vertex_t v;
    int k;

    pvr_prim(hdr, sizeof(*hdr));
    v.z = z;
    for(k = 0; k < 4; k++) {
        v.flags = (k == 3) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        v.x = x + (k & 1) * w;
        v.y = y + (k >> 1) * h;
        v.u = (k & 1) ? u1 : u0;
        v.v = (k >> 1) ? v1 : v0;
        v.argb = argb[k];
        v.oargb = oargb ? oargb[k] : 0;
        pvr_prim(&v, sizeof(v));
    }
}

static void quad(const pvr_poly_hdr_t *hdr, float x, float y, float w, float h, float z, uint32_t argb) {
    uint32_t c[4] = { argb, argb, argb, argb };
    quad_full(hdr, x, y, w, h, z, 0.0f, 0.0f, 1.0f, 1.0f, c, NULL);
}

static void quad_uv(const pvr_poly_hdr_t *hdr, float x, float y, float w, float h, float u0, float v0, float u1, float v1) {
    uint32_t c[4] = { 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff };
    quad_full(hdr, x, y, w, h, 1.0f, u0, v0, u1, v1, c, NULL);
}

static void tri(const pvr_poly_hdr_t *hdr, const float xy[3][2], float z, uint32_t argb) {
    pvr_vertex_t v;
    int k;

    pvr_prim(hdr, sizeof(*hdr));
    v.z = z;
    v.u = v.v = 0.0f;
    v.argb = argb;
    v.oargb = 0;
    for(k = 0; k < 3; k++) {
        v.flags = (k == 2) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        v.x = xy[k][0];
        v.y = xy[k][1];
        pvr_prim(&v, sizeof(v));
    }
}

/* Deterministic pseudo-random numbers, the same on every run. */
static uint32_t rng_state = 1;
static uint32_t rng(void) {
    rng_state = rng_state * 1664525u + 1013904223u;
    return rng_state >> 8;
}

/* Raw upload: for textures whose texels are all the same, or that are not twiddled. */
static pvr_ptr_t upload_raw(const void *data, int bytes) {
    static uint8_t buf[4096] __attribute__((aligned(32)));
    pvr_ptr_t p = pvr_mem_malloc(bytes);
    memcpy(buf, data, bytes);
    pvr_txr_load(buf, p, bytes);
    return p;
}

static void draw_frame(int page) {
    pvr_wait_ready();
    pvr_scene_begin();
    test_page(page);
    pvr_scene_finish();
}

#if !defined(PAGE) && !defined(LABELS)
static void dump_frame(int page) {
    static uint32_t copy[640 * 480 * FB_BYTES / 4];
    const volatile uint32_t *fb;
    char name[160];
    FILE *f;
    int i;

    for(i = 0; i < 4; i++)                              /* let the last submitted frame reach the screen */
        vid_waitvbl();
    fb = (const volatile uint32_t *)(0xa5000000 + (PVR_GET(PVR_FB_ADDR) & 0x007ffffc));
    for(i = 0; i < 640 * 480 * FB_BYTES / 4; i++)
        copy[i] = fb[i];
    sprintf(name, FB_BYTES == 4 ? DUMP_DIR "/page%d.hw.raw" : DUMP_DIR "/page%d.hw16.raw", page);
    f = fopen(name, "wb");
    if(!f) {
        printf("dump: cannot open %s\n", name);
        return;
    }
    fwrite(copy, 1, sizeof(copy), f);
    fclose(f);
    printf("dump: wrote %s\n", name);
}
#endif

int main(int argc, char **argv) {
    static pvr_init_params_t params = {
#ifdef TEST_MODIFIERS
        { PVR_BINSIZE_32, PVR_BINSIZE_16, PVR_BINSIZE_32, PVR_BINSIZE_16, PVR_BINSIZE_32 },
#else
        { PVR_BINSIZE_32, PVR_BINSIZE_0, PVR_BINSIZE_32, PVR_BINSIZE_0, PVR_BINSIZE_32 },
#endif
        1024 * 1024, 0, 0,
#ifdef TEST_AUTOSORT
        0,
#else
        1,              /* translucent autosort off: drawn in submission order */
#endif
        3, 0
    };

    (void)argc;
    (void)argv;
    vid_set_mode(DM_640x480, TEST_PIXEL_MODE);
    pvr_init(&params);
    pvr_set_bg_color(0.0f, 0.0f, 0.0f);
    printf("FPU_CULL_VAL %08x\n", (unsigned)PVR_GET(0x78));
    test_setup();
#if defined(LABELS)
    test_describe();
    printf("LABELS DONE\n");
    for(;;)
        thd_sleep(100);
#elif defined(PAGE)
    (void)test_describe;
    for(;;)
        draw_frame(PAGE);
#else
    {
        int page, frame;
        (void)test_describe;
        for(page = 0; page < NUM_PAGES; page++) {
            for(frame = 0; frame < 6; frame++)
                draw_frame(page);
            dump_frame(page);
        }
    }
#endif
    pvr_shutdown();
    return 0;
}

#endif
