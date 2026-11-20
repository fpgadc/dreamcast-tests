/* The first pixel's numbers far past 2^128 (2026-11-19, after pvr-tests/zovf_lim; hunt zovf).

   zovf_lim's big triangles (0, 0), (2048, 0), (0, 2048), every tile a reading of its own a x0 + b y0, with the slopes
   2, 4 and 8 times as large: F1 a x0 + b y0 up to about 16 2^128 (c = -0.6 2^128), F2 a x0 alone (b < 0), F3 the sum
   with c (c = 0.6 2^128); the 4x ones also with every depth negated. Sign maps as zovf_lim's. Triangles and labels:
   agents/nulldc3/hunt/zovf/zgen.cpp, mode far (cases.h). */
#define NUM_PAGES 20
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
