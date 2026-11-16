/* Plane values of the TSP read through a wrapping texture coordinate. See gen.py.

   The texture is 1024x8, not twiddled, RGB565, and texel (i, any row) holds the number i: green = i >> 5,
   blue = i & 31. Point sampled, repeating, no shading: a pixel shows the texel its U selects. */
#define NUM_PAGES 8
#include "pvrtest.h"
#include "cases.h"

static pvr_poly_hdr_t hdr;

static void test_setup(void) {
    static uint16_t t[1024 * 8];
    pvr_poly_cxt_t cxt;
    pvr_ptr_t txr;
    int i, j;

    for(j = 0; j < 8; j++)
        for(i = 0; i < 1024; i++)
            t[j * 1024 + i] = i;
    txr = pvr_mem_malloc(sizeof(t));
    pvr_txr_load(t, txr, sizeof(t));
    cxt_textured(&cxt, PVR_LIST_TR_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, 1024, 8, txr, PVR_FILTER_NONE);
    cxt.gen.shading = PVR_SHADE_FLAT;
    pvr_poly_compile(&hdr, &cxt);
}

static void test_describe(void) {
    int p;
    for(p = 0; p < NUM_PAGES; p++)
        label_span(p, 0, 20, 15, "page %d", p);
}

static void test_page(int page) {
    pvr_vertex_t p;
    int i, k;

    pvr_list_begin(PVR_LIST_TR_POLY);
    p.oargb = 0; p.argb = 0xffffffff; p.v = 0.5f;
    for(i = 0; i < NUM_TRIS; i++) {
        if(tris[i].page != page)
            continue;
        pvr_prim(&hdr, sizeof(hdr));
        for(k = 0; k < 3; k++) {
            p.flags = (k == 2) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
            p.x = tris[i].v[k][0];
            p.y = tris[i].v[k][1];
            p.u = tris[i].v[k][2];
            p.z = tris[i].v[k][3];
            pvr_prim(&p, sizeof(p));
        }
    }
    pvr_list_finish();
}
