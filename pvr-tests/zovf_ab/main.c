/* The depth slopes a and b on a grid across 2^128 and 2^129 (2026-11-19, after pvr-tests/zovf_lim; hunt zovf).

   Which slopes wrap (regime C: the register steps by 2^124 a pixel, zword's words) and what a finite slope next to a
   wrapping one does: every pair of a, b from 0, 2^126, 2^127, 1.5 2^127, 2^128 - 2^104, 2^128, 2^128 + 2^105,
   1.5 2^128, 2^129 - 2^105, 2^129, 1.5 2^129, 2^130, 2^132: a triangle with the right angle at the cell's top left
   corner at depth 0, the corner 32 right at a / 32, the one 32 down at b / 32, so that a and b are exactly those (one
   product each); the constant is large. Group 0 (pages 0, 1): b >= 0; group 1 (pages 2, 3): b <= 0. Cell 13 ia + ib.
   Sign maps as zovf_lim's. Triangles and labels: agents/nulldc3/hunt/zovf/zgen.cpp, mode ab (cases.h). */
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
