/* The setup's zero rule in the depth unit's coordinate differences (2026-11-20, hunt depth2; after depthgaps_zadd,
   depthgaps_field1).

   pvr_isp_depth.h: a zero added to a number of 2^-126 .. 1.5 2^-126 of the other sign gives +0 in every addition of
   the depth setup; measured for the sums of its products (depthgaps_zadd, _field1). GAP: the coordinate differences
   (y1 - y2 .., x2 - x1 ..) go through the same adder. Here 16 triangles near the origin (gen.cpp) with one
   coordinate of two corners among +-0 and +-(1 .. 1.5) 2^-126, where such a difference meets the rule and multiplies
   a depth of 2^126 or more or an infinity: with the rule the product is a zero (or infinity times zero, 0), without
   it a number (or an infinity), and refsw3 stores other words at every pixel without it (2,599 pixels: 0 against
   small depths, 2^127 against -2^127 or a word above the largest float, ..). One triangle a page (textured white,
   decal) against the background: probe 0 Equal, 0 Greater, 2^127 Equal, -2^127 Equal, the largest float Greater;
   page = 5 case + probe. */
#define NUM_PAGES 80
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
