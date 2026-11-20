/* hugepos --tri all through the translucent SORT (hugepos itself draws the translucent list in the order sent:
   pvrtest2.h's default). The three triangles of pvr-tests/hugepos, corners at +-3M, +-4M with M = 2^m, every
   corner at depth 1, in the translucent list sorted per pixel (TEST_AUTOSORT):
     a  (-4M, -3M) (4M, 3M) (-3M, 4M)       red    0xffff0000
     b  (-64, -64) (704, 0) (-3M, 4M)       green  0xff00ff00
     c  (-4M, 100) (4M, 380) (320, -1000)   blue   0xff0000ff
   From 2^62 a's determinant is past 2^128 and the ISP's depth of a is garbage (README detinf), from 2^116 c's and
   b's too: what the sort does with them against the background's key and against each other.
     add      blend (ONE, ONE): every layer the sort takes, added
     near     blend (ONE, ZERO): the last layer drawn, the nearest key (the sort goes far to near)
     abc/cba  the list order (ties: the later one is the nearer key)
     bg       ISP_BACKGND_D (the sort's first reference): 0.0001 (pvrsetup.h's) or -1
   Regions: each page in rows of tiles. */
#define NUM_PAGES 16
#define TEST_AUTOSORT
#include "pvrtest2.h"

enum { ADD, NEAR };
static const struct { int m, blend, rev; uint32_t bg; } pages[NUM_PAGES] = {
    { 30, ADD, 0, 0x38d1b717 }, { 62, ADD, 0, 0x38d1b717 }, { 64, ADD, 0, 0x38d1b717 }, { 112, ADD, 0, 0x38d1b717 },
    { 114, ADD, 0, 0x38d1b717 }, { 116, ADD, 0, 0x38d1b717 }, { 118, ADD, 0, 0x38d1b717 }, { 120, ADD, 0, 0x38d1b717 },
    { 30, NEAR, 0, 0x38d1b717 }, { 62, NEAR, 0, 0x38d1b717 }, { 116, NEAR, 0, 0x38d1b717 }, { 120, NEAR, 0, 0x38d1b717 },
    { 30, NEAR, 1, 0x38d1b717 }, { 62, NEAR, 1, 0x38d1b717 },
    { 62, NEAR, 0, 0xbf800000 }, { 116, NEAR, 0, 0xbf800000 },
};

static void test_setup(void) {
}

static void test_describe(void) {
    int p, r;
    for(p = 0; p < NUM_PAGES; p++)
        for(r = 0; r < 15; r++)
            label_span(p, r * COLS, COLS, 1, "M = 2^%d, %s, %s, bg %08x, tile row %d", pages[p].m,
                       pages[p].blend == ADD ? "add" : "near", pages[p].rev ? "cba" : "abc", pages[p].bg, r);
}

static void tri(const f32b xy[3][2], uint32_t argb, int blend) {
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
    tsp.DstInstr = blend == ADD ? BlendInstr_One : BlendInstr_Zero;
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
    int bl = pages[page].blend;

    pvr_frame.bg_depth = pages[page].bg;
    ta_list_begin(ListType_Translucent);
    if(!pages[page].rev) {
        tri(a, 0xffff0000, bl); tri(b, 0xff00ff00, bl); tri(c, 0xff0000ff, bl);
    } else {
        tri(c, 0xff0000ff, bl); tri(b, 0xff00ff00, bl); tri(a, 0xffff0000, bl);
    }
    ta_list_end();
}
