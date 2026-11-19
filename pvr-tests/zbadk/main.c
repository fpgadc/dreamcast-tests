/* The depths of a triangle with an overflowing corner, to 1/32 of a binade (2026-11-19, after pvr-tests/zbadbg).

   zbadbg: a 32 x 32 right triangle with the corner at its right angle (TL) at +inf, +-0.5 elsewhere, has depths
   -K (17 - s) along the diagonal (s = x + y + 1, pixel centers from the tile's first pixel), K a little above 2^123,
   as far as binades tell; the other side is beyond the largest float. Here the negative side against backgrounds
   of -2^e (1 + j / 32), e = 122 .. 127, j = 0 .. 31 (a negative background of 64 and more is stored as itself:
   pvr-tests/bgneg). Every cell the triangle (textured green, decal), DepthMode Greater: green where its depth is
   above the background in the ISP's order (pvr-tests/zsign).
   Rows (every column the same, at its own place on the screen): TL at +inf, -inf, NaN, 2^127, the largest float;
   the other two corners at +0.5. Page p: e = 122 + p / 32, j = p % 32. */
#define NUM_PAGES 192
#include "pvrtest2.h"
#include "texgen.h"

static const uint32_t S[5] = { 0x7f800000u, 0xff800000u, 0x7fc00000u, 0x7f000000u, 0x7f7fffffu };
static uint32_t texG;

static uint32_t bg(int page) { return 0x80000000u | (uint32_t)(122 + 127 + page / 32) << 23 | (uint32_t)(page % 32) << 18; }

static uint32_t green(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0x07e0; }

static void test_setup(void)
{
    const tex_layout_t l = { TEX_TWIDDLED, 16, 8, 8, 0 };
    texG = tex_build(&l, green, NULL);
}

static void test_describe(void)
{
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 100; c++)
            label(p, c, "TL at %08x, others 0.5, background %08x", (unsigned)S[c / 20], (unsigned)bg(p));
}

static void vtx(int x, int y, uint32_t z, int end)
{
    ta_vertex3(f32b_int(x), f32b_int(y), z, 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, end);
}

static void test_page(int page)
{
    int c;
    pvr_frame.bg_color = 0x000040;
    pvr_frame.bg_depth = bg(page);
    ta_list_begin(ListType_Opaque);
    for(c = 0; c < 100; c++) {
        PCW pcw = { .full = 0 };
        ISP_TSP isp = { .full = 0 };
        TSP tsp = { .full = 0 };
        TCW tcw = { .full = 0 };
        const int x = cell_px(c), y = cell_py(c);
        pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
        pcw.ListType = ListType_Opaque;
        pcw.Group_En = 1;
        pcw.Strip_Len = 1;
        pcw.Texture = 1;
        isp.DepthMode = DepthMode_Greater;
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
        vtx(x, y, S[c / 20], 0); vtx(x + 32, y, 0x3f000000u, 0); vtx(x, y + 32, 0x3f000000u, 1);
    }
    ta_list_end();
}
