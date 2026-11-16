/* The pixel / texel center register (HALF_OFFSET, 0x80): bit 0 ISP ("FPU") pixel half offset, bit 1 TSP pixel
   half offset, bit 2 texel half offset. Page p draws the same picture with the register set to p (0..7);
   KOS and the boot ROM leave it at 7.

   Coverage and depth (ISP bit):
     columns 0-7, rows 0-7   an 8x8 white quad at (8 + i/8, 8 + j/8), column i, row j
     row 8, cells 0-7        A red with depth rising along x by z * 2^-k over the cell (k = 2, 4, 6, 8), B green
                             flat at A's middle value, GREATER; cells 4-7 the same rising along y
   Shading (TSP bit):
     row 0, cells 9-12       Gouraud, 0 -> 255 across 8 pixels (x), across 8 pixels (y), across 32 (x), across 32 (y)
     row 0, cells 13-14      the 8 pixel ramps on a quad whose depth also rises 1 -> 4 along the ramp
   Texture position (TSP and texel bits). Texture: 8x8, 8 bpp with an ARGB8888 palette, red = column,
   green = row, blue = checkerboard.
     row 1, cells 9-17       point sampled at 4x, u shifted by d/8 pixel, d = -4..4
     row 2, cells 9-17       the same shifting v
     row 3, cells 9-17       bilinear at 4x, u shifted by d/8 pixel
     row 4, cells 9-17       bilinear at 4x, v shifted
     row 5, cells 9-17       point sampled 1:1 (8x8 quad at +8, +8), u shifted by d/8 texel
     row 6, cells 9-12       bilinear, one texel step (0 -> 255) over 32 pixels along u, started k/128 texel in
     row 6, cells 13-16      the same along v
     row 7, cells 9-12       point sampled at 4x on a quad whose depth rises 1 -> 4 along x; along y; bilinear x; y */
#define NUM_PAGES 8
#include "pvrtest.h"

static pvr_ptr_t pattern, ramp_h, ramp_v;
static pvr_poly_hdr_t hdr_white, hdr_gouraud, hdr_a, hdr_gt;
#define PAL8 (PVR_TXRFMT_PAL8BPP | PVR_TXRFMT_8BPP_PAL(0))

static void test_setup(void) {
    pvr_poly_cxt_t cxt;
    uint8_t t[64];
    int i;

    pvr_set_pal_format(PVR_PAL_ARGB8888);
    for(i = 0; i < 64; i++) {
        pvr_set_pal_entry(i, 0xff000000 | ((i % 8) * 36 << 16) | ((i / 8) * 36 << 8) | ((((i % 8) ^ (i / 8)) & 1) ? 255 : 0));
        t[i] = i;
    }
    pvr_set_pal_entry(64, 0xff000000);
    pvr_set_pal_entry(65, 0xffffffff);
    pattern = pvr_mem_malloc(64);
    pvr_txr_load_ex(t, pattern, 8, 8, PVR_TXRLOAD_8BPP);
    for(i = 0; i < 64; i++) t[i] = 64 + ((i % 8) >= 4);
    ramp_h = pvr_mem_malloc(64);
    pvr_txr_load_ex(t, ramp_h, 8, 8, PVR_TXRLOAD_8BPP);
    for(i = 0; i < 64; i++) t[i] = 64 + ((i / 8) >= 4);
    ramp_v = pvr_mem_malloc(64);
    pvr_txr_load_ex(t, ramp_v, 8, 8, PVR_TXRLOAD_8BPP);

    cxt_plain(&cxt, PVR_LIST_TR_POLY);
    pvr_poly_compile(&hdr_white, &cxt);
    cxt_plain(&cxt, PVR_LIST_TR_POLY);
    cxt.gen.shading = PVR_SHADE_GOURAUD;
    pvr_poly_compile(&hdr_gouraud, &cxt);
    cxt_plain(&cxt, PVR_LIST_TR_POLY);
    cxt.depth.write = PVR_DEPTHWRITE_ENABLE;
    pvr_poly_compile(&hdr_a, &cxt);
    cxt_plain(&cxt, PVR_LIST_TR_POLY);
    cxt.depth.comparison = PVR_DEPTHCMP_GREATER;
    pvr_poly_compile(&hdr_gt, &cxt);
}

static void test_describe(void) {
    static const char *rows[7] = { "point 4x, u", "point 4x, v", "bilinear 4x, u", "bilinear 4x, v", "point 1:1, u" };
    int p, c, r;

    for(p = 0; p < NUM_PAGES; p++) {
        for(c = 0; c < 64; c++)
            label(p, (c / 8) * COLS + c % 8, "reg %d: quad at +%d/8, +%d/8", p, c % 8, c / 8);
        for(c = 0; c < 8; c++)
            label(p, 8 * COLS + c, "reg %d: depth slope %s, k %d", p, c < 4 ? "x" : "y", 2 + 2 * (c % 4));
        for(c = 0; c < 6; c++)
            label(p, 9 + c, "reg %d: gouraud %d", p, c);
        for(r = 0; r < 5; r++)
            for(c = 0; c < 9; c++)
                label(p, (r + 1) * COLS + 9 + c, "reg %d: %s shifted %+d/8", p, rows[r], c - 4);
        for(c = 0; c < 8; c++)
            label(p, 6 * COLS + 9 + c, "reg %d: bilinear step along %s, start +%d/128", p, c < 4 ? "u" : "v", c % 4);
        for(c = 0; c < 4; c++)
            label(p, 7 * COLS + 9 + c, "reg %d: perspective %d", p, c);
    }
}

/* a quad with per-corner depth, color and texture coordinates; corners TL, TR, BL, BR */
static void quad_z(const pvr_poly_hdr_t *hdr, float x, float y, float w, float h, const float z[4], const uint32_t col[4],
                   float u0, float v0, float u1, float v1) {
    pvr_vertex_t v;
    int k;

    pvr_prim(hdr, sizeof(*hdr));
    v.oargb = 0;
    for(k = 0; k < 4; k++) {
        v.flags = (k == 3) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        v.x = x + (k & 1) * w;
        v.y = y + (k >> 1) * h;
        v.z = z[k];
        v.u = (k & 1) ? u1 : u0;
        v.v = (k >> 1) ? v1 : v0;
        v.argb = col[k];
        pvr_prim(&v, sizeof(v));
    }
}

static void textured(pvr_poly_hdr_t *hdr, pvr_ptr_t txr, int filter) {
    pvr_poly_cxt_t cxt;
    cxt_textured(&cxt, PVR_LIST_TR_POLY, PAL8, 8, 8, txr, filter);
    pvr_poly_compile(hdr, &cxt);
}

static void test_page(int page) {
    static const float one[4] = { 1, 1, 1, 1 };
    static const float zx[4] = { 1, 4, 1, 4 }, zy[4] = { 1, 1, 4, 4 };
    static const uint32_t white[4] = { 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff };
    static const uint32_t rx[4] = { 0xff000000, 0xffffffff, 0xff000000, 0xffffffff };
    static const uint32_t ry[4] = { 0xff000000, 0xff000000, 0xffffffff, 0xffffffff };
    pvr_poly_hdr_t hp, hb, hh, hv;
    int c, r;

    PVR_SET(0x80, page);
    textured(&hp, pattern, PVR_FILTER_NONE);
    textured(&hb, pattern, PVR_FILTER_BILINEAR);
    textured(&hh, ramp_h, PVR_FILTER_BILINEAR);
    textured(&hv, ramp_v, PVR_FILTER_BILINEAR);

    pvr_list_begin(PVR_LIST_TR_POLY);
    for(c = 0; c < 64; c++) {
        int cell = (c / 8) * COLS + c % 8;
        quad(&hdr_white, cell_x(cell) + 8.0f + (c % 8) / 8.0f, cell_y(cell) + 8.0f + (c / 8) / 8.0f, 8.0f, 8.0f, 1.0f, 0xffffffff);
    }
    for(c = 0; c < 8; c++) {
        int cell = 8 * COLS + c, k = 2 + 2 * (c % 4);
        float hi = 1.0f + 1.0f / (float)(1 << k), mid = 1.0f + 0.5f / (float)(1 << k);
        float z[4] = { 1.0f, c < 4 ? hi : 1.0f, c < 4 ? 1.0f : hi, hi };
        static const uint32_t red[4] = { 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000 };
        quad_z(&hdr_a, cell_x(cell), cell_y(cell), 32.0f, 32.0f, z, red, 0, 0, 1, 1);
        quad(&hdr_gt, cell_x(cell), cell_y(cell), 32.0f, 32.0f, mid, 0xff00ff00);
    }
    /* Gouraud */
    quad_z(&hdr_gouraud, cell_x(9) + 8, cell_y(9), 8.0f, 32.0f, one, rx, 0, 0, 1, 1);
    quad_z(&hdr_gouraud, cell_x(10), cell_y(10) + 8, 32.0f, 8.0f, one, ry, 0, 0, 1, 1);
    quad_z(&hdr_gouraud, cell_x(11), cell_y(11), 32.0f, 32.0f, one, rx, 0, 0, 1, 1);
    quad_z(&hdr_gouraud, cell_x(12), cell_y(12), 32.0f, 32.0f, one, ry, 0, 0, 1, 1);
    quad_z(&hdr_gouraud, cell_x(13) + 8, cell_y(13), 8.0f, 32.0f, zx, rx, 0, 0, 1, 1);
    quad_z(&hdr_gouraud, cell_x(14), cell_y(14) + 8, 32.0f, 8.0f, zy, ry, 0, 0, 1, 1);
    /* texture position */
    for(c = 0; c < 9; c++) {
        float d = (c - 4) / 8.0f, s = d / 32.0f, t = d / 8.0f;
        int cell;
        cell = 1 * COLS + 9 + c; quad_z(&hp, cell_x(cell), cell_y(cell), 32, 32, one, white, s, 0, 1 + s, 1);
        cell = 2 * COLS + 9 + c; quad_z(&hp, cell_x(cell), cell_y(cell), 32, 32, one, white, 0, s, 1, 1 + s);
        cell = 3 * COLS + 9 + c; quad_z(&hb, cell_x(cell), cell_y(cell), 32, 32, one, white, s, 0, 1 + s, 1);
        cell = 4 * COLS + 9 + c; quad_z(&hb, cell_x(cell), cell_y(cell), 32, 32, one, white, 0, s, 1, 1 + s);
        cell = 5 * COLS + 9 + c; quad_z(&hp, cell_x(cell) + 8, cell_y(cell) + 8, 8, 8, one, white, t, 0, 1 + t, 1);
    }
    for(c = 0; c < 4; c++) {
        float a = (3.5f + c / 128.0f) / 8.0f, b = a + 1.0f / 8.0f;
        int cell = 6 * COLS + 9 + c;
        quad_z(&hh, cell_x(cell), cell_y(cell), 32, 32, one, white, a, 0.25f, b, 0.25f);
        cell += 4;
        quad_z(&hv, cell_x(cell), cell_y(cell), 32, 32, one, white, 0.25f, a, 0.25f, b);
    }
    r = 7 * COLS + 9;
    quad_z(&hp, cell_x(r), cell_y(r), 32, 32, zx, white, 0, 0, 1, 1);
    quad_z(&hp, cell_x(r + 1), cell_y(r + 1), 32, 32, zy, white, 0, 0, 1, 1);
    quad_z(&hb, cell_x(r + 2), cell_y(r + 2), 32, 32, zx, white, 0, 0, 1, 1);
    quad_z(&hb, cell_x(r + 3), cell_y(r + 3), 32, 32, zy, white, 0, 0, 1, 1);
    pvr_list_finish();
}
