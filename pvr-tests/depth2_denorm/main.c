/* Positive depths below 2^-126 against each other (2026-11-20, hunt depth2; after depthgaps_edges, depthgaps_tiny).

   pvr_isp_depth.h GAP: a pixel of a sloped plane whose depth lies between 0 and 2^-126 is above 0 and below 2^-126
   (depthgaps_edges), refsw3 stores it as a float's denormal (2^-149 grid, ordered by value); how such depths order
   among each other (2^-128 against 2^-127 against 1.5 2^-127, and on what grid) is not measured. Here two planes P
   (DepthMode Always, green) and Q (the page's mode, white where it passes) over one tile each, triangles (X, Y),
   (X + 64, Y), (X, Y + 64), corner depths z = n0 + nx x + ny y in units of 2^-146: slopes of 2^-126 .. 2^-125 a
   pixel (smaller ones are zero), the pixels j = i - 1 of the tile between 0 and 2^-126, on the 2^-145 grid of the
   start, from about 2^-138 up; Q a little off P (its n0, nx, ny moved: differences of 0 .. a few hundred 2^-145).
   gen.cpp picked the 70 cases (tiles (2a, 2b)) so that the grids such a store could have (2^-145 .. 2^-128, cut
   or rounded, or one word for all) give different pictures. Pages 0 .. 2: Greater, Equal, Less. Pages 3 .. 5: a
   second set of 70 (CASES2), Q the same plane as P moved by 2^-141 .. 2^-126 either way: the two depths of a pixel
   in different binades (2^-128 against 2^-127 against 1.5 2^-127 ..), Greater, Equal, Less. */
#define NUM_PAGES 6
#include "pvrtest2.h"
#include "texgen.h"

typedef struct { int64_t n0, nx, ny; } plane_t;
typedef struct { int tx, ty; plane_t p, q; } case_t;
#include "cases.h"

static const int MD[3] = { DepthMode_Greater, DepthMode_Equal, DepthMode_Less };
static const char *MN[3] = { "Greater", "Equal", "Less" };
static uint32_t texG, texW;

static uint32_t green(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0x07e0; }
static uint32_t white(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0xffff; }

/* n 2^-146 as a float (gen.cpp only uses exact ones) */
static uint32_t fbits(int64_t n)
{
    const uint32_t sign = n < 0 ? 0x80000000u : 0;
    uint64_t m = n < 0 ? (uint64_t)-n : (uint64_t)n;
    int e = -146;
    if(!m)
        return sign;
    while(m >= (1ull << 24)) {
        m >>= 1;
        e++;
    }
    while(m < (1ull << 23)) {
        m <<= 1;
        e--;
    }
    return sign | (uint32_t)(e + 150) << 23 | (uint32_t)(m & 0x7fffffu);
}

static void test_setup(void)
{
    const tex_layout_t l = { TEX_TWIDDLED, 16, 8, 8, 0 };
    texG = tex_build(&l, green, NULL);
    texW = tex_build(&l, white, NULL);
}

static void test_describe(void)
{
    int p, c;
    for(p = 0; p < NUM_PAGES; p++) {
        const case_t *cs = p < 3 ? CASES : CASES2;
        const int n = p < 3 ? NCASES : NCASES2;
        for(c = 0; c < n; c++)
            label(p, cs[c].ty * COLS + cs[c].tx, "P n0 %lld nx %lld ny %lld, Q n0 %lld nx %lld ny %lld (2^-146), Q %s", (long long)cs[c].p.n0,
                  (long long)cs[c].p.nx, (long long)cs[c].p.ny, (long long)cs[c].q.n0, (long long)cs[c].q.nx, (long long)cs[c].q.ny, MN[p % 3]);
    }
}

static void tri(int X, int Y, const plane_t *pl, int mode, uint32_t tex)
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
    tcw.TexAddr = tex >> 3;
    ta_polygon(pcw, isp, tsp, tcw);
    ta_vertex3(f32b_int(X), f32b_int(Y), fbits(pl->n0), 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, 0);
    ta_vertex3(f32b_int(X + 64), f32b_int(Y), fbits(pl->n0 + 64 * pl->nx), 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, 0);
    ta_vertex3(f32b_int(X), f32b_int(Y + 64), fbits(pl->n0 + 64 * pl->ny), 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, 1);
}

static void test_page(int page)
{
    const case_t *cs = page < 3 ? CASES : CASES2;
    const int n = page < 3 ? NCASES : NCASES2;
    int c;
    pvr_frame.bg_color = 0x000040;
    pvr_frame.bg_depth = 0;
    ta_list_begin(ListType_Opaque);
    for(c = 0; c < n; c++) {
        tri(32 * cs[c].tx, 32 * cs[c].ty, &cs[c].p, DepthMode_Always, texG);
        tri(32 * cs[c].tx, 32 * cs[c].ty, &cs[c].q, MD[page % 3], texW);
    }
    ta_list_end();
}
