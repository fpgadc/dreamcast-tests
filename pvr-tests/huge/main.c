/* Huge, tiny and special values in depth and in vertex positions. Analyze with huge.py.

   page 0  depth. Column c: z = 2^e, e = -126 + 13 c (column 19: e = 127).
       row 0  a white quad at z, compare ALWAYS                 row 1  the same, compare GEQUAL against the empty buffer
       row 2  A red at z, B green at 2z, GREATER (should pass; column 19 uses 1.5 z)
       row 3  A red at z, B green 64 units in the last place above, GREATER
       row 4  A red at z, B green at z / 2, GREATER (should fail)
       row 5  white at -z, ALWAYS                               row 6  white at -z, GEQUAL against the empty buffer
       row 7  A red at 1.0, B green at -z, GREATER              row 8  A red at -z, B green at 1.0, GREATER
     special values, column = 0.0, -0.0, smallest denormal, largest denormal, smallest normal, largest
     finite, +inf, -inf, quiet NaN, negative quiet NaN, signaling NaN:
       row 10 white, ALWAYS      row 11 white, GEQUAL against the empty buffer
       row 12 A red at 1.0, B green at the value, GREATER       row 13 A red at the value, B green at 1.0, GREATER
       row 14 A red at the value, B green at the same value with the same vertices, EQUAL
   pages 1-13  position. One white triangle with corners (-4M, -3M), (4M, 3M), (-3M, 4M): it covers the part of
       the screen below the diagonal 3x = 4y. M = 2^m, m = 8, 10, 12, 14, 16, 18, 20, 24, 30, 40, 60, 100, 120.
   page 14  the same with M = infinity.    page 15  the same triangle (m = 8) with NaN as one x coordinate. */
#define NUM_PAGES 16
#include "pvrtest2.h"

static const int ms[13] = { 8, 10, 12, 14, 16, 18, 20, 24, 30, 40, 60, 100, 120 };
static const uint32_t special[11] = { 0x00000000, 0x80000000, 0x00000001, 0x007fffff, 0x00800000, 0x7f7fffff,
                                      0x7f800000, 0xff800000, 0x7fc00000, 0xffc00000, 0x7f800001 };

static uint32_t pow2(int e) { return (uint32_t)(e + 127) << 23; }

/* An untextured flat polygon in the translucent list; the depth is compared with DepthMode and written
   unless ZWriteDis. Gouraud shaded as KallistiOS's context was by default. */
static void polygon(int DepthMode, int ZWriteDis) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };

    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Translucent;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Gouraud = 1;
    isp.DepthMode = DepthMode;
    isp.ZWriteDis = ZWriteDis;
    isp.CullMode = CullMode_None;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    ta_polygon(pcw, isp, tsp, tcw);
}

static void test_setup(void) {
}

static void test_describe(void) {
    int r, c, p;

    for(r = 0; r < 15; r++)
        if(r != 9)
            for(c = 0; c < (r < 9 ? 20 : 11); c++)
                label(0, r * COLS + c, "depth row %d column %d", r, c);
    for(p = 1; p < NUM_PAGES; p++)
        label_span(p, 0, COLS, 15, p < 14 ? "triangle, M = 2^%d" : p == 14 ? "triangle, M = inf" : "triangle, NaN x", p < 14 ? ms[p - 1] : 0);
}

static void quad_cmp(int DepthMode, int ZWriteDis, int x, int y, int w, int h, f32b z, uint32_t argb) {
    polygon(DepthMode, ZWriteDis);
    quad0(x, y, w, h, z, argb);
}

static void pair(int cell, f32b za, f32b zb, int cmp, int same) {
    int x = cell_px(cell), y = cell_py(cell);
    quad_cmp(DepthMode_Always, 0, x, y, 32, 32, za, 0xffff0000);                    /* A writes its depth */
    if(same)
        quad_cmp(cmp, 1, x, y, 32, 32, zb, 0xff00ff00);
    else
        quad_cmp(cmp, 1, x + 4, y + 4, 24, 24, zb, 0xff00ff00);
}

static void test_page(int page) {
    int c;
    const f32b one = F32B_ONE;

    ta_list_begin(ListType_Translucent);
    if(page == 0) {
        for(c = 0; c < 20; c++) {
            int e = (c == 19) ? 127 : -126 + 13 * c;
            uint32_t b = pow2(e);
            f32b z = b, nz = b | 0x80000000;
            quad_cmp(DepthMode_Always, 1, cell_px(c), cell_py(c), 32, 32, z, 0xffffffff);
            quad_cmp(DepthMode_GreaterEqual, 1, cell_px(20 + c), cell_py(20 + c), 32, 32, z, 0xffffffff);
            pair(40 + c, z, c == 19 ? b | 0x400000 : pow2(e + 1), DepthMode_Greater, 0);
            pair(60 + c, z, b + 64, DepthMode_Greater, 0);
            pair(80 + c, z, c == 0 ? b - 0x400000 : pow2(e - 1), DepthMode_Greater, 0);
            quad_cmp(DepthMode_Always, 1, cell_px(100 + c), cell_py(100 + c), 32, 32, nz, 0xffffffff);
            quad_cmp(DepthMode_GreaterEqual, 1, cell_px(120 + c), cell_py(120 + c), 32, 32, nz, 0xffffffff);
            pair(140 + c, one, nz, DepthMode_Greater, 0);
            pair(160 + c, nz, one, DepthMode_Greater, 0);
        }
        for(c = 0; c < 11; c++) {
            f32b z = special[c];
            quad_cmp(DepthMode_Always, 1, cell_px(200 + c), cell_py(200 + c), 32, 32, z, 0xffffffff);
            quad_cmp(DepthMode_GreaterEqual, 1, cell_px(220 + c), cell_py(220 + c), 32, 32, z, 0xffffffff);
            pair(240 + c, one, z, DepthMode_Greater, 0);
            pair(260 + c, z, one, DepthMode_Greater, 0);
            pair(280 + c, z, z, DepthMode_Equal, 1);
        }
    }
    else {
        f32b m = (page < 14) ? pow2(ms[page - 1]) : (page == 14) ? 0x7f800000u : F(256.0f);
        f32b p4 = F(4.0f), p3 = F(3.0f);
        f32b t[3][2] = { { sf_neg(sf_mul(p4, m)), sf_neg(sf_mul(p3, m)) }, { sf_mul(p4, m), sf_mul(p3, m) },
                         { sf_neg(sf_mul(p3, m)), sf_mul(p4, m) } };
        int k;
        if(page == 15)
            t[2][0] = 0x7fc00000u;
        polygon(DepthMode_Always, 1);
        for(k = 0; k < 3; k++)
            ta_vertex0(t[k][0], t[k][1], one, 0xffffffff, k == 2);
    }
    ta_list_end();
}
