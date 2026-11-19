/* The depth of a triangle with one corner at a NaN, an infinity or a huge depth (2026-11-19, after pvr-tests/zspec).

   zspec: such a triangle (the other corners at 0.5) is not one depth on a console: the depth changes across it at
   x + y = 16 / 17 of the cell, as if the depth unit evaluated its plane through the overflow; refsw3 stores 2^127
   over all of it (pvr-tests/bgbig: what a triangle whose plane overflows everywhere stores). Here the depth is
   read against a probe: every cell a triangle textured green (decal: its colour shows whatever the sign of its 1/w),
   DepthMode Always, then a white textured quad over the cell at the page's probe depth P, DepthMode Greater (rows
   0-5) or Less (rows 6-11, the same triangles). White where the triangle's depth is below P (above P) in the ISP's
   order (pvr-tests/zsign).
   Columns: the bad corner's depth S[col % 10], the two others at +0.5 (columns 0-9) or -0.5 (10-19).
   Rows (and 6 + row): the cell's corners TL (x, y), TR (x + 32, y), BL (x, y + 32):
     0  TL, TR, BL with TL at S          1  TL, TR, BL with TR at S          2  TL, TR, BL with BL at S
     3  as 0 with legs of 16             4  TR, BL, TL with TL at S (another order)
     5  zspec's quad: TL at S, TR, BL, BR at the other depth (two triangles)
   Pages: P = PROBE[page]. */
#define NUM_PAGES 17
#include "pvrtest2.h"
#include "texgen.h"

static const uint32_t S[10] = { 0x7fc00000u, 0xffc00000u, 0x7f800000u, 0xff800000u, 0x7f7fffffu,
                                0xff7fffffu, 0x7f000000u, 0xff000000u, 0x7149f2cau, 0x71800000u };
static const char *SNAME[10] = { "NaN", "-NaN", "+inf", "-inf", "max", "-max", "2^127", "-2^127", "1e30", "2^100" };
static const uint32_t PROBE[NUM_PAGES] = {
    0x00000000u, 0x03800000u, 0x83800000u, 0x35800000u, 0xb5800000u, 0x3e800000u, 0xbe800000u, 0x3f000000u,
    0xbf000000u, 0x3f800000u, 0xbf800000u, 0x49800000u, 0xc9800000u, 0x71800000u, 0xf1800000u, 0x7f000000u,
    0xff000000u,
};      /* 0, +-2^-120, +-2^-20, +-0.25, +-0.5, +-1, +-2^20, +-2^100, +-2^127 */
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
    static const char *geo[6] = { "TL bad", "TR bad", "BL bad", "TL bad, legs 16", "TL bad, order TR BL TL", "quad, TL bad" };
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 240; c++)
            label(p, c, "%s at %s, others %s, probe %08x %s", geo[(c / 20) % 6], SNAME[c % 10], c % 20 < 10 ? "0.5" : "-0.5",
                  (unsigned)PROBE[p], c < 120 ? "Greater" : "Less");
}

static void begin(int list, int mode, uint32_t tex, int strip)
{
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    (void)strip;
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = list;
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

static void cell(int c)
{
    const int x = cell_px(c), y = cell_py(c), geo = (c / 20) % 6;
    const uint32_t s = S[c % 10], o = c % 20 < 10 ? 0x3f000000u : 0xbf000000u;
    const int L = geo == 3 ? 16 : 32;
    begin(ListType_Opaque, DepthMode_Always, texG, 0);
    switch(geo) {
    case 1: vtx(x, y, o, 0); vtx(x + L, y, s, 0); vtx(x, y + L, o, 1); break;
    case 2: vtx(x, y, o, 0); vtx(x + L, y, o, 0); vtx(x, y + L, s, 1); break;
    case 4: vtx(x + L, y, o, 0); vtx(x, y + L, o, 0); vtx(x, y, s, 1); break;
    case 5: vtx(x, y, s, 0); vtx(x + L, y, o, 0); vtx(x, y + L, o, 0); vtx(x + L, y + L, o, 1); break;
    default: vtx(x, y, s, 0); vtx(x + L, y, o, 0); vtx(x, y + L, o, 1); break;
    }
}

static void probe(int c, int mode, uint32_t z)
{
    const int x = cell_px(c), y = cell_py(c);
    int k;
    begin(ListType_Opaque, mode, texW, 0);
    for(k = 0; k < 4; k++)
        vtx(x + (k & 1) * 32, y + (k >> 1) * 32, z, k == 3);
}

static void test_page(int page)
{
    int c;
    pvr_frame.bg_color = 0x000040;
    ta_list_begin(ListType_Opaque);
    for(c = 0; c < 240; c++) {
        cell(c);
        probe(c, c < 120 ? DepthMode_Greater : DepthMode_Less, PROBE[page]);
    }
    ta_list_end();
}
