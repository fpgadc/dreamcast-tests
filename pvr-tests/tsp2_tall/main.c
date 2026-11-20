/* tsp2_tall: the V coordinate of a mipmapped texture declared TALLER than wide (2026-11-20).

   texfar_decl measured a mipmapped texture declared shorter than wide (d = TexU - TexV > 0): V is made at the full
   height with its fraction bits, less the declared half texel (refsw3 TextureFilter). Declared taller (d < 0, the
   texture is still the TexU square) refsw3 makes V at the declared height, 8 fraction bits and half a declared
   texel there, and shifts it down by -d: the same as V at the square's height less the declared half texel,
   128 >> -d, which is a fraction of a 1/256 texel for no d (|d| <= 7); a weight taken from the declared height's
   fraction, or the square's own half texel, would differ. Every cell a flat quad (1/w = 2^-n at every corner, so
   that the level comes from w alone: texfar_mip), u texel 0's middle, v = um 2^-(TexV + 11) with um the plane's
   16 bit mantissa, so that the quotient times 2^(TexV + 11) (V in 1/256 of a declared texel) is F = um - 1
   (below zero -um). Column c: um = 0x8001 + c (3 2^-d + 5), so that the columns walk through the square's weights
   with every bit below them set some way. Rows: rows[]. Texel (x, y) of the level 2^l wide: 565, red (y >> 1)
   & 31, green 63 on odd rows and 0 on even ones (a V weight shows as green, step by step), blue l | (x & 1) << 4.
   Results (one console, 2026-11-20): refsw3's V as it was, 300 of 300 regions; V at the square's height with the
   square's own half texel 20; the declared coordinate's row shifted down with its own fraction as the weight 42. */
#define NUM_PAGES 1
#include "pvrtest2.h"
#include "texgen.h"

typedef struct { const char *name; int texU, texV, n, mipD; int clampV, flipV, bil, neg; } row_t;
static const row_t rows[15] = {
    { "8 declared 8x1024 wrap bilinear",        0, 7, 24, 4, 0, 0, 1, 0 },
    { "8 declared 8x1024 wrap bilinear, -v",    0, 7, 24, 4, 0, 0, 1, 1 },
    { "8 declared 8x1024 FlipV bilinear",       0, 7, 24, 4, 0, 1, 1, 0 },
    { "8 declared 8x1024 ClampV bilinear",      0, 7, 24, 4, 1, 0, 1, 0 },
    { "8 declared 8x512 wrap bilinear",         0, 6, 24, 4, 0, 0, 1, 0 },
    { "8 declared 8x256 wrap bilinear",         0, 5, 24, 4, 0, 0, 1, 0 },
    { "8 declared 8x128 wrap bilinear",         0, 4, 24, 4, 0, 0, 1, 0 },
    { "8 declared 8x32 wrap bilinear, -v",      0, 2, 24, 4, 0, 0, 1, 1 },
    { "8 declared 8x16 wrap bilinear",          0, 1, 24, 4, 0, 0, 1, 0 },
    { "64 declared 64x1024 wrap bilinear",      3, 7, 24, 4, 0, 0, 1, 0 },
    { "64 declared 64x1024 bilinear, 1/w 2^-61", 3, 7, 61, 4, 0, 0, 1, 0 },
    { "64 declared 64x1024 bilinear, 1/w 2^-62", 3, 7, 62, 4, 0, 0, 1, 0 },
    { "32 declared 32x1024 FlipV bilinear, -v", 2, 7, 24, 4, 0, 1, 1, 1 },
    { "128 declared 128x1024 bilinear, 1/w 2^-62", 4, 7, 62, 4, 0, 0, 1, 0 },
    { "8 declared 8x1024 wrap point",           0, 7, 24, 4, 0, 0, 0, 0 },
};
static uint32_t tex8, tex32, tex64, tex128;

static uint32_t c565(int r, int g, int b) { return (uint32_t)(r & 31) << 11 | (uint32_t)(g & 63) << 5 | (uint32_t)(b & 31); }
static uint32_t mipfn(const void *ctx, int size, int x, int y)
{
    int l = 0;
    (void)ctx;
    while((1 << l) < size) l++;
    return c565(y >> 1, (y & 1) ? 63 : 0, l | (x & 1) << 4);
}

static int um_of(int c)
{
    const row_t *r = &rows[c / 20];
    return 0x8001 + (c % 20) * (3 * (1 << (r->texV - r->texU)) + 5);
}
static uint32_t attr(int c)
{
    const row_t *r = &rows[c / 20];
    const int ea = -(r->texV + 11);                        /* v = um 2^ea */
    return (uint32_t)r->neg << 31 | (uint32_t)(ea + 15 + 127) << 23 | (uint32_t)(um_of(c) - 0x8000) << 8;
}

static void test_setup(void)
{
    tex_layout_t l = { TEX_MIP, 16, 8, 8, 0 };
    tex8 = tex_build(&l, mipfn, NULL);
    l.w = l.h = 32;
    tex32 = tex_build(&l, mipfn, NULL);
    l.w = l.h = 64;
    tex64 = tex_build(&l, mipfn, NULL);
    l.w = l.h = 128;
    tex128 = tex_build(&l, mipfn, NULL);
}

static void test_describe(void)
{
    int c;
    for(c = 0; c < 300; c++)
        label(0, c, "%s, um 0x%x (attr %08x)", rows[c / 20].name, um_of(c), (unsigned)attr(c));
}

static void test_page(int page)
{
    int c, k;
    (void)page;
    pvr_frame.bg_color = 0x000040;
    ta_list_begin(ListType_Opaque);
    for(c = 0; c < 300; c++) {
        PCW pcw = { .full = 0 };
        ISP_TSP isp = { .full = 0 };
        TSP tsp = { .full = 0 };
        TCW tcw = { .full = 0 };
        const row_t *r = &rows[c / 20];
        const uint32_t a = attr(c);
        const uint32_t mid = (uint32_t)(127 - 1 - 3 - r->texU) << 23;     /* texel 0's middle */
        pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
        pcw.ListType = ListType_Opaque;
        pcw.Group_En = 1;
        pcw.Strip_Len = 1;
        pcw.Texture = 1;
        isp.DepthMode = DepthMode_Always;
        isp.CullMode = CullMode_None;
        isp.Texture = 1;
        tsp.SrcInstr = BlendInstr_One;
        tsp.DstInstr = BlendInstr_Zero;
        tsp.FogCtrl = FogCtrl_None;
        tsp.UseAlpha = 1;
        tsp.ShadInstr = ShadInstr_Decal;
        tsp.FilterMode = r->bil ? FilterMode_Bilinear : FilterMode_Point;
        tsp.MipMapD = (uint32_t)r->mipD;
        tsp.TexU = (uint32_t)r->texU;
        tsp.TexV = (uint32_t)r->texV;
        tsp.ClampV = (uint32_t)r->clampV;
        tsp.FlipV = (uint32_t)r->flipV;
        tcw.PixelFmt = PixelFmt_565;
        tcw.MipMapped = 1;
        tcw.TexAddr = (r->texU == 4 ? tex128 : r->texU == 3 ? tex64 : r->texU == 2 ? tex32 : tex8) >> 3;
        ta_polygon(pcw, isp, tsp, tcw);
        for(k = 0; k < 4; k++)
            ta_vertex3(f32b_int(cell_px(c) + (k & 1) * 32), f32b_int(cell_py(c) + (k >> 1) * 32), (uint32_t)(127 - r->n) << 23,
                       mid, a, 0xffffffffu, 0, k == 3);
    }
    ta_list_end();
}
