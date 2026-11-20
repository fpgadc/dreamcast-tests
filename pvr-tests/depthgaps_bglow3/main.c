/* A negative background's low fraction bits: rounded (2026-11-19, hunt depthgaps (c); after depthgaps_bglow2).

   depthgaps_bglow2: a negative background W + L (W with its low 5 fraction bits clear, magnitude 64 and more) ties
   with a polygon at W for L = 0 .. 15 and is below it for L = 16 .. 31; at W = 0xc2ffffe0, 0xfeffffe0, 0xff7fffe0
   (the largest fraction) it ties for every L. So: the magnitude rounded to 19 bits, half up, the fraction not
   carried into the exponent? Here the other side of each: per group a background base B[g] + L, L = 0 .. 31, one
   render each, against up to three polygons whose stored words are known exactly, DepthMode Greater, Less, Equal.
   Polygon k > 0: a flat quad at that word (exact: checked with refsw3's depth unit at these places); k = 1 .. 31
   below 32: the pixel k of zword's triangle (+inf at the top left corner), the word kWrap[-k] (m = 31: 0xff000000,
   m = 30: 0xff7fffe0, as zword measured with backgrounds).
   Results (written at the end, not hashed): cell 20 g + 3 q + mode, column L: green where polygon q was drawn. */
#define NUM_PAGES 1
#define TEST_DRAW_PAGE
#include "pvrtest2.h"
#include "texgen.h"

#define NG 8
static const uint32_t B[NG] = { 0xc2c80000u, 0xc9ffffe0u, 0xc2800000u, 0xc2ffffc0u, 0xff800000u, 0xffffffe0u, 0x42c80000u,
                                0x7f800000u };
static const uint32_t Q[NG][3] = {
    { 0xc2c80000u, 0xc2c80020u, 0xc2c7ffe0u },
    { 0xc9ffffe0u, 0xca000000u, 0xc9ffffc0u },
    { 0xc2800000u, 0xc2800020u, 0 },
    { 0xc2ffffc0u, 0xc2ffffe0u, 0 },
    { 1, 2, 0 },                        /* m = 31, 30 */
    { 1, 2, 0 },
    { 0x42c80000u, 0x42c80020u, 0 },
    { 31, 0, 0 },                       /* m = 1: 2^127 */
};
static const char *MODE[3] = { "Greater", "Less", "Equal" };
static uint32_t texG;

static uint32_t green(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0x07e0; }

static void test_setup(void)
{
    const tex_layout_t l = { TEX_TWIDDLED, 16, 8, 8, 0 };
    texG = tex_build(&l, green, NULL);
}

static void test_describe(void)
{
    int g, q, m;
    for(g = 0; g < NG; g++)
        for(q = 0; q < 3; q++)
            for(m = 0; m < 3; m++)
                if(Q[g][q])
                    label(0, 20 * g + 3 * q + m, "background %08x + column; polygon %s %08x; %s", (unsigned)B[g],
                          Q[g][q] < 32 ? "kWrap of pixel" : "word", (unsigned)Q[g][q], MODE[m]);
}

static void begin(int mode)
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
    tcw.TexAddr = texG >> 3;
    ta_polygon(pcw, isp, tsp, tcw);
}

static void vtx(int x, int y, uint32_t z, int end)
{
    ta_vertex3(f32b_int(x), f32b_int(y), z, 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, end);
}

static void test_page(int page) { (void)page; }

static void test_draw_page(int page)
{
    static uint32_t res[NG][9][32];
    const uint32_t half = 0x3f000000u, inf = 0x7f800000u;
    int g, L, q, m, k, row;
    (void)page;
    for(g = 0; g < NG; g++)
        for(L = 0; L < 32; L++) {
            pvr_frame.bg_color = 0x000040;
            pvr_frame.bg_depth = B[g] + (uint32_t)L;
            pvr_frame_begin();
            ta_list_begin(ListType_Opaque);
            for(q = 0; q < 3; q++)
                for(m = 0; m < 3; m++) {
                    const int x = 64 * (3 * q + m), y = 384;
                    if(!Q[g][q])
                        continue;
                    begin(m == 0 ? DepthMode_Greater : m == 1 ? DepthMode_Less : DepthMode_Equal);
                    if(Q[g][q] < 32) { vtx(x, y, inf, 0); vtx(x + 32, y, half, 0); vtx(x, y + 32, half, 1); }
                    else
                        for(k = 0; k < 4; k++)
                            vtx(x + (k & 1) * 32, y + (k >> 1) * 32, Q[g][q], k == 3);
                }
            ta_list_end();
            pvr_frame_render();
            for(q = 0; q < 3; q++)
                for(m = 0; m < 3; m++) {
                    uint32_t w;
                    const int pix = Q[g][q] < 32 ? (int)Q[g][q] : 0;
                    if(!Q[g][q])
                        continue;
                    pvrio_read32(SETUP_FB + (384u * 640 + (uint32_t)(64 * (3 * q + m) + pix)) * 4, 1, &w);
                    res[g][3 * q + m][L] = (w & 0xffffff) == 0x00ff00 ? 0x00ff00u : 0x000040u;
                }
        }
    pvrio_unhashed = 1;
    for(g = 0; g < NG; g++)
        for(k = 0; k < 9; k++)
            for(row = 0; row < 32; row++)
                pvrio_w32(SETUP_FB + ((uint32_t)(32 * g + row) * 640 + (uint32_t)(32 * k)) * 4, res[g][k], 32);
    pvrio_unhashed = 0;
}
