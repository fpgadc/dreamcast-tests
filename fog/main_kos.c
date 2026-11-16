/* Fog.

   256 flat squares (four per cell), color 0x204080. For the table modes square n is at
   1/z = w = 2^(n/16 - 4), i.e. 1/16 .. about 60000; the fog color is (1.0, 0.9, 0.5, 0.1).
   cells 0-63 are in the translucent list, cells 80-143 the same in the opaque list.

   page 0  table fog, linear table from 1 to 200, far depth as KOS sets it
   page 1  table fog, custom table (a sawtooth, so the interpolation between entries shows), far depth 100
   page 2  vertex fog: the fog amount is the offset color's alpha, n = 0..255; fog color 0x33cc66
   page 3  table fog mode 2 with the table of page 1
   page 4  no fog (reference) */
#define NUM_PAGES 5
#include "pvrtest.h"

static float saw[129], depth_w[256];

static void test_setup(void) {
    int i;
    for(i = 0; i < 129; i++)
        saw[i] = ((i * 37) % 128) / 127.0f;
    depth_w[0] = 1.0f / 16.0f;                  /* w = 2^(n/16 - 4), without libm */
    for(i = 1; i < 256; i++)
        depth_w[i] = (i % 16) ? depth_w[i - 1] * 1.0442737824f : depth_w[i - 16] * 2.0f;
}

static void test_describe(void) {
    static const char *name[NUM_PAGES] = { "table fog, linear", "table fog, sawtooth", "vertex fog", "table fog mode 2", "no fog" };
    int p, c;

    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 64; c++) {
            label(p, c, "TR %s, squares %d..%d", name[p], c * 4, c * 4 + 3);
            label(p, 80 + c, "OP %s, squares %d..%d", name[p], c * 4, c * 4 + 3);
        }
}

static void squares(pvr_list_t list, int first, int page) {
    static const int type[NUM_PAGES] = { PVR_FOG_TABLE, PVR_FOG_TABLE, PVR_FOG_VERTEX, PVR_FOG_TABLE2, PVR_FOG_DISABLE };
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;
    int n, k;

    cxt_plain(&cxt, list);
    cxt.gen.fog_type = type[page];
    cxt.gen.shading = PVR_SHADE_FLAT;
    if(page == 2)
        cxt.gen.specular = PVR_SPECULAR_ENABLE;
    pvr_poly_compile(&hdr, &cxt);
    for(n = 0; n < 256; n++) {
        uint32_t col[4], off[4];
        float w = depth_w[n];
        for(k = 0; k < 4; k++) {
            col[k] = 0xff204080;
            off[k] = (uint32_t)n << 24;
        }
        quad_full(&hdr, cell_x(first + n / 4) + (n & 1) * 16.0f, cell_y(first + n / 4) + ((n >> 1) & 1) * 16.0f, 16.0f, 16.0f,
                  page == 2 ? 1.0f : 1.0f / w, 0.0f, 0.0f, 1.0f, 1.0f, col, off);
    }
}

static void test_page(int page) {
    pvr_fog_table_color(1.0f, 0.9f, 0.5f, 0.1f);
    PVR_SET(PVR_FOG_VERTEX_COLOR, 0x0033cc66);      /* KOS has no working pvr_fog_vertex_color() */
    if(page == 0)
        pvr_fog_table_linear(1.0f, 200.0f);
    else {
        pvr_fog_far_depth(100.0f);
        pvr_fog_table_custom(saw);
    }
    pvr_list_begin(PVR_LIST_OP_POLY);
    squares(PVR_LIST_OP_POLY, 80, page);
    pvr_list_finish();
    pvr_list_begin(PVR_LIST_TR_POLY);
    squares(PVR_LIST_TR_POLY, 0, page);
    pvr_list_finish();
}
