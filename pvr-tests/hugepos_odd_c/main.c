/* Where do huge vertex positions stop working? (pvr-tests/huge: a triangle with corners at 2^100 is drawn, one
   with corners at 2^120 is not.)  With M = 2^m, m = ms[page] (gen.py):
     a  (-4M, -3M) (4M, 3M) (-3M, 4M)       three huge corners; covers the screen below the diagonal 3x = 4y
     b  (-64, -64) (704, 0) (-3M, 4M)       one huge corner: a band from the top of the screen down to the left
     c  (-4M, 100) (4M, 380) (320, -1000)   two corners huge in x only: the screen above a gently falling line
   HP_TRI 1, 2, 3: one of them alone, white, in the opaque list. HP_TRI 0: all three in the translucent list,
   red, green and blue, added up (blend ONE, ONE). Analyze with read.py.

   The PVR is driven directly (../../common/pvrtest2.h); main_kos.c is the earlier version on KallistiOS's
   driver, and the parameter words set here are the ones it sent. */
#define NUM_PAGES 16
#include "pvrtest2.h"
#include "cases.h"

#define LIST (HP_TRI ? ListType_Opaque : ListType_Translucent)

static void test_setup(void) {
}

static void test_describe(void) {
    int p;
    for(p = 0; p < NUM_PAGES; p++)
        label_span(p, 0, COLS, 15, "M = 2^%d", ms[p]);
}

static void tri(const f32b xy[3][2], uint32_t argb) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    int k;

    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = LIST;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    isp.DepthMode = DepthMode_Always;
    isp.ZWriteDis = 1;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = HP_TRI ? BlendInstr_Zero : BlendInstr_One;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    ta_polygon(pcw, isp, tsp, tcw);
    for(k = 0; k < 3; k++)
        ta_vertex0(xy[k][0], xy[k][1], F32B_ONE, argb, k == 2);
}

static void test_page(int page) {
    f32b m = f32b_scaled(1, ms[page]);                  /* 2^m */
    f32b m3 = sf_mul(F(3.0f), m), m4 = sf_mul(F(4.0f), m);      /* exact */
    f32b a[3][2] = { { sf_neg(m4), sf_neg(m3) }, { m4, m3 }, { sf_neg(m3), m4 } };
    f32b b[3][2] = { { F(-64.0f), F(-64.0f) }, { F(704.0f), F32B_ZERO }, { sf_neg(m3), m4 } };
    f32b c[3][2] = { { sf_neg(m4), F(100.0f) }, { m4, F(380.0f) }, { F(320.0f), F(-1000.0f) } };

    ta_list_begin(LIST);
    if(HP_TRI == 0 || HP_TRI == 1)
        tri(a, HP_TRI ? 0xffffffff : 0xffff0000);
    if(HP_TRI == 0 || HP_TRI == 2)
        tri(b, HP_TRI ? 0xffffffff : 0xff00ff00);
    if(HP_TRI == 0 || HP_TRI == 3)
        tri(c, HP_TRI ? 0xffffffff : 0xff0000ff);
    ta_list_end();
}
