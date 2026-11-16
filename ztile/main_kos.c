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
    for(p = 0; p < 2; p++)
        for(c = 0; c < 240; c++)
            label(p, c, "page %d, j = %d", p, c - 120);
    for(p = 2; p < NUM_PAGES; p++)
        for(c = 0; c < 120; c++)
            label_span(p, (c / 10) * COLS + (c % 10) * 2, 2, 1, "page %d, j = %d", p, c - 60);
}

static void sloped(float x0, float y0, float w, float zl, float zr) {
    pvr_vertex_t v;
    int k;

    pvr_prim(&hdr_a, sizeof(hdr_a));
    v.argb = 0xffff0000; v.oargb = 0; v.u = v.v = 0.0f;
    for(k = 0; k < 4; k++) {
        v.flags = (k == 3) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        v.x = x0 + (k & 1) * w;
        v.y = y0 + (k >> 1) * 32.0f;
        v.z = (k & 1) ? zr : zl;
        pvr_prim(&v, sizeof(v));
    }
}

static void test_page(int page) {
    const pvr_poly_hdr_t *hb = (page & 1) ? &hdr_eq : &hdr_gt;
    const double step = 1.0 / 262144.0;
    int c;

    pvr_list_begin(PVR_LIST_TR_POLY);
    if(page < 2) {
        float zl = 40.0f, zr = 0.52f;
        double center = (double)zl + ((double)zr - (double)zl) * (15.5 / 16.0);
        for(c = 0; c < 240; c++) {
            sloped(cell_x(c) + 16.0f, cell_y(c), 16.0f, zl, zr);
            quad(hb, cell_x(c) + 28.0f, cell_y(c), 4.0f, 32.0f, (float)(center + (c - 120) * step), 0xff00ff00);
        }
    }
    else {
        float zl = 32.0f, zr = 0.52f;
        double center = (double)zl + ((double)zr - (double)zl) * (63.5 / 64.0);
        float zm = (float)((double)zl + ((double)zr - (double)zl) * 0.5);
        for(c = 0; c < 120; c++) {
            float x = (c % 10) * 64.0f, y = (c / 10) * 32.0f;
            if(page < 4)
                sloped(x, y, 64.0f, zl, zr);
            else {
                sloped(x, y, 32.0f, zl, zm);
                sloped(x + 32.0f, y, 32.0f, zm, zr);
            }
            quad(hb, x + 60.0f, y, 4.0f, 32.0f, (float)(center + (c - 60) * step), 0xff00ff00);
        }
    }
    pvr_list_finish();
}
