/* Where does the TSP start a line of a triangle when a nearer polygon hides part of it? See gen.py.

   The texture is that of uvmeas: 1024x8, texel i holds the number i. Every cell has one textured triangle
   (depth 1) and up to two red bands (depth 2, nearer) over whole columns of the cell. With LIST_TR 0 all of
   it is in the opaque list with depth test and write; with LIST_TR 1 in the translucent list. */
#define NUM_PAGES 8
#include "pvrtest.h"
#include "cases.h"

static pvr_poly_hdr_t hdr, hdr_band;
#define LIST (LIST_TR ? PVR_LIST_TR_POLY : PVR_LIST_OP_POLY)

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
    cxt_textured(&cxt, LIST, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, 1024, 8, txr, PVR_FILTER_NONE);
    cxt.gen.shading = PVR_SHADE_FLAT;
    cxt.depth.comparison = PVR_DEPTHCMP_GEQUAL;
    cxt.depth.write = PVR_DEPTHWRITE_ENABLE;
    pvr_poly_compile(&hdr, &cxt);
    cxt_plain(&cxt, LIST);
    cxt.gen.shading = PVR_SHADE_FLAT;
    cxt.depth.comparison = PVR_DEPTHCMP_GEQUAL;
    cxt.depth.write = PVR_DEPTHWRITE_ENABLE;
    pvr_poly_compile(&hdr_band, &cxt);
}

static void test_describe(void) {
    int p;
    for(p = 0; p < NUM_PAGES; p++)
        label_span(p, 0, 20, 15, "page %d", p);
}

static void test_page(int page) {
    pvr_vertex_t p;
    int i, k;

    pvr_list_begin(LIST);
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
    for(i = 0; i < NUM_BANDS; i++)
        if(bands[i].page == page)
            quad(&hdr_band, cell_x(bands[i].cell) + bands[i].a, cell_y(bands[i].cell), bands[i].b - bands[i].a, 32.0f, 2.0f, 0xffff0000);
    pvr_list_finish();
}
