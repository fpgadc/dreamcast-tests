/* Is the depth's shared exponent taken per tile or once per triangle?

   As zblock: A is a quad whose depth falls steeply along x, B is a flat quad over A's last 4 pixel columns at
   A's nominal depth there plus j steps of 2^-18. Read the last column.

   page 0  A covers only the right half of the cell (x = 16..32) and falls from 40 to 0.52. Extended back to
           the tile's first pixel the plane is about 78 (a higher binade than anything inside the quad).
           Cells are 32 wide, j = c - 120. Compare GREATER.           page 1: the same, EQUAL.
   page 2  A is two tiles wide (x = 0..64) and falls from 32 to 0.52; B is at the end of the second tile, where
           the plane is about 16 at that tile's first pixel. Cells are 64 wide (10 per row), j = c - 60.
           Compare GREATER.                                            page 3: the same, EQUAL.
   page 4  as page 2 with A drawn as two separate one-tile quads with the same plane (control: this is what
           "per tile" should look like if the split itself changes nothing). GREATER.   page 5: EQUAL. */
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
    for(p = 0; p < 2; p++)
        for(c = 0; c < 240; c++)
            label(p, c, "page %d, j = %d", p, c - 120);
    for(p = 2; p < NUM_PAGES; p++)
        for(c = 0; c < 120; c++)
            label_span(p, (c / 10) * COLS + (c % 10) * 2, 2, 1, "page %d, j = %d", p, c - 60);
}

static void test_page(int page) {
    const int mode = (page & 1) ? DepthMode_Equal : DepthMode_Greater;
    const int64_t step = (int64_t)1 << 14;      /* 2^-18 in units of 2^-32 */
    int c;

    ta_list_begin(ListType_Translucent);
    if(page < 2) {
        f32b zl = F(40.0f), zr = F(0.52f);
        int64_t zl_n = fix32(zl), zr_n = fix32(zr);
        int64_t center = zl_n + (((zr_n - zl_n) * 31) >> 5);        /* (zr - zl) * (15.5 / 16) */
        for(c = 0; c < 240; c++) {
            sloped(cell_px(c) + 16, cell_py(c), 16, zl, zr);
            polygon(mode, 0);
            quad0(cell_px(c) + 28, cell_py(c), 4, 32, fix32_to_f32b(center + (c - 120) * step), 0xff00ff00);
        }
    }
    else {
        f32b zl = F(32.0f), zr = F(0.52f);
        int64_t zl_n = fix32(zl), zr_n = fix32(zr);
        int64_t center = zl_n + (((zr_n - zl_n) * 127) >> 7);       /* (zr - zl) * (63.5 / 64) */
        f32b zm = fix32_to_f32b(zl_n + ((zr_n - zl_n) >> 1));       /* (float)(zl + (zr - zl) * 0.5) */
        for(c = 0; c < 120; c++) {
            int x = (c % 10) * 64, y = (c / 10) * 32;
            if(page < 4)
                sloped(x, y, 64, zl, zr);
            else {
                sloped(x, y, 32, zl, zm);
                sloped(x + 32, y, 32, zm, zr);
            }
            polygon(mode, 0);
            quad0(x + 60, y, 4, 32, fix32_to_f32b(center + (c - 60) * step), 0xff00ff00);
        }
    }
    ta_list_end();
}
