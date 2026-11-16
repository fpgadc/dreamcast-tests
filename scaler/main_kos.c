/* The scaler (SCALER_CTL): vertical scaling / flicker filter and horizontal halving.

   The picture is the same on every page; page p writes scaler[p] to the register:
       0x00400  no scaling (reference)        0x00401  slight vertical scale (what KOS uses for interlace)
       0x00800  vertical scale 2:1            0x00600  vertical scale 1.5:1
       0x10400  horizontal halving
   Content: rows 0-1 single-pixel horizontal lines (white, then alternating red / green / blue);
   rows 2-3 single-pixel vertical lines; rows 4-5 a one-pixel checkerboard; rows 6-7 a Gouraud ramp;
   rows 8-14 flat blocks of 16 colors, one cell each, repeated. */
#define NUM_PAGES 5
#include "pvrtest.h"

static const uint32_t scaler[NUM_PAGES] = { 0x00400, 0x00401, 0x00800, 0x00600, 0x10400 };
static pvr_poly_hdr_t hdr_flat, hdr_gouraud;

static void test_setup(void) {
    pvr_poly_cxt_t cxt;

    cxt_plain(&cxt, PVR_LIST_OP_POLY);
    cxt.gen.shading = PVR_SHADE_FLAT;
    pvr_poly_compile(&hdr_flat, &cxt);
    cxt_plain(&cxt, PVR_LIST_OP_POLY);
    cxt.gen.shading = PVR_SHADE_GOURAUD;
    pvr_poly_compile(&hdr_gouraud, &cxt);
}

static void test_describe(void) {
    static const char *rows[5] = { "horizontal lines", "vertical lines", "checkerboard", "ramp", "color blocks" };
    int p, r;

    for(p = 0; p < NUM_PAGES; p++)
        for(r = 0; r < 15; r++)
            label_span(p, r * COLS, COLS, 1, "scaler %05x, %s, row %d", (unsigned)scaler[p], rows[r < 8 ? r / 2 : 4], r);
}

static void test_page(int page) {
    static const uint32_t rgb[3] = { 0xffff0000, 0xff00ff00, 0xff0000ff };
    static const uint32_t up[4] = { 0xff000000, 0xffffffff, 0xff000000, 0xffffffff };
    int i, j;

    PVR_SET(PVR_SCALER_CFG, scaler[page]);
    pvr_list_begin(PVR_LIST_OP_POLY);
    for(i = 0; i < 32; i += 2)
        quad(&hdr_flat, 0.0f, (float)i, 640.0f, 1.0f, 1.0f, 0xffffffff);
    for(i = 0; i < 32; i += 2)
        quad(&hdr_flat, 0.0f, 32.0f + i, 640.0f, 1.0f, 1.0f, rgb[(i / 2) % 3]);
    for(i = 0; i < 640; i += 2)
        quad(&hdr_flat, (float)i, 64.0f, 1.0f, 64.0f, 1.0f, i < 320 ? 0xffffffff : rgb[(i / 2) % 3]);
    for(j = 0; j < 64; j += 2)
        for(i = 0; i < 640; i += 4) {
            quad(&hdr_flat, (float)i, 128.0f + j, 1.0f, 1.0f, 1.0f, 0xffffffff);
            quad(&hdr_flat, (float)i + 2.0f, 128.0f + j, 1.0f, 1.0f, 1.0f, 0xffffffff);
            quad(&hdr_flat, (float)i + 1.0f, 129.0f + j, 1.0f, 1.0f, 1.0f, 0xffffffff);
            quad(&hdr_flat, (float)i + 3.0f, 129.0f + j, 1.0f, 1.0f, 1.0f, 0xffffffff);
        }
    quad_full(&hdr_gouraud, 0.0f, 192.0f, 640.0f, 64.0f, 1.0f, 0, 0, 1, 1, up, NULL);
    for(j = 8; j < 15; j++)
        for(i = 0; i < COLS; i++) {
            int k = (i + j * 5) % 16;
            quad(&hdr_flat, i * 32.0f, j * 32.0f, 32.0f, 32.0f, 1.0f,
                 0xff000000 | ((k & 1) ? 0xff0000 : 0) | ((k & 2) ? 0x00ff00 : 0) | ((k & 4) ? 0x0000ff : 0) | ((k & 8) ? 0x404040 : 0));
        }
    pvr_list_finish();
}
