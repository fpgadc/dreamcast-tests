/* Where do huge vertex positions stop working? (pvr-tests/huge: a triangle with corners at 2^100 is drawn, one
   with corners at 2^120 is not.)  With M = 2^m, m = ms[page] (gen.py):
     a  (-4M, -3M) (4M, 3M) (-3M, 4M)       three huge corners; covers the screen below the diagonal 3x = 4y
     b  (-64, -64) (704, 0) (-3M, 4M)       one huge corner: a band from the top of the screen down to the left
     c  (-4M, 100) (4M, 380) (320, -1000)   two corners huge in x only: the screen above a gently falling line
   HP_TRI 1, 2, 3: one of them alone, white, in the opaque list. HP_TRI 0: all three in the translucent list,
   red, green and blue, added up (blend ONE, ONE). Analyze with read.py. */
#define NUM_PAGES 16
#include "pvrtest.h"
#include "cases.h"

static pvr_poly_hdr_t hdr;

static float pow2f(int e) {
    union { uint32_t u; float f; } v;
    v.u = (uint32_t)(e + 127) << 23;
    return v.f;
}

static void test_setup(void) {
    pvr_poly_cxt_t cxt;
    cxt_plain(&cxt, HP_TRI ? PVR_LIST_OP_POLY : PVR_LIST_TR_POLY);
    if(!HP_TRI) {
        cxt.blend.src = PVR_BLEND_ONE;
        cxt.blend.dst = PVR_BLEND_ONE;
    }
    cxt.gen.shading = PVR_SHADE_FLAT;
    pvr_poly_compile(&hdr, &cxt);
}

static void test_describe(void) {
    int p;
    for(p = 0; p < NUM_PAGES; p++)
        label_span(p, 0, COLS, 15, "M = 2^%d", ms[p]);
}

static void test_page(int page) {
    float m = pow2f(ms[page]);
    float a[3][2] = { { -4.0f * m, -3.0f * m }, { 4.0f * m, 3.0f * m }, { -3.0f * m, 4.0f * m } };
    float b[3][2] = { { -64.0f, -64.0f }, { 704.0f, 0.0f }, { -3.0f * m, 4.0f * m } };
    float c[3][2] = { { -4.0f * m, 100.0f }, { 4.0f * m, 380.0f }, { 320.0f, -1000.0f } };

    pvr_list_begin(HP_TRI ? PVR_LIST_OP_POLY : PVR_LIST_TR_POLY);
    if(HP_TRI == 0 || HP_TRI == 1)
        tri(&hdr, a, 1.0f, HP_TRI ? 0xffffffff : 0xffff0000);
    if(HP_TRI == 0 || HP_TRI == 2)
        tri(&hdr, b, 1.0f, HP_TRI ? 0xffffffff : 0xff00ff00);
    if(HP_TRI == 0 || HP_TRI == 3)
        tri(&hdr, c, 1.0f, HP_TRI ? 0xffffffff : 0xff0000ff);
    pvr_list_finish();
}
