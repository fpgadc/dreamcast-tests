/* Where a triangle whose depth plane overflows changes sign (2026-11-19, after pvr-tests/zbad, zbadbg, zbadk).

   zbad: a 32 x 32 right triangle at a tile's first pixel with its right-angle corner at +inf (or a NaN, the largest
   float, 2^127) and +-0.5 elsewhere has depths of one sign up to x + y = 15 of the tile, about zero at 16, and the
   other sign beyond; 1e30 and 2^100 there are ordinary. Here the sign map alone, against a probe at depth 0: every
   cell a triangle (textured green, decal, DepthMode Always), then a white textured quad over the cell at depth 0,
   DepthMode Greater (page 0, 2) or Less (page 1, 3): white where the triangle's depth is below 0 (above 0) in the
   ISP's order; neither: 0.
   Pages 0, 1: rows k = 0 .. 14, the right-angle corner at 2^(113 + k), the others at 0.5; columns: the cell's place
   (column c of the screen, row k).
   Pages 2, 3: rows: 0 +inf legs 32, 1 +inf legs 16, 2 +inf legs 8, 3 +inf legs 64 (even columns; it covers the
   cell right of it and row 4), 5 +inf legs 16 at (16, 0) of the tile, 6 +inf legs 16 at (8, 8), 7 +inf legs 32 at
   (8, 8) (even columns; into the next tiles and row 8), 9 -inf legs 32, 10 NaN legs 32, 11 2^127 legs 16 at (8, 8),
   12 +inf with the other corners at -2^100, 13 at 2^100, 14 +inf pointing left (corners TR, TL, BR). */
#define NUM_PAGES 4
#include "pvrtest2.h"
#include "texgen.h"

static uint32_t texG, texW;
static uint32_t green(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0x07e0; }
static uint32_t white(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0xffff; }

static void test_setup(void)
{
    const tex_layout_t l = { TEX_TWIDDLED, 16, 8, 8, 0 };
    texG = tex_build(&l, green, NULL);
    texW = tex_build(&l, white, NULL);
}

static const char *GEO[15] = { "+inf legs 32", "+inf legs 16", "+inf legs 8", "+inf legs 64", "", "+inf legs 16 at (16, 0)",
                               "+inf legs 16 at (8, 8)", "+inf legs 32 at (8, 8)", "", "-inf legs 32", "NaN legs 32",
                               "2^127 legs 16 at (8, 8)", "+inf, others -2^100", "+inf, others 2^100", "+inf pointing left" };

static int cell_used(int page, int c)
{
    const int row = c / 20;
    if(page < 2)
        return 1;
    if(row == 4 || row == 8)
        return 0;
    return (row != 3 && row != 7) || (c % 2) == 0;
}

static void test_describe(void)
{
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 300; c++) {
            if(p < 2)
                label(p, c, "right angle at 2^%d, others 0.5, probe 0 %s", 113 + c / 20, p == 0 ? "Greater" : "Less");
            else if(cell_used(p, c))
                label(p, c, "%s, probe 0 %s", GEO[c / 20], p == 2 ? "Greater" : "Less");
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

static void vtx(int x, int y, uint32_t z, int end)
{
    ta_vertex3(f32b_int(x), f32b_int(y), z, 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, end);
}

/* the right triangle: corners (x, y) at s, (x + L, y), (x, y + L) at o */
static void tri(int x, int y, int L, uint32_t s, uint32_t o1, uint32_t o2)
{
    begin(DepthMode_Always, texG);
    vtx(x, y, s, 0); vtx(x + L, y, o1, 0); vtx(x, y + L, o2, 1);
}

static void test_page(int page)
{
    static const uint32_t INF = 0x7f800000u, HALF = 0x3f000000u;
    int c, k;
    pvr_frame.bg_color = 0x000040;
    ta_list_begin(ListType_Opaque);
    for(c = 0; c < 300; c++) {
        const int x = cell_px(c), y = cell_py(c), row = c / 20;
        if(!cell_used(page, c))
            continue;
        if(page < 2)
            tri(x, y, 32, (uint32_t)(113 + row + 127) << 23, HALF, HALF);
        else switch(row) {
            case 0: tri(x, y, 32, INF, HALF, HALF); break;
            case 1: tri(x, y, 16, INF, HALF, HALF); break;
            case 2: tri(x, y, 8, INF, HALF, HALF); break;
            case 3: tri(x, y, 64, INF, HALF, HALF); break;
            case 5: tri(x + 16, y, 16, INF, HALF, HALF); break;
            case 6: tri(x + 8, y + 8, 16, INF, HALF, HALF); break;
            case 7: tri(x + 8, y + 8, 32, INF, HALF, HALF); break;
            case 9: tri(x, y, 32, 0xff800000u, HALF, HALF); break;
            case 10: tri(x, y, 32, 0x7fc00000u, HALF, HALF); break;
            case 11: tri(x + 8, y + 8, 16, 0x7f000000u, HALF, HALF); break;
            case 12: tri(x, y, 32, INF, 0xf1800000u, 0xf1800000u); break;
            case 13: tri(x, y, 32, INF, 0x71800000u, 0x71800000u); break;
            case 14: begin(DepthMode_Always, texG); vtx(x + 32, y, HALF, 0); vtx(x, y, INF, 0); vtx(x + 32, y + 32, HALF, 1); break;
        }
    }
    for(c = 0; c < 300; c++) {
        begin(page % 2 == 0 ? DepthMode_Greater : DepthMode_Less, texW);
        for(k = 0; k < 4; k++)
            vtx(cell_px(c) + (k & 1) * 32, cell_py(c) + (k >> 1) * 32, 0, k == 3);
    }
    ta_list_end();
}
