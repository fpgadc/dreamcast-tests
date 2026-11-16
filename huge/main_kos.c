/* Huge, tiny and special values in depth and in vertex positions. Analyze with huge.py.

   page 0  depth. Column c: z = 2^e, e = -126 + 13 c (column 19: e = 127).
       row 0  a white quad at z, compare ALWAYS                 row 1  the same, compare GEQUAL against the empty buffer
       row 2  A red at z, B green at 2z, GREATER (should pass; column 19 uses 1.5 z)
       row 3  A red at z, B green 64 units in the last place above, GREATER
       row 4  A red at z, B green at z / 2, GREATER (should fail)
       row 5  white at -z, ALWAYS                               row 6  white at -z, GEQUAL against the empty buffer
       row 7  A red at 1.0, B green at -z, GREATER              row 8  A red at -z, B green at 1.0, GREATER
     special values, column = 0.0, -0.0, smallest denormal, largest denormal, smallest normal, largest
     finite, +inf, -inf, quiet NaN, negative quiet NaN, signaling NaN:
       row 10 white, ALWAYS      row 11 white, GEQUAL against the empty buffer
       row 12 A red at 1.0, B green at the value, GREATER       row 13 A red at the value, B green at 1.0, GREATER
       row 14 A red at the value, B green at the same value with the same vertices, EQUAL
   pages 1-13  position. One white triangle with corners (-4M, -3M), (4M, 3M), (-3M, 4M): it covers the part of
       the screen below the diagonal 3x = 4y. M = 2^m, m = 8, 10, 12, 14, 16, 18, 20, 24, 30, 40, 60, 100, 120.
   page 14  the same with M = infinity.    page 15  the same triangle (m = 8) with NaN as one x coordinate. */
#define NUM_PAGES 16
#include "pvrtest.h"

static const int ms[13] = { 8, 10, 12, 14, 16, 18, 20, 24, 30, 40, 60, 100, 120 };
static const uint32_t special[11] = { 0x00000000, 0x80000000, 0x00000001, 0x007fffff, 0x00800000, 0x7f7fffff,
                                      0x7f800000, 0xff800000, 0x7fc00000, 0xffc00000, 0x7f800001 };
static pvr_poly_hdr_t hdr_cmp[8], hdr_a;

static float from_bits(uint32_t b) {
    union { uint32_t u; float f; } v;
    v.u = b;
    return v.f;
}

static uint32_t pow2(int e) { return (uint32_t)(e + 127) << 23; }

static void test_setup(void) {
    pvr_poly_cxt_t cxt;
    int i;

    for(i = 0; i < 8; i++) {
        cxt_plain(&cxt, PVR_LIST_TR_POLY);
        cxt.depth.comparison = i;
        pvr_poly_compile(&hdr_cmp[i], &cxt);
    }
    cxt_plain(&cxt, PVR_LIST_TR_POLY);
    cxt.depth.write = PVR_DEPTHWRITE_ENABLE;
    pvr_poly_compile(&hdr_a, &cxt);
}

static void test_describe(void) {
    int r, c, p;

    for(r = 0; r < 15; r++)
        if(r != 9)
            for(c = 0; c < (r < 9 ? 20 : 11); c++)
                label(0, r * COLS + c, "depth row %d column %d", r, c);
    for(p = 1; p < NUM_PAGES; p++)
        label_span(p, 0, COLS, 15, p < 14 ? "triangle, M = 2^%d" : p == 14 ? "triangle, M = inf" : "triangle, NaN x", p < 14 ? ms[p - 1] : 0);
}

static void pair(int cell, float za, float zb, int cmp, int same) {
    float x = cell_x(cell), y = cell_y(cell);
    quad(&hdr_a, x, y, 32.0f, 32.0f, za, 0xffff0000);
    if(same)
        quad(&hdr_cmp[cmp], x, y, 32.0f, 32.0f, zb, 0xff00ff00);
    else
        quad(&hdr_cmp[cmp], x + 4.0f, y + 4.0f, 24.0f, 24.0f, zb, 0xff00ff00);
}

static void test_page(int page) {
    int c;

    pvr_list_begin(PVR_LIST_TR_POLY);
    if(page == 0) {
        for(c = 0; c < 20; c++) {
            int e = (c == 19) ? 127 : -126 + 13 * c;
            uint32_t b = pow2(e);
            float z = from_bits(b), nz = from_bits(b | 0x80000000);
            quad(&hdr_cmp[PVR_DEPTHCMP_ALWAYS], cell_x(c), cell_y(c), 32.0f, 32.0f, z, 0xffffffff);
            quad(&hdr_cmp[PVR_DEPTHCMP_GEQUAL], cell_x(20 + c), cell_y(20 + c), 32.0f, 32.0f, z, 0xffffffff);
            pair(40 + c, z, from_bits(c == 19 ? b | 0x400000 : pow2(e + 1)), PVR_DEPTHCMP_GREATER, 0);
            pair(60 + c, z, from_bits(b + 64), PVR_DEPTHCMP_GREATER, 0);
            pair(80 + c, z, from_bits(c == 0 ? b - 0x400000 : pow2(e - 1)), PVR_DEPTHCMP_GREATER, 0);
            quad(&hdr_cmp[PVR_DEPTHCMP_ALWAYS], cell_x(100 + c), cell_y(100 + c), 32.0f, 32.0f, nz, 0xffffffff);
            quad(&hdr_cmp[PVR_DEPTHCMP_GEQUAL], cell_x(120 + c), cell_y(120 + c), 32.0f, 32.0f, nz, 0xffffffff);
            pair(140 + c, 1.0f, nz, PVR_DEPTHCMP_GREATER, 0);
            pair(160 + c, nz, 1.0f, PVR_DEPTHCMP_GREATER, 0);
        }
        for(c = 0; c < 11; c++) {
            float z = from_bits(special[c]);
            quad(&hdr_cmp[PVR_DEPTHCMP_ALWAYS], cell_x(200 + c), cell_y(200 + c), 32.0f, 32.0f, z, 0xffffffff);
            quad(&hdr_cmp[PVR_DEPTHCMP_GEQUAL], cell_x(220 + c), cell_y(220 + c), 32.0f, 32.0f, z, 0xffffffff);
            pair(240 + c, 1.0f, z, PVR_DEPTHCMP_GREATER, 0);
            pair(260 + c, z, 1.0f, PVR_DEPTHCMP_GREATER, 0);
            pair(280 + c, z, z, PVR_DEPTHCMP_EQUAL, 1);
        }
    }
    else {
        float m = (page < 14) ? from_bits(pow2(ms[page - 1])) : (page == 14) ? from_bits(0x7f800000) : 256.0f;
        float t[3][2] = { { -4.0f * m, -3.0f * m }, { 4.0f * m, 3.0f * m }, { -3.0f * m, 4.0f * m } };
        if(page == 15)
            t[2][0] = from_bits(0x7fc00000);
        tri(&hdr_cmp[PVR_DEPTHCMP_ALWAYS], t, 1.0f, 0xffffffff);
    }
    pvr_list_finish();
}
