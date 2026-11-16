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
#include "pvrtest.h"

static pvr_poly_hdr_t hdr_a, hdr_gt, hdr_eq;

static void test_setup(void) {
    pvr_poly_cxt_t cxt;

    cxt_plain(&cxt, PVR_LIST_TR_POLY);
    cxt.depth.write = PVR_DEPTHWRITE_ENABLE;
    pvr_poly_compile(&hdr_a, &cxt);
    cxt_plain(&cxt, PVR_LIST_TR_POLY);
    cxt.depth.comparison = PVR_DEPTHCMP_GREATER;
    pvr_poly_compile(&hdr_gt, &cxt);
    cxt_plain(&cxt, PVR_LIST_TR_POLY);
    cxt.depth.comparison = PVR_DEPTHCMP_EQUAL;
    pvr_poly_compile(&hdr_eq, &cxt);
}

static void test_describe(void) {
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 240; c++)
            label(p, c, "page %d, j = %d", p, c - 120);
}

static void sloped(int cell, float zl, float zr) {
    pvr_vertex_t v;
    int k;

    pvr_prim(&hdr_a, sizeof(hdr_a));
    v.argb = 0xffff0000; v.oargb = 0; v.u = v.v = 0.0f;
    for(k = 0; k < 4; k++) {
        v.flags = (k == 3) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        v.x = cell_x(cell) + (k & 1) * 32.0f;
        v.y = cell_y(cell) + (k >> 1) * 32.0f;
        v.z = (k & 1) ? zr : zl;
        pvr_prim(&v, sizeof(v));
    }
}

static void test_page(int page) {
    int falling = (page < 2) || (page >= 4);
    float zl = falling ? 32.0f : 1.0f, zr = falling ? 0.52f : 32.77f;
    double center = (double)zl + ((double)zr - (double)zl) * (31.5 / 32.0);
    double step = (page < 2) ? 1.0 / 262144.0 : (page < 4) ? 1.0 / 16384.0 : 1.0 / 8192.0;
    int c;

    pvr_list_begin(PVR_LIST_TR_POLY);
    for(c = 0; c < 240; c++) {
        sloped(c, zl, zr);
        quad((page & 1) ? &hdr_eq : &hdr_gt, cell_x(c) + 28.0f, cell_y(c), 4.0f, 32.0f, (float)(center + (c - 120) * step), 0xff00ff00);
    }
    pvr_list_finish();
}
