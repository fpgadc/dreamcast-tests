/* Products against sums at 2^128 in the depth setup (2026-11-19, after pvr-tests/zovf_lim, zovf_ab, zovf_sat; hunt zovf).

   Group 0 (pages 0, 1): the slopes, one at a time: the right-angle corner B at the cell's top left corner, R 32 right,
   D 32 down; a = (t1 + t2) + 0 with t1 B's product and t2 R's (D at B's depth: b = 0), or b = (s1 + 0) + s3 with D the
   partner (R at B's depth: a = 0); the product at 2^128 and the sum below, the products below and the sum at 2^128,
   one product at or just below 2^128, ...; legs 32, so that a wrapping slope (regime C, zovf_ab) shows. Sign maps as
   zovf_lim's (probe quads at 0, page 0 Greater, page 1 Less).
   Group 1 (pages 2, 3): the constant: B's product with a partner's (u1 + u3, the partial sum; + u2, the final one), at
   2^128 and below, in cells where a x0 + b y0 stays below 2^128, read as zovf_sat does: a constant that overflows
   saturates the start to +-2^127 at the tile's first pixel; DepthMode Equal against a background of +2^127 (page 2)
   and -2^127 (page 3). Triangles and labels: agents/nulldc3/hunt/zovf/zgen.cpp, mode ps (cases.h). */
#define NUM_PAGES 4
#include "pvrtest2.h"
#include "texgen.h"
#include "cases.h"

static uint32_t texG, texW;
static uint32_t green(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0x07e0; }
static uint32_t white(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0xffff; }

static void test_setup(void)
{
    const tex_layout_t l = { TEX_TWIDDLED, 16, 8, 8, 0 };
    texG = tex_build(&l, green, NULL);
    texW = tex_build(&l, white, NULL);
}

static void test_describe(void)
{
#ifndef _arch_dreamcast
    int p, i;
    for(p = 0; p < NUM_PAGES; p++)
        for(i = 0; i < NUM_LABELS; i++)
            if(labels[i].group == (p < 2 ? 0 : 1))
                label(p, labels[i].cell, "%s, %s", labels[i].text, p == 0 ? "probe 0 Greater" : p == 1 ? "probe 0 Less" :
                      p == 2 ? "Equal, background +2^127" : "Equal, background -2^127");
#endif
}

static void begin(int mode, uint32_t tex)
{
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Opaque;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Texture = 1;
    isp.DepthMode = mode;
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
    tcw.TexAddr = tex >> 3;
    ta_polygon(pcw, isp, tsp, tcw);
}

static void test_page(int page)
{
    const int g = page < 2 ? 0 : 1;
    int i, c, k;
    pvr_frame.bg_color = 0x000040;
    if(g == 1)
        pvr_frame.bg_depth = page == 2 ? 0x7f000000u : 0xff000000u;
    ta_list_begin(ListType_Opaque);
    for(i = 0; i < NUM_CASES; i++) {
        if(cases[i].group != g)
            continue;
        begin(g == 0 ? DepthMode_Always : DepthMode_Equal, texG);
        for(k = 0; k < 3; k++)
            ta_vertex3(cases[i].v[k][0], cases[i].v[k][1], cases[i].v[k][2], 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, k == 2);
    }
    if(g == 0)
        for(c = 0; c < 300; c++) {
            begin(page == 0 ? DepthMode_Greater : DepthMode_Less, texW);
            for(k = 0; k < 4; k++)
                ta_vertex3(f32b_int(cell_px(c) + (k & 1) * 32), f32b_int(cell_py(c) + (k >> 1) * 32), 0, 0x3e800000u, 0x3e800000u,
                           0xffffffffu, 0, k == 3);
        }
    ta_list_end();
}
