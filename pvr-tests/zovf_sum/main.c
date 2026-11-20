/* The depth setup's last two operations on their own (2026-11-19, hunt zovf round 2): the tile's start is
   ((a x0 + b y0) + c) * inv, and the sum (a x0 + b y0) + c, or its product with inv, reaches 2^128 while every
   number before them stays below it. pvr-tests/zovf_sat / zovf_word: when c or a x0 + b y0 is 2^128 or more the start
   saturates to +-2^127 at the tile's first pixel. Is that the rule of the last sum and of the last product too, or do
   they hold the true start (~2^106 on the big triangles here)?

   Pages 0 - 15: one triangle over the whole screen ((0, 0), (2048, 0), (0, 2048): c = z0 2^22, a = 2048 (z1 - z0),
   b = 2048 (z2 - z0), det 2^22), every tile a reading; variant v = page / 2 (4 .. 7: 0 .. 3 negated), page even:
   DepthMode Equal against a background of +-2^127 (the first pixel of a saturated tile), odd: Greater than +2^126
   (Less than -2^126 for the negated ones).
     V1  a = 0: pre = (a x0 + b y0) + c exactly 2^128 in row 7, past it in rows 8 - 14
     V2  a = 0: pre = 2^128 - 2^104 in row 7, past 2^128 in rows 8 - 14
     V3  a = -b / 10: b y0 + c exactly 2^128 in row 7; pre crosses 2^128 near column 10 of row 8 (the order
         (b y0 + c) + a x0 would overflow every tile of rows 7 - 14)
     V4  b = -a / 7: a x0 + c exactly 2^128 in column 10; pre crosses 2^128 near row 7
   Pages 16 - 21: slivers (det 2^-11; tile 0's 2^-2) in row 0 (the pixel right of the tile's first) and column 0 (the
   pixel below it), pre ~2^117 and every number before it far below 2^128, the start pre * inv from 2^128 + 2^118 to
   2^128 + 2^125, controls from 2^128 - 2^118 to 2^128 - 2^122, the plane stepping toward zero by 2^120 .. 2^127 a
   pixel. Page 16 Greater than +2^127, 17 Less than +2^127, 18 Greater than 0; 19 - 21 the negated slivers, Less than
   -2^127, Greater than -2^127, Less than 0. Triangles and labels: agents/nulldc3/hunt/zovf/zgen.cpp (zgen3.h), mode
   sum (cases.h). */
#define NUM_PAGES 22
#include "pvrtest2.h"
#include "texgen.h"
#include "cases.h"

static uint32_t texG;
static uint32_t green(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0x07e0; }

static int page_group(int p) { return p < 16 ? p / 2 : p < 19 ? 8 : 9; }
static void page_mode(int p, int *mode, uint32_t *bg)
{
    if(p < 16) {
        const int neg = p / 2 >= 4;
        if(p % 2 == 0) { *mode = DepthMode_Equal; *bg = neg ? 0xff000000u : 0x7f000000u; }
        else { *mode = neg ? DepthMode_Less : DepthMode_Greater; *bg = neg ? 0xfe800000u : 0x7e800000u; }
        return;
    }
    switch(p) {
    case 16: *mode = DepthMode_Greater; *bg = 0x7f000000u; break;
    case 17: *mode = DepthMode_Less; *bg = 0x7f000000u; break;
    case 18: *mode = DepthMode_Greater; *bg = 0; break;
    case 19: *mode = DepthMode_Less; *bg = 0xff000000u; break;
    case 20: *mode = DepthMode_Greater; *bg = 0xff000000u; break;
    default: *mode = DepthMode_Less; *bg = 0; break;
    }
}

static void test_setup(void)
{
    const tex_layout_t l = { TEX_TWIDDLED, 16, 8, 8, 0 };
    texG = tex_build(&l, green, NULL);
}

static void test_describe(void)
{
#ifndef _arch_dreamcast
    static const char *mname[8] = { "Never", "Less", "Equal", "LEqual", "Greater", "NotEqual", "GEqual", "Always" };
    int p, i;
    for(p = 0; p < NUM_PAGES; p++) {
        int mode; uint32_t bg;
        page_mode(p, &mode, &bg);
        for(i = 0; i < NUM_LABELS; i++)
            if(labels[i].group == page_group(p))
                label(p, labels[i].cell, "%s; %s against a background of %08x", labels[i].text, mname[mode & 7], bg);
    }
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
    int i, k, mode;
    uint32_t bg;
    page_mode(page, &mode, &bg);
    pvr_frame.bg_color = 0x000040;
    pvr_frame.bg_depth = bg;
    ta_list_begin(ListType_Opaque);
    for(i = 0; i < NUM_CASES; i++) {
        if(cases[i].group != page_group(page))
            continue;
        begin(mode, texG);
        for(k = 0; k < 3; k++)
            ta_vertex3(cases[i].v[k][0], cases[i].v[k][1], cases[i].v[k][2], 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, k == 2);
    }
    ta_list_end();
}
