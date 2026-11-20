/* How a depth below zero is stored, from the depth unit's fixed point steps (2026-11-19, hunt depthgaps (c), (b);
   after depthgaps_search).

   depthgaps_search: a ramp from a start of exactly 0 with negative steps stores, at every pixel whose depth is
   below the block's top binade in magnitude (the block's exponent e from the slopes: |v| < 2^(e - 1)), one word
   just above -2^(e - 1) in the ISP's order (not a probe), and in the top binade v's magnitude rounded up to the 0x100
   grid; positive ramps store their depths. Here more planes, each read out over tile (0, 0) by search.h: a quad
   over the tile, corners (0, 0) (32, 0) (0, 32) (32, 32) at depths z0 + 32 (k & 1) dx + 32 (k >> 1) dy.
   Result cell s (the last probe at or below the depth, bit 0 set where equal): S[s]. */
#define NUM_PAGES 1
#define TEST_DRAW_PAGE
#include "pvrtest2.h"
#include "texgen.h"
#include "../depthgaps_search/search.h"

typedef struct { int32_t z0, dx, dy; int e; const char *name; } plane_t;      /* z = (z0 + dx x + dy y) 2^e */
#define NS 12
static const plane_t S[NS] = {
    { -100 * 1024, 3, 5, -10, "start -100, slopes tiny, in the start's binade" },
    { -100 * 64, 128, 64, -6, "start -100, slopes +2, +1: down to -100 + 96 = -4 and below the binade" },
    { -100 * 64, -128, -64, -6, "start -100, slopes -2, -1: beyond -128" },
    { 10 * 64, -64, -32, -6, "start +10, slopes -1, -0.5: crossing zero to -36.5" },
    { -10 * 64, 64, 32, -6, "start -10, slopes +1, +0.5: crossing zero to +36.5" },
    { -1000 * 16, 16 * 16, 3, -4, "start -1000, slopes 16, tiny: to -500" },
    { -96 * 256, 251, 0, -8, "start -96, slope +0.98: to -65.6, in the binade" },
    { -96 * 256, -251, 0, -8, "start -96, slope -0.98: to -126.4, in the binade" },
    { -64 * 256, 257, 0, -8, "start -64, slope +1.004: below 64 at once" },
    { -127 * 256, -1, -2, -8, "start -127, slopes tiny negative: across -128" },
    { -3 * 4096, 1, 1, -12, "start -3, slopes 2^-12" },
    { 0, -77, -5, -10, "start 0, slopes -0.075, -0.005" },
};
static int cur;

static void test_setup(void) { srch_setup(); }

static void test_describe(void)
{
    int s;
    for(s = 0; s < NS; s++)
        label(0, s, "%s: stored depths read out", S[s].name);
}

static void quad(void)
{
    const plane_t *p = &S[cur];
    int k;
    srch_begin(DepthMode_Always, srch_texG);
    for(k = 0; k < 4; k++)
        srch_vtx(f32b_int((k & 1) * 32), f32b_int((k >> 1) * 32), f32b_scaled(p->z0 + 32 * ((k & 1) * p->dx + (k >> 1) * p->dy), p->e), k == 3);
}

static void test_page(int page) { (void)page; }

static void test_draw_page(int page)
{
    (void)page;
    for(cur = 0; cur < NS; cur++) {
        srch_run(quad, 0);
        srch_keep(cur);
    }
    srch_store(NS, 0);
}
