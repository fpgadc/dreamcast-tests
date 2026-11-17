/* Which pixels are inside a polygon whose edge passes through pixel centers? See gen.py.
   Every polygon is white, flat, depth 1, drawn alone over the black background; as a triangle, a strip of two
   triangles or a sprite (COV_PRIM 0, 1, 2), in the list COV_LIST. With COV_PRIM 3 the triangles are modifier
   volumes, one each, in front of a white polygon over the whole screen that has cheap shadow: what a triangle
   covers is darker. */
#define NUM_PAGES 8
#include "cases.h"
#if COV_PRIM == 3
#define TEST_MODIFIERS
#endif
#include "pvrtest.h"

static pvr_poly_hdr_t hdr, hdr_probe;

static void test_setup(void) {
    pvr_poly_cxt_t cxt;
    cxt_plain(&cxt, COV_LIST);
    cxt.gen.shading = PVR_SHADE_FLAT;
    cxt.depth.comparison = PVR_DEPTHCMP_GREATER;
    pvr_poly_compile(&hdr_probe, &cxt);
    cxt_plain(&cxt, COV_PRIM == 3 ? PVR_LIST_OP_POLY : COV_LIST);
    cxt.gen.shading = COV_VCOL == 2 ? PVR_SHADE_GOURAUD : PVR_SHADE_FLAT;
#if COV_PRIM != 3
    cxt.gen.culling = COV_CULL;
#endif
#if COV_PRIM == 4
    cxt.depth.write = PVR_DEPTHWRITE_ENABLE;
#endif
#if COV_PRIM == 3
    cxt.depth.write = PVR_DEPTHWRITE_ENABLE;
    cxt.fmt.modifier = PVR_MODIFIER_ENABLE;
    cxt.gen.modifier_mode = PVR_MODIFIER_CHEAP_SHADOW;
#endif
    pvr_poly_compile(&hdr, &cxt);
}

static void test_describe(void) {
    int p;
    for(p = 0; p < NUM_PAGES; p++)
        label_span(p, 0, 20, 15, "page %d", p);
}

/* a triangle with a depth per corner; COV_VCOL: corners red, green, blue */
static void tri3(const pvr_poly_hdr_t *h, const float xy[4][2], uint32_t argb) {
    static const float zs[3] = COV_ZS;
    static const uint32_t col[3] = { 0xffff0000, 0xff00ff00, 0xff0000ff };
    pvr_vertex_t v;
    int k;

    pvr_prim(h, sizeof(*h));
    v.u = v.v = 0.0f; v.oargb = 0;
    for(k = 0; k < 3; k++) {
        v.flags = (k == 2) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        v.x = xy[k][0];
        v.y = xy[k][1];
        v.z = zs[k];
        v.argb = COV_VCOL ? col[k] : argb;
        pvr_prim(&v, sizeof(v));
    }
}

static void strip4(const float xy[4][2]) {
    pvr_vertex_t v;
    int k;

    pvr_prim(&hdr, sizeof(hdr));
    v.z = COV_Z; v.u = v.v = 0.0f; v.argb = 0xffffffff; v.oargb = 0;
    for(k = 0; k < 4; k++) {
        v.flags = (k == 3) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        v.x = xy[k][0];
        v.y = xy[k][1];
        pvr_prim(&v, sizeof(v));
    }
}

static void sprite4(const float p[4][2]) {
    pvr_sprite_cxt_t cxt;
    pvr_sprite_hdr_t sh;
    pvr_sprite_col_t v;

    pvr_sprite_cxt_col(&cxt, COV_LIST);
    cxt.gen.culling = COV_CULL;
    cxt.depth.comparison = PVR_DEPTHCMP_ALWAYS;
    cxt.depth.write = PVR_DEPTHWRITE_DISABLE;
    pvr_sprite_compile(&sh, &cxt);
    sh.argb = 0xffffffff;
    pvr_prim(&sh, sizeof(sh));
    memset(&v, 0, sizeof(v));
    v.flags = PVR_CMD_VERTEX_EOL;
    v.ax = p[0][0]; v.ay = p[0][1]; v.az = COV_Z;
    v.bx = p[1][0]; v.by = p[1][1]; v.bz = COV_Z;
    v.cx = p[2][0]; v.cy = p[2][1]; v.cz = COV_Z;
    v.dx = p[3][0]; v.dy = p[3][1];
    pvr_prim(&v, sizeof(v));
}

#if COV_PRIM == 3
static void volume(const float p[4][2]) {
    pvr_mod_hdr_t mh;
    pvr_modifier_vol_t v;

    pvr_mod_compile(&mh, PVR_LIST_OP_MOD, PVR_MODIFIER_INCLUDE_LAST_POLY, PVR_CULLING_NONE);
    pvr_prim(&mh, sizeof(mh));
    memset(&v, 0, sizeof(v));
    v.flags = PVR_CMD_VERTEX_EOL;
    v.ax = p[0][0]; v.ay = p[0][1]; v.az = 1.0f;
    v.bx = p[1][0]; v.by = p[1][1]; v.bz = 1.0f;
    v.cx = p[2][0]; v.cy = p[2][1]; v.cz = 1.0f;
    pvr_prim(&v, sizeof(v));
}
#endif

static void test_page(int page) {
    int i;

    if(COV_HALFOFS != 7)
        PVR_SET(0x80, COV_HALFOFS);
#if COV_PRIM == 3
    PVR_SET(PVR_CHEAP_SHADOW, 0x180);
    pvr_list_begin(PVR_LIST_OP_POLY);
    quad(&hdr, 0.0f, 0.0f, 640.0f, 480.0f, 0.5f, 0xffffffff);
    pvr_list_finish();
    pvr_list_begin(PVR_LIST_OP_MOD);
    for(i = 0; i < NUM_TRIS; i++)
        if(tris[i].page == page)
            volume(tris[i].v);
    pvr_list_finish();
#else
    pvr_list_begin(COV_LIST);
    for(i = 0; i < NUM_TRIS; i++)
        if(tris[i].page == page) {
            if(COV_PRIM == 0)
                tri3(&hdr, tris[i].v, 0xffffffff);
            else if(COV_PRIM == 4)
                tri3(&hdr, tris[i].v, 0xffff0000);
            else if(COV_PRIM == 1)
                strip4(tris[i].v);
            else
                sprite4(tris[i].v);
        }
    if(COV_PRIM == 4)
        quad(&hdr_probe, 0.0f, 0.0f, 640.0f, 480.0f, COV_PROBE_Z, 0xff00ff00);
    pvr_list_finish();
#endif
}
