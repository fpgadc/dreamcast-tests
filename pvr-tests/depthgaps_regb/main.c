/* Planes whose constant overflows, read out (2026-11-19, hunt depthgaps (b); after zword, depthgaps_negconv).

   zword: a triangle whose plane's constant c overflows (2^128) but whose slopes do not stores 0 at the tile's
   first pixel and -(2^(e + 1) - ulp) at every other one, e the block's exponent from the slopes; measured with
   slopes below zero only. depthgaps_negconv: a ramp from a start of exactly 0 whose depths are below zero stores
   exactly that word (UW) where the magnitude is under the block's top binade. So is the overflow a start of 0?
   Big triangles over tile (0, 0) whose constant overflows (corners hundreds of pixels apart: z e past 2^128) at
   depths that search.h can read (up to 2^118), every pixel read out: result cell s (the last probe at or below
   the depth, bit 0 set where equal) for S[s]: corners (x, y, z) three times. */
#define NUM_PAGES 1
#define TEST_DRAW_PAGE
#include "pvrtest2.h"
#include "texgen.h"
#include "../depthgaps_search/search.h"

typedef struct { int16_t x[3], y[3]; uint32_t z[3]; const char *name; } tri_t;
#define NS 8
static const tri_t S[NS] = {
    { { 0, 640, 0 }, { 0, 0, 480 }, { 0x77800000u, 0x3f000000u, 0x3f000000u }, "(0,0) 2^112, (640,0) 0.5, (0,480) 0.5: slopes below 0" },
    { { 0, 640, 0 }, { 0, 0, 480 }, { 0xf7800000u, 0x3f000000u, 0x3f000000u }, "(0,0) -2^112, others 0.5: slopes above 0" },
    { { 0, 640, 0 }, { 0, 0, 480 }, { 0x77f33333u, 0x3f000000u, 0x3f000000u }, "(0,0) 1.9 2^112, others 0.5" },
    { { -64, 640, -64 }, { -64, -64, 480 }, { 0x3f000000u, 0x78000000u, 0xf8000000u }, "(-64,-64) 0.5, (640,-64) 2^113, (-64,480) -2^113: mixed slopes" },
    { { -64, 640, -64 }, { -64, -64, 480 }, { 0x3f000000u, 0xf8000000u, 0x78000000u }, "(-64,-64) 0.5, (640,-64) -2^113, (-64,480) 2^113: mixed slopes" },
    { { 0, 640, 0 }, { 0, 0, 480 }, { 0x7a000000u, 0x3f000000u, 0x3f000000u }, "(0,0) 2^117, others 0.5" },
    { { 0, 320, 0 }, { 0, 0, 320 }, { 0x77800000u, 0x3f000000u, 0x3f000000u }, "(0,0) 2^112, (320,0) 0.5, (0,320) 0.5" },
    { { 0, 640, 0 }, { 0, 0, 480 }, { 0x77800000u, 0x77000000u, 0x3f000000u }, "(0,0) 2^112, (640,0) 2^111, (0,480) 0.5" },
};
static int cur;

static void test_setup(void) { srch_setup(); }

static void test_describe(void)
{
    int s;
    for(s = 0; s < NS; s++)
        label(0, s, "%s: stored depths read out", S[s].name);
}

static void tri(void)
{
    int k;
    srch_begin(DepthMode_Always, srch_texG);
    for(k = 0; k < 3; k++)
        srch_vtx(f32b_int(S[cur].x[k]), f32b_int(S[cur].y[k]), S[cur].z[k], k == 2);
}

static void test_page(int page) { (void)page; }

static void test_draw_page(int page)
{
    (void)page;
    for(cur = 0; cur < NS; cur++) {
        srch_run(tri, 0);
        srch_keep(cur);
    }
    srch_store(NS, 0);
}
