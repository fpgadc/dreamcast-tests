/* texfar_mipv: the V coordinate of a mipmapped texture declared shorter than wide, far outside it (2026-11-19).

   A mipmapped texture is square; when its TSP word says TexV < TexU the V coordinate is made with the declared
   height (half texel and all) and then scaled up by 2^(TexU - TexV) to the full height (pvr-tests/sample,
   "declared"). texfar_mip showed that the scaling is of the whole coordinate: clamped, a v of 2^64 .. the largest
   float is the last row. Here the scaled coordinate's low bits: v has 17 significant bits (the divider's
   mantissa; attribute um 2^ea, 1/w flat, q = (2 um - 2) 2^(ea - 1), as texfar_bits) at every top from the declared
   height to past where they leave the texel bits after the scaling, wrapped, flipped and clamped, both signs, at
   the largest level (1/w = 2^-24) and smaller ones (2^-61, 2^-62: the level from w alone, as texfar_mip).
   Column c: F, the coordinate in 1/256 texel at the declared height, has its top bit at 2^(T - 1), T = s - 1 + c,
   s = TexV + 3 + 8. Patterns P1 um 0xa5a6 (q 0x14b4a), P2 um 0x8000 (q 0xffff). u is texel 0's middle. Texel
   (x, y) of the level 2^l wide: 565, red x & 31, green (x >> 5) & 1 | (y & 31) << 1, blue l | (y >> 5) << 4. */
#define NUM_PAGES 1
#include "pvrtest2.h"
#include "texgen.h"

typedef struct { const char *name; int texU, texV, w; int clampV, flipV, bil, neg, um; } row_t;
static const row_t rows[15] = {
    { "64x8 wrap, P1",                       3, 0, 24, 0, 0, 0, 0, 0xa5a6 },
    { "64x8 wrap, -P1",                      3, 0, 24, 0, 0, 0, 1, 0xa5a6 },
    { "64x8 FlipV, P1",                      3, 0, 24, 0, 1, 0, 0, 0xa5a6 },
    { "64x8 FlipV, -P1",                     3, 0, 24, 0, 1, 0, 1, 0xa5a6 },
    { "64x8 wrap bilinear, P1",              3, 0, 24, 0, 0, 1, 0, 0xa5a6 },
    { "64x8 wrap, P1, 1/w 2^-62",            3, 0, 62, 0, 0, 0, 0, 0xa5a6 },
    { "64x32 wrap, P1",                      3, 2, 24, 0, 0, 0, 0, 0xa5a6 },
    { "64x32 FlipV, -P1",                    3, 2, 24, 0, 1, 0, 1, 0xa5a6 },
    { "64x16 wrap, P1",                      3, 1, 24, 0, 0, 0, 0, 0xa5a6 },
    { "64x16 ClampV, P1",                    3, 1, 24, 1, 0, 0, 0, 0xa5a6 },
    { "64x16 ClampV, -P1",                   3, 1, 24, 1, 0, 0, 1, 0xa5a6 },
    { "256x8 wrap, P1",                      5, 0, 24, 0, 0, 0, 0, 0xa5a6 },
    { "256x8 FlipV bilinear, -P1",           5, 0, 24, 0, 1, 1, 1, 0xa5a6 },
    { "64x8 wrap, P2",                       3, 0, 24, 0, 0, 0, 0, 0x8000 },
    { "64x8 wrap bilinear, -P2, 1/w 2^-61",  3, 0, 61, 0, 0, 1, 1, 0x8000 },
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

static int top_of(int c) { return rows[c / 20].texV + 3 + 8 - 1 + c % 20; }
static uint32_t attr(int c)
{
    const row_t *r = &rows[c / 20];
    const int s = r->texV + 3 + 8, T = top_of(c);
    const int ea = r->um == 0x8000 ? T - 15 - s : T - 16 - s;      /* the attribute um 2^ea */
    return (uint32_t)r->neg << 31 | (uint32_t)(ea + 15 + 127) << 23 | (uint32_t)(r->um - 0x8000) << 8;
}

static void test_setup(void)
{
    tex_layout_t l = { TEX_MIP, 16, 64, 64, 0 };
    tex64 = tex_build(&l, mipfn, NULL);
    l.w = l.h = 256;
    tex256 = tex_build(&l, mipfn, NULL);
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
        tsp.MipMapD = 4;
        tsp.TexU = (uint32_t)r->texU;
        tsp.TexV = (uint32_t)r->texV;
        tsp.ClampV = (uint32_t)r->clampV;
        tsp.FlipV = (uint32_t)r->flipV;
        tcw.PixelFmt = PixelFmt_565;
        tcw.MipMapped = 1;
        tcw.TexAddr = (r->texU == 5 ? tex256 : tex64) >> 3;
        ta_polygon(pcw, isp, tsp, tcw);
        for(k = 0; k < 4; k++)
            ta_vertex3(f32b_int(cell_px(c) + (k & 1) * 32), f32b_int(cell_py(c) + (k >> 1) * 32), (uint32_t)(127 - r->w) << 23,
                       mid, a, 0xffffffffu, 0, k == 3);
    }
    ta_list_end();
}
