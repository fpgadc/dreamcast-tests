/* Trilinear filtering at the smallest mipmap levels and past them (2026-11-19).

   Pass B of trilinear filtering (FilterMode 3) reads the level after the pixel's own, faded in by its fraction k;
   pass A (FilterMode 2) the pixel's own level, faded out. At the smallest level (1x1) there is no level after it,
   and a level computed past the 1x1 (texels much smaller than pixels) has none at all. refsw3 adds one to the level
   for pass B with no bound of its own (refsw_tile.cpp mipChoose: the level is cut to 11 first, so pass B may ask
   for 12) and relies on the texture filter's clamp to the 1x1 level; what a console reads there is not measured
   (pvr-tests/trifade covers levels 0 .. 4 of a 64 x 64 texture, mipmeas the level itself).

   Every cell is a flat quad (1/w = 1 at its four corners, u along x only, v a quarter texel), decal, opaque, on
   0x000040; its level L and fraction f (of 128; k = 2 f, 127: 256) are the model's (gen.cpp, cases.h). Every level
   of a texture is one colour (565, twiddled): 1x1 white, 2x2 yellow, 4x4 cyan, 8x8 magenta, 16x16 red, 32x32 green,
   64x64 blue; the three texels in front of the 1x1 level (the start of a mipmapped texture, unused) are orange
   (0xfc00), azure (0x041f) and violet (0x801f), so a read there shows.
   Columns 0-9: an 8 x 8 texture (TexU 0; its 1x1 level is level 3), 10-19: 64 x 64 (TexU 3; 1x1 is level 6). The
   (L, f) of a column, top the 1x1 level: (top - 1, 64), (top, 0), (top, 64), (top, 127), (top + 1, 0),
   (top + 1, 64), (top + 2, 64), (11, 64), (12, 64), (20, 64).
   Rows:
     0  pass A                      1  pass B
     2  bilinear                    3  point sampled
     4  A then B, the way games draw it: the same quad twice in the translucent list, A (ONE, ZERO), B (ONE, ONE)
     5  pass A, VQ                  6  pass B, VQ (the same texture compressed, 16 bit texels)
     7  pass A, SupSample           8  pass B, SupSample (the level one finer: pvr-tests/supsample2) */
#define NUM_PAGES 1
#include "pvrtest2.h"
#include "texgen.h"
#ifndef CASES_H
#define CASES_H "cases.h"
#endif
#include CASES_H

enum { TEX8, TEX64, VQ8, VQ64, NTEX };
static uint32_t tex[NTEX];

static uint32_t level_colour(const void *ctx, int size, int x, int y)
{
    static const uint16_t c[7] = { 0xffff, 0xffe0, 0x07ff, 0xf81f, 0xf800, 0x07e0, 0x001f };    /* 1x1 .. 64x64 */
    int k = 0;
    (void)ctx; (void)x; (void)y;
    while((1 << k) < size)
        k++;
    return c[k];
}

static void test_setup(void)
{
    /* the texels in front of the 1x1 level, and the 1x1 level */
    static const uint16_t front[4] = { 0xfc00, 0x041f, 0x801f, 0xffff };
    tex_layout_t l = { TEX_MIP, 16, 8, 8, 0 };
    tex[TEX8] = tex_build(&l, level_colour, NULL);
    pvr_tex_load(tex[TEX8], front, sizeof(front));
    l.w = l.h = 64;
    tex[TEX64] = tex_build(&l, level_colour, NULL);
    pvr_tex_load(tex[TEX64], front, sizeof(front));
    l.layout = TEX_VQ_MIP;
    l.w = l.h = 8;
    tex[VQ8] = tex_build(&l, level_colour, NULL);
    l.w = l.h = 64;
    tex[VQ64] = tex_build(&l, level_colour, NULL);
}

static void test_describe(void)
{
    static const char *row[9] = { "pass A", "pass B", "bilinear", "point", "A then B", "pass A, VQ", "pass B, VQ",
                                  "pass A, SupSample", "pass B, SupSample" };
    int c;
    for(c = 0; c < 180; c++) {
        const int t = (c % 20) / 10, col = c % 10;
        label(0, c, "%s, %s: level %d, fraction %d / 128", t ? "64 x 64" : "8 x 8", row[c / 20], LEV[t][col], FRAC[t][col]);
    }
}

static void quad(int c, int list, int filter, int vq, int super, int src, int dst)
{
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    const int t = (c % 20) / 10, col = c % 10, x = cell_px(c), y = cell_py(c);
    const f32b su = SU[t][col], v = t ? 0x3c800000u /* 1/256 */ : 0x3e000000u /* 1/32 */;
    const f32b u1 = su + (5u << 23);                    /* 32 su: the exponent plus 5 */
    int k;
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = list;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Texture = 1;
    isp.DepthMode = list == ListType_Opaque ? DepthMode_Always : DepthMode_GreaterEqual;
    isp.CullMode = CullMode_None;
    isp.Texture = 1;
    tsp.SrcInstr = src;
    tsp.DstInstr = dst;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    tsp.ShadInstr = ShadInstr_Decal;
    tsp.FilterMode = filter;
    tsp.SupSample = super;
    tsp.MipMapD = 4;
    tsp.TexU = t ? 3 : 0;
    tsp.TexV = t ? 3 : 0;
    tcw.PixelFmt = PixelFmt_565;
    tcw.MipMapped = 1;
    tcw.VQ_Comp = vq;
    tcw.TexAddr = tex[(vq ? VQ8 : TEX8) + t] >> 3;
    ta_polygon(pcw, isp, tsp, tcw);
    for(k = 0; k < 4; k++)
        ta_vertex3(f32b_int(x + (k & 1) * 32), f32b_int(y + (k >> 1) * 32), F32B_ONE, (k & 1) ? u1 : 0, v,
                   0xffffffffu, 0, k == 3);
}

static void test_page(int page)
{
    int c;
    (void)page;
    pvr_frame.bg_color = 0x000040;
    ta_list_begin(ListType_Opaque);
    for(c = 0; c < 180; c++) {
        const int row = c / 20;
        static const int filter[9] = { FilterMode_TrilinearA, FilterMode_TrilinearB, FilterMode_Bilinear, FilterMode_Point, 0,
                                       FilterMode_TrilinearA, FilterMode_TrilinearB, FilterMode_TrilinearA, FilterMode_TrilinearB };
        if(row != 4)
            quad(c, ListType_Opaque, filter[row], row == 5 || row == 6, row >= 7, BlendInstr_One, BlendInstr_Zero);
    }
    ta_list_end();
    ta_list_begin(ListType_Translucent);
    for(c = 80; c < 100; c++) {
        quad(c, ListType_Translucent, FilterMode_TrilinearA, 0, 0, BlendInstr_One, BlendInstr_Zero);
        quad(c, ListType_Translucent, FilterMode_TrilinearB, 0, 0, BlendInstr_One, BlendInstr_One);
    }
    ta_list_end();
}
