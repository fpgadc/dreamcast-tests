/* SupSample (TSP bit 12, "super sample texture"): what a console does with it (2026-11-18). No test set it before;
   Outtrigger's menu portraits (polly2-data outtrigger_menu: 128x128 VQ 4444, point sampled, modulate alpha,
   blended) do, and refsw3, which ignored the bit, missed 20,610 pixels there (mrdriller_start and _ingame too).

   The console's rule, refsw3 = console on all 19 pages (and on outtrigger_menu, mrdriller_*):
   - the texture is read at four points: the pixel's centre and half a pixel right, down and both, each with the
     polygon's own filter (point, bilinear) and the centre's level;
   - the planes are evaluated for them in half pixel steps: along the run (2 n + hx) steps of A / 2, the centre's
     point included, instead of n of A; half a line down is the line's value plus B / 2 (each half coefficient cut
     to the evaluator's grid like a whole one). Evaluating the plane at the points themselves misses 870 pixels of
     page 15, whole steps plus a half 798, every line in half steps too 2,119;
   - each point is shaded on its own (texture with the base and offset colours) and the four colours averaged: each
     row's two, (a + b) >> 1, then the rows. The four texels averaged before shading miss 56,421 pixels of page 18;
     the sum >> 2 4,334 of Outtrigger's.

   Every cell is a quad with a 16x16 texture of arbitrary texels (each format's every bit used), at depth 1, decal,
   (ONE, ZERO), drawn into a 0888 framebuffer: the pixel is the colour of the texel the TSP made. The coordinates
   are linear on screen: u = (p + 16 X nu) / 1024 at the corner X of the cell (p a phase of 1/64 texel, nu / 4
   texels a pixel, 1 .. 12), v likewise along Y with its own nv and phase; cells take every pair of scales.
     page 0  565, point              page 1  565, bilinear
     page 2  4444, point             page 3  4444, bilinear
     page 4  1555, point             page 5  1555, bilinear
     page 6  4444 VQ, point (Outtrigger's kind)                 page 7  4444 VQ, bilinear
     page 8  565, point, SupSample off (the control: refsw3 has this already)
   The alpha, which a 0888 framebuffer does not keep: the same cells blended (SRC_ALPHA, ZERO) on black, so the
   colour is the texel's colour times its alpha (Outtrigger's portraits are blended with theirs):
     page 9  4444, point             page 10 4444, bilinear
     page 11 1555, point             page 12 1555, bilinear
     page 13 4444 VQ, point          page 14 4444 VQ, bilinear
   Coordinates that are not round (Outtrigger's are arbitrary floats): u = (P + X Sx + Y Sy) / 2^22 at the corner
   (X, Y), every cell its own 21 bit P and slopes of 1/4 .. 1 texel a pixel along one axis and +-1/4 along the
   other (a turned, sheared texture), v likewise:
     page 15 4444 VQ, point          page 16 4444 VQ, bilinear          page 17 565, point
     page 18 4444 VQ, point, as Outtrigger's portraits: modulate alpha with a vertex colour of the cell's own,
             (SRC_ALPHA, INV_SRC_ALPHA) in the translucent list over a background of 0x803020 */
#define NUM_PAGES 19
#include "pvrtest2.h"
#include "texgen.h"

static uint32_t tex[4];                 /* 565, 4444, 1555 twiddled; 4444 VQ */

static uint32_t rnd16(const void *ctx, int size, int x, int y)
{
    uint32_t h = (uint32_t)(y * 16 + x) * 2654435761u + *(const uint32_t *)ctx;
    (void)size;
    h ^= h >> 15; h *= 2246822519u; h ^= h >> 13;
    return h & 0xffff;
}

/* VQ: 16x16 texels are 64 entries of 2x2, all different */
static void test_setup(void)
{
    static const uint32_t seed[4] = { 0x1234567u, 0x89abcdeu, 0x5a5a5a5u, 0x3c3c3c3u };
    int k;
    for(k = 0; k < 4; k++) {
        const tex_layout_t l = { k == 3 ? TEX_VQ : TEX_TWIDDLED, 16, 16, 16, 0 };
        tex[k] = tex_build(&l, rnd16, &seed[k]);
    }
}

/* the alpha pages 9 .. 14 are pages 2 .. 7 blended; 15 .. 18 have coordinates that are not round */
static int pg_base(int page) { return page == 17 ? 0 : page == 16 ? 7 : page >= 15 ? 6 : page >= 9 ? page - 7 : page; }
static int pg_fmt(int page) { page = pg_base(page); return page == 0 || page == 1 || page == 8 ? PixelFmt_565 : page == 4 || page == 5 ? PixelFmt_1555 : PixelFmt_4444; }
static int pg_tex(int page) { return pg_base(page) >= 6 && pg_base(page) <= 7 ? 3 : pg_fmt(page) == PixelFmt_565 ? 0 : pg_fmt(page) == PixelFmt_4444 ? 1 : 2; }

/* a cell's scales (quarter texels a pixel) and phases (1/64 texel) */
static void cell(int c, int *nu, int *nv, int *pu, int *pv)
{
    uint32_t h = (uint32_t)c * 2654435761u + 77u;
    h ^= h >> 15; h *= 2246822519u; h ^= h >> 13;
    *nu = 1 + c % 12;
    *nv = 1 + (c / 12) % 12;
    *pu = (int)(h & 1023);
    *pv = (int)((h >> 10) & 1023);
}

/* the not round pages: a cell's P, slopes along x and y for u and v, and its colour */
static uint32_t hsh(uint32_t v) { v *= 2654435761u; v ^= v >> 15; v *= 2246822519u; v ^= v >> 13; return v; }
static void rcell(int c, int32_t q[6], uint32_t *col)
{
    const uint32_t h0 = hsh((uint32_t)c * 7u + 1u), h1 = hsh((uint32_t)c * 7u + 2u), h2 = hsh((uint32_t)c * 7u + 3u),
                   h3 = hsh((uint32_t)c * 7u + 4u), h4 = hsh((uint32_t)c * 7u + 5u), h5 = hsh((uint32_t)c * 7u + 6u);
    q[0] = (int32_t)(h0 & 0x1fffff);                                   /* Pu */
    q[1] = (int32_t)(0x10000 + (h1 & 0x2ffff));                        /* u along x: 1/4 .. 1 texel (2^18) */
    q[2] = (int32_t)(h2 & 0x1ffff) - 0x10000;                          /* u along y: -1/4 .. 1/4 */
    q[3] = (int32_t)(h3 & 0x1fffff);                                   /* Pv */
    q[4] = (int32_t)(h4 & 0x1ffff) - 0x10000;                          /* v along x */
    q[5] = (int32_t)(0x10000 + (h5 & 0x2ffff));                        /* v along y */
    *col = hsh((uint32_t)c * 7u + 7u);
}

static void test_describe(void)
{
    static const char *fmt[3] = { "1555", "565", "4444" };
    int p, c, nu, nv, pu, pv;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 300; c++) {
            cell(c, &nu, &nv, &pu, &pv);
            if(p >= 15) {
                int32_t q[6]; uint32_t col;
                rcell(c, q, &col);
                label(p, c, "%s%s %s, not round: u (%d + %d X + %d Y) / 2^22, v (%d + %d X + %d Y) / 2^22%s", fmt[pg_fmt(p)], pg_tex(p) == 3 ? " VQ" : "",
                      pg_base(p) & 1 ? "bilinear" : "point", (int)q[0], (int)q[1], (int)q[2], (int)q[3], (int)q[4], (int)q[5], p == 18 ? ", modulate alpha, blended" : "");
            } else
            label(p, c, "%s%s %s%s: u %d/4 texel/px from %d/64, v %d/4 from %d/64", fmt[pg_fmt(p)], pg_tex(p) == 3 ? " VQ" : "",
                  pg_base(p) & 1 ? "bilinear" : "point", p == 8 ? ", no SupSample" : p >= 9 ? ", times alpha" : "", nu, pu, nv, pv);
        }
}

static void test_page(int page)
{
    int c, k, nu, nv, pu, pv;
    const int tr = page == 18;
    pvr_frame.bg_color = tr ? 0x803020 : page >= 9 && page <= 14 ? 0 : 0x000040;
    ta_list_begin(tr ? ListType_Translucent : ListType_Opaque);
    for(c = 0; c < 300; c++) {
        PCW pcw = { .full = 0 };
        ISP_TSP isp = { .full = 0 };
        TSP tsp = { .full = 0 };
        TCW tcw = { .full = 0 };
        const int x = cell_px(c), y = cell_py(c);
        cell(c, &nu, &nv, &pu, &pv);
        pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
        pcw.ListType = tr ? ListType_Translucent : ListType_Opaque;
        pcw.Group_En = 1;
        pcw.Strip_Len = 1;
        pcw.Texture = 1;
        isp.DepthMode = DepthMode_Always;
        isp.CullMode = CullMode_None;
        isp.Texture = 1;
        tsp.SrcInstr = page >= 9 && page <= 14 || tr ? BlendInstr_SrcAlpha : BlendInstr_One;
        tsp.DstInstr = tr ? BlendInstr_InvSrcAlpha : BlendInstr_Zero;
        tsp.FogCtrl = FogCtrl_None;
        tsp.UseAlpha = 1;
        tsp.ShadInstr = tr ? ShadInstr_ModulateAlpha : ShadInstr_Decal;
        tsp.FilterMode = pg_base(page) & 1 ? FilterMode_Bilinear : FilterMode_Point;
        tsp.SupSample = page != 8;
        tsp.MipMapD = 4;
        tsp.TexU = 1;                   /* 16 */
        tsp.TexV = 1;
        tcw.PixelFmt = (uint32_t)pg_fmt(page);
        tcw.VQ_Comp = pg_tex(page) == 3;
        tcw.TexAddr = tex[pg_tex(page)] >> 3;
        ta_polygon(pcw, isp, tsp, tcw);
        for(k = 0; k < 4; k++) {
            const int X = (k & 1) * 32, Y = (k >> 1) * 32;
            if(page >= 15) {
                int32_t q[6]; uint32_t col;
                rcell(c, q, &col);
                ta_vertex3(f32b_int(x + X), f32b_int(y + Y), F32B_ONE, f32b_scaled(q[0] + X * q[1] + Y * q[2], -22),
                           f32b_scaled(q[3] + X * q[4] + Y * q[5], -22), tr ? col : 0xffffffff, 0, k == 3);
            } else
            ta_vertex3(f32b_int(x + X), f32b_int(y + Y), F32B_ONE, f32b_scaled(pu + 16 * X * nu, -10),
                       f32b_scaled(pv + 16 * Y * nv, -10), 0xffffffff, 0, k == 3);
        }
    }
    ta_list_end();
}
