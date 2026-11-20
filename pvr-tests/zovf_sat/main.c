/* What a tile stores when its start is an infinity on one side only (2026-11-19, after pvr-tests/zovf_lim, zovf_c,
   zovf_far; hunt zovf).

   The depth setup's numbers are infinities from 2^128 (zovf_lim, zovf_c, zovf_far, zovf_ab): when only one of the start's
   two terms, a x0 + b y0 and c, is one (zovf's six cells with c exactly 2^128 among them), the sign maps show the
   infinity's sign over the whole tile. Here what that is: every triangle green (textured, decal) against the
   background at +2^127 (group 0: page 0 DepthMode Equal, page 1 Greater) and at -2^127 (group 1: page 2 Equal, page 3
   Less), the background being stored as itself (pvr-tests/bgbig). Cells: zovf's triangles at their own cells (34, 45,
   60: the six cells; 35, 46, 61: regime B; 33, 44, 20, 30: below) and negated 8 rows down; the constant alone at
   (1 + 2^-k) 2^128 (cells 100-103, negated 120-123). Triangles and labels: agents/nulldc3/hunt/zovf/zgen.cpp, mode
   sat (cases.h). */
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
                label(p, labels[i].cell, "%s, background %s, %s", labels[i].text, p < 2 ? "+2^127" : "-2^127",
                      p % 2 == 0 ? "Equal" : p == 1 ? "Greater" : "Less");
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
    static const uint32_t BG[2] = { 0x7f000000u, 0xff000000u };
    static const int MODE[4] = { DepthMode_Equal, DepthMode_Greater, DepthMode_Equal, DepthMode_Less };
    int i, k;
    pvr_frame.bg_color = 0x000040;
    pvr_frame.bg_depth = BG[page / 2];
    ta_list_begin(ListType_Opaque);
    for(i = 0; i < NUM_CASES; i++) {
        if(cases[i].group != page / 2)
            continue;
        begin(MODE[page], texG);
        for(k = 0; k < 3; k++)
            ta_vertex3(cases[i].v[k][0], cases[i].v[k][1], cases[i].v[k][2], 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, k == 2);
    }
    ta_list_end();
}
