/* How is a depth stored once it has been interpolated: on the tile's shared exponent, cut to its own 19 bit
   mantissa, or as a full float?

   A is a quad over the whole cell whose depth slopes steeply along x, so that the depth at the right edge is
   in a very different binade from the depth at the tile's first pixel (which sets the shared exponent).
   B is flat, over the last 4 pixel columns, at A's nominal depth in the last pixel column plus j steps;
   cell c has j = c - 120 (20 columns x 12 rows). Read pixel (31, 16) of each cell.

   page 0  A falls from 32 to 0.52: at the last column it is about 1.01. B steps are 2^-18 (the grid of a
           19 bit depth near 1), compare GREATER. On the shared exponent of 32, A would sit on a 2^-13 grid.
   page 1  the same, compare EQUAL.
   page 2  A rises from 1 to 32.77: at the last column about 32.27. B steps are 2^-14 (the grid of a 19 bit
           depth near 32), compare GREATER. On the shared exponent of 1, A would sit on a 2^-18 grid.
   page 3  the same, compare EQUAL.
   page 4  as page 0 with B steps of 2^-13, to see the coarse grid directly; page 5 the same, EQUAL. */
#define NUM_PAGES 6
#include "pvrtest2.h"

/* Translucent, untextured, flat-coloured polygon header: depth compare `mode`, depth write on or off. */
static void polygon(int mode, int zwrite) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };

    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Translucent;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Gouraud = 1;
    isp.DepthMode = mode;
    isp.ZWriteDis = !zwrite;
    isp.CullMode = CullMode_None;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    ta_polygon(pcw, isp, tsp, tcw);
}

/* The depths of this test were computed in double precision and rounded once to float. Every intermediate
   value is a multiple of 2^-32 below 2^8, so it is exact in a double and in an int64 of units 2^-32: the
   integer is the same number, and sf_round makes the one rounding. */
static int64_t fix32(f32b a) {              /* a positive float, in units of 2^-32 */
    int e;
    uint32_t m = sf_unpack(a, &e);
    return (int64_t)m << (e + 32);
}
static f32b fix32_to_f32b(int64_t n) { return sf_round(0, (uint64_t)n, -32, 0); }

/* A quad with a flat colour, depth `zl` at its left edge and `zr` at its right edge, x0, y0, w in whole pixels. */
static void sloped(int x0, int y0, int w, f32b zl, f32b zr) {
    int k;
    polygon(DepthMode_Always, 1);
    for(k = 0; k < 4; k++)
        ta_vertex0(f32b_int(x0 + (k & 1) * w), f32b_int(y0 + (k >> 1) * 32), (k & 1) ? zr : zl, 0xffff0000, k == 3);
}

static void test_setup(void) {
}

static void test_describe(void) {
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 240; c++)
            label(p, c, "page %d, j = %d", p, c - 120);
}

static void test_page(int page) {
    int falling = (page < 2) || (page >= 4);
    f32b zl = falling ? F(32.0f) : F32B_ONE, zr = falling ? F(0.52f) : F(32.77f);
    /* center = zl + (zr - zl) * (31.5 / 32.0), step = 2^-18, 2^-14 or 2^-13 */
    int64_t zl_n = fix32(zl), zr_n = fix32(zr);
    int64_t center = zl_n + (((zr_n - zl_n) * 63) >> 6);
    int64_t step = (page < 2) ? (int64_t)1 << 14 : (page < 4) ? (int64_t)1 << 18 : (int64_t)1 << 19;
    int c;

    ta_list_begin(ListType_Translucent);
    for(c = 0; c < 240; c++) {
        sloped(cell_px(c), cell_py(c), 32, zl, zr);
        polygon((page & 1) ? DepthMode_Equal : DepthMode_Greater, 0);
        quad0(cell_px(c) + 28, cell_py(c), 4, 32, fix32_to_f32b(center + (c - 120) * step), 0xff00ff00);
    }
    ta_list_end();
}
