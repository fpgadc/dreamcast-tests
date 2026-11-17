/* Gouraud colors on triangles with exactly known planes. See gen.py. */
#define NUM_PAGES 8
#include "pvrtest.h"
#include "cases.h"

static pvr_poly_hdr_t hdr;

static void test_setup(void) {
    pvr_poly_cxt_t cxt;

    cxt_plain(&cxt, PVR_LIST_TR_POLY);
    cxt.gen.shading = PVR_SHADE_GOURAUD;
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
    p.oargb = 0; p.z = 1.0f; p.u = p.v = 0.0f;
    for(i = 0; i < NUM_TRIS; i++) {
        if(tris[i].page != page)
            continue;
        pvr_prim(&hdr, sizeof(hdr));
        for(k = 0; k < 3; k++) {
            p.flags = (k == 2) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
            p.x = tris[i].xy[k][0];
            p.y = tris[i].xy[k][1];
            p.argb = tris[i].argb[k];
            pvr_prim(&p, sizeof(p));
        }
    }
    pvr_list_finish();
}
