/* Depth: the compare modes, depth writes, and how fine a depth difference is resolved.

   page 0  Each cell: A, a red quad over the whole cell at z = 0.5 (always passes, writes depth);
           B, green, the left 24 pixels, at z = 0.25 / 0.5 / 0.75 with the compare mode under test and
           depth write on or off; C, blue, the top 16 rows at z = 0.6 with GEQUAL (it is hidden wherever
           B wrote 0.75). 8 modes x 3 depths x write on/off = 48 cells per list:
           cells 0-47 opaque list, 60-107 punch-through list, 120-167 translucent list (in order).
   page 1  precision. A red at z, then B green at z * (1 + 2^-k) with GREATER (cells: k = 1..32).
           Green means the difference was resolved.
           cells 0-31: z = 1, 40-71: z = 1000, 80-111: z = 0.001 (translucent list, in order);
           cells 120-151, 160-191, 200-231: the same in the opaque list. */
#define NUM_PAGES 2
#include "pvrtest.h"

static const char *mode_name[8] = { "never", "less", "equal", "lequal", "greater", "notequal", "gequal", "always" };
static const float b_depth[3] = { 0.25f, 0.5f, 0.75f };
static const float z_base[3] = { 1.0f, 1000.0f, 0.001f };

static void test_setup(void) {
}

static void test_describe(void) {
    static const char *lists[3] = { "OP", "PT", "TR" };
    int l, c, k, b;

    for(l = 0; l < 3; l++)
        for(c = 0; c < 48; c++)
            label(0, l * 60 + c, "%s %s, B at %.2f, write %s", lists[l], mode_name[c & 7], (double)b_depth[(c >> 3) % 3],
                  c >= 24 ? "off" : "on");
    for(l = 0; l < 2; l++)
        for(b = 0; b < 3; b++)
            for(k = 1; k <= 32; k++)
                label(1, l * 120 + b * 40 + k - 1, "%s z = %g, B at z * (1 + 2^-%d)", l ? "OP" : "TR", (double)z_base[b], k);
}

static void flat(pvr_poly_hdr_t *hdr, pvr_list_t list, int cmp, int write) {
    pvr_poly_cxt_t cxt;

    cxt_plain(&cxt, list);
    cxt.depth.comparison = cmp;
    cxt.depth.write = write ? PVR_DEPTHWRITE_ENABLE : PVR_DEPTHWRITE_DISABLE;
    pvr_poly_compile(hdr, &cxt);
}

static void modes(pvr_list_t list, int first) {
    pvr_poly_hdr_t a, b, cc;
    int c;

    flat(&a, list, PVR_DEPTHCMP_ALWAYS, 1);
    flat(&cc, list, PVR_DEPTHCMP_GEQUAL, 0);
    for(c = 0; c < 48; c++) {
        float x = cell_x(first + c), y = cell_y(first + c);
        flat(&b, list, c & 7, c < 24);
        quad(&a, x, y, 32.0f, 32.0f, 0.5f, 0xffff0000);
        quad(&b, x, y, 24.0f, 32.0f, b_depth[(c >> 3) % 3], 0xff00ff00);
        quad(&cc, x, y, 32.0f, 16.0f, 0.6f, 0xff0000ff);
    }
}

static void precision(pvr_list_t list, int first) {
    pvr_poly_hdr_t a, b;
    int i, k;

    flat(&a, list, PVR_DEPTHCMP_ALWAYS, 1);
    flat(&b, list, PVR_DEPTHCMP_GREATER, 0);
    for(i = 0; i < 3; i++) {
        for(k = 1; k <= 32; k++) {
            int c = first + i * 40 + k - 1;
            double z = z_base[i], zb = z * (1.0 + 1.0 / (double)(1ull << k));
            quad(&a, cell_x(c), cell_y(c), 32.0f, 32.0f, (float)z, 0xffff0000);
            quad(&b, cell_x(c) + 4.0f, cell_y(c) + 4.0f, 24.0f, 24.0f, (float)zb, 0xff00ff00);
        }
    }
}

static void test_page(int page) {
    pvr_list_begin(PVR_LIST_OP_POLY);
    if(page == 0) modes(PVR_LIST_OP_POLY, 0);
    else precision(PVR_LIST_OP_POLY, 120);
    pvr_list_finish();
    pvr_list_begin(PVR_LIST_TR_POLY);
    if(page == 0) modes(PVR_LIST_TR_POLY, 120);
    else precision(PVR_LIST_TR_POLY, 0);
    pvr_list_finish();
    pvr_list_begin(PVR_LIST_PT_POLY);
    if(page == 0) modes(PVR_LIST_PT_POLY, 60);
    pvr_list_finish();
}
