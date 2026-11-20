/* hugepos_sw4: huge triangles' coverage (agents/nulldc3/hunt/hugepos/mktest.py from a table of corners).
   One triangle a page (cases.h: x0 y0 x1 y1 x2 y2, the bits of the floats), textured with an 8x8 texture that is
   white everywhere, ShadInstr decal, point sampled: every pixel it covers is white, whatever its TSP planes are
   (garbage at huge determinants). Opaque, depth compare always, no culling, background black. */
#define NUM_PAGES 24
#include "pvrtest2.h"
#include "texgen.h"
#include "cases.h"

static uint32_t tex;

static uint32_t white(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0xffffu; }

static void test_setup(void) {
    tex_layout_t l = { TEX_RASTER, 16, 8, 8, 0 };
    tex = tex_build(&l, white, NULL);
}

static void test_describe(void) {
    int p;
    for(p = 0; p < NUM_PAGES; p++)
        label_span(p, 0, COLS, 15, "%s", L[p]);
}

static void test_page(int page) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    int k;

    ta_list_begin(ListType_Opaque);
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Opaque;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Texture = 1;
    isp.DepthMode = DepthMode_Always;
    isp.CullMode = CullMode_None;
    isp.ZWriteDis = 1;
    isp.Texture = 1;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    tsp.ShadInstr = ShadInstr_Decal;
    tsp.FilterMode = FilterMode_Point;
    tsp.MipMapD = 4;
    tcw.PixelFmt = PixelFmt_565;
    tcw.ScanOrder = 1;
    tcw.TexAddr = tex >> 3;
    ta_polygon(pcw, isp, tsp, tcw);
    for(k = 0; k < 3; k++)
        ta_vertex3(V[page][2 * k], V[page][2 * k + 1], F32B_ONE, 0, 0, 0xffffffffu, 0, k == 2);
    ta_list_end();
}
