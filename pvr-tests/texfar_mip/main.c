/* texfar_mip: mipmapped textures with coordinates far outside them, at every level (2026-11-19).

   A flat attribute gives the mipmap level from w alone: the polygon's K is zero, which the level unit reads as
   2^(-127 + log2 size) (README "The TSP's numbers at their limits"), times w^2. So 1/w = 2^-24 is the largest
   level, 2^-60 .. 2^-63 the levels in between, 2^-64 the 1x1 level (texfar_edge). The coordinate is made at the
   full size, the half texel taken off there, and shifted down to the level (refsw3 TextureFilter); a texture
   declared shorter than wide (TexV < TexU) makes V with the declared size and scales it up by 2^(TexU - TexV).
   Columns as texfar_edge's (F the coordinate in 1/256 texel at the full size of the row's U, or the declared V):
     0 (1 - 2^-16) 2^127   1 2^127   2 (1 - 2^-15) 2^128   3 (1 - 2^-16) 2^128   4 2^128   5 (1 + 2^-15) 2^128
     6 1.5 2^128   7 (1 - 2^-16) 2^129   8 2^129   9 2^132   10 the largest float   11 -(1 - 2^-16) 2^128
     12 -2^128   13 -(1 + 2^-15) 2^128   14 the largest negative float   15 +inf   16 NaN   17 -inf
     18 2^64   19 the middle of the last texel
   Texel (x, y) of the level that is 2^l wide: 565, red x & 31, green (x >> 5) & 1 | (y & 31) << 1, blue l | (y >> 5) << 4. */
#define NUM_PAGES 1
#include "pvrtest2.h"
#include "texgen.h"

typedef struct { const char *name; int texU, texV, w; int clampU, clampV, flipU, bil, onV; } row_t;
static const row_t rows[15] = {
    { "64 ClampU, 1/w 2^-24",            3, 3, 24, 1, 0, 0, 0, 0 },
    { "64 ClampU, 1/w 2^-60",            3, 3, 60, 1, 0, 0, 0, 0 },
    { "64 ClampU, 1/w 2^-61",            3, 3, 61, 1, 0, 0, 0, 0 },
    { "64 ClampU, 1/w 2^-62",            3, 3, 62, 1, 0, 0, 0, 0 },
    { "64 ClampU, 1/w 2^-63",            3, 3, 63, 1, 0, 0, 0, 0 },
    { "64 ClampU bilinear, 1/w 2^-24",   3, 3, 24, 1, 0, 0, 1, 0 },
    { "64 ClampU bilinear, 1/w 2^-61",   3, 3, 61, 1, 0, 0, 1, 0 },
    { "64 ClampU bilinear, 1/w 2^-62",   3, 3, 62, 1, 0, 0, 1, 0 },
    { "64 wrap bilinear, 1/w 2^-61",     3, 3, 61, 0, 0, 0, 1, 0 },
    { "64 FlipU bilinear, 1/w 2^-61",    3, 3, 61, 0, 0, 1, 1, 0 },
    { "64 ClampV, 1/w 2^-24",            3, 3, 24, 0, 1, 0, 0, 1 },
    { "64 declared 64x8 ClampV, 1/w 2^-24", 3, 0, 24, 0, 1, 0, 0, 1 },
    { "64 declared 64x8 ClampV bilinear, 1/w 2^-61", 3, 0, 61, 0, 1, 0, 1, 1 },
    { "256 ClampU, 1/w 2^-24",           5, 5, 24, 1, 0, 0, 0, 0 },
    { "256 ClampU bilinear, 1/w 2^-62",  5, 5, 62, 1, 0, 0, 1, 0 },
};
static uint32_t tex64, tex256;

static uint32_t c565(int r, int g, int b) { return (uint32_t)(r & 31) << 11 | (uint32_t)(g & 63) << 5 | (uint32_t)(b & 31); }
static uint32_t mipfn(const void *ctx, int size, int x, int y)
{
    int l = 0;
    (void)ctx;
    while((1 << l) < size) l++;
    return c565(x, ((x >> 5) & 1) | (y << 1), l | ((y >> 5) & 1) << 4);
}

/* as texfar_edge: the attribute for column c of a coordinate that is the attribute times 2^s in 1/256 texel */
static uint32_t attr(int c, int s)
{
    static const struct { int e, frac; } k[] = {
        { 127, 0 }, { 127, 0x100 }, { 127, 0x7fffff }, { 128, 0 }, { 128, 0x100 }, { 128, 0x200 },
        { 128, 0x400000 }, { 129, 0 }, { 129, 0x100 }, { 132, 0x100 },
    };
    if(c < 10) {
        const int e = k[c].e - s - (k[c].frac == 0x7fffff);
        return (uint32_t)(e + 127) << 23 | (uint32_t)k[c].frac;
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
static int shift_of(const row_t *r) { return (r->onV ? r->texV : r->texU) + 3 + 8; }

static void test_setup(void)
{
    tex_layout_t l = { TEX_MIP, 16, 64, 64, 0 };
    tex64 = tex_build(&l, mipfn, NULL);
    l.w = l.h = 256;
    tex256 = tex_build(&l, mipfn, NULL);
}

static void test_describe(void)
{
    static const char *col[20] = { "(1-2^-16) 2^127", "2^127", "(1-2^-15) 2^128", "(1-2^-16) 2^128", "2^128",
        "(1+2^-15) 2^128", "1.5 2^128", "(1-2^-16) 2^129", "2^129", "2^132", "max float", "-(1-2^-16) 2^128",
        "-2^128", "-(1+2^-15) 2^128", "-max float", "+inf", "NaN", "-inf", "2^64", "last texel's middle" };
    int c;
    for(c = 0; c < 300; c++)
        label(0, c, "%s, F %s (attr %08x)", rows[c / 20].name, col[c % 20], (unsigned)attr(c % 20, shift_of(&rows[c / 20])));
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
        const uint32_t a = attr(c % 20, shift_of(r));
        const uint32_t mid = (uint32_t)(127 - 1 - 3 - (r->onV ? r->texU : r->texV)) << 23;     /* texel 0's middle */
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
        tsp.TexU = (uint32_t)r->texU;
        tsp.TexV = (uint32_t)r->texV;
        tsp.ClampU = (uint32_t)r->clampU;
        tsp.ClampV = (uint32_t)r->clampV;
        tsp.FlipU = (uint32_t)r->flipU;
        tcw.PixelFmt = PixelFmt_565;
        tcw.MipMapped = 1;
        tcw.TexAddr = (r->texU == 5 ? tex256 : tex64) >> 3;
        ta_polygon(pcw, isp, tsp, tcw);
        for(k = 0; k < 4; k++)
            ta_vertex3(f32b_int(cell_px(c) + (k & 1) * 32), f32b_int(cell_py(c) + (k >> 1) * 32), (uint32_t)(127 - r->w) << 23,
                       r->onV ? mid : a, r->onV ? a : mid, 0xffffffffu, 0, k == 3);
    }
    ta_list_end();
}
