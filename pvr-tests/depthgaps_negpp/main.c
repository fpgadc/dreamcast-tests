/* Flat polygons at negative depths one step of the stored depth's 19 bits apart, against each other
   (2026-11-19, hunt depthgaps (c); after depthgaps_bglow3).

   depthgaps_bglow3: a flat polygon at 0xc2c7ffe0 ties with the same backgrounds (0xc2c80000 .. 0f) as one at
   0xc2c80000; 0xc9ffffc0 with those of 0xc9ffffe0; 0xc2ffffc0 and 0xc2ffffe0 with 0xc2ffffc0 .. ff. So: do two such
   polygons store the same word? Each cell: a flat quad at A (DepthMode Always, textured green), then one at B over it
   (Greater, Less or Equal: columns 0, 1, 2 of each block; textured white). White where B passes.
   Rows: A = W[row]; the blocks of columns (0 - 2, 3 - 5, ...): B = A + 32 d, d = -2, -1, 0, 1, 2 (the word's
   steps of the 19 bit grid; for a negative depth + 32 is a larger magnitude). Page 0 at x = 0 .. 480, page 1 the
   same pairs with B first (Always) and A over it. */
#define NUM_PAGES 2
#include "pvrtest2.h"
#include "texgen.h"

#define NW 12
static const uint32_t W[NW] = { 0xc2c80000u, 0xc2c7ffe0u, 0xc2800000u, 0xc2ffffc0u, 0xc9ffffc0u, 0xbf800000u, 0xbf8000a0u,
                                0xc2c80060u, 0xf1800040u, 0x42c80000u, 0x3f8000a0u, 0xc2a00000u };
static const char *MODE[3] = { "Greater", "Less", "Equal" };
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
    int p, r, d, m;
    for(p = 0; p < NUM_PAGES; p++)
        for(r = 0; r < NW; r++)
            for(d = 0; d < 5; d++)
                for(m = 0; m < 3; m++) {
                    const uint32_t a = W[r], b = W[r] + (uint32_t)(32 * (d - 2));
                    label(p, r * 20 + d * 3 + m, "first %08x (Always), then %08x %s", (unsigned)(p ? b : a), (unsigned)(p ? a : b), MODE[m]);
                }
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

static void quad(int x, int y, uint32_t z)
{
    int k;
    for(k = 0; k < 4; k++)
        ta_vertex3(f32b_int(x + (k & 1) * 32), f32b_int(y + (k >> 1) * 32), z, 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, k == 3);
}

static void test_page(int page)
{
    int r, d, m;
    pvr_frame.bg_color = 0x000040;
    pvr_frame.bg_depth = 0;
    ta_list_begin(ListType_Opaque);
    for(r = 0; r < NW; r++)
        for(d = 0; d < 5; d++)
            for(m = 0; m < 3; m++) {
                const int c = r * 20 + d * 3 + m;
                const uint32_t a = W[r], b = W[r] + (uint32_t)(32 * (d - 2));
                begin(DepthMode_Always, texG);
                quad(cell_px(c), cell_py(c), page ? b : a);
                begin(m == 0 ? DepthMode_Greater : m == 1 ? DepthMode_Less : DepthMode_Equal, texW);
                quad(cell_px(c), cell_py(c), page ? a : b);
            }
    ta_list_end();
}
