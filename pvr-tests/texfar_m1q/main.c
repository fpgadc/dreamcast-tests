/* texfar_m1q: huge quotients over a 1/w of -1 grid unit, the largest quotients there are (2026-11-19).

   A 1/w that evaluates to exactly -1 grid unit is read by the divider as 0xFFFF 2^-143, just under 2^-127
   (texfar_m1; refsw3 pvr_tsp_plane.h DenominatorSeen), the smallest denominator but for a zero one (2^-128,
   texfar_quot), so this is where the quotient is largest; first written (as texfar_q256) for 2^-143 and quotients
   up to 2^259, which it showed are not there. Every cell one triangle, TL (x0, y0 + 15), TR (x0 + 64, y0 + 15),
   BL (x0, y0 + 16), which covers the 32 pixels of the cell's line 15 and nothing else: 1/w +2^k at the top, -2^k
   at the bottom, and 2^-21 2^k less at TR, so that on that line the plane is exactly 0 but for a slope of
   -2^(k - 27) a pixel, which rounds down to -1 unit at the line's first pixel (the run steps on by floor(A) = -1
   unit a pixel: -2, -3 .. units further on, ordinary numbers); the attribute +2^p at the top and -2^p at the bottom
   (or the other way round), so attribute / w is 2^(p + k) on the line and the quotient at the first pixel about
   2^(p + k + 127). k, p and the quotient's top QT (the quotient in [2^(QT - 1), 2^QT)) per cell from
   agents/nulldc3/hunt/texfar/q256gen.cpp, which runs refsw3's planes and divider at each cell's place (the table
   below; a place that cannot reach the column's QT has the largest below it). In 1/256 texel F = q 2^s, s =
   log2 size + 8: up to 2^261. Rows: rows[]. Texel (x, y) is 565 with red x & 31, green (x >> 5) & 63, blue y & 31. */
#define NUM_PAGES 1
#include "pvrtest2.h"
#include "texgen.h"

/* q256gen: k, p and the quotient's top per cell */
static const signed char KP[300][2] = {
    {100,16}, {100,15}, {100,14}, {100,13}, {100,12}, {100,11}, {100,10}, {100,9}, {100,7}, {100,5}, {100,1}, {100,-3}, {100,-7}, {100,-13}, {100,-23}, {100,-43}, {100,-63}, {100,-83}, {100,-93}, {100,-98},
    {100,16}, {100,15}, {100,14}, {100,13}, {100,12}, {100,11}, {100,10}, {100,9}, {100,7}, {100,5}, {100,1}, {100,-3}, {100,-7}, {100,-13}, {100,-23}, {100,-43}, {100,-63}, {100,-83}, {100,-93}, {100,-98},
    {100,15}, {100,15}, {100,14}, {100,13}, {100,12}, {100,11}, {100,10}, {100,9}, {100,7}, {100,5}, {100,1}, {100,-3}, {100,-7}, {100,-13}, {100,-23}, {100,-43}, {100,-63}, {100,-83}, {100,-93}, {100,-98},
    {100,15}, {100,15}, {100,14}, {100,13}, {100,12}, {100,11}, {100,10}, {100,9}, {100,7}, {100,5}, {100,1}, {100,-3}, {100,-7}, {100,-13}, {100,-23}, {100,-43}, {100,-63}, {100,-83}, {100,-93}, {100,-98},
    {100,14}, {100,14}, {100,14}, {100,13}, {100,12}, {100,11}, {100,10}, {100,9}, {100,7}, {100,5}, {100,1}, {100,-3}, {100,-7}, {100,-13}, {100,-23}, {100,-43}, {100,-63}, {100,-83}, {100,-93}, {100,-98},
    {100,14}, {100,14}, {100,14}, {100,13}, {100,12}, {100,11}, {100,10}, {100,9}, {100,7}, {100,5}, {100,1}, {100,-3}, {100,-7}, {100,-13}, {100,-23}, {100,-43}, {100,-63}, {100,-83}, {100,-93}, {100,-98},
    {100,14}, {100,14}, {100,14}, {100,13}, {100,12}, {100,11}, {100,10}, {100,9}, {100,7}, {100,5}, {100,1}, {100,-3}, {100,-7}, {100,-13}, {100,-23}, {100,-43}, {100,-63}, {100,-83}, {100,-93}, {100,-98},
    {100,14}, {100,14}, {100,14}, {100,13}, {100,12}, {100,11}, {100,10}, {100,9}, {100,7}, {100,5}, {100,1}, {100,-3}, {100,-7}, {100,-13}, {100,-23}, {100,-43}, {100,-63}, {100,-83}, {100,-93}, {100,-98},
    {100,13}, {100,13}, {100,13}, {100,13}, {100,12}, {100,11}, {100,10}, {100,9}, {100,7}, {100,5}, {100,1}, {100,-3}, {100,-7}, {100,-13}, {100,-23}, {100,-43}, {100,-63}, {100,-83}, {100,-93}, {100,-98},
    {100,13}, {100,13}, {100,13}, {100,13}, {100,12}, {100,11}, {100,10}, {100,9}, {100,7}, {100,5}, {100,1}, {100,-3}, {100,-7}, {100,-13}, {100,-23}, {100,-43}, {100,-63}, {100,-83}, {100,-93}, {100,-98},
    {100,13}, {100,13}, {100,13}, {100,13}, {100,12}, {100,11}, {100,10}, {100,9}, {100,7}, {100,5}, {100,1}, {100,-3}, {100,-7}, {100,-13}, {100,-23}, {100,-43}, {100,-63}, {100,-83}, {100,-93}, {100,-98},
    {100,13}, {100,13}, {100,13}, {100,13}, {100,12}, {100,11}, {100,10}, {100,9}, {100,7}, {100,5}, {100,1}, {100,-3}, {100,-7}, {100,-13}, {100,-23}, {100,-43}, {100,-63}, {100,-83}, {100,-93}, {100,-98},
    {100,13}, {100,13}, {100,13}, {100,13}, {100,12}, {100,11}, {100,10}, {100,9}, {100,7}, {100,5}, {100,1}, {100,-3}, {100,-7}, {100,-13}, {100,-23}, {100,-43}, {100,-63}, {100,-83}, {100,-93}, {100,-98},
    {100,13}, {100,13}, {100,13}, {100,13}, {100,12}, {100,11}, {100,10}, {100,9}, {100,7}, {100,5}, {100,1}, {100,-3}, {100,-7}, {100,-13}, {100,-23}, {100,-43}, {100,-63}, {100,-83}, {100,-93}, {100,-98},
    {100,13}, {100,13}, {100,13}, {100,13}, {100,12}, {100,11}, {100,10}, {100,9}, {100,7}, {100,5}, {100,1}, {100,-3}, {100,-7}, {100,-13}, {100,-23}, {100,-43}, {100,-63}, {100,-83}, {100,-93}, {100,-98},
};
static const short QT[300] = {
    243, 242, 241, 240, 239, 238, 237, 236, 234, 232, 228, 224, 220, 214, 204, 184, 164, 144, 134, 129,
    243, 242, 241, 240, 239, 238, 237, 236, 234, 232, 228, 224, 220, 214, 204, 184, 164, 144, 134, 129,
    242, 242, 241, 240, 239, 238, 237, 236, 234, 232, 228, 224, 220, 214, 204, 184, 164, 144, 134, 129,
    242, 242, 241, 240, 239, 238, 237, 236, 234, 232, 228, 224, 220, 214, 204, 184, 164, 144, 134, 129,
    241, 241, 241, 240, 239, 238, 237, 236, 234, 232, 228, 224, 220, 214, 204, 184, 164, 144, 134, 129,
    241, 241, 241, 240, 239, 238, 237, 236, 234, 232, 228, 224, 220, 214, 204, 184, 164, 144, 134, 129,
    241, 241, 241, 240, 239, 238, 237, 236, 234, 232, 228, 224, 220, 214, 204, 184, 164, 144, 134, 129,
    241, 241, 241, 240, 239, 238, 237, 236, 234, 232, 228, 224, 220, 214, 204, 184, 164, 144, 134, 129,
    240, 240, 240, 240, 239, 238, 237, 236, 234, 232, 228, 224, 220, 214, 204, 184, 164, 144, 134, 129,
    240, 240, 240, 240, 239, 238, 237, 236, 234, 232, 228, 224, 220, 214, 204, 184, 164, 144, 134, 129,
    240, 240, 240, 240, 239, 238, 237, 236, 234, 232, 228, 224, 220, 214, 204, 184, 164, 144, 134, 129,
    240, 240, 240, 240, 239, 238, 237, 236, 234, 232, 228, 224, 220, 214, 204, 184, 164, 144, 134, 129,
    240, 240, 240, 240, 239, 238, 237, 236, 234, 232, 228, 224, 220, 214, 204, 184, 164, 144, 134, 129,
    240, 240, 240, 240, 239, 238, 237, 236, 234, 232, 228, 224, 220, 214, 204, 184, 164, 144, 134, 129,
    240, 240, 240, 240, 239, 238, 237, 236, 234, 232, 228, 224, 220, 214, 204, 184, 164, 144, 134, 129,
};

typedef struct { const char *name; int tex; int clampU, clampV, flipU, bil, onV, neg; } row_t;
enum { T8, T64, T1024, T8x1024 };
static const int texU[4] = { 0, 3, 7, 0 }, texV[4] = { 0, 0, 0, 7 };
static const row_t rows[15] = {
    { "8 ClampU",                       T8,      1, 0, 0, 0, 0, 0 },
    { "8 wrap, below zero",             T8,      0, 0, 0, 0, 0, 1 },
    { "1024 ClampU",                    T1024,   1, 0, 0, 0, 0, 0 },
    { "1024 wrap, below zero",          T1024,   0, 0, 0, 0, 0, 1 },
    { "8 ClampU bilinear",              T8,      1, 0, 0, 1, 0, 0 },
    { "8 wrap bilinear, below zero",    T8,      0, 0, 0, 1, 0, 1 },
    { "8x1024 ClampV",                  T8x1024, 0, 1, 0, 0, 1, 0 },
    { "1024 wrap bilinear, below zero", T1024,   0, 0, 0, 1, 0, 1 },
    { "64 ClampU",                      T64,     1, 0, 0, 0, 0, 0 },
    { "8 wrap",                         T8,      0, 0, 0, 0, 0, 0 },
    { "1024 wrap",                      T1024,   0, 0, 0, 0, 0, 0 },
    { "8 FlipU",                        T8,      0, 0, 1, 0, 0, 0 },
    { "8 FlipU bilinear",               T8,      0, 0, 1, 1, 0, 0 },
    { "8 ClampU, below zero",           T8,      1, 0, 0, 0, 0, 1 },
    { "1024 wrap bilinear",             T1024,   0, 0, 0, 1, 0, 0 },
};
static uint32_t tex[4];

static uint32_t c565(int r, int g, int b) { return (uint32_t)(r & 31) << 11 | (uint32_t)(g & 63) << 5 | (uint32_t)(b & 31); }
static uint32_t plain(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; return c565(x, x >> 5, y); }

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
        label(0, c, "%s, q top %d (k %d, p %d)", rows[c / 20].name, QT[c], KP[c][0], KP[c][1]);
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
        const uint32_t a = (uint32_t)(127 + KP[c][0]) << 23;
        const uint32_t z[3] = { a, a - 4, a | 0x80000000u };                           /* TL, TR, BL */
        const uint32_t att = (uint32_t)(127 + KP[c][1]) << 23;
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
        tcw.PixelFmt = PixelFmt_565;
        tcw.ScanOrder = 1;
        tcw.TexAddr = tex[r->tex] >> 3;
        ta_polygon(pcw, isp, tsp, tcw);
        for(k = 0; k < 3; k++) {
            const uint32_t at = att ^ ((uint32_t)((k == 2) ^ r->neg) << 31);       /* +2^p at the top, -2^p at the bottom */
            ta_vertex3(f32b_int(cell_px(c) + (k == 1) * 64), f32b_int(cell_py(c) + 15 + (k == 2)), z[k],
                       r->onV ? mid : at, r->onV ? at : mid, 0xffffffffu, 0, k == 2);
        }
    }
    ta_list_end();
}
