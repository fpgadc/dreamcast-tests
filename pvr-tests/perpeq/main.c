/* FPU_PERP_VAL (0x084) against a determinant of exactly its size, and one ulp either side (2026-11-19).

   pvr-tests/perpval: a triangle whose determinant is at most FPU_PERP_VAL in size is taken as one of determinant
   zero (depth: the mean of the corners; colours flat at the third corner); whether a determinant equal to it counts
   was not measured (refsw3 takes |det| <= value). Here every triangle has an exact determinant (right triangles on a
   grid of 1/8, det = a b), and the pages put the register at, just below and just above some of them.
     rows 0, 1  Gouraud red / green / blue, 1/w 1 / 2 / 1.25 at the corners, winding A, B: the colour shows
     rows 2, 3  the same, then a white quad over the cell at depth 1.5, GreaterEqual: white where the triangle's
                depth is at most 1.5 (the mean of the corners, 1.4167, is), the triangle's colour where it is above
     rows 4, 5  rows 0, 1 in the translucent list (SRC_ALPHA, INV_SRC_ALPHA, alpha 255)
   Columns: a b = 2^k (1 or 1.5), k = col / 2 - 2: 1/4, 3/8, 1/2, 3/4, ... 128, 192.
   Pages: FPU_PERP_VAL = VAL[]: 1, 2, 12, 3/8, 64, each exactly and one ulp below and above. */
#define NUM_PAGES 15
#include "pvrtest2.h"

static const uint32_t VAL[NUM_PAGES] = {
    0x3f800000u, 0x3f7fffffu, 0x3f800001u,      /* 1 */
    0x40000000u, 0x3fffffffu, 0x40000001u,      /* 2 */
    0x41400000u, 0x413fffffu, 0x41400001u,      /* 12 */
    0x3ec00000u, 0x3ebfffffu, 0x3ec00001u,      /* 3/8 */
    0x42800000u, 0x427fffffu, 0x42800001u,      /* 64 */
};

static void legs(int col, int *a8, int *b8)
{
    const int k = col / 2 - 2, lo = k >= 0 ? k / 2 : -((-k + 1) / 2), hi = k - lo;
    *a8 = (col & 1) ? 12 << (lo + 3) >> 3 : 8 << (lo + 3) >> 3;
    *b8 = 8 << (hi + 3) >> 3;
}

static void test_setup(void) { }

static void test_describe(void)
{
    static const char *row[6] = { "Gouraud, A", "Gouraud, B", "depth probe 1.5, A", "depth probe 1.5, B",
                                  "translucent, A", "translucent, B" };
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 120; c++) {
            int a8, b8;
            legs(c % 20, &a8, &b8);
            label(p, c, "PERP_VAL %08x, %s: a %d/8, b %d/8 (det %d/64)", (unsigned)VAL[p], row[c / 20], a8, b8, a8 * b8);
        }
}

static void tri(int list, f32b x0, f32b y0, f32b z0, uint32_t c0, f32b x1, f32b y1, f32b z1, uint32_t c1,
                f32b x2, f32b y2, f32b z2, uint32_t c2)
{
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = list;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Gouraud = 1;
    isp.DepthMode = DepthMode_Always;
    isp.CullMode = CullMode_None;
    isp.Gouraud = 1;
    tsp.SrcInstr = list == ListType_Translucent ? BlendInstr_SrcAlpha : BlendInstr_One;
    tsp.DstInstr = list == ListType_Translucent ? BlendInstr_InvSrcAlpha : BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = list == ListType_Translucent;
    ta_polygon(pcw, isp, tsp, tcw);
    ta_vertex0(x0, y0, z0, c0, 0);
    ta_vertex0(x1, y1, z1, c1, 0);
    ta_vertex0(x2, y2, z2, c2, 1);
}

static void cell(int c, int list)
{
    const int row = c / 20, x = cell_px(c), y = cell_py(c);
    int a8, b8;
    legs(c % 20, &a8, &b8);
    {
        const f32b X0 = f32b_scaled(8 * (x + 4) + 3, -3), Y0 = f32b_scaled(8 * (y + 4) + 3, -3);
        const f32b X1 = f32b_scaled(8 * (x + 4) + 3 + a8, -3), Y2 = f32b_scaled(8 * (y + 4) + 3 + b8, -3);
        const f32b z0 = F32B_ONE, z1 = 0x40000000u /* 2 */, z2 = 0x3fa00000u /* 1.25 */;
        if(row & 1)
            tri(list, X0, Y0, z0, 0xffff2020u, X0, Y2, z2, 0xff2020ffu, X1, Y0, z1, 0xff20ff20u);
        else
            tri(list, X0, Y0, z0, 0xffff2020u, X1, Y0, z1, 0xff20ff20u, X0, Y2, z2, 0xff2020ffu);
    }
}

static void probe(int c)
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
    isp.DepthMode = DepthMode_GreaterEqual;
    isp.CullMode = CullMode_None;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    ta_polygon(pcw, isp, tsp, tcw);
    for(k = 0; k < 4; k++)
        ta_vertex0(f32b_int(cell_px(c) + (k & 1) * 32), f32b_int(cell_py(c) + (k >> 1) * 32), 0x3fc00000u /* 1.5 */,
                   0xffffffffu, k == 3);
}

static void test_page(int page)
{
    int c;
    pvr_frame.bg_color = 0x202020;
    pvrio_reg(FPU_PERP_VAL_addr, VAL[page]);
    ta_list_begin(ListType_Opaque);
    for(c = 0; c < 80; c++)
        cell(c, ListType_Opaque);
    for(c = 40; c < 80; c++)
        probe(c);
    ta_list_end();
    ta_list_begin(ListType_Translucent);
    for(c = 80; c < 120; c++)
        cell(c, ListType_Translucent);
    ta_list_end();
}
