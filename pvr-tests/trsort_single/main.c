/* hugepos --tri all, one triangle at a time: where do the green and blue that hugepos_all shows inside the red
   triangle's area come from? (README hugepos: "from 2^62 the green and blue triangles also show inside the red
   one's area, where they do not cover".)  The same triangles, colours, blend (ONE, ONE), list (translucent, in the
   order sent: pvrtest2.h's default, as hugepos) and corners as pvr-tests/hugepos, but only ONE triangle a page:
     a  (-4M, -3M) (4M, 3M) (-3M, 4M)       red    0xffff0000
     b  (-64, -64) (704, 0) (-3M, 4M)       green  0xff00ff00
     c  (-4M, 100) (4M, 380) (320, -1000)   blue   0xff0000ff
   with M = 2^m. If a page shows a channel other than its triangle's, the triangle's own shading made it (its TSP
   colour planes), not another triangle. Regions: each page in rows of tiles. */
#define NUM_PAGES 16
#include "pvrtest2.h"

static const struct { char tri; int m; } pages[NUM_PAGES] = {
    { 'a', 60 }, { 'a', 62 }, { 'a', 64 }, { 'a', 100 }, { 'a', 116 }, { 'a', 118 }, { 'a', 120 },
    { 'b', 62 }, { 'b', 114 }, { 'b', 116 }, { 'b', 118 }, { 'b', 120 },
    { 'c', 62 }, { 'c', 112 }, { 'c', 114 }, { 'c', 116 },
};

static void test_setup(void) {
}

static void test_describe(void) {
    int p, r;
    for(p = 0; p < NUM_PAGES; p++)
        for(r = 0; r < 15; r++)
            label_span(p, r * COLS, COLS, 1, "%c alone, M = 2^%d, tile row %d", pages[p].tri, pages[p].m, r);
}

static void tri(const f32b xy[3][2], uint32_t argb) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    int k;

    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Translucent;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    isp.DepthMode = DepthMode_Always;
    isp.ZWriteDis = 1;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_One;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    ta_polygon(pcw, isp, tsp, tcw);
    for(k = 0; k < 3; k++)
        ta_vertex0(xy[k][0], xy[k][1], F32B_ONE, argb, k == 2);
}

static void test_page(int page) {
    f32b m = f32b_scaled(1, pages[page].m);                     /* 2^m */
    f32b m3 = sf_mul(F(3.0f), m), m4 = sf_mul(F(4.0f), m);      /* exact */
    f32b a[3][2] = { { sf_neg(m4), sf_neg(m3) }, { m4, m3 }, { sf_neg(m3), m4 } };
    f32b b[3][2] = { { F(-64.0f), F(-64.0f) }, { F(704.0f), F32B_ZERO }, { sf_neg(m3), m4 } };
    f32b c[3][2] = { { sf_neg(m4), F(100.0f) }, { m4, F(380.0f) }, { F(320.0f), F(-1000.0f) } };

    ta_list_begin(ListType_Translucent);
    switch(pages[page].tri) {
    case 'a': tri(a, 0xffff0000); break;
    case 'b': tri(b, 0xff00ff00); break;
    default:  tri(c, 0xff0000ff); break;
    }
    ta_list_end();
}
