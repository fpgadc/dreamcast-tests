/* The 1/w the TSP computes, read at every pixel of general triangles. See gen.py and ../tspw.

   Every polygon is black with table fog, every fog table pair is (0, 255) and the fog color is white: the
   gray level of a pixel shows bits 11..18 of the mantissa of the 1/w the TSP computed there. */
#define NUM_PAGES 8
#include "pvrtest.h"
#include "cases.h"

static pvr_poly_hdr_t hdr_fog;

static void test_setup(void) {
    pvr_poly_cxt_t cxt;

    cxt_plain(&cxt, PVR_LIST_TR_POLY);
    cxt.gen.fog_type = PVR_FOG_TABLE;
    cxt.gen.shading = PVR_SHADE_FLAT;
    pvr_poly_compile(&hdr_fog, &cxt);
}

static void test_describe(void) {
    int p;
    for(p = 0; p < NUM_PAGES; p++)
        label_span(p, 0, 20, 15, "page %d", p);
}

static void test_page(int page) {
    pvr_vertex_t p;
    int i, k;

    for(i = 0; i < 128; i++)
        PVR_SET(PVR_FOG_TABLE_BASE + 4 * i, 0x00ff);        /* every pair is (0, 255) */
    PVR_SET(PVR_FOG_DENSITY, 0x8000);                       /* density 1.0 */
    PVR_SET(PVR_FOG_TABLE_COLOR, 0x00ffffff);

    pvr_list_begin(PVR_LIST_TR_POLY);
    p.oargb = 0; p.u = p.v = 0.0f; p.argb = 0xff000000;
    for(i = 0; i < NUM_TRIS; i++) {
        if(tris[i].page != page)
            continue;
        pvr_prim(&hdr_fog, sizeof(hdr_fog));
        for(k = 0; k < 3; k++) {
            p.flags = (k == 2) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
            p.x = tris[i].v[k][0];
            p.y = tris[i].v[k][1];
            p.z = tris[i].v[k][2];
            pvr_prim(&p, sizeof(p));
        }
    }
    pvr_list_finish();
}
