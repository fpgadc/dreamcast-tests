/* The order of two polygons' depths when one or both are below zero (2026-11-19).

   pvr-tests/bgspec: probes below zero against a background of -inf order as -2^-21 < -0.5 < -1 < -2 < -1e30 <
   -inf < 0, the bits of the float read as a signed integer, not as floats order. Here no background is involved:
   every cell a flat quad at depth A (DepthMode Always, green), then a white flat quad at depth B over it, DepthMode
   Greater (pages 0, 1) or Less (2, 3): white where B > A (B < A) as the ISP compares them. Rows: A = V[row] (pages
   0, 2) or V[row + 5] (1, 3); columns: B = V[col]. V: -2^100, -2^60, -2, -1.5, -1, -0.5, -2^-20, -2^-21, -2^-100,
   -0, +0, 2^-100, 2^-21, 0.5, 1, 1.5, 2, 2^60, 2^100, 2^-20. */
#define NUM_PAGES 4
#include "pvrtest2.h"

static const uint32_t V[20] = {
    0xf1800000u, 0xdd800000u, 0xc0000000u, 0xbfc00000u, 0xbf800000u, 0xbf000000u, 0xb5800000u, 0xb5000000u,
    0x8d800000u, 0x80000000u, 0x00000000u, 0x0d800000u, 0x35000000u, 0x3f000000u, 0x3f800000u, 0x3fc00000u,
    0x40000000u, 0x5d800000u, 0x71800000u, 0x35800000u,
};

static void test_setup(void) { }

static void test_describe(void)
{
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 300; c++)
            label(p, c, "A %08x, B %08x, %s", (unsigned)V[c / 20 + (p & 1) * 5], (unsigned)V[c % 20], p < 2 ? "Greater" : "Less");
}

static void quad(int c, int mode, uint32_t z, uint32_t colour)
{
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    int k;
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Opaque;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    isp.DepthMode = mode;
    isp.CullMode = CullMode_None;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    ta_polygon(pcw, isp, tsp, tcw);
    for(k = 0; k < 4; k++)
        ta_vertex0(f32b_int(cell_px(c) + (k & 1) * 32), f32b_int(cell_py(c) + (k >> 1) * 32), z, colour, k == 3);
}

static void test_page(int page)
{
    int c;
    pvr_frame.bg_color = 0x000040;
    ta_list_begin(ListType_Opaque);
    for(c = 0; c < 300; c++) {
        quad(c, DepthMode_Always, V[c / 20 + (page & 1) * 5], 0xff40c040u);
        quad(c, page < 2 ? DepthMode_Greater : DepthMode_Less, V[c % 20], 0xffffffffu);
    }
    ta_list_end();
}
