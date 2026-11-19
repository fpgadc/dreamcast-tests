/* SupSample and the mipmap level (2026-11-19; pvr-tests/supsample2 found it): each of the four points of a SupSample
   pixel takes its level and trilinear fraction from its OWN 1/w, ONE LEVEL FINER than a pixel without SupSample
   would (the points are half a pixel apart: as if the texels a pixel were halved); a pixel whose level would be
   the largest map with a fraction (level 0 .. 1 of the measure) is the largest map without one.

   Every level of the textures is ONE colour (every level its own), so the colour of a point sampled cell is the
   level it was given, a trilinear pass's colour that level times its fraction, and the average of four points that
   straddle two levels a mix of the two colours. Cells 0 .. 149 have SupSample; 150 .. 299 are the same without it
   (the control: what refsw3 had before). Coordinates are u = (P + X Sx + Y Sy) / 2^22 at the corners X, Y of the
   cell, v likewise.
   Flat pages (z = 1 at the four corners: one level and fraction for the whole cell): the slope along x is swept
   evenly in the logarithm, 1/16 .. 2^11 texels a pixel of the largest map (every level boundary, past the smallest
   map), cell c at 2^(c / 10 - 4) (cells 0 .. 149); along y a quarter of it at most, either sign.
   Perspective pages: every corner its own z, 1/8 .. 2, and slopes of 1/8 .. 8 texels a pixel at z = 1: the level
   changes inside the cell, and next to a change the four points of a pixel straddle it.
     page  0  256x256 mip 565, point, flat, D 4              page  1  the same, D 1 .. 15
     page  2  trilinear A, flat, D 4                          page  3  trilinear B, flat, D 4
     page  4  point, flat, DCalcCtrl 1, D 1 .. 15             page  5  trilinear A, flat, DCalcCtrl 1, D 1 .. 15
     page  6  8x8 mip 565 (four levels: the smallest map is reached early), point, flat, D 4
     page  7  the same, trilinear B
     page  8  256x256, point, perspective, D 4                page  9  point, perspective, D 1 .. 15, DCalcCtrl 1
     page 10  trilinear A, perspective                        page 11  trilinear B, perspective
     page 12  64x64 VQ mip 4444, bilinear, perspective, x alpha
     page 13  64x64 raster texture with the MipMapped bit (one level: the fraction fades the pass), trilinear A, flat
   "x alpha": (SRC_ALPHA, ZERO) on black; the others (ONE, ZERO) on 0x000040. */
#define NUM_PAGES 14
#include "pvrtest2.h"
#include "texgen.h"

enum { M256, M8, MVQ, RAST, NTEX };
typedef struct { int tex, filter, persp, dcalc, drand; } page_t;
static const page_t pages[NUM_PAGES] = {
    { M256, FilterMode_Point, 0, 0, 0 },
    { M256, FilterMode_Point, 0, 0, 1 },
    { M256, FilterMode_TrilinearA, 0, 0, 0 },
    { M256, FilterMode_TrilinearB, 0, 0, 0 },
    { M256, FilterMode_Point, 0, 1, 1 },
    { M256, FilterMode_TrilinearA, 0, 1, 1 },
    { M8, FilterMode_Point, 0, 0, 0 },
    { M8, FilterMode_TrilinearB, 0, 0, 0 },
    { M256, FilterMode_Point, 1, 0, 0 },
    { M256, FilterMode_Point, 1, 1, 1 },
    { M256, FilterMode_TrilinearA, 1, 0, 0 },
    { M256, FilterMode_TrilinearB, 1, 0, 0 },
    { MVQ, FilterMode_Bilinear, 1, 0, 0 },
    { RAST, FilterMode_TrilinearA, 0, 0, 0 },
};
#define CELLS 150
static const int tex_log[NTEX] = { 8, 3, 6, 6 };    /* log2 of the size */

static uint32_t tex[NTEX];

static uint32_t hsh(uint32_t v) { v *= 2654435761u; v ^= v >> 15; v *= 2246822519u; v ^= v >> 13; v *= 3266489917u; v ^= v >> 16; return v; }

static uint32_t level_colour(const void *ctx, int size, int x, int y)
{
    (void)x; (void)y;
    return hsh((uint32_t)size * 31u + *(const uint32_t *)ctx) & 0xffff;
}

static void test_setup(void)
{
    static const uint32_t seed[NTEX] = { 0x5150515u, 0x0f0f0f0u, 0x7a7a7a7u, 0x1234321u };
    static const int layout[NTEX] = { TEX_MIP, TEX_MIP, TEX_VQ_MIP, TEX_RASTER };
    int k;
    for(k = 0; k < NTEX; k++) {
        const tex_layout_t l = { layout[k], 16, 1 << tex_log[k], 1 << tex_log[k], 0 };
        tex[k] = tex_build(&l, level_colour, &seed[k]);
    }
}

static int tex_fmt(int t) { return t == MVQ ? PixelFmt_4444 : PixelFmt_565; }

typedef struct { int32_t pu, sux, suy, pv, svx, svy; uint32_t zm[4]; int mipd; } cell_t;

/* 2^(n / 10) in units of 2^-12, n >= 0 (a table of the tenths of an octave, then shifted) */
static uint32_t pow2_tenths(int n)
{
    static const uint32_t frac[10] = { 4096, 4390, 4705, 5043, 5405, 5793, 6208, 6654, 7132, 7643 };
    return frac[n % 10] << (n / 10);
}

static void cell(int page, int c, cell_t *p)
{
    const page_t *g = &pages[page];
    const int cc = c % CELLS;
    const uint32_t s = (uint32_t)cc * 977u + (uint32_t)page * 100003u;
    const int L = tex_log[g->tex];
    int k;
    int32_t m;
    p->mipd = g->drand ? 1 + (int)(hsh(s + 40) % 15) : 4;
    if(!g->persp) {
        /* 2^(cc / 10 - 4) texels a pixel of the largest map (size 2^L): u per pixel 2^(cc / 10 - 4 - L), in units
           of 2^-22: 2^(cc / 10 + 18 - L); capped where X * S would not fit */
        const uint64_t v = L >= 6 ? (uint64_t)pow2_tenths(cc) >> (L - 6) : (uint64_t)pow2_tenths(cc) << (6 - L);
        m = (int32_t)(v > (1u << 25) ? 1u << 25 : v);
        for(k = 0; k < 4; k++)
            p->zm[k] = 1u << 22;
    } else {
        const uint32_t h = hsh(s + 4);
        const int e = 19 - L + (int)(h % 7);                  /* 2^(19 - L) .. : 1/8 .. 8 texels a pixel */
        m = (int32_t)((1u << e) + ((h >> 8) & ((1u << e) - 1)));
        for(k = 0; k < 4; k++)
            p->zm[k] = (1u << 19) + hsh(s + 10 + (uint32_t)k) % ((1u << 23) - (1u << 19));   /* / 2^22: 1/8 .. 2 */
    }
    if(hsh(s + 3) & 1) m = -m;
    p->sux = m;
    p->svy = (int32_t)(hsh(s + 5) % (uint32_t)((m < 0 ? -m : m) / 4 + 1)) * ((hsh(s + 6) & 1) ? -1 : 1);
    p->suy = (int32_t)(hsh(s + 7) % (uint32_t)((m < 0 ? -m : m) / 8 + 1));
    p->svx = (int32_t)(hsh(s + 8) % (uint32_t)((m < 0 ? -m : m) / 8 + 1));
    p->pu = (int32_t)(hsh(s + 1) & 0x3fffff);
    p->pv = (int32_t)(hsh(s + 2) & 0x3fffff);
}

static void test_describe(void)
{
    static const char *tn[NTEX] = { "256 mip", "8 mip", "64 VQ mip", "64 raster with MipMapped" };
    static const char *fn[4] = { "point", "bilinear", "trilinear A", "trilinear B" };
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 300; c++) {
            const page_t *g = &pages[p];
            cell_t q;
            cell(p, c, &q);
            label(p, c, "%s%s %s %s%s, D %d: u (%d + %d X + %d Y) / 2^22, v (%d + %d X + %d Y) / 2^22, z %u %u %u %u / 2^22",
                  c < CELLS ? "" : "no SupSample: ", tn[g->tex], fn[g->filter], g->persp ? "perspective" : "flat", g->dcalc ? ", DCalcCtrl 1" : "",
                  q.mipd, (int)q.pu, (int)q.sux, (int)q.suy, (int)q.pv, (int)q.svx, (int)q.svy,
                  (unsigned)q.zm[0], (unsigned)q.zm[1], (unsigned)q.zm[2], (unsigned)q.zm[3]);
        }
}

static void test_page(int page)
{
    const page_t *g = &pages[page];
    const int xa = g->tex == MVQ;
    int c, k;
    pvr_frame.bg_color = xa ? 0 : 0x000040;
    ta_list_begin(ListType_Opaque);
    for(c = 0; c < 300; c++) {
        PCW pcw = { .full = 0 };
        ISP_TSP isp = { .full = 0 };
        TSP tsp = { .full = 0 };
        TCW tcw = { .full = 0 };
        const int x = cell_px(c), y = cell_py(c);
        cell_t q;
        cell(page, c, &q);
        pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
        pcw.ListType = ListType_Opaque;
        pcw.Group_En = 1;
        pcw.Strip_Len = 1;
        pcw.Texture = 1;
        isp.DepthMode = DepthMode_Always;
        isp.CullMode = CullMode_None;
        isp.Texture = 1;
        isp.DCalcCtrl = (uint32_t)g->dcalc;
        tsp.SrcInstr = xa ? BlendInstr_SrcAlpha : BlendInstr_One;
        tsp.DstInstr = BlendInstr_Zero;
        tsp.FogCtrl = FogCtrl_None;
        tsp.UseAlpha = 1;
        tsp.ShadInstr = ShadInstr_Decal;
        tsp.FilterMode = (uint32_t)g->filter;
        tsp.SupSample = c < CELLS;
        tsp.MipMapD = (uint32_t)q.mipd;
        tsp.TexU = (uint32_t)(tex_log[g->tex] - 3);
        tsp.TexV = tsp.TexU;
        tcw.PixelFmt = (uint32_t)tex_fmt(g->tex);
        tcw.VQ_Comp = g->tex == MVQ;
        tcw.MipMapped = 1;
        tcw.ScanOrder = g->tex == RAST;
        tcw.TexAddr = tex[g->tex] >> 3;
        ta_polygon(pcw, isp, tsp, tcw);
        for(k = 0; k < 4; k++) {
            const int X = (k & 1) * 32, Y = (k >> 1) * 32;
            ta_vertex3(f32b_int(x + X), f32b_int(y + Y), f32b_scaled((int32_t)q.zm[k], -22),
                       f32b_scaled(q.pu + X * q.sux + Y * q.suy, -22), f32b_scaled(q.pv + X * q.svx + Y * q.svy, -22),
                       0xffffffffu, 0, k == 3);
        }
    }
    ta_list_end();
}
