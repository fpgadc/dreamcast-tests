/* FPU_PERP_VAL (0x084, "IEEE floating point value for perpendicular triangle compare"), TODO A2 (2026-11-19). 22
   polly2-data dumps set 1/128 (0x3c000000), every test and the other dumps 0; what is compared, and what happens to
   a triangle on the other side, is not known. This test does not set the register: run it as perpval+084=<value>
   next to the plain run (run2.sh) and compare the console with itself.
   Each cell: a Gouraud triangle (red, green, blue corners), depth (1/w) 1 at its first corner and rising with a
   slope of 2^e a pixel, drawn with DepthMode Always; then a white quad over the cell at depth 1.5, GreaterEqual: white
   where the triangle's depth is at most 1.5, the triangle's colour where it is above, the background where it is
   not drawn. So a triangle culled, flattened or clamped as it turns edge-on shows. Columns: e = -10 .. 18.5 in steps
   of 1.5; rows:
     0 .. 4    triangles of legs 28, 14, 7, 3, 1.5 pixels
     5 .. 9    slivers 28 pixels long, 1, 1/2, 1/4, 1/8, 1/16 pixel wide
     10 .. 14  legs 28 with the slope along y, along the diagonal, against x (depth falling: 1 at the far corner),
               and two with the first corner's depth 2^20 and 2^-20
   Pages: 0 as above; 1 the probe at depth 1 + 2^(e - 1) * 14 (the middle of the triangle's range); 2 CullMode small;
   3 the triangle in the translucent list (autosort) and the probe opaque. */
#define NUM_PAGES 4
#include <math.h>
#include "pvrtest2.h"

static void test_setup(void) { }

/* 2^(-10 + 1.5 col) */
static double slope(int col) { return ldexp((col & 1) ? 1.4142135623730951 : 1.0, -10 + (3 * col) / 2); }

static void test_describe(void)
{
    static const char *row[15] = { "legs 28", "legs 14", "legs 7", "legs 3", "legs 1.5", "sliver 1", "sliver 1/2",
                                   "sliver 1/4", "sliver 1/8", "sliver 1/16", "slope along y", "slope diagonal",
                                   "depth falling", "first corner 2^20", "first corner 2^-20" };
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 300; c++)
            label(p, c, "%s, slope 2^%.1f a pixel", row[c / 20], -10 + 1.5 * (c % 20));
}

static void tri(int list, int cull, double x0, double y0, double z0, double x1, double y1, double z1, double x2, double y2, double z2)
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
    isp.CullMode = cull;
    isp.Gouraud = 1;
    tsp.SrcInstr = list == ListType_Translucent ? BlendInstr_SrcAlpha : BlendInstr_One;
    tsp.DstInstr = list == ListType_Translucent ? BlendInstr_InvSrcAlpha : BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = list == ListType_Translucent;
    ta_polygon(pcw, isp, tsp, tcw);
    ta_vertex0(F((float)x0), F((float)y0), F((float)z0), 0xffff2020u, 0);
    ta_vertex0(F((float)x1), F((float)y1), F((float)z1), 0xff20ff20u, 0);
    ta_vertex0(F((float)x2), F((float)y2), F((float)z2), 0xff2020ffu, 1);
}

static void probe(int x, int y, float z)
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
        ta_vertex0(f32b_int(x + (k & 1) * 32), f32b_int(y + (k >> 1) * 32), F(z), 0xffffffffu, k == 3);
}

static void cell_tri(int page, int c, int list)
{
    const int x = cell_px(c), y = cell_py(c), row = c / 20;
    const double s = slope(c % 20);                                     /* depth slope a pixel */
    const int cull = page == 2 ? CullMode_Small : CullMode_None;
    double L = 28, w = 28;
    if(row < 5) L = w = (const double[]){ 28, 14, 7, 3, 1.5 }[row];
    else if(row < 10) w = ldexp(1.0, -(row - 5));
    {
        const double X = x + 2, Y = y + 2;
        double z0 = 1, dzx = s, dzy = 0;
        if(row == 10) { dzx = 0; dzy = s; }
        if(row == 11) { dzx = s / 2; dzy = s / 2; }
        if(row == 12) { z0 = 1 + s * L; dzx = -s; }
        if(row == 13) z0 = ldexp(1.0, 20);
        if(row == 14) z0 = ldexp(1.0, -20);
        if(row >= 5 && row < 10)        /* a sliver along x, w high */
            tri(list, cull, X, Y, z0, X + L, Y, z0 + dzx * L, X, Y + w, z0 + dzy * w);
        else
            tri(list, cull, X, Y, z0, X + L, Y, z0 + dzx * L, X, Y + w, z0 + dzy * w);
    }
}

static void test_page(int page)
{
    int c;
    pvr_frame.bg_color = 0x202020;
    if(page != 3) {
        ta_list_begin(ListType_Opaque);
        for(c = 0; c < 300; c++)
            cell_tri(page, c, ListType_Opaque);
        for(c = 0; c < 300; c++) {
            const double s = slope(c % 20);
            probe(cell_px(c), cell_py(c), page == 1 ? (float)(1 + s * 14) : 1.5f);
        }
        ta_list_end();
    } else {
        ta_list_begin(ListType_Opaque);
        for(c = 0; c < 300; c++)
            probe(cell_px(c), cell_py(c), 1.5f);
        ta_list_end();
        ta_list_begin(ListType_Translucent);
        for(c = 0; c < 300; c++)
            cell_tri(page, c, ListType_Translucent);
        ta_list_end();
    }
}
