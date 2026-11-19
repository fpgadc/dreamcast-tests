/* FPU_CULL_VAL (0x078) at values no game uses: below zero, zeros, infinities, NaNs, a denormal (2026-11-19).

   refsw3 culls a triangle of CullMode 1-3 when |det| <= FPU_CULL_VAL (pvr-tests/raster page 4: equal is culled, at
   1.0), the determinant as the ISP's setup has it. Which value the compare takes when the register is not an
   ordinary positive float is not measured: a float compare (below zero nothing is culled, NaN nothing, +infinity
   everything), or the size alone as FPU_PERP_VAL does (pvr-tests/perpval: -1 acts as 1), or the bits as an integer.
   One register value a page; every cell a triangle (depth 1, DepthMode Always, opaque) of a known determinant:
     rows 0, 1    slivers 28 pixels long along x, w = q 2^-14 pixel high (det 28 w), winding A, B; CullMode small
     rows 2, 3    the same with w = q 2^-6
     rows 4, 5    right triangles of legs a, b on a grid of 1/8 (det a b exactly: 1/4 .. 192), winding A, B
     rows 6-9     rows 2, 3 with CullMode 2 (cull clockwise) and 3 (cull counterclockwise)
     rows 10-13   rows 4, 5 with CullMode 2 and 3
     row 14       row 2 with CullMode none (drawn whatever the register)
   q = 2, 3, 4, 6, 8, 11, 16, 23, 32, 45, 64, 91, 128, 181, 256, 362, 512, 724, 1024, 1448 (steps of sqrt 2) along
   the row. Pages: FPU_CULL_VAL = VAL[] below. */
#define NUM_PAGES 16
#include "pvrtest2.h"

static const uint32_t VAL[NUM_PAGES] = {
    0x3f800000u, 0xbf800000u, 0x40000000u, 0xc0000000u,     /* 1, -1, 2, -2 */
    0x3f000000u, 0xbf000000u, 0x00000000u, 0x80000000u,     /* 0.5, -0.5, +0, -0 */
    0x7f800000u, 0xff800000u, 0x7fc00000u, 0xffc00000u,     /* +inf, -inf, NaN, -NaN */
    0x7f800001u, 0x00000001u, 0x80000001u, 0x7149f2cau,     /* signalling NaN, 2^-149, -2^-149, 1e30 */
};
static const char *VALNAME[NUM_PAGES] = { "1", "-1", "2", "-2", "0.5", "-0.5", "+0", "-0", "+inf", "-inf", "NaN",
                                          "-NaN", "sNaN", "2^-149", "-2^-149", "1e30" };
static const int Q[20] = { 2, 3, 4, 6, 8, 11, 16, 23, 32, 45, 64, 91, 128, 181, 256, 362, 512, 724, 1024, 1448 };

/* legs of the exact triangles, in eighths: a b = 2^k (1 or 1.5), k = col / 2 - 2 */
static void legs(int col, int *a8, int *b8)
{
    const int k = col / 2 - 2, lo = k >= 0 ? k / 2 : -((-k + 1) / 2), hi = k - lo;    /* lo = floor(k / 2) */
    *a8 = (col & 1) ? 12 << (lo + 3) >> 3 : 8 << (lo + 3) >> 3;
    *b8 = 8 << (hi + 3) >> 3;
}

static int row_kind(int row) { return row == 14 ? 2 : row >= 10 ? 4 + (row & 1) : row >= 6 ? 2 + (row & 1) : row; }
static int row_cull(int row)
{
    if(row == 14) return CullMode_None;
    if(row >= 10) return row < 12 ? 2 : 3;
    if(row >= 6) return row < 8 ? 2 : 3;
    return CullMode_Small;
}

static void test_setup(void) { }

static void test_describe(void)
{
    static const char *kind[6] = { "sliver w = q 2^-14, A", "sliver w = q 2^-14, B", "sliver w = q 2^-6, A",
                                   "sliver w = q 2^-6, B", "legs a b, A", "legs a b, B" };
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 300; c++) {
            const int row = c / 20, col = c % 20, k = row_kind(row);
            if(k >= 4) {
                int a8, b8;
                legs(col, &a8, &b8);
                label(p, c, "CULL_VAL %s, CullMode %d, %s: a %d/8, b %d/8", VALNAME[p], row_cull(row), kind[k], a8, b8);
            } else
                label(p, c, "CULL_VAL %s, CullMode %d, %s: q %d", VALNAME[p], row_cull(row), kind[k], Q[col]);
        }
}

static void tri(int cull, f32b x0, f32b y0, f32b x1, f32b y1, f32b x2, f32b y2, uint32_t col)
{
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Opaque;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    isp.DepthMode = DepthMode_Always;
    isp.CullMode = cull;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    ta_polygon(pcw, isp, tsp, tcw);
    ta_vertex0(x0, y0, F32B_ONE, col, 0);
    ta_vertex0(x1, y1, F32B_ONE, col, 0);
    ta_vertex0(x2, y2, F32B_ONE, col, 1);
}

static void cell(int c)
{
    static const uint32_t colour[6] = { 0xffff4040u, 0xff40ff40u, 0xff4040ffu, 0xffffff40u, 0xffff40ffu, 0xff40ffffu };
    const int row = c / 20, col = c % 20, k = row_kind(row), x = cell_px(c), y = cell_py(c);
    f32b X[3], Y[3];
    if(k < 4) {
        /* (X0, Yc - w/2), (X0 + 28, Yc - w/2), (X0, Yc + w/2), Yc = y + 16.5: w/2 = q 2^-15 (or 2^-7) */
        const int sh = k < 2 ? 15 : 7, yc = (2 * (y + 16) + 1) << (sh - 1);       /* Yc 2^sh */
        X[0] = f32b_int(x + 2); X[1] = f32b_int(x + 30); X[2] = f32b_int(x + 2);
        Y[0] = f32b_scaled(yc - Q[col], -sh); Y[1] = Y[0]; Y[2] = f32b_scaled(yc + Q[col], -sh);
    } else {
        /* the right angle at (x + 4 3/8, y + 4 3/8), legs a along x and b along y */
        int a8, b8;
        legs(col, &a8, &b8);
        X[0] = f32b_scaled(8 * (x + 4) + 3, -3); Y[0] = f32b_scaled(8 * (y + 4) + 3, -3);
        X[1] = f32b_scaled(8 * (x + 4) + 3 + a8, -3); Y[1] = Y[0];
        X[2] = X[0]; Y[2] = f32b_scaled(8 * (y + 4) + 3 + b8, -3);
    }
    if(k & 1)
        tri(row_cull(row), X[0], Y[0], X[2], Y[2], X[1], Y[1], colour[k]);       /* winding B */
    else
        tri(row_cull(row), X[0], Y[0], X[1], Y[1], X[2], Y[2], colour[k]);
}

static void test_page(int page)
{
    int c;
    pvr_frame.bg_color = 0x202020;
    pvrio_reg(FPU_CULL_VAL_addr, VAL[page]);
    ta_list_begin(ListType_Opaque);
    for(c = 0; c < 300; c++)
        cell(c);
    ta_list_end();
}
