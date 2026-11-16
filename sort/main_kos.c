/* Translucent autosort (the translucent list is sorted per pixel by the hardware).

   Each cell has overlapping half-transparent quads (SRC_ALPHA, 1 - SRC_ALPHA; alpha 0x80) over black:
   red over the left 24 pixels, green over the right 24, blue over the middle 16 rows. They are always
   submitted red, green, blue; what changes is their depth (larger z = nearer).

   page 0
     cells 0-5     the six depth orders of 0.3, 0.5, 0.7
     cells 6-9     ties: all three at 0.5; red = green; green = blue; red = blue (the third at 0.7)
     cells 10-11   red and green intersect: their depths slope in opposite directions (0.3..0.7 across the cell)
     cells 12-13   the same six-order test with depth write off; with depth compare ALWAYS
     cells 20-33   the same as 0-13 on top of an opaque gray quad at 0.4 (it hides what is behind it)
   page 1  the same, with additive blending (ONE, ONE), where order must not matter */
#define NUM_PAGES 2
#define TEST_AUTOSORT
#include "pvrtest.h"

static const float orders[6][3] = { { 0.3f, 0.5f, 0.7f }, { 0.3f, 0.7f, 0.5f }, { 0.5f, 0.3f, 0.7f },
                                    { 0.5f, 0.7f, 0.3f }, { 0.7f, 0.3f, 0.5f }, { 0.7f, 0.5f, 0.3f } };
static const float ties[4][3] = { { 0.5f, 0.5f, 0.5f }, { 0.5f, 0.5f, 0.7f }, { 0.7f, 0.5f, 0.5f }, { 0.5f, 0.7f, 0.5f } };

static void test_setup(void) {
}

static void test_describe(void) {
    int p, b, c;

    for(p = 0; p < 2; p++)
        for(b = 0; b < 2; b++) {
            const char *bg = b ? ", over opaque 0.4" : "";
            const char *bl = p ? "add" : "alpha";
            for(c = 0; c < 6; c++)
                label(p, b * 20 + c, "%s: R %.1f G %.1f B %.1f%s", bl, (double)orders[c][0], (double)orders[c][1], (double)orders[c][2], bg);
            for(c = 0; c < 4; c++)
                label(p, b * 20 + 6 + c, "%s: tie R %.1f G %.1f B %.1f%s", bl, (double)ties[c][0], (double)ties[c][1], (double)ties[c][2], bg);
            label(p, b * 20 + 10, "%s: R and G intersect%s", bl, bg);
            label(p, b * 20 + 11, "%s: R and G intersect, submitted G first%s", bl, bg);
            label(p, b * 20 + 12, "%s: R 0.7 G 0.5 B 0.3, depth write off%s", bl, bg);
            label(p, b * 20 + 13, "%s: R 0.7 G 0.5 B 0.3, compare always%s", bl, bg);
        }
}

static void three(const pvr_poly_hdr_t *hdr, int cell, const float z[3]) {
    float x = cell_x(cell), y = cell_y(cell);
    quad(hdr, x, y, 24.0f, 32.0f, z[0], 0x80ff0000);
    quad(hdr, x + 8.0f, y, 24.0f, 32.0f, z[1], 0x8000ff00);
    quad(hdr, x, y + 8.0f, 32.0f, 16.0f, z[2], 0x800000ff);
}

static void sloped(const pvr_poly_hdr_t *hdr, int cell, uint32_t argb, float zl, float zr) {
    pvr_vertex_t v;
    int k;

    pvr_prim(hdr, sizeof(*hdr));
    v.argb = argb; v.oargb = 0; v.u = v.v = 0.0f;
    for(k = 0; k < 4; k++) {
        v.flags = (k == 3) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        v.x = cell_x(cell) + (k & 1) * 32.0f;
        v.y = cell_y(cell) + (k >> 1) * 32.0f;
        v.z = (k & 1) ? zr : zl;
        pvr_prim(&v, sizeof(v));
    }
}

static void test_page(int page) {
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr, hdr_nowrite, hdr_always;
    int b, c, m;

    pvr_list_begin(PVR_LIST_OP_POLY);
    cxt_plain(&cxt, PVR_LIST_OP_POLY);
    cxt.depth.write = PVR_DEPTHWRITE_ENABLE;
    pvr_poly_compile(&hdr, &cxt);
    quad(&hdr, 0.0f, 32.0f, 14 * 32.0f, 32.0f, 0.4f, 0xff404040);
    pvr_list_finish();

    pvr_list_begin(PVR_LIST_TR_POLY);
    for(m = 0; m < 3; m++) {
        cxt_plain(&cxt, PVR_LIST_TR_POLY);
        cxt.depth.comparison = (m == 2) ? PVR_DEPTHCMP_ALWAYS : PVR_DEPTHCMP_GEQUAL;
        cxt.depth.write = (m == 1) ? PVR_DEPTHWRITE_DISABLE : PVR_DEPTHWRITE_ENABLE;
        cxt.blend.src = page ? PVR_BLEND_ONE : PVR_BLEND_SRCALPHA;
        cxt.blend.dst = page ? PVR_BLEND_ONE : PVR_BLEND_INVSRCALPHA;
        pvr_poly_compile(m == 0 ? &hdr : m == 1 ? &hdr_nowrite : &hdr_always, &cxt);
    }
    for(b = 0; b < 2; b++) {
        for(c = 0; c < 6; c++)
            three(&hdr, b * 20 + c, orders[c]);
        for(c = 0; c < 4; c++)
            three(&hdr, b * 20 + 6 + c, ties[c]);
        sloped(&hdr, b * 20 + 10, 0x80ff0000, 0.3f, 0.7f);
        sloped(&hdr, b * 20 + 10, 0x8000ff00, 0.7f, 0.3f);
        sloped(&hdr, b * 20 + 11, 0x8000ff00, 0.7f, 0.3f);
        sloped(&hdr, b * 20 + 11, 0x80ff0000, 0.3f, 0.7f);
        three(&hdr_nowrite, b * 20 + 12, orders[5]);
        three(&hdr_always, b * 20 + 13, orders[5]);
    }
    pvr_list_finish();
}
