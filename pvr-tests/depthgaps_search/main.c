/* The stored depth of sloped polygons below zero, read out (2026-11-19, hunt depthgaps (c); after
   depthgaps_negramp, depthgaps_negladder).

   depthgaps_negladder: in a ramp below zero whose start is 0 (A = -a 2^-20, B = -b 2^-20: v = A i + B j exactly),
   the pixels of the block's top binade (|v| in [16, 32), the block's exponent from the slopes) store v's magnitude
   rounded up to the 0x100 grid, as a flat polygon does; the pixels below 16 store something above every probe
   within 0x800 of v. Here every pixel of tile (0, 0) read out with search.h (a binary search with exact probes, 29
   renders): result cell c (rows of 32 words: the last probe at or below the depth, bit 0 set where equal).
     cell 0: the ramp below zero; cell 1: the same above zero (A, B > 0); cell 2: A < 0 < B;
     cell 3: a flat quad over the tile at -100 (0xc2c80000) */
#define NUM_PAGES 1
#define TEST_DRAW_PAGE
#include "pvrtest2.h"
#include "texgen.h"
#include "search.h"

static const int32_t RA = 0x0b69d3, RB = 54321;
static int sa, sb;
static uint32_t flat;

static void test_setup(void) { srch_setup(); }

static void test_describe(void)
{
    label(0, 0, "ramp a %d, b %d below zero: stored depths read out", (int)RA, (int)RB);
    label(0, 1, "ramp a %d, b %d above zero: stored depths read out", (int)RA, (int)RB);
    label(0, 2, "ramp -a %d, +b %d: stored depths read out", (int)RA, (int)RB);
    label(0, 3, "flat quad at -100: stored depths read out");
}

static void ramp(void)
{
    const uint32_t hx = 0x3f000000u, fx = 0x42810000u;        /* 0.5, 64.5 */
    srch_begin(DepthMode_Always, srch_texG);
    srch_vtx(hx, hx, 0, 0);
    srch_vtx(fx, hx, f32b_scaled(sa * RA, 6 - 20), 0);
    srch_vtx(hx, fx, f32b_scaled(sb * RB, 6 - 20), 1);
}

static void quad(void)
{
    int k;
    srch_begin(DepthMode_Always, srch_texG);
    for(k = 0; k < 4; k++)
        srch_vtx(f32b_int((k & 1) * 32), f32b_int((k >> 1) * 32), flat, k == 3);
}

static void test_page(int page) { (void)page; }

static void test_draw_page(int page)
{
    (void)page;
    sa = -1; sb = -1; srch_run(ramp, 0); srch_keep(0);
    sa = 1; sb = 1; srch_run(ramp, 0); srch_keep(1);
    sa = -1; sb = 1; srch_run(ramp, 0); srch_keep(2);
    flat = 0xc2c80000u; srch_run(quad, 0); srch_keep(3);
    srch_store(4, 0);
}
