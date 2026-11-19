/* texfar_edge: texture coordinates at and around 2^128 in 1/256 texel, and up to the largest float (2026-11-19).

   pvr-tests/texfar showed that a clamped coordinate of 1.5 2^128 in 1/256 texel and more is the last texel on a
   console, where refsw3 took texel 0 from 2^128 on. This puts the coordinate the TSP's divider delivers (q, a
   17 bit mantissa) exactly at the edge, for textures 8, 64 and 1024 texels wide (shift s = log2 size + 8: the
   coordinate in 1/256 texel is F = q 2^s), along U and V, point and bilinear, clamp, wrap and flip, mipmapped.
   Every cell a quad of 32 x 32 pixels, 1/w = 2^-64 at every corner, the attribute flat. With 1/w = 2^-64 the
   divider's reciprocal is 0.FFFF, so an attribute of 2^k gives q = (1 - 2^-16) 2^k and one of (1 + 2^-15) 2^k
   (mantissa 0x8001 in the plane's 16 bits) gives q = 2^k exactly (refsw3 pvr_tsp_plane.h, Divide).
   Columns (F the coordinate in 1/256 texel of the row's texture):
     0 (1 - 2^-16) 2^127   1 2^127   2 (1 - 2^-15) 2^128   3 (1 - 2^-16) 2^128   4 2^128   5 (1 + 2^-15) 2^128
     6 1.5 2^128   7 (1 - 2^-16) 2^129   8 2^129   9 2^132   10 the largest float   11 -(1 - 2^-16) 2^128
     12 -2^128   13 -(1 + 2^-15) 2^128   14 the largest negative float   15 +inf   16 NaN   17 -inf
     18 2^64   19 the middle of the last texel (u = 1 - 1 / (2 size))
   Rows: the texture and the mode, below (rows[]). Texel (x, y) of the plain textures is 565 with red x & 31,
   green (x >> 5) & 63, blue y & 31, so that a point sample tells the texel; the mipmapped one has blue
   log2 of the level's size + 8 (y & 1). */
#define NUM_PAGES 1
#include "pvrtest2.h"
#include "texgen.h"

typedef struct { const char *name; int tex; int clampU, clampV, flipU, flipV, bil, onV, both; } row_t;
enum { T8, T64, T1024, T8x1024, TMIP64 };
static const int texU[5] = { 0, 3, 7, 0, 3 }, texV[5] = { 0, 0, 0, 7, 3 };
static const row_t rows[15] = {
    { "8 ClampU",              T8,     1, 0, 0, 0, 0, 0, 0 },
    { "64 ClampU",             T64,    1, 0, 0, 0, 0, 0, 0 },
    { "1024 ClampU",           T1024,  1, 0, 0, 0, 0, 0, 0 },
    { "8 ClampV",              T8,     0, 1, 0, 0, 0, 1, 0 },
    { "8x1024 ClampV",         T8x1024, 0, 1, 0, 0, 0, 1, 0 },
    { "8 ClampU bilinear",     T8,     1, 0, 0, 0, 1, 0, 0 },
    { "1024 ClampU bilinear",  T1024,  1, 0, 0, 0, 1, 0, 0 },
    { "8 wrap bilinear",       T8,     0, 0, 0, 0, 1, 0, 0 },
    { "1024 wrap bilinear",    T1024,  0, 0, 0, 0, 1, 0, 0 },
    { "8 FlipU bilinear",      T8,     0, 0, 1, 0, 1, 0, 0 },
    { "8 ClampU ClampV, v = u", T8,    1, 1, 0, 0, 0, 0, 1 },
    { "mip 64 ClampU",         TMIP64, 1, 0, 0, 0, 0, 0, 0 },
    { "mip 64 ClampU bilinear", TMIP64, 1, 0, 0, 0, 1, 0, 0 },
    { "8x1024 FlipV bilinear", T8x1024, 0, 0, 0, 1, 1, 1, 0 },
    { "8 ClampV bilinear",     T8,     0, 1, 0, 0, 1, 1, 0 },
};
static uint32_t tex[5];

static uint32_t c565(int r, int g, int b) { return (uint32_t)(r & 31) << 11 | (uint32_t)(g & 63) << 5 | (uint32_t)(b & 31); }
static uint32_t plain(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; return c565(x, x >> 5, y); }
static uint32_t mipfn(const void *ctx, int size, int x, int y)
{
    int l = 0;
    (void)ctx;
    while((1 << l) < size) l++;
    return c565(x, y, l + 8 * (y & 1));
}

/* the attribute for column c of a texture whose coordinate in 1/256 texel is the attribute times 2^s */
static uint32_t attr(int c, int s)
{
    static const struct { int sign, e, frac; } k[] = {
        { 0, 127, 0 }, { 0, 127, 0x100 }, { 0, 127, 0x7fffff }, { 0, 128, 0 }, { 0, 128, 0x100 }, { 0, 128, 0x200 },
        { 0, 128, 0x400000 }, { 0, 129, 0 }, { 0, 129, 0x100 }, { 0, 132, 0x100 },
    };
    if(c < 10) {        /* q = (1 + frac / 2^23)(1 - ...) 2^(e - s): exponent field e - s + 127 (frac 0x7fffff: one binade lower) */
        const int e = k[c].e - s - (k[c].frac == 0x7fffff);
        return (uint32_t)k[c].sign << 31 | (uint32_t)(e + 127) << 23 | (uint32_t)k[c].frac;
    }
    switch(c) {
    case 10: return 0x7f7fffffu;
    case 11: return 0x80000000u | (uint32_t)(128 - s + 127) << 23;
    case 12: return 0x80000000u | (uint32_t)(128 - s + 127) << 23 | 0x100;
    case 13: return 0x80000000u | (uint32_t)(128 - s + 127) << 23 | 0x200;
    case 14: return 0xff7fffffu;
    case 15: return 0x7f800000u;
    case 16: return 0x7fc00000u;
    case 17: return 0xff800000u;
    case 18: return (uint32_t)(64 - s + 127) << 23 | 0x100;
    default: { const int l = s - 8; return (uint32_t)126 << 23 | (0x800000u - (0x800000u >> l)); }  /* 1 - 2^-(l + 1) */
    }
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
    l.layout = TEX_MIP; l.w = l.h = 64;
    tex[TMIP64] = tex_build(&l, mipfn, NULL);
}

static void test_describe(void)
{
    static const char *col[20] = { "(1-2^-16) 2^127", "2^127", "(1-2^-15) 2^128", "(1-2^-16) 2^128", "2^128",
        "(1+2^-15) 2^128", "1.5 2^128", "(1-2^-16) 2^129", "2^129", "2^132", "max float", "-(1-2^-16) 2^128",
        "-2^128", "-(1+2^-15) 2^128", "-max float", "+inf", "NaN", "-inf", "2^64", "last texel's middle" };
    int c;
    for(c = 0; c < 300; c++) {
        const row_t *r = &rows[c / 20];
        const int s = (r->onV ? texV[r->tex] : texU[r->tex]) + 3 + 8;
        label(0, c, "%s, F %s (attr %08x)", r->name, col[c % 20], (unsigned)attr(c % 20, s));
    }
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
        const int s = (r->onV ? texV[r->tex] : texU[r->tex]) + 3 + 8;
        const uint32_t a = attr(c % 20, s);
        /* the other coordinate: the middle of texel 0 (1 / (2 size)) */
        const uint32_t mid = (uint32_t)(127 - 1 - 3 - (r->onV ? texU[r->tex] : texV[r->tex])) << 23;
        f32b uu = a, vv = mid;
        if(r->onV) { uu = mid; vv = a; }
        if(r->both) vv = a;
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
        tcw.ScanOrder = r->tex == TMIP64 ? 0 : 1;
        tcw.MipMapped = r->tex == TMIP64;
        tcw.TexAddr = tex[r->tex] >> 3;
        ta_polygon(pcw, isp, tsp, tcw);
        for(k = 0; k < 4; k++)
            ta_vertex3(f32b_int(cell_px(c) + (k & 1) * 32), f32b_int(cell_py(c) + (k >> 1) * 32), 0x1f800000u /* 2^-64 */,
                       uu, vv, 0xffffffffu, 0, k == 3);
    }
    ta_list_end();
}
