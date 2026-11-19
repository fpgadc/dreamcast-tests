/* texfar_bits: the low bits of a texture coordinate far outside the texture (2026-11-19).

   refsw3 keeps the coordinate in 1/256 texel (F) as the integer it is, in two's complement, however large: wrap and
   flip read its low bits (from 2^30 on refsw3 carries only the sign and the low 26 bits, which is all they need).
   pvr-tests/coordfix measured F up to 2^25; texfar and texfar_edge only powers of two and their neighbours, whose
   low bits are zeros. Here F has 17 significant bits (the divider's mantissa) at every top from the texture's
   size to past where the lowest of them leaves the texel bits: if the hardware's coordinate were narrower than
   that (a register of 24 or 32 bits that wraps or saturates), wrap and flip would show it.
   Every cell a quad, 1/w = 2^-64 at every corner, the attribute flat: um 2^ea with um the plane's 16 bit mantissa;
   with 1/w = 2^-64 the divider makes q = (2 um - 2) 2^(ea - 1) (um 0x8000: 0xffff 2^(ea - 1)) (refsw3
   pvr_tsp_plane.h, Divide), and F = q 2^s, s = log2 size + 8. Column c: F's top bit is 2^(T - 1), T = T0 + c
   (T0 = the texture's s - 1: 17 for 1024 texels, 10 for 8, 13 for 64). Patterns: P1 um 0xa5a6 (q 0x14b4a),
   P2 um 0x8000 (q 0xffff), P3 um 0xffff (q 0x1fffc). Texel (x, y) is 565 with red x & 31, green (x >> 5) & 63,
   blue y & 31. */
#define NUM_PAGES 1
#include "pvrtest2.h"
#include "texgen.h"

typedef struct { const char *name; int tex; int clampU, clampV, flipU, flipV, bil, onV, neg, um; } row_t;
enum { T8, T64, T1024, T8x1024 };
static const int texU[4] = { 0, 3, 7, 0 }, texV[4] = { 0, 0, 0, 7 };
static const row_t rows[15] = {
    { "1024 wrap, P1",                T1024,   0, 0, 0, 0, 0, 0, 0, 0xa5a6 },
    { "1024 wrap, -P1",               T1024,   0, 0, 0, 0, 0, 0, 1, 0xa5a6 },
    { "1024 FlipU, P1",               T1024,   0, 0, 1, 0, 0, 0, 0, 0xa5a6 },
    { "1024 FlipU, -P1",              T1024,   0, 0, 1, 0, 0, 0, 1, 0xa5a6 },
    { "1024 wrap bilinear, P1",       T1024,   0, 0, 0, 0, 1, 0, 0, 0xa5a6 },
    { "1024 wrap bilinear, -P1",      T1024,   0, 0, 0, 0, 1, 0, 1, 0xa5a6 },
    { "1024 wrap, P2",                T1024,   0, 0, 0, 0, 0, 0, 0, 0x8000 },
    { "1024 wrap, -P2",               T1024,   0, 0, 0, 0, 0, 0, 1, 0x8000 },
    { "8x1024 wrap V, P1",            T8x1024, 0, 0, 0, 0, 0, 1, 0, 0xa5a6 },
    { "8x1024 FlipV, -P1",            T8x1024, 0, 0, 0, 1, 0, 1, 1, 0xa5a6 },
    { "8 wrap, P1",                   T8,      0, 0, 0, 0, 0, 0, 0, 0xa5a6 },
    { "8 FlipU, -P1",                 T8,      0, 0, 1, 0, 0, 0, 1, 0xa5a6 },
    { "64 wrap, P1",                  T64,     0, 0, 0, 0, 0, 0, 0, 0xa5a6 },
    { "64 FlipU bilinear, P1",        T64,     0, 0, 1, 0, 1, 0, 0, 0xa5a6 },
    { "1024 wrap, P3",                T1024,   0, 0, 0, 0, 0, 0, 0, 0xffff },
};
static uint32_t tex[4];

static uint32_t c565(int r, int g, int b) { return (uint32_t)(r & 31) << 11 | (uint32_t)(g & 63) << 5 | (uint32_t)(b & 31); }
static uint32_t plain(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; return c565(x, x >> 5, y); }

static int shift_of(const row_t *r) { return (r->onV ? texV[r->tex] : texU[r->tex]) + 3 + 8; }
static int top_of(int c) { return shift_of(&rows[c / 20]) - 1 + c % 20; }
static uint32_t attr(int c)
{
    const row_t *r = &rows[c / 20];
    const int s = shift_of(r), T = top_of(c);
    const int ea = r->um == 0x8000 ? T - 15 - s : T - 16 - s;      /* the attribute um 2^ea */
    return (uint32_t)r->neg << 31 | (uint32_t)(ea + 15 + 127) << 23 | (uint32_t)(r->um - 0x8000) << 8;
}

static void test_setup(void)
{
    tex_layout_t l = { TEX_RASTER, 16, 8, 8, 0 };
    tex[T8] = tex_build(&l, plain, NULL);
    l.w = 64;
    tex[T64] = tex_build(&l, plain, NULL);
    l.w = 1024;
    tex[T1024] = tex_build(&l, plain, NULL);
    l.w = 8; l.h = 1024;
    tex[T8x1024] = tex_build(&l, plain, NULL);
}

static void test_describe(void)
{
    int c;
    for(c = 0; c < 300; c++)
        label(0, c, "%s, F top 2^%d (attr %08x)", rows[c / 20].name, top_of(c) - 1, (unsigned)attr(c));
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
        const uint32_t mid = (uint32_t)(127 - 1 - 3 - (r->onV ? texU[r->tex] : texV[r->tex])) << 23;   /* texel 0's middle */
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
        tsp.MipMapD = 4;
        tsp.TexU = (uint32_t)texU[r->tex];
        tsp.TexV = (uint32_t)texV[r->tex];
        tsp.ClampU = (uint32_t)r->clampU;
        tsp.ClampV = (uint32_t)r->clampV;
        tsp.FlipU = (uint32_t)r->flipU;
        tsp.FlipV = (uint32_t)r->flipV;
        tcw.PixelFmt = PixelFmt_565;
        tcw.ScanOrder = 1;
        tcw.TexAddr = tex[r->tex] >> 3;
        ta_polygon(pcw, isp, tsp, tcw);
        for(k = 0; k < 4; k++)
            ta_vertex3(f32b_int(cell_px(c) + (k & 1) * 32), f32b_int(cell_py(c) + (k >> 1) * 32), 0x1f800000u /* 2^-64 */,
                       r->onV ? mid : a, r->onV ? a : mid, 0xffffffffu, 0, k == 3);
    }
    ta_list_end();
}
