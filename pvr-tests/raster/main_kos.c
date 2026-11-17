/* Rasterization: which pixels a polygon covers.

   page 0  an 8x8 axis-aligned white quad at (8 + i/16, 8 + j/16) in column i, row j   (i = 0..15, j = 0..14)
   page 1  a right triangle (0,0) (12,0) (0,12) at the same offsets
   page 2  two triangles sharing an edge, with random corners on a 1/16 pixel grid, drawn additively at
           brightness 100: a pixel on the shared edge that both cover shows 200, one that neither covers 0
   page 3  the same shapes as page 2, as one four-vertex strip
   page 4  culling: none / small / counter-clockwise / clockwise (rows of 20), for a clockwise and a
           counter-clockwise triangle of decreasing size (legs 16, 8, 4, 2, 1, 1/2, 1/4, 1/8, 1/16, 1/32 pixel,
           the small ones placed over a pixel center) */
#define NUM_PAGES 5
#include "pvrtest.h"

static pvr_poly_hdr_t hdr_white, hdr_add, hdr_cull[4];
static float shapes[256][4][2];

static void test_setup(void) {
    pvr_poly_cxt_t cxt;
    int c, k;

    cxt_plain(&cxt, PVR_LIST_TR_POLY);
    pvr_poly_compile(&hdr_white, &cxt);
    cxt_plain(&cxt, PVR_LIST_TR_POLY);
    cxt.blend.dst = PVR_BLEND_ONE;
    pvr_poly_compile(&hdr_add, &cxt);
    for(k = 0; k < 4; k++) {
        cxt_plain(&cxt, PVR_LIST_TR_POLY);
        cxt.gen.culling = k;
        pvr_poly_compile(&hdr_cull[k], &cxt);
    }
    rng_state = 4242;
    for(c = 0; c < 256; c++)
        for(k = 0; k < 4; k++) {
            /* corner k in its own quadrant of the cell, so the quad is convex-ish and the diagonal is shared */
            shapes[c][k][0] = cell_x(c) + (k & 1) * 14.0f + 2.0f + (rng() % 193) / 16.0f;
            shapes[c][k][1] = cell_y(c) + (k >> 1) * 14.0f + 2.0f + (rng() % 193) / 16.0f;
        }
}

static const float legs[10] = { 16, 8, 4, 2, 1, 0.5f, 0.25f, 0.125f, 0.0625f, 0.03125f };

static void test_describe(void) {
    static const char *cull[4] = { "none", "small", "ccw", "cw" };
    int c, k;

    for(c = 0; c < 256; c++) {
        if(c / 16 < 15) {
            label(0, (c / 16) * COLS + c % 16, "quad 8x8 at +%d/16, +%d/16", c % 16, c / 16);
            label(1, (c / 16) * COLS + c % 16, "triangle at +%d/16, +%d/16", c % 16, c / 16);
        }
        if(c < 240) {
            label(2, c, "two triangles, shape %d", c);
            label(3, c, "strip, shape %d", c);
        }
    }
    for(k = 0; k < 4; k++)
        for(c = 0; c < 20; c++)
            label(4, k * 20 + c, "cull %s, %s, leg %g px", cull[k], c < 10 ? "cw" : "ccw", (double)legs[c % 10]);
}

static void test_page(int page) {
    int c, k;

    pvr_list_begin(PVR_LIST_TR_POLY);
    for(c = 0; c < 256 && page < 4; c++) {
        /* 16 columns of cells on pages 0 and 1 */
        int cell = (page < 2) ? (c / 16) * COLS + c % 16 : c;
        float x = cell_x(cell) + 8.0f + (c % 16) / 16.0f, y = cell_y(cell) + 8.0f + (c / 16) / 16.0f;
        if(page == 0 && c / 16 < 15)
            quad(&hdr_white, x, y, 8.0f, 8.0f, 1.0f, 0xffffffff);
        else if(page == 1 && c / 16 < 15) {
            float t[3][2] = { { x, y }, { x + 12.0f, y }, { x, y + 12.0f } };
            tri(&hdr_white, t, 1.0f, 0xffffffff);
        }
        else if(page == 2 && c < 240) {
            float t1[3][2] = { { shapes[c][0][0], shapes[c][0][1] }, { shapes[c][1][0], shapes[c][1][1] }, { shapes[c][2][0], shapes[c][2][1] } };
            float t2[3][2] = { { shapes[c][2][0], shapes[c][2][1] }, { shapes[c][1][0], shapes[c][1][1] }, { shapes[c][3][0], shapes[c][3][1] } };
            tri(&hdr_add, t1, 1.0f, 0xff646464);
            tri(&hdr_add, t2, 1.0f, 0xff646464);
        }
        else if(page == 3 && c < 240) {
            pvr_vertex_t v;
            pvr_prim(&hdr_add, sizeof(hdr_add));
            v.z = 1.0f; v.u = v.v = 0.0f; v.argb = 0xff646464; v.oargb = 0;
            for(k = 0; k < 4; k++) {
                v.flags = (k == 3) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
                v.x = shapes[c][k][0];
                v.y = shapes[c][k][1];
                pvr_prim(&v, sizeof(v));
            }
        }
    }
    if(page == 4) {
        for(k = 0; k < 4; k++)
            for(c = 0; c < 20; c++) {
                float s = legs[c % 10];
                /* the right angle just up and left of the pixel center at (16.5, 16.5) of the cell */
                float x = cell_x(k * 20 + c) + 16.5f - (s < 2.0f ? s * 0.25f : 0.0f), y = cell_y(k * 20 + c) + 16.5f - (s < 2.0f ? s * 0.25f : 0.0f);
                float cw[3][2] = { { x, y }, { x + s, y }, { x, y + s } }, ccw[3][2] = { { x, y }, { x, y + s }, { x + s, y } };
                tri(&hdr_cull[k], c < 10 ? cw : ccw, 1.0f, 0xffffffff);
            }
    }
    pvr_list_finish();
}
