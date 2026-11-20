/* Where the overflowing register starts (2026-11-19, hunt depthgaps (a); after zbad, zword, depthgaps_bglow).

   zword: when a slope of the depth plane overflows, the register steps by +-2^124 a pixel and the word stored is
   kWrap[m], m = (sa i + sb j) mod 32 from the tile's first pixel. depthgaps_bglow: zword's triangle with +inf at the
   top right corner one tile from the left edge of the screen (x = 0) starts one step on, m = i + 1, as one cell of
   zbad did (NaN there). There the bad corner's cross product (e2 = x2 y0 - x0 y2) is 0, so the plane's constant
   c does not overflow. Here: triangles with legs 32 and one bad corner, in many places, against a background.
   Pages: the background +100 (Greater draws where m = 1 .. 15), -100 (everything but m = 16), 2^127 (m = 2 .. 15).
   Cells: column 0, rows 1 - 14: bad corner at the top right, x = 0 (c finite), V[row - 1];
          row 0, columns 1 - 19: bad corner at the bottom left, y = 0 (c finite), V[col - 1];
          row 2, 4, 6 columns 2 - 19 and row 8 columns 2 - 7: the same variants with x, y not 0 (c overflows):
          top right at rows 2 / 4, bottom left at rows 6 / 8 (V[col - 2] and V[18 + col - 2]);
          row 10: top left corner bad, V[col - 2] (c always overflows: e1 > 0). */
#define NUM_PAGES 3
#include "pvrtest2.h"
#include "texgen.h"

typedef struct { uint32_t bad, other; const char *name; } var_t;
#define NV 24
static const var_t V[NV] = {
    { 0x7f800000u, 0x3f000000u, "+inf, others 0.5" }, { 0x7f800000u, 0xbf000000u, "+inf, others -0.5" },
    { 0x7fc00000u, 0x3f000000u, "NaN, others 0.5" }, { 0x7fc00000u, 0xbf000000u, "NaN, others -0.5" },
    { 0x7f7fffffu, 0x3f000000u, "max, others 0.5" }, { 0x7f7fffffu, 0xbf000000u, "max, others -0.5" },
    { 0x7f000000u, 0x3f000000u, "2^127, others 0.5" }, { 0x7f000000u, 0xbf000000u, "2^127, others -0.5" },
    { 0xff800000u, 0x3f000000u, "-inf, others 0.5" }, { 0xff800000u, 0xbf000000u, "-inf, others -0.5" },
    { 0xff7fffffu, 0x3f000000u, "-max, others 0.5" }, { 0xff7fffffu, 0xbf000000u, "-max, others -0.5" },
    { 0x7d800000u, 0x3f000000u, "2^124, others 0.5" }, { 0x7d800000u, 0xbf000000u, "2^124, others -0.5" },
    { 0x7f800000u, 0x00000000u, "+inf, others 0" }, { 0x7f800000u, 0x42c80000u, "+inf, others 100" },
    { 0x7f800000u, 0xc2c80000u, "+inf, others -100" }, { 0x7f800000u, 0x5d800000u, "+inf, others 2^60" },
    { 0x7f800000u, 0xdd800000u, "+inf, others -2^60" }, { 0x7f800000u, 0x7b800000u, "+inf, others 2^120" },
    { 0xff800000u, 0x42c80000u, "-inf, others 100" }, { 0x7fc00000u, 0x42c80000u, "NaN, others 100" },
    { 0x7f000000u, 0x5d800000u, "2^127, others 2^60" }, { 0x7f7fffffu, 0x00000000u, "max, others 0" },
};
static const uint32_t BG[NUM_PAGES] = { 0x42c80000u, 0xc2c80000u, 0x7f000000u };
static uint32_t texG;

static uint32_t green(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0x07e0; }

static void test_setup(void)
{
    const tex_layout_t l = { TEX_TWIDDLED, 16, 8, 8, 0 };
    texG = tex_build(&l, green, NULL);
}

/* cell c: geometry (0 top right, 1 bottom left, 2 top left bad), variant; -1: none */
static int cellinfo(int c, int *geo)
{
    const int r = c / 20, col = c % 20;
    if(col == 0 && r >= 1 && r <= 14) { *geo = 0; return r - 1 < NV ? r - 1 : -1; }
    if(r == 0 && col >= 1) { *geo = 1; return col - 1; }
    if(col < 2) return -1;
    if(r == 2) { *geo = 0; return col - 2; }
    if(r == 4 && col - 2 + 18 < NV) { *geo = 0; return col - 2 + 18; }
    if(r == 6) { *geo = 1; return col - 2; }
    if(r == 8 && col - 2 + 18 < NV) { *geo = 1; return col - 2 + 18; }
    if(r == 10) { *geo = 2; return col - 2; }
    if(r == 12 && col - 2 + 18 < NV) { *geo = 2; return col - 2 + 18; }
    return -1;
}

static void test_describe(void)
{
    static const char *G[3] = { "top right", "bottom left", "top left" };
    int p, c, g;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 300; c++) {
            const int v = cellinfo(c, &g);
            if(v >= 0)
                label(p, c, "%s bad at (%d, %d): %s; background %08x", G[g], cell_px(c), cell_py(c), V[v].name, (unsigned)BG[p]);
        }
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

static void test_page(int page)
{
    int c, g;
    pvr_frame.bg_color = 0x000040;
    pvr_frame.bg_depth = BG[page];
    ta_list_begin(ListType_Opaque);
    for(c = 0; c < 300; c++) {
        const int v = cellinfo(c, &g), x = cell_px(c), y = cell_py(c);
        if(v < 0)
            continue;
        begin(DepthMode_Greater);
        if(g == 0) { vtx(x, y, V[v].other, 0); vtx(x + 32, y, V[v].bad, 0); vtx(x, y + 32, V[v].other, 1); }
        else if(g == 1) { vtx(x, y, V[v].other, 0); vtx(x + 32, y, V[v].other, 0); vtx(x, y + 32, V[v].bad, 1); }
        else { vtx(x, y, V[v].bad, 0); vtx(x + 32, y, V[v].other, 0); vtx(x, y + 32, V[v].other, 1); }
    }
    ta_list_end();
}
