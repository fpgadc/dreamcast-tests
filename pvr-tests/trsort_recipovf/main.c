/* trsort_recip with depths that overflow: does a zero reciprocal (a determinant of 2^126 or more) make the depth 0
   whatever the slopes and the constant, an infinity among them included (zero times an infinity: the setup's +0)?
   The triangle of trsort_recip, (0, 0) (1024, 0) (512, Y), determinant exactly 1024 Y = f 2^126, with f = 1 (the
   reciprocal zero) or f = 1 - 2^-24 (just below: the overflow regimes of an ordinary determinant), corner depths
     inf 1 1        an infinite corner (slopes and constant infinite)
     1 2^127 1      a slope past 2^128 before the division
     2^120 x3       the constant past 2^128 (the products of z and the corners' x y)
     -inf 1 1
   Opaque list, DepthMode Always, depth written, blue; then 10 probes over it, strips of 64 x 480 at flat depths
   (DepthMode Greater: drawn where the probe is above the stored depth), green:
     -2^127, -1, -2^-100, +0, 2^-100, 1, 2^100, 2^127, the largest float, 1.5 2^127
   The probe at -2^127 is white (0xffffffff): its planes overflow (1/w plane: C = -inf, A = B = +0) and in a run
   that starts right of x = 0 a channel of 0 came out 0xff on the console but less in refsw3 (0xcd at (2, 0)),
   which divides (x + y) 2^-127 by (x + y + 1/2) 2^-127 there (first run of this test, green, pages 1, 3, 7:
   1717, 1590, 1505 pixels near the screen's origin; where the probe is drawn the same on both). With every
   channel 0xff the three colour planes are the 1/w plane and the quotient is 1 either way: the strip tells only
   where the probe is drawn.
   Regions: one per probe strip and page. */
#define NUM_PAGES 8
#include "pvrtest2.h"

static const struct { int below; f32b z[3]; } pages[NUM_PAGES] = {
    { 0, { 0x7f800000, 0x3f800000, 0x3f800000 } },
    { 1, { 0x7f800000, 0x3f800000, 0x3f800000 } },
    { 0, { 0x3f800000, 0x7f000000, 0x3f800000 } },
    { 1, { 0x3f800000, 0x7f000000, 0x3f800000 } },
    { 0, { 0x7b800000, 0x7b800000, 0x7b800000 } },
    { 1, { 0x7b800000, 0x7b800000, 0x7b800000 } },
    { 0, { 0xff800000, 0x3f800000, 0x3f800000 } },
    { 1, { 0xff800000, 0x3f800000, 0x3f800000 } },
};

static const f32b probes[10] = { 0xff000000, 0xbf800000, 0x8d800000, 0x00000000, 0x0d800000, 0x3f800000,
                                 0x71800000, 0x7f000000, 0x7f7fffff, 0x7f400000 };

static void test_setup(void) {
}

static void test_describe(void) {
    int p, i;
    for(p = 0; p < NUM_PAGES; p++)
        for(i = 0; i < 10; i++)
            label_span(p, 2 * i, 2, 15, "det %s2^126, z %08x %08x %08x, probe %08x", pages[p].below ? "(1 - 2^-24) " : "",
                       pages[p].z[0], pages[p].z[1], pages[p].z[2], probes[i]);
}

static void poly(int depthMode, int zwdis) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };

    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Opaque;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    isp.DepthMode = depthMode;
    isp.ZWriteDis = zwdis;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    ta_polygon(pcw, isp, tsp, tcw);
}

static void test_page(int page) {
    f32b Y = pages[page].below ? f32b_scaled(0xffffff, 116 - 24) : f32b_scaled(1, 116);
    int i;

    ta_list_begin(ListType_Opaque);
    poly(DepthMode_Always, 0);
    ta_vertex0(F32B_ZERO, F32B_ZERO, pages[page].z[0], 0xff0000ff, 0);
    ta_vertex0(f32b_int(1024), F32B_ZERO, pages[page].z[1], 0xff0000ff, 0);
    ta_vertex0(f32b_int(512), Y, pages[page].z[2], 0xff0000ff, 1);
    for(i = 0; i < 10; i++) {
        poly(DepthMode_Greater, 1);
        quad0(64 * i, 0, 64, 480, probes[i], i == 0 ? 0xffffffff : 0xff00ff00);
    }
    ta_list_end();
}
