/* texfar_decl: the V coordinate of a mipmapped texture declared much shorter than wide (2026-11-19).

   texfar_mipv found that a declared V size 2^d times smaller than the stored square (d = TexU - TexV) does not
   scale the coordinate made at the declared size (8 fraction bits there, so d zeros at the bottom after the
   scaling): the console has fraction bits down there, which the bilinear weight (bits 3 .. 7 of the coordinate
   at the level) sees from d = 4 + level on. Rule under test (refsw3 TextureFilter): V = floor(v 2^(TexU + 11)) -
   (128 << d), the half texel of the declared height, then >> level. Every cell a flat quad (1/w = 2^-n at every
   corner, so the level comes from w alone: texfar_mip; 1.5 2^-61 gives level 1 of the 256 texel textures, with a
   reciprocal that is not 0.FFFF), v = um 2^-(TexU + 11) (um the plane's 16 bit mantissa),
   so that the divider's quotient times 2^(TexU + 11) is F = um - 1 exactly (um - 1/2 ... below zero), u texel 0's
   middle. Column c: F = 0x9234 + 2 c (+, or -F - 1). Rows: rows[]. Texel (x, y) of the level 2^l wide: 565, red
   (y >> 1) & 31, green 63 on odd rows and 0 on even ones (so that a V weight shows as green, step by step), blue
   l | (x & 1) << 4. */
#define NUM_PAGES 1
#include "pvrtest2.h"
#include "texgen.h"

typedef struct { const char *name; int texU, texV, n, mipD; int clampV, flipV, bil, neg; } row_t;
static const row_t rows[15] = {
    { "256x8 wrap bilinear",             5, 0, 24, 4, 0, 0, 1, 0 },
    { "256x8 wrap bilinear, -v",         5, 0, 24, 4, 0, 0, 1, 1 },
    { "256x8 FlipV bilinear",            5, 0, 24, 4, 0, 1, 1, 0 },
    { "256x8 ClampV bilinear",           5, 0, 24, 4, 1, 0, 1, 0 },
    { "256x8 wrap bilinear, 1/w 1.5 2^-61", 5, 0, 605, 4, 0, 0, 1, 0 },
    { "256x8 wrap bilinear, -v, 1/w 1.5 2^-61", 5, 0, 605, 4, 0, 0, 1, 1 },
    { "256x8 wrap bilinear, 1/w 2^-61",  5, 0, 61, 4, 0, 0, 1, 0 },
    { "256x16 wrap bilinear",            5, 1, 24, 4, 0, 0, 1, 0 },
    { "256x16 wrap bilinear, -v",        5, 1, 24, 4, 0, 0, 1, 1 },
    { "256x16 wrap bilinear, 1/w 1.5 2^-61", 5, 1, 605, 4, 0, 0, 1, 0 },
    { "256x32 wrap bilinear",            5, 2, 24, 4, 0, 0, 1, 0 },
    { "256x8 wrap point",                5, 0, 24, 4, 0, 0, 0, 0 },
    { "128x8 wrap bilinear",             4, 0, 24, 4, 0, 0, 1, 0 },
    { "128x8 FlipV bilinear, -v",        4, 0, 24, 4, 0, 1, 1, 1 },
    { "64x8 wrap bilinear",              3, 0, 24, 4, 0, 0, 1, 0 },
};
static uint32_t tex64, tex128, tex256;

static uint32_t c565(int r, int g, int b) { return (uint32_t)(r & 31) << 11 | (uint32_t)(g & 63) << 5 | (uint32_t)(b & 31); }
static uint32_t mipfn(const void *ctx, int size, int x, int y)
{
    int l = 0;
    (void)ctx;
    while((1 << l) < size) l++;
    return c565(y >> 1, (y & 1) ? 63 : 0, l | (x & 1) << 4);
}

static uint32_t attr(int c)
{
    const row_t *r = &rows[c / 20];
    const int um = 0x9235 + 2 * (c % 20), ea = -(r->texU + 11);     /* v = um 2^ea */
    return (uint32_t)r->neg << 31 | (uint32_t)(ea + 15 + 127) << 23 | (uint32_t)(um - 0x8000) << 8;
}

static void test_setup(void)
{
    tex_layout_t l = { TEX_MIP, 16, 64, 64, 0 };
    tex64 = tex_build(&l, mipfn, NULL);
    l.w = l.h = 128;
    tex128 = tex_build(&l, mipfn, NULL);
    l.w = l.h = 256;
    tex256 = tex_build(&l, mipfn, NULL);
}

static void test_describe(void)
{
    int c;
    for(c = 0; c < 300; c++)
        label(0, c, "%s, F %s0x%x (attr %08x)", rows[c / 20].name, rows[c / 20].neg ? "-" : "", 0x9234 + 2 * (c % 20), (unsigned)attr(c));
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
        tcw.TexAddr = (r->texU == 5 ? tex256 : r->texU == 4 ? tex128 : tex64) >> 3;
        ta_polygon(pcw, isp, tsp, tcw);
        for(k = 0; k < 4; k++)
            ta_vertex3(f32b_int(cell_px(c) + (k & 1) * 32), f32b_int(cell_py(c) + (k >> 1) * 32), r->n == 605 ? 0x21400000u /* 1.5 2^-61 */ : (uint32_t)(127 - r->n) << 23,
                       mid, a, 0xffffffffu, 0, k == 3);
    }
    ta_list_end();
}
