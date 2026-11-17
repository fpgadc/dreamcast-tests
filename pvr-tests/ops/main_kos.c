/* One operation at a time: the cases of cases.h (made by gen.py) are inputs on which changing a single
   operation of the fitted arithmetic changes the result. See gen.py for the four kinds of case and
   analyze.py for the verdicts.

   depth   red triangle, then a green quad over the whole cell with compare EQUAL
   fog     black triangle with table fog (white, every table pair (0, 255), density 1)
   color   Gouraud gray triangle
   cover   white triangle */
#define NUM_PAGES 12
#include "pvrtest.h"
#undef NUM_PAGES
#include "cases.h"

static pvr_poly_hdr_t hdr_a, hdr_eq, hdr_fog, hdr_gouraud, hdr_flat;

static void test_setup(void) {
    pvr_poly_cxt_t cxt;

    cxt_plain(&cxt, PVR_LIST_TR_POLY);
    cxt.depth.write = PVR_DEPTHWRITE_ENABLE;
    cxt.gen.shading = PVR_SHADE_FLAT;
    pvr_poly_compile(&hdr_a, &cxt);
    cxt_plain(&cxt, PVR_LIST_TR_POLY);
    cxt.depth.comparison = PVR_DEPTHCMP_EQUAL;
    cxt.gen.shading = PVR_SHADE_FLAT;
    pvr_poly_compile(&hdr_eq, &cxt);
    cxt_plain(&cxt, PVR_LIST_TR_POLY);
    cxt.gen.fog_type = PVR_FOG_TABLE;
    cxt.gen.shading = PVR_SHADE_FLAT;
    pvr_poly_compile(&hdr_fog, &cxt);
    cxt_plain(&cxt, PVR_LIST_TR_POLY);
    cxt.gen.shading = PVR_SHADE_GOURAUD;
    pvr_poly_compile(&hdr_gouraud, &cxt);
    cxt_plain(&cxt, PVR_LIST_TR_POLY);
    cxt.gen.shading = PVR_SHADE_FLAT;
    pvr_poly_compile(&hdr_flat, &cxt);
}

static void test_describe(void) {
    static const char *kind[4] = { "depth", "fog", "color", "cover" };
    int i;
    for(i = 0; i < NUM_CASES; i++)
        label(cases[i].page, cases[i].cell, "case %d (%s)", i, kind[cases[i].kind]);
}

static void triangle(const pvr_poly_hdr_t *hdr, const case_t *c, const uint32_t col[3]) {
    pvr_vertex_t v;
    int k;

    pvr_prim(hdr, sizeof(*hdr));
    v.oargb = 0; v.u = v.v = 0.0f;
    for(k = 0; k < 3; k++) {
        v.flags = (k == 2) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        v.x = c->v[k][0];
        v.y = c->v[k][1];
        v.z = c->v[k][2];
        v.argb = col[k];
        pvr_prim(&v, sizeof(v));
    }
}

static void test_page(int page) {
    int i, k;

    for(i = 0; i < 128; i++)
        PVR_SET(PVR_FOG_TABLE_BASE + 4 * i, 0x00ff);
    PVR_SET(PVR_FOG_DENSITY, 0x8000);
    PVR_SET(PVR_FOG_TABLE_COLOR, 0x00ffffff);

    pvr_list_begin(PVR_LIST_TR_POLY);
    for(i = 0; i < NUM_CASES; i++) {
        const case_t *c = &cases[i];
        uint32_t col[3];
        if(c->page != page)
            continue;
        if(c->kind == 0) {
            col[0] = col[1] = col[2] = 0xffff0000;
            triangle(&hdr_a, c, col);
            quad(&hdr_eq, cell_x(c->cell), cell_y(c->cell), 32.0f, 32.0f, c->bz, 0xff00ff00);
        }
        else if(c->kind == 1) {
            col[0] = col[1] = col[2] = 0xff000000;
            triangle(&hdr_fog, c, col);
        }
        else if(c->kind == 2) {
            for(k = 0; k < 3; k++)
                col[k] = 0xff000000 | (c->col[k] << 16) | (c->col[k] << 8) | c->col[k];
            triangle(&hdr_gouraud, c, col);
        }
        else {
            col[0] = col[1] = col[2] = 0xffffffff;
            triangle(&hdr_flat, c, col);
        }
    }
    pvr_list_finish();
}
