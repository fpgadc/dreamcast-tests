/* A depth slope of 2^128 in other company (2026-11-19, after pvr-tests/zovf_lim, zovf_ab; hunt zovf).

   zovf_ab: a slope of 2^128 made of one product (the corner R 32 right at 2^123, the right-angle corner at 0) wraps
   (regime C) whatever b. Here 16 variants around it, each at a cell near the top (rows 0, 2) and further down (rows
   7, 9), and with every depth negated (rows 2, 9): zero or tiny other terms in the sums, the big corner at the right
   angle or not, legs 16 or 32, b 0, 2^127 or 2^128; W1 at the screen's origin (the constant 0). Sign maps as
   zovf_lim's (page 0 Greater, page 1 Less against a probe at 0). Triangles and labels:
   agents/nulldc3/hunt/zovf/zgen.cpp, mode wrap (cases.h). */
#define NUM_PAGES 2
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
            if(labels[i].group == p / 2)
                label(p, labels[i].cell, "%s, probe 0 %s", labels[i].text, p % 2 == 0 ? "Greater" : "Less");
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
    const int g = page / 2;
    int i, c, k;
    pvr_frame.bg_color = 0x000040;
    ta_list_begin(ListType_Opaque);
    for(i = 0; i < NUM_CASES; i++) {
        if(cases[i].group != g)
            continue;
        begin(DepthMode_Always, texG);
        for(k = 0; k < 3; k++)
            ta_vertex3(cases[i].v[k][0], cases[i].v[k][1], cases[i].v[k][2], 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, k == 2);
    }
    for(c = 0; c < 300; c++) {
        begin(page % 2 == 0 ? DepthMode_Greater : DepthMode_Less, texW);
        for(k = 0; k < 4; k++)
            ta_vertex3(f32b_int(cell_px(c) + (k & 1) * 32), f32b_int(cell_py(c) + (k >> 1) * 32), 0, 0x3e800000u, 0x3e800000u,
                       0xffffffffu, 0, k == 3);
    }
    ta_list_end();
}
