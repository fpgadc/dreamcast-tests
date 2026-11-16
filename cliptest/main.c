/* Does the user tile clip do what zmeas needs? A red quad over the whole screen, then big triangles
   (0,0) (2048,0) (0,2048) with different clip settings. Each page tries one way of submitting.

   green: clip rectangle = tile (5, 5), mode INSIDE        blue: clip rectangle = tiles (8..9, 5..6), mode INSIDE
   white: clip rectangle = tiles (12..13, 8..9), mode OUTSIDE, drawn over rows 8-11 only (a 640x128 quad)

   page 0  headers as KOS compiles them
   page 1  group enable bit (23) set in the headers
   page 2  as page 1 with one never-visible padding triangle between a clipped polygon and the next clip command
   page 3  as page 1 with two padding triangles

   Result on a console: without padding each polygon is clipped by the clip command that FOLLOWS it (the
   tile accelerator is still working on the last strip when the rectangle changes). */
#define NUM_PAGES 4
#include "pvrtest.h"

static void test_setup(void) { }

static void test_describe(void) {
    int p;
    for(p = 0; p < NUM_PAGES; p++)
        label_span(p, 0, 20, 15, "clip page %d", p);
}

static void clip(int x0, int y0, int x1, int y1) {
    alignas(32) uint32_t cmd[8] = { PVR_CMD_USERCLIP, 0, 0, 0, x0, y0, x1, y1 };
    pvr_prim(cmd, sizeof(cmd));
}

static void big(pvr_list_t list, int mode, int group, uint32_t argb) {
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;
    pvr_vertex_t v;
    static const float xy[3][2] = { { 0, 0 }, { 2048, 0 }, { 0, 2048 } };
    int k;

    cxt_plain(&cxt, list);
    cxt.gen.clip_mode = mode;
    cxt.gen.shading = PVR_SHADE_FLAT;
    pvr_poly_compile(&hdr, &cxt);
    if(group)
        hdr.cmd |= 0x00800000;
    pvr_prim(&hdr, sizeof(hdr));
    v.argb = argb; v.oargb = 0; v.u = v.v = 0; v.z = 1.0f;
    for(k = 0; k < 3; k++) {
        v.flags = (k == 2) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        v.x = xy[k][0];
        v.y = xy[k][1];
        pvr_prim(&v, sizeof(v));
    }
}

/* a never-visible padding triangle: one pixel, far behind everything, clip disabled */
static void pad(pvr_list_t list, int n) {
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;
    float t[3][2] = { { 2, 2 }, { 4, 2 }, { 2, 4 } };

    cxt_plain(&cxt, list);
    cxt.depth.comparison = PVR_DEPTHCMP_GEQUAL;
    pvr_poly_compile(&hdr, &cxt);
    hdr.cmd |= 0x00800000;
    while(n--)
        tri(&hdr, t, 0.0000001f, 0xff000000);
}

static void test_page(int page) {
    pvr_list_t list = PVR_LIST_TR_POLY;
    int group = page >= 1, s = 1;
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;

    pvr_list_begin(list);
    cxt_plain(&cxt, list);
    pvr_poly_compile(&hdr, &cxt);
    if(group)
        hdr.cmd |= 0x00800000;
    quad(&hdr, 0.0f, 0.0f, 640.0f, 480.0f, 0.5f, 0xffff0000);
    clip(5 * s, 5 * s, 5 * s, 5 * s);
    big(list, PVR_USERCLIP_INSIDE, group, 0xff00ff00);
    pad(list, page == 2 ? 1 : page == 3 ? 2 : 0);
    clip(8 * s, 5 * s, 9 * s, 6 * s);
    big(list, PVR_USERCLIP_INSIDE, group, 0xff0000ff);
    pad(list, page == 2 ? 1 : page == 3 ? 2 : 0);
    clip(12 * s, 8 * s, 13 * s, 9 * s);
    cxt_plain(&cxt, list);
    cxt.gen.clip_mode = PVR_USERCLIP_OUTSIDE;
    pvr_poly_compile(&hdr, &cxt);
    if(group)
        hdr.cmd |= 0x00800000;
    quad(&hdr, 0.0f, 256.0f, 640.0f, 128.0f, 1.0f, 0xffffffff);
    pvr_list_finish();
}
