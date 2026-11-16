/* Bump maps: the intensity computed from a bump texel (S, R) and the light parameters in the offset color.

   256 squares (four per cell, cells 0-63): S = 17 * (n / 16), R = 17 * (n % 16), from uniform 8x8 bump
   textures, drawn with the replace environment so the result shows directly.
   Page p uses the light parameters K1, K2, K3, Q of lights[p] (offset color bytes A, R, G, B).
   cells 80-143: the same with the modulate environment and base color 0x80c0ff40. */
#define NUM_PAGES 5
#include "pvrtest.h"

static const uint32_t lights[NUM_PAGES] = { 0x00ff0000, 0x0000ff00, 0x80408040, 0x0080ffc8, 0xff000000 };
static pvr_ptr_t txr[256];

static void test_setup(void) {
    uint16_t t[64];
    int n, i;

    for(n = 0; n < 256; n++) {
        for(i = 0; i < 64; i++)
            t[i] = (uint16_t)((17 * (n / 16)) << 8 | (17 * (n % 16)));
        txr[n] = upload_raw(t, sizeof(t));
    }
}

static void test_describe(void) {
    int p, c;

    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 64; c++) {
            label(p, c, "K1 K2 K3 Q = %08x, replace, S %d, R %d..", (unsigned)lights[p], 17 * (c / 4), 17 * ((c * 4) % 16));
            label(p, 80 + c, "K1 K2 K3 Q = %08x, modulate, S %d, R %d..", (unsigned)lights[p], 17 * (c / 4), 17 * ((c * 4) % 16));
        }
}

static void test_page(int page) {
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;
    int n, m, k;

    pvr_list_begin(PVR_LIST_TR_POLY);
    for(m = 0; m < 2; m++) {
        for(n = 0; n < 256; n++) {
            uint32_t col[4], off[4];
            for(k = 0; k < 4; k++) {
                col[k] = m ? 0x80c0ff40 : 0xffffffff;
                off[k] = lights[page];
            }
            cxt_textured(&cxt, PVR_LIST_TR_POLY, PVR_TXRFMT_BUMP, 8, 8, txr[n], PVR_FILTER_NONE);
            cxt.txr.env = m ? PVR_TXRENV_MODULATE : PVR_TXRENV_REPLACE;
            cxt.gen.specular = PVR_SPECULAR_ENABLE;
            cxt.gen.shading = PVR_SHADE_FLAT;
            pvr_poly_compile(&hdr, &cxt);
            quad_full(&hdr, cell_x(m * 80 + n / 4) + (n & 1) * 16.0f, cell_y(m * 80 + n / 4) + ((n >> 1) & 1) * 16.0f, 16.0f, 16.0f, 1.0f,
                      0.0f, 0.0f, 1.0f, 1.0f, col, off);
        }
    }
    pvr_list_finish();
}
