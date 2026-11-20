/* The order of the overflow register's 32 words, every pair (2026-11-19, hunt depthgaps (a); after zword,
   depthgaps_ties).

   zword: when a slope overflows, every pixel stores a word of the register m = (m0 + sa i + sb j) mod 32 alone,
   read to the 19 bits a background probe has: several m share a bracket (17; 18, 19; 20 .. 22; 24 .. 29; 2 .. 15
   above every probe). depthgaps_ties: m + 1 is above m for m = 17 .. 30 (one of zword's triangles over another
   that starts one step on). Here the whole order: zword's triangle with legs 64 and +inf at its top left corner
   (DepthMode Always, textured green: m1 = -(i + j), the tile covered), then the same with +inf at its top right
   corner (the page's mode, white: m2 = i). Pixel (i, j) of the tile compares the word of m2 = i with the word of
   m1 = -(i + j): every pair once. Cells: the pair at three places (both triangles' constants cancel their starts:
   m0 = 0). Pages: Equal, Greater, Less. */
#define NUM_PAGES 3
#include "pvrtest2.h"
#include "texgen.h"

static const int CX[3] = { 64, 320, 192 }, CY[3] = { 64, 64, 256 };
static const char *MODE[3] = { "Equal", "Greater", "Less" };
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
    int p, k;
    for(p = 0; p < NUM_PAGES; p++)
        for(k = 0; k < 3; k++)
            label(p, (CY[k] / 32) * 20 + CX[k] / 32, "word of m2 = i against m1 = -(i + j), tile at (%d, %d), %s", CX[k], CY[k], MODE[p]);
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

static void vtx(int x, int y, uint32_t z, int end)
{
    ta_vertex3(f32b_int(x), f32b_int(y), z, 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, end);
}

static void test_page(int page)
{
    const uint32_t half = 0x3f000000u, inf = 0x7f800000u;
    int k;
    pvr_frame.bg_color = 0x000040;
    pvr_frame.bg_depth = 0;
    ta_list_begin(ListType_Opaque);
    for(k = 0; k < 3; k++) {
        const int x = CX[k], y = CY[k];
        begin(DepthMode_Always, texG);
        vtx(x, y, inf, 0); vtx(x + 64, y, half, 0); vtx(x, y + 64, half, 1);
        begin(page == 0 ? DepthMode_Equal : page == 1 ? DepthMode_Greater : DepthMode_Less, texW);
        vtx(x, y, half, 0); vtx(x + 64, y, inf, 0); vtx(x, y + 64, half, 1);
    }
    ta_list_end();
}
