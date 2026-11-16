/* Measure the depth the ISP stores for a triangle. See gen.py.

   Every page draws all the polygons A (red; depth always passes and is written), in order, and then one
   probe B per measured cell (green, compare GREATER): a flat quad over the whole cell at the value this page
   takes from the ladder of that cell. A pixel is green when the stored depth of B is greater than that of
   the A on top there. */
#define NUM_PAGES 16
#include "pvrtest.h"
#include "cases.h"

static pvr_poly_hdr_t hdr_a, hdr_b;

static void test_setup(void) {
    pvr_poly_cxt_t cxt;

    cxt_plain(&cxt, PVR_LIST_TR_POLY);
    cxt.depth.write = PVR_DEPTHWRITE_ENABLE;
    cxt.gen.shading = PVR_SHADE_FLAT;
    pvr_poly_compile(&hdr_a, &cxt);
    cxt_plain(&cxt, PVR_LIST_TR_POLY);
    cxt.depth.comparison = PVR_DEPTHCMP_GREATER;
    cxt.gen.shading = PVR_SHADE_FLAT;
    pvr_poly_compile(&hdr_b, &cxt);
}

static void test_describe(void) {
    int p, i;
    for(p = 0; p < NUM_PAGES; p++)
        for(i = 0; i < NUM_PROBES; i++)
            label(p, probes[i].cell, "probe %d", i);
}

static void triangle(const pvr_poly_hdr_t *hdr, const float v[3][3], uint32_t argb) {
    pvr_vertex_t p;
    int k;

    pvr_prim(hdr, sizeof(*hdr));
    p.oargb = 0; p.u = p.v = 0.0f; p.argb = argb;
    for(k = 0; k < 3; k++) {
        p.flags = (k == 2) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        p.x = v[k][0];
        p.y = v[k][1];
        p.z = v[k][2];
        pvr_prim(&p, sizeof(p));
    }
}

static void test_page(int page) {
    int i;

    pvr_list_begin(PVR_LIST_TR_POLY);
    for(i = 0; i < NUM_TRIS; i++) {
        const tri_t *t = &tris[i];
        if(t->quad)
            quad(&hdr_a, cell_x(t->cell), cell_y(t->cell), 32.0f, 32.0f, t->v[0][2], 0xffff0000);
        else
            triangle(&hdr_a, t->v, 0xffff0000);
    }
    for(i = 0; i < NUM_PROBES; i++)
        quad(&hdr_b, cell_x(probes[i].cell), cell_y(probes[i].cell), 32.0f, 32.0f, probes[i].bz[page], 0xff00ff00);
    pvr_list_finish();
}
