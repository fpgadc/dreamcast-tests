/* The setup's zero rule in the depth unit's edges and determinant (2026-11-20, hunt depth2; after depth2_zcoord).

   pvr_isp_depth.h: a zero added to a number of 2^-126 .. 1.5 2^-126 of the other sign gives +0 in every addition of
   the depth setup; measured for the sums of its products (depthgaps_zadd, _field1) and the coordinate differences
   (depth2_zcoord). GAP: the edges x_j y_k - x_k y_j (the constant's factors) and the determinant's sums. Here 13
   triangles near the origin (gen.cpp, coordinates among +-0, +-(1 .. 1.5) 2^-126 and other tiny ones, +-0.25 .. 32;
   depths with infinities) where refsw3 stores other words at a quarter of the pixels or more without the rule there
   (an edge of +0 against one of +-(1 .. 1.5) 2^-126: an infinite depth times it is 0 or an infinity in the constant;
   160 pixels, every one of them). One triangle a page (textured white,
   decal) against the background: probe 0 Equal, 0 Greater, 2^127 Equal, -2^127 Equal, the largest float Greater;
   page = 5 case + probe. A 14th triangle (x -1.25 2^-126, 1.5 2^-126, 32, y 0.75, -0, -(1.5 - 2^-23) 2^-126, z +inf,
   0.5, -inf; the generator's next) drew against the probes 0 Equal and 0 Greater and then hung the console against
   2^127 Equal (the render did not end; 2026-11-20 08:45): left out. */
#define NUM_PAGES 65
#include "pvrtest2.h"
#include "texgen.h"
#include "cases.h"

static const uint32_t PROBE[5] = { 0, 0, 0x7f000000u, 0xff000000u, 0x7f7fffffu };
static const int PMODE[5] = { DepthMode_Equal, DepthMode_Greater, DepthMode_Equal, DepthMode_Equal, DepthMode_Greater };
static const char *PNAME[5] = { "0 Equal", "0 Greater", "2^127 Equal", "-2^127 Equal", "the largest float Greater" };
static uint32_t texW;

static uint32_t white(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0xffff; }

static void test_setup(void)
{
    const tex_layout_t l = { TEX_TWIDDLED, 16, 8, 8, 0 };
    texW = tex_build(&l, white, NULL);
}

static void test_describe(void)
{
    int p;
    for(p = 0; p < NUM_PAGES; p++) {
        const uint32_t *c = CASES[p / 5];
        label_span(p, 0, 2, 2, "x %08x %08x %08x, y %08x %08x %08x, z %08x %08x %08x against the background %s", c[0], c[1], c[2], c[3], c[4], c[5],
                   c[6], c[7], c[8], PNAME[p % 5]);
    }
}

static void test_page(int page)
{
    const uint32_t *c = CASES[page / 5];
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    int k;
    pvr_frame.bg_color = 0x000040;
    pvr_frame.bg_depth = PROBE[page % 5];
    ta_list_begin(ListType_Opaque);
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Opaque;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Texture = 1;
    isp.DepthMode = PMODE[page % 5];
    isp.CullMode = CullMode_None;
    isp.Texture = 1;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    tsp.ShadInstr = ShadInstr_Decal;
    tsp.FilterMode = FilterMode_Point;
    tsp.MipMapD = 4;
    tcw.PixelFmt = PixelFmt_565;
    tcw.TexAddr = texW >> 3;
    ta_polygon(pcw, isp, tsp, tcw);
    for(k = 0; k < 3; k++)
        ta_vertex3(c[k], c[3 + k], c[6 + k], 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, k == 2);
    ta_list_end();
}
