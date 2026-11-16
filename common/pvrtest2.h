/* Harness for tests that drive the PVR directly (pvrsetup.h), without KallistiOS's driver. A test's main.c
   defines NUM_PAGES and the three test_* functions below, like the tests on pvrtest.h, and builds three ways:

     for a PC (gcc)              prints the commands that render every page in nulldc3's refsw2-offline and
                                 write each to <folder>/page<n>.off.raw:   ./test <folder> | refsw2-offline -
                                 ./test --labels prints the labels
     for a Dreamcast (kos-cc)    renders every page and writes each to DUMP_DIR/page<n>.hw.raw (dc-load)
     with -DPAGE=n               renders page n forever (for an emulator's screenshot)

   The screen is a grid of 32x32 cells (one tile each), 20 across, numbered row by row.

   Floats: a test carries them as bit patterns (f32b) that its generator computed; nothing here or in a test
   calculates with floats at run time, so the PC and the SH4 send the same bits (the "PVRIO crc" lines of
   two runs show it). */
#ifndef PVRTEST2_H
#define PVRTEST2_H

#include <stdarg.h>
#include "pvrsetup.h"

#define COLS 20

#ifndef TEST_OPB_WORDS
#ifdef TEST_MODIFIERS
#define TEST_OPB_WORDS { 32, 16, 32, 16, 32 }
#else
#define TEST_OPB_WORDS { 32, 0, 32, 0, 32 }
#endif
#endif
#ifndef TEST_FB_PACKMODE
#define TEST_FB_PACKMODE fb_packmode_0888
#endif
#ifndef TEST_FB_DITHER
#define TEST_FB_DITHER 0
#endif

static void test_setup(void);           /* once, after pvr_setup */
static void test_describe(void);        /* calls label() for everything drawn */
static void test_page(int page);        /* between pvr_frame_begin and pvr_frame_render: sends the lists */

static inline int cell_px(int c) { return (c % COLS) * 32; }
static inline int cell_py(int c) { return (c / COLS) * 32; }

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

/* The four vertices of a rectangle given in whole pixels (top left, top right, bottom left, bottom right),
   untextured, packed colour; the polygon parameter has been sent. */
static void quad0(int x, int y, int w, int h, f32b z, uint32_t BaseCol) {
    int k;
    for(k = 0; k < 4; k++)
        ta_vertex0(f32b_int(x + (k & 1) * w), f32b_int(y + (k >> 1) * h), z, BaseCol, k == 3);
}

/* The same with a texture: 32 bit UV from (u0, v0) at the top left to (u1, v1) at the bottom right. */
static void quad3(int x, int y, int w, int h, f32b z, f32b u0, f32b v0, f32b u1, f32b v1, uint32_t BaseCol, uint32_t OffsCol) {
    int k;
    for(k = 0; k < 4; k++)
        ta_vertex3(f32b_int(x + (k & 1) * w), f32b_int(y + (k >> 1) * h), z, (k & 1) ? u1 : u0, (k >> 1) ? v1 : v0, BaseCol, OffsCol, k == 3);
}

/* Deterministic pseudo-random numbers, the same on every run (and the same as pvrtest.h's). */
static uint32_t rng_state = 1;
static uint32_t rng(void) {
    rng_state = rng_state * 1664525u + 1013904223u;
    return rng_state >> 8;
}

static void draw_page(int page) {
    pvr_frame_begin();
    test_page(page);
    pvr_frame_render();
}

int main(int argc, char **argv) {
    static const pvr_setup_t setup = {
        TEST_OPB_WORDS,
#ifdef TEST_AUTOSORT
        0,
#else
        1,                              /* translucent polygons in the order sent */
#endif
        TEST_FB_PACKMODE, TEST_FB_DITHER
    };

    (void)argc; (void)argv;
#ifndef _arch_dreamcast
    if(argc > 1 && !strcmp(argv[1], "--labels")) {      /* the setup runs, silently: labels may depend on it */
        pvrio_mute = 1;
        pvr_setup(&setup);
        test_setup();
        pvrio_mute = 0;
        test_describe();
        return 0;
    }
    if(argc > 1)
        pvrio_folder = argv[1];
#endif
    pvr_setup(&setup);
    test_setup();
#if defined(PAGE)
    (void)test_describe;
    for(;;) {
        draw_page(PAGE);
        pvrio_vblank();
    }
#else
    {
        char name[32];
        int page;
        (void)test_describe;
        for(page = 0; page < NUM_PAGES; page++) {
            draw_page(page);
            snprintf(name, sizeof(name), "page%d", page);
            pvr_frame_dump(name);
        }
    }
#endif
    pvrio_close();
    return 0;
}

#endif
