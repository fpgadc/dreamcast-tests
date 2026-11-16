/* 16 bit framebuffers: how 8 bit colors are reduced, with and without dither.

   Built as fb565 (RGB565) and, through ../fb555/main.c, as fb555 (RGB555).

   1024 flat squares, four per cell (cells 0-255): gray levels 0..255, then red, green and blue alone 0..255.
   Each square is 16x16, so it holds 16 periods of the 4x4 dither pattern.
   Rows 13, 14: a 640 pixel wide Gouraud ramp, gray 0 -> 255 and 255 -> 0.
   page 0: dither off. page 1: dither on (bit 3 of FB_W_CTRL). */
#define NUM_PAGES 2
#ifndef TEST_PIXEL_MODE
#define TEST_PIXEL_MODE PM_RGB565
#endif
#include "pvrtest.h"

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
    static const char *ch[4] = { "gray", "red", "green", "blue" };
    int p, c;

    for(p = 0; p < 2; p++) {
        for(c = 0; c < 256; c++)
            label(p, c, "dither %s, %s %d..%d", p ? "on" : "off", ch[c / 64], (c % 64) * 4, (c % 64) * 4 + 3);
        label_span(p, 260, 20, 1, "dither %s, ramp 0 -> 255", p ? "on" : "off");
        label_span(p, 280, 20, 1, "dither %s, ramp 255 -> 0", p ? "on" : "off");
    }
}

static void test_page(int page) {
    static const uint32_t mask[4] = { 0xffffff, 0xff0000, 0x00ff00, 0x0000ff };
    static const uint32_t up[4] = { 0xff000000, 0xffffffff, 0xff000000, 0xffffffff };
    static const uint32_t down[4] = { 0xffffffff, 0xff000000, 0xffffffff, 0xff000000 };
    int n;

    PVR_SET(PVR_FB_CFG_2, (PVR_GET(PVR_FB_CFG_2) & ~8u) | (page ? 8 : 0));
    pvr_list_begin(PVR_LIST_OP_POLY);
    for(n = 0; n < 1024; n++) {
        uint32_t v = n & 255;
        quad(&hdr_flat, cell_x(n / 4) + (n & 1) * 16.0f, cell_y(n / 4) + ((n >> 1) & 1) * 16.0f, 16.0f, 16.0f, 1.0f,
             0xff000000 | (((v << 16) | (v << 8) | v) & mask[n / 256]));
    }
    quad_full(&hdr_gouraud, 0.0f, 13 * 32.0f, 640.0f, 32.0f, 1.0f, 0, 0, 1, 1, up, NULL);
    quad_full(&hdr_gouraud, 0.0f, 14 * 32.0f, 640.0f, 32.0f, 1.0f, 0, 0, 1, 1, down, NULL);
    pvr_list_finish();
}
