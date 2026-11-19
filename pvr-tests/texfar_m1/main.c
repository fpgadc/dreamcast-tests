/* texfar_m1: what the TSP's divider makes of a 1/w of -1 grid unit (2026-11-19).

   texfar_q256's lines (one triangle per cell whose 1/w is exactly -1 grid unit at the first pixel of the cell's
   line 15: TL (x0, y0 + 15) +2^k, TR (x0 + 64, y0 + 15) 2^k (1 - 2^-21), BL (x0, y0 + 16) -2^k) showed the other
   coordinate (a flat attribute, so also -1 unit at that pixel) a hair below zero on the console, where refsw3,
   which reads -1 unit as -2^-143 in the numerator and 2^-143 as the denominator, had -1. Here the attribute is
   +-um 2^p at the top and -+um 2^p at the bottom (um the 16 bit mantissa: P1 0xa5a6, P2 0x8000), so that
   attribute / w at that pixel (num) has its top bit at 2^(T - 1), T = -122 .. 40 by column, and the quotient num / D
   shows D, whatever the divider reads, through a 1024 texel wrapping texture: bits 2^-10 .. 2^-1 of the quotient.
   p per cell and refsw3's num and quotient: agents/nulldc3/hunt/texfar/m1gen.cpp (-999: nothing drawn: p out of
   range or 1/w not -1 unit there). Rows: k, sign, pattern, filter (rows[]). Texel (x, y) is 565 with red x & 31,
   green (x >> 5) & 63, blue y & 31. */
#define NUM_PAGES 1
#include "pvrtest2.h"
#include "texgen.h"

/* m1gen: p per cell (-999: no quad) */
static const short P[300] = {
    -103, -101, -99, -97, -95, -93, -91, -89, -85, -81, -71, -61, -41, -21, -1, 19, 29, 39, 49, 59,
    -113, -111, -109, -107, -105, -103, -101, -99, -95, -91, -81, -71, -51, -31, -11, 9, 19, 29, 39, 49,
    -123, -121, -119, -117, -115, -113, -111, -109, -105, -101, -91, -81, -61, -41, -21, -1, 9, 19, 29, 39,
    -999, -999, -999, -999, -125, -123, -121, -119, -115, -111, -101, -91, -71, -51, -31, -11, -1, 9, 19, 29,
    -999, -999, -999, -999, -999, -999, -999, -999, -999, -999, -121, -111, -91, -71, -51, -31, -21, -11, -1, 9,
    -999, -999, -999, -999, -999, -999, -999, -999, -999, -999, -999, -999, -121, -101, -81, -61, -51, -41, -31, -21,
    -999, -999, -999, -999, -999, -999, -999, -999, -999, -999, -999, -999, -999, -999, -121, -101, -91, -81, -71, -61,
    -103, -101, -99, -97, -95, -93, -91, -89, -85, -81, -71, -61, -41, -21, -1, 19, 29, 39, 49, 59,
    -999, -999, -999, -999, -999, -999, -999, -999, -999, -999, -121, -111, -91, -71, -51, -31, -21, -11, -1, 9,
    -999, -999, -999, -999, -999, -999, -999, -999, -999, -999, -999, -999, -999, -999, -121, -101, -91, -81, -71, -61,
    -103, -101, -99, -97, -95, -93, -91, -89, -85, -81, -71, -61, -41, -21, -1, 19, 29, 39, 49, 59,
    -999, -999, -999, -999, -999, -999, -999, -999, -999, -999, -121, -111, -91, -71, -51, -31, -21, -11, -1, 9,
    -103, -101, -99, -97, -95, -93, -91, -89, -85, -81, -71, -61, -41, -21, -1, 19, 29, 39, 49, 59,
    -999, -999, -999, -999, -999, -999, -999, -999, -999, -999, -121, -111, -91, -71, -51, -31, -21, -11, -1, 9,
    -999, -999, -999, -999, -999, -999, -999, -999, -999, -999, -999, -999, -999, -999, -121, -101, -91, -81, -71, -61,
};

typedef struct { int k, neg, um, bil; } row_t;
static const row_t rows[15] = {
    { -20, 0, 0xa5a6, 0 }, { -10, 0, 0xa5a6, 0 }, { 0, 0, 0xa5a6, 0 }, { 10, 0, 0xa5a6, 0 }, { 30, 0, 0xa5a6, 0 },
    { 60, 0, 0xa5a6, 0 }, { 100, 0, 0xa5a6, 0 }, { -20, 1, 0xa5a6, 0 }, { 30, 1, 0xa5a6, 0 }, { 100, 1, 0xa5a6, 0 },
    { -20, 0, 0x8000, 0 }, { 30, 0, 0x8000, 0 }, { -20, 0, 0xa5a6, 1 }, { 30, 0, 0xa5a6, 1 }, { 100, 0, 0xa5a6, 1 },
};
static const int T[20] = { -122, -120, -118, -116, -114, -112, -110, -108, -104, -100, -90, -80, -60, -40, -20, 0, 10, 20, 30, 40 };
static uint32_t tex;

static uint32_t c565(int r, int g, int b) { return (uint32_t)(r & 31) << 11 | (uint32_t)(g & 63) << 5 | (uint32_t)(b & 31); }
static uint32_t plain(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; return c565(x, x >> 5, y); }

static void test_setup(void)
{
    tex_layout_t l = { TEX_RASTER, 16, 1024, 8, 0 };
    tex = tex_build(&l, plain, NULL);
}

static void test_describe(void)
{
    int c;
    for(c = 0; c < 300; c++) {
        const row_t *r = &rows[c / 20];
        label(0, c, "k %d, %s%s%s, num top %d (p %d)", r->k, r->neg ? "-" : "", r->um == 0x8000 ? "P2" : "P1", r->bil ? ", bilinear" : "",
              T[c % 20], P[c]);
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
        const uint32_t a = (uint32_t)(127 + r->k) << 23;
        const uint32_t z[3] = { a, a - 4, a | 0x80000000u };                           /* TL, TR, BL */
        const uint32_t att = (uint32_t)(127 + P[c]) << 23 | (uint32_t)(r->um - 0x8000) << 8;
        const uint32_t mid = (uint32_t)(127 - 1 - 3) << 23;                            /* the middle of row 0 */
        if(P[c] == -999)
            continue;
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
        tsp.TexU = 7;
        tsp.TexV = 0;
        tcw.PixelFmt = PixelFmt_565;
        tcw.ScanOrder = 1;
        tcw.TexAddr = tex >> 3;
        ta_polygon(pcw, isp, tsp, tcw);
        for(k = 0; k < 3; k++) {
            const uint32_t at = att ^ ((uint32_t)((k == 2) ^ r->neg) << 31);
            ta_vertex3(f32b_int(cell_px(c) + (k == 1) * 64), f32b_int(cell_py(c) + 15 + (k == 2)), z[k], at, mid, 0xffffffffu, 0, k == 2);
        }
    }
    ta_list_end();
}
