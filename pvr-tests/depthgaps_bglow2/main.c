/* A negative background's low fraction bits, every one (2026-11-19, hunt depthgaps (c); after depthgaps_bglow).

   depthgaps_bglow: a negative background W + 16 and W + 31 is not cut to W (a polygon at W is above it, Greater),
   W + 1 is. Here every L = 0 .. 31: one render per background B[b] + L, a polygon whose stored word is B[b] exactly
   drawn three times, DepthMode Greater, Less, Equal (textured green, decal), the three pixels read back.
     b 0 .. 4  flat quads at -64, -100, -127.99976 (0xc2ffffe0), -2^20, -2^100 (exact: few bits, a power of two
               determinant)
     b 5, 6, 8 zword's triangle (+inf at the top left corner, legs 32), the pixel of m = 23 (0xfeffffe0), 30
               (0xff7fffe0), 31 (0xff000000)
     b 7       a flat quad at +100 (a positive background is cut to 19 bits: bgdepth)
   The renders do not depend on what is read back; the results are written into the framebuffer at the end (not
   hashed): cell b * 20 + mode (mode 0 Greater, 1 Less, 2 Equal), column L: green where the polygon was drawn. */
#define NUM_PAGES 1
#define TEST_DRAW_PAGE
#include "pvrtest2.h"
#include "texgen.h"

#define NB 9
static const uint32_t B[NB] = { 0xc2800000u, 0xc2c80000u, 0xc2ffffe0u, 0xc9800000u, 0xf1800000u, 0xfeffffe0u, 0xff7fffe0u,
                                0x42c80000u, 0xff000000u };
static const int PIX[NB] = { 0, 0, 0, 0, 0, 9, 2, 0, 1 };      /* the pixel read in the top row of the polygon's tile */
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
    int b, m;
    for(b = 0; b < NB; b++)
        for(m = 0; m < 3; m++)
            label(0, b * 20 + m, "polygon word %08x, background %08x + column, %s", (unsigned)B[b], (unsigned)B[b], MODE[m]);
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
    static uint32_t res[NB * 3][32];
    const uint32_t half = 0x3f000000u, inf = 0x7f800000u;
    int b, L, m, k, row;
    (void)page;
    for(b = 0; b < NB; b++)
        for(L = 0; L < 32; L++) {
            pvr_frame.bg_color = 0x000040;
            pvr_frame.bg_depth = B[b] + (uint32_t)L;
            pvr_frame_begin();
            ta_list_begin(ListType_Opaque);
            for(m = 0; m < 3; m++) {
                const int x = 64 * m, y = 384;
                begin(m == 0 ? DepthMode_Greater : m == 1 ? DepthMode_Less : DepthMode_Equal);
                if(b == 5 || b == 6 || b == 8) { vtx(x, y, inf, 0); vtx(x + 32, y, half, 0); vtx(x, y + 32, half, 1); }
                else
                    for(k = 0; k < 4; k++)
                        vtx(x + (k & 1) * 32, y + (k >> 1) * 32, B[b], k == 3);
            }
            ta_list_end();
            pvr_frame_render();
            for(m = 0; m < 3; m++) {
                uint32_t w;
                pvrio_read32(SETUP_FB + (384u * 640 + (uint32_t)(64 * m + PIX[b])) * 4, 1, &w);
                res[b * 3 + m][L] = (w & 0xffffff) == 0x00ff00 ? 0x00ff00u : 0x000040u;
            }
        }
    pvrio_unhashed = 1;
    for(b = 0; b < NB; b++)
        for(m = 0; m < 3; m++)
            for(row = 0; row < 32; row++)
                pvrio_w32(SETUP_FB + ((uint32_t)(32 * b + row) * 640 + (uint32_t)(32 * m)) * 4, res[b * 3 + m], 32);
    pvrio_unhashed = 0;
}
