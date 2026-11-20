/* A negative background's low fraction bits (2026-11-19, hunt depthgaps (c); after pvr-tests/bgneg, bgneg2, zword).

   bgneg: a negative background of magnitude 64 and more is stored as itself, but every background there had its low
   5 fraction bits clear; a positive one loses them (bgdepth). Here backgrounds W + L, L = 0, 1, 16, 31, against
   polygons whose stored words are known exactly (low 5 bits of the word clear):
     row 0  flat quad at -100      0xc2c80000          row 1  flat quad at -2^100   0xf1800000
     row 2  flat quad at +100      0x42c80000          (exact: depths of few bits, a power of two determinant)
     row 3  zword's triangle, +inf at the top left corner (legs 32): every pixel a word of the overflow register,
            kWrap[-(i + j) mod 32]; m = 23 is 0xfeffffe0 and m = 30 0xff7fffe0 (zword: Equal with those backgrounds)
     row 4  the same, +inf at the top right corner: kWrap[i mod 32]
   Columns 0, 1, 2: DepthMode Greater, Less, Equal (textured green, decal). A kept background W + L is below the
   polygon at W (a negative depth's fraction bits are inverted in the ISP's order, pvr-tests/zsign): Greater draws;
   one cut to W ties: Equal draws.
   Pages: the background W[page / 4] + L[page % 4]. */
#define NUM_PAGES 20
#include "pvrtest2.h"
#include "texgen.h"

static const uint32_t W[5] = { 0xc2c80000u, 0xf1800000u, 0xfeffffe0u, 0xff7fffe0u, 0x42c80000u };
static const uint32_t L[4] = { 0, 1, 16, 31 };
static const char *ROW[5] = { "flat -100", "flat -2^100", "flat +100", "+inf TL triangle", "+inf TR triangle" };
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
    int p, r, m;
    for(p = 0; p < NUM_PAGES; p++)
        for(r = 0; r < 5; r++)
            for(m = 0; m < 3; m++)
                label(p, r * 20 + m, "background %08x, %s, %s", (unsigned)(W[p / 4] + L[p % 4]), ROW[r], MODE[m]);
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
    static const uint32_t FLAT[3] = { 0xc2c80000u, 0xf1800000u, 0x42c80000u };
    const uint32_t half = 0x3f000000u, inf = 0x7f800000u;
    int r, m, k;
    pvr_frame.bg_color = 0x000040;
    pvr_frame.bg_depth = W[page / 4] + L[page % 4];
    ta_list_begin(ListType_Opaque);
    for(r = 0; r < 5; r++)
        for(m = 0; m < 3; m++) {
            const int c = r * 20 + m, x = cell_px(c), y = cell_py(c);
            begin(m == 0 ? DepthMode_Greater : m == 1 ? DepthMode_Less : DepthMode_Equal);
            if(r < 3)
                for(k = 0; k < 4; k++)
                    vtx(x + (k & 1) * 32, y + (k >> 1) * 32, FLAT[r], k == 3);
            else if(r == 3) { vtx(x, y, inf, 0); vtx(x + 32, y, half, 0); vtx(x, y + 32, half, 1); }
            else { vtx(x, y, half, 0); vtx(x + 32, y, inf, 0); vtx(x, y + 32, half, 1); }
        }
    ta_list_end();
}
