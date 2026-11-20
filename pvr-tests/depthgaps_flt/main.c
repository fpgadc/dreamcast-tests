/* A plane whose start is tiny next to its slopes (2026-11-20, hunt depthgaps round 2; after depthgaps_negconv).

   refsw3 steps a plane from its start in fixed point at the start's exponent (pvr_isp_depth.h, the block form); where
   the slopes are more than about 2^71 times the start that does not fit, and refsw3 evaluated the plane in floating
   point at every pixel instead (the "float form"), storing a depth below zero as a float word, not as the depth unit
   stores one (StoredDepth::Neg). The console steps every plane: the start is cut to the slopes' grid (zstart), so
   such a start is 0 and the block's exponent comes from the slopes. Here planes z = C + A (x - y) through the
   diagonal, whose start at every diagonal tile's first pixel is C exactly: the triangle (0, 0) C, (64, 64) C, and
   (64, 0) or (0, 64) at 64 A (the tiny C there is lost; a and b come out as +-2048 or +-128 exactly, c as -+4096 C),
   read out over tile (0, 0) by depthgaps_search/search.h. Result cell s: S[s]. */
#define NUM_PAGES 1
#define TEST_DRAW_PAGE
#include "pvrtest2.h"
#include "texgen.h"
#include "../depthgaps_search/search.h"

typedef struct { uint32_t c, z2; int lower; const char *name; } plane_t;
#define NS 9
static const plane_t S[NS] = {
    { 0x17800000u, 0xc2000000u, 0, "C 2^-80, A -0.5: upper right half below zero (float form in refsw3 before)" },
    { 0x17800000u, 0xc2000000u, 1, "C 2^-80, A +0.5: lower left half below zero (float form)" },
    { 0x17800000u, 0xc3000000u, 0, "C 2^-80, A -2 (float form)" },
    { 0x0d800000u, 0xbd800000u, 0, "C 2^-100, A -2^-10 (float form)" },
    { 0x21800000u, 0xc2000000u, 0, "C 2^-60, A -0.5 (block form: control)" },
    { 0x1a000000u, 0xc2000000u, 0, "C 2^-75, A -0.5 (float form)" },
    { 0x1b000000u, 0xc2000000u, 0, "C 2^-73, A -0.5 (float form)" },
    { 0x1b800000u, 0xc2000000u, 0, "C 2^-72, A -0.5 (block form)" },
    { 0x97800000u, 0x42000000u, 0, "C -2^-80, A +0.5: upper right half above zero, the diagonal at C" },
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
    const plane_t *p = &S[cur];
    srch_begin(DepthMode_Always, srch_texG);
    srch_vtx(f32b_int(0), f32b_int(0), p->c, 0);
    srch_vtx(f32b_int(64), f32b_int(64), p->c, 0);
    srch_vtx(f32b_int(p->lower ? 0 : 64), f32b_int(p->lower ? 64 : 0), p->z2, 1);
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
