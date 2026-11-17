/* Texture sampling: where a pixel samples, bilinear weights, and the wrap modes.

   Texture A is 8x8 RGB565: red = column, green = row (both in steps that survive any 5/6 bit expansion),
   blue = checkerboard.

   page 0  point sampling, texture magnified 4x (a 32x32 quad). The texture coordinates are shifted by
           d = -1 .. +1 pixel in steps of 1/8: cells 0-16 along u, 20-36 along v, 40-56 along both.
           cells 60-76: the texture 1:1 (an 8x8 quad), shifted along u by d = -1 .. +1 texel in steps of 1/8
           cells 80-96: the texture at half size (a 4x4 quad), same shifts
   page 1  the same with bilinear filtering
   page 2  bilinear weights. One texel step stretched over the 32 pixels of a cell, starting k/256 of a
           texel further along in each of 8 cells, so all 256 fractional positions are covered.
           Four color steps (0->255, 255->0, 0->1 .. see below), horizontally (cells 0-31) and
           vertically (cells 40-71)
   page 3  wrap modes: 64x64 quads with coordinates -1 .. 3, point sampled, for the 16 combinations of
           flip (none, V, U, UV) and clamp (none, V, U, UV) */
#define NUM_PAGES 4
#include "pvrtest.h"

static pvr_ptr_t tex_a, tex_h[4], tex_v[4];
static const uint8_t steps[4][2] = { { 0, 255 }, { 255, 0 }, { 0, 8 }, { 64, 192 } };

static uint16_t grey565(int v) { return (uint16_t)(((v >> 3) << 11) | ((v >> 2) << 5) | (v >> 3)); }

static void test_setup(void) {
    uint16_t t[64];
    int i, j, k;

    for(j = 0; j < 8; j++)
        for(i = 0; i < 8; i++)
            t[j * 8 + i] = (uint16_t)(((i * 4 + 2) << 11) | ((j * 8 + 4) << 5) | (((i ^ j) & 1) ? 31 : 0));
    tex_a = pvr_mem_malloc(128);
    pvr_txr_load_ex(t, tex_a, 8, 8, PVR_TXRLOAD_16BPP);
    for(k = 0; k < 4; k++) {
        for(j = 0; j < 8; j++)
            for(i = 0; i < 8; i++)
                t[j * 8 + i] = grey565(steps[k][i >= 4]);
        tex_h[k] = pvr_mem_malloc(128);
        pvr_txr_load_ex(t, tex_h[k], 8, 8, PVR_TXRLOAD_16BPP);
        for(j = 0; j < 8; j++)
            for(i = 0; i < 8; i++)
                t[j * 8 + i] = grey565(steps[k][j >= 4]);
        tex_v[k] = pvr_mem_malloc(128);
        pvr_txr_load_ex(t, tex_v[k], 8, 8, PVR_TXRLOAD_16BPP);
    }
}

static void test_describe(void) {
    int page, c, k;

    for(page = 0; page < 2; page++) {
        const char *f = page ? "bilinear" : "point";
        for(c = 0; c <= 16; c++) {
            label(page, c, "%s 4x, u shifted %+d/8 px", f, c - 8);
            label(page, 20 + c, "%s 4x, v shifted %+d/8 px", f, c - 8);
            label(page, 40 + c, "%s 4x, u and v shifted %+d/8 px", f, c - 8);
            label(page, 60 + c, "%s 1:1, u shifted %+d/8 texel", f, c - 8);
            label(page, 80 + c, "%s half size, u shifted %+d/8 texel", f, c - 8);
        }
    }
    for(k = 0; k < 4; k++)
        for(c = 0; c < 8; c++) {
            label(2, k * 8 + c, "bilinear %d->%d along u, start +%d/256 texel", steps[k][0], steps[k][1], c);
            label(2, 40 + k * 8 + c, "bilinear %d->%d along v, start +%d/256 texel", steps[k][0], steps[k][1], c);
        }
    for(c = 0; c < 16; c++)
        label_span(3, (c / 8) * 80 + (c % 8) * 2, 2, 2, "wrap: flip %s clamp %s",
                   (const char *[]){ "none", "V", "U", "UV" }[c / 4], (const char *[]){ "none", "V", "U", "UV" }[c % 4]);
}

static void textured(pvr_poly_hdr_t *hdr, pvr_ptr_t txr, int filter, int flip, int clamp) {
    pvr_poly_cxt_t cxt;

    cxt_textured(&cxt, PVR_LIST_TR_POLY, PVR_TXRFMT_RGB565, 8, 8, txr, filter);
    cxt.txr.uv_flip = flip;
    cxt.txr.uv_clamp = clamp;
    pvr_poly_compile(hdr, &cxt);
}

static void test_page(int page) {
    pvr_poly_hdr_t hdr;
    int c, k;

    pvr_list_begin(PVR_LIST_TR_POLY);
    if(page < 2) {
        textured(&hdr, tex_a, page ? PVR_FILTER_BILINEAR : PVR_FILTER_NONE, PVR_UVFLIP_NONE, PVR_UVCLAMP_NONE);
        for(c = 0; c <= 16; c++) {
            float d = (c - 8) / 8.0f, s = d / 32.0f, t = d / 8.0f;
            quad_uv(&hdr, cell_x(c), cell_y(c), 32.0f, 32.0f, s, 0.0f, 1.0f + s, 1.0f);
            quad_uv(&hdr, cell_x(20 + c), cell_y(20 + c), 32.0f, 32.0f, 0.0f, s, 1.0f, 1.0f + s);
            quad_uv(&hdr, cell_x(40 + c), cell_y(40 + c), 32.0f, 32.0f, s, s, 1.0f + s, 1.0f + s);
            quad_uv(&hdr, cell_x(60 + c) + 8.0f, cell_y(60 + c) + 8.0f, 8.0f, 8.0f, t, 0.0f, 1.0f + t, 1.0f);
            quad_uv(&hdr, cell_x(80 + c) + 8.0f, cell_y(80 + c) + 8.0f, 4.0f, 4.0f, t, 0.0f, 1.0f + t, 1.0f);
        }
    }
    else if(page == 2) {
        for(k = 0; k < 4; k++) {
            for(c = 0; c < 8; c++) {
                /* texel centers 3 and 4 are at 3.5/8 and 4.5/8 */
                float a = (3.5f + c / 256.0f) / 8.0f, b = a + 1.0f / 8.0f;
                textured(&hdr, tex_h[k], PVR_FILTER_BILINEAR, PVR_UVFLIP_NONE, PVR_UVCLAMP_NONE);
                quad_uv(&hdr, cell_x(k * 8 + c), cell_y(k * 8 + c), 32.0f, 32.0f, a, 0.25f, b, 0.25f);
                textured(&hdr, tex_v[k], PVR_FILTER_BILINEAR, PVR_UVFLIP_NONE, PVR_UVCLAMP_NONE);
                quad_uv(&hdr, cell_x(40 + k * 8 + c), cell_y(40 + k * 8 + c), 32.0f, 32.0f, 0.25f, a, 0.25f, b);
            }
        }
    }
    else {
        static const int flips[4] = { PVR_UVFLIP_NONE, PVR_UVFLIP_V, PVR_UVFLIP_U, PVR_UVFLIP_UV };
        static const int clamps[4] = { PVR_UVCLAMP_NONE, PVR_UVCLAMP_V, PVR_UVCLAMP_U, PVR_UVCLAMP_UV };
        for(c = 0; c < 16; c++) {
            int cell = (c / 8) * 80 + (c % 8) * 2;
            textured(&hdr, tex_a, PVR_FILTER_NONE, flips[c / 4], clamps[c % 4]);
            quad_uv(&hdr, cell_x(cell), cell_y(cell), 64.0f, 64.0f, -1.0f, -1.0f, 3.0f, 3.0f);
        }
    }
    pvr_list_finish();
}
