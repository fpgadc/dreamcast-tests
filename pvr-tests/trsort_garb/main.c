/* The translucent sort (TEST_AUTOSORT) with garbage depths: trsort_recipovf's triangle T, (0, 0) (1024, 0) (512, Y),
   determinant exactly 1024 Y = f 2^126, whose ISP depth runs through the overflow regimes (f = 1 - 2^-24: diagonal
   stripes of wrapped, saturated and below-zero depths) or is 0 everywhere (f = 1: a zero reciprocal), against the
   10 flat probe strips of trsort_recipovf (64 x 480 each, depths -2^127, -1, -2^-100, +0, 2^-100, 1, 2^100,
   2^127, the largest float, 1.5 2^127), all of them in the translucent list:
     T       corner depths  inf 1 1 / 1 2^127 1 / 2^120 x3, blue (its colour garbage: its planes overflow)
     probes  green; the one at -2^127 white (trsort_recipovf: in runs right of x = 0 its 0 channels are white on
             the console, not in refsw3; white makes the colour planes the 1/w plane, a quotient of 1 either way)
     near    blend (ONE, ZERO): the pixel is the nearest key's (the sort goes far to near)
     add     blend (ONE, ONE): every layer the sort takes, added
     Tp/pT   the list order: T first or the probes first (ties: the later one is the nearer key)
     bg      ISP_BACKGND_D, the sort's first reference: 0.0001 (pvrsetup.h's) or -2^127
   If the sort orders the stored depths as the depth test does (trsort_recipovf, opaque, DepthMode Greater), near
   shows a probe exactly where trsort_recipovf draws it, and nothing at all of a layer at or below bg.
   Regions: one per probe strip and page. */
#define NUM_PAGES 16
#define TEST_AUTOSORT
#include "pvrtest2.h"

enum { NEAR, ADD };
#define BG_LOW  0x38d1b717u        /* 0.0001 */
#define BG_NEG  0xff000000u        /* -2^127 */
static const f32b zsets[3][3] = {
    { 0x7f800000, 0x3f800000, 0x3f800000 },     /* inf 1 1 */
    { 0x3f800000, 0x7f000000, 0x3f800000 },     /* 1 2^127 1 */
    { 0x7b800000, 0x7b800000, 0x7b800000 },     /* 2^120 x3 */
};
static const struct { int below, z, blend, probesFirst; uint32_t bg; } pages[NUM_PAGES] = {
    { 1, 0, NEAR, 0, BG_LOW }, { 1, 1, NEAR, 0, BG_LOW }, { 1, 2, NEAR, 0, BG_LOW },
    { 1, 0, NEAR, 0, BG_NEG }, { 1, 1, NEAR, 0, BG_NEG }, { 1, 2, NEAR, 0, BG_NEG },
    { 1, 0, NEAR, 1, BG_NEG }, { 1, 1, NEAR, 1, BG_NEG }, { 1, 2, NEAR, 1, BG_NEG },
    { 1, 0, ADD, 0, BG_NEG }, { 1, 1, ADD, 0, BG_NEG }, { 1, 2, ADD, 0, BG_NEG },
    { 0, 1, NEAR, 0, BG_NEG }, { 0, 1, NEAR, 1, BG_NEG },
    { 0, 0, ADD, 0, BG_LOW }, { 1, 0, ADD, 1, BG_LOW },
};
static const f32b probes[10] = { 0xff000000, 0xbf800000, 0x8d800000, 0x00000000, 0x0d800000, 0x3f800000,
                                 0x71800000, 0x7f000000, 0x7f7fffff, 0x7f400000 };

static void test_setup(void) {
}

static void test_describe(void) {
    int p, i;
    for(p = 0; p < NUM_PAGES; p++)
        for(i = 0; i < 10; i++)
            label_span(p, 2 * i, 2, 15, "det %s2^126, z %08x %08x %08x, %s, %s, bg %08x, probe %08x",
                       pages[p].below ? "(1 - 2^-24) " : "", zsets[pages[p].z][0], zsets[pages[p].z][1], zsets[pages[p].z][2],
                       pages[p].blend == NEAR ? "near" : "add", pages[p].probesFirst ? "pT" : "Tp", pages[p].bg, probes[i]);
}

static void poly(int blend) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };

    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Translucent;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    isp.DepthMode = DepthMode_Always;
    isp.ZWriteDis = 1;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = blend == ADD ? BlendInstr_One : BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    ta_polygon(pcw, isp, tsp, tcw);
}

static void triangle(int page) {
    f32b Y = pages[page].below ? f32b_scaled(0xffffff, 116 - 24) : f32b_scaled(1, 116);
    const f32b* z = zsets[pages[page].z];
    poly(pages[page].blend);
    ta_vertex0(F32B_ZERO, F32B_ZERO, z[0], 0xff0000ff, 0);
    ta_vertex0(f32b_int(1024), F32B_ZERO, z[1], 0xff0000ff, 0);
    ta_vertex0(f32b_int(512), Y, z[2], 0xff0000ff, 1);
}

static void probeStrips(int page) {
    int i;
    for(i = 0; i < 10; i++) {
        poly(pages[page].blend);
        quad0(64 * i, 0, 64, 480, probes[i], i == 0 ? 0xffffffff : 0xff00ff00);
    }
}

static void test_page(int page) {
    pvr_frame.bg_depth = pages[page].bg;
    ta_list_begin(ListType_Translucent);
    if(!pages[page].probesFirst) {
        triangle(page); probeStrips(page);
    } else {
        probeStrips(page); triangle(page);
    }
    ta_list_end();
}
