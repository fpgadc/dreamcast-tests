/* Punch-through alpha test: which alpha values pass for a given reference value.

   The background is a red opaque quad. In the punch-through list, 256 green squares (four per cell) have
   alpha 0..255. Page p uses the reference value refs[p] (register 0x11c).
       cells 0-63     textured: the alpha comes from an 8 bpp texture with an ARGB8888 palette
       cells 80-143   untextured: the alpha is the vertex alpha
       cells 160-223  textured, modulate alpha env: texel alpha 255 x vertex alpha */
#define NUM_PAGES 5
#include "pvrtest.h"

static const int refs[NUM_PAGES] = { 0, 1, 127, 128, 255 };
static pvr_ptr_t txr[256];
#define PAL8 (PVR_TXRFMT_PAL8BPP | PVR_TXRFMT_8BPP_PAL(0))

static void test_setup(void) {
    uint8_t t[64];
    int i;

    pvr_set_pal_format(PVR_PAL_ARGB8888);
    for(i = 0; i < 256; i++) {
        pvr_set_pal_entry(i, ((uint32_t)i << 24) | 0x00ff00);
        memset(t, i, sizeof(t));
        txr[i] = upload_raw(t, sizeof(t));
    }
}

static void test_describe(void) {
    int p, c;

    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 64; c++) {
            label(p, c, "ref %d, texture alpha %d..%d", refs[p], c * 4, c * 4 + 3);
            label(p, 80 + c, "ref %d, vertex alpha %d..%d", refs[p], c * 4, c * 4 + 3);
            label(p, 160 + c, "ref %d, texel 255 x vertex alpha %d..%d", refs[p], c * 4, c * 4 + 3);
        }
}

static void test_page(int page) {
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;
    int n;

    PVR_SET(0x011c, refs[page]);
    pvr_list_begin(PVR_LIST_OP_POLY);
    cxt_plain(&cxt, PVR_LIST_OP_POLY);
    pvr_poly_compile(&hdr, &cxt);
    quad(&hdr, 0.0f, 0.0f, 640.0f, 480.0f, 0.5f, 0xffff0000);
    pvr_list_finish();

    pvr_list_begin(PVR_LIST_PT_POLY);
    for(n = 0; n < 256; n++) {
        float qx = (n & 1) * 16.0f, qy = ((n >> 1) & 1) * 16.0f;
        cxt_textured(&cxt, PVR_LIST_PT_POLY, PAL8, 8, 8, txr[n], PVR_FILTER_NONE);
        pvr_poly_compile(&hdr, &cxt);
        quad(&hdr, cell_x(n / 4) + qx, cell_y(n / 4) + qy, 16.0f, 16.0f, 1.0f, 0xffffffff);
        cxt_plain(&cxt, PVR_LIST_PT_POLY);
        pvr_poly_compile(&hdr, &cxt);
        quad(&hdr, cell_x(80 + n / 4) + qx, cell_y(80 + n / 4) + qy, 16.0f, 16.0f, 1.0f, ((uint32_t)n << 24) | 0x00ff00);
        cxt_textured(&cxt, PVR_LIST_PT_POLY, PAL8, 8, 8, txr[255], PVR_FILTER_NONE);
        cxt.txr.env = PVR_TXRENV_MODULATEALPHA;
        pvr_poly_compile(&hdr, &cxt);
        quad(&hdr, cell_x(160 + n / 4) + qx, cell_y(160 + n / 4) + qy, 16.0f, 16.0f, 1.0f, ((uint32_t)n << 24) | 0xffffff);
    }
    pvr_list_finish();
}
