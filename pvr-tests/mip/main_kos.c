/* Mipmaps, VQ and strided textures.

   The mipmapped texture is 16x16 RGB565 with one flat color per level, so only the level selection (and,
   with trilinear, the blend between levels) shows:
       16x16 red, 8x8 green, 4x4 blue, 2x2 white, 1x1 yellow
   pages 0, 1, 2: point sampling, trilinear pass 1, trilinear pass 2. Row b (0..14) uses mipmap bias b + 1
   (in quarters: 0.25 .. 3.75). Columns 0-9: a square quad of 32, 24, 16, 12, 8, 6, 4, 3, 2, 1 pixels showing the
   whole texture. Columns 10-17: quads of 32x4, 4x32, 16x2, 2x16, 32x16, 16x32, 8x1, 1x8 pixels.
   page 3: bilinear.

   page 4: cells 0-1 (2x2 span at cell 0): an 8x8 VQ texture, every codebook entry and index different, at 8x
           (point sampled, with POINT_FIX); cell 4 (2x2 span): the same texture with bilinear filtering;
           cell 8 (4x1 span): a strided texture, declared 64x8 with a stride of 32 texels, shown 2x. */
#define NUM_PAGES 5
#include "pvrtest.h"

static pvr_ptr_t mip_txr, vq_txr, stride_txr;
static const float sq[10] = { 32, 24, 16, 12, 8, 6, 4, 3, 2, 1 };
static const float rect[8][2] = { { 32, 4 }, { 4, 32 }, { 16, 2 }, { 2, 16 }, { 32, 16 }, { 16, 32 }, { 8, 1 }, { 1, 8 } };

static void test_setup(void) {
    static uint16_t mip[348];
    static uint8_t vq[2048 + 16];
    static uint16_t st[32 * 8];
    static const uint16_t level_color[5] = { 0xffe0, 0xffff, 0x001f, 0x07e0, 0xf800 };   /* 1x1 .. 16x16 */
    int i, k, n = 3, size = 1;      /* the 1x1 level is the fourth word */

    for(k = 0; k < 5; k++, size *= 4)
        for(i = 0; i < size; i++)
            mip[n++] = level_color[k];
    mip_txr = upload_raw(mip, sizeof(mip));

    for(k = 0; k < 256; k++)
        for(i = 0; i < 4; i++) {
            uint16_t t = (uint16_t)(((k & 31) << 11) | (((k >> 5) * 8 + i * 2) << 5) | (i * 10));
            vq[k * 8 + i * 2] = t & 0xff;
            vq[k * 8 + i * 2 + 1] = t >> 8;
        }
    for(i = 0; i < 16; i++)
        vq[2048 + i] = (uint8_t)(i * 13 + 5);
    vq_txr = upload_raw(vq, sizeof(vq));

    for(i = 0; i < 32 * 8; i++)
        st[i] = (uint16_t)(((i % 32) << 11) | ((i / 32) * 8 << 5) | ((i & 1) ? 31 : 0));
    stride_txr = upload_raw(st, sizeof(st));
}

static void test_describe(void) {
    static const char *f[4] = { "point", "trilinear 1", "trilinear 2", "bilinear" };
    int p, b, c;

    for(p = 0; p < 4; p++)
        for(b = 0; b < 15; b++) {
            for(c = 0; c < 10; c++)
                label(p, b * COLS + c, "mip %s, bias %d/4, %gx%g px", f[p], b + 1, (double)sq[c], (double)sq[c]);
            for(c = 0; c < 8; c++)
                label(p, b * COLS + 10 + c, "mip %s, bias %d/4, %gx%g px", f[p], b + 1, (double)rect[c][0], (double)rect[c][1]);
        }
    label_span(4, 0, 2, 2, "VQ 8x8 at 8x, point");
    label_span(4, 4, 2, 2, "VQ 8x8 at 8x, bilinear");
    label_span(4, 8, 4, 1, "stride 32, 64x8 at 2x, point");
}

static void test_page(int page) {
    static const int filter[4] = { PVR_FILTER_NONE, PVR_FILTER_TRILINEAR1, PVR_FILTER_TRILINEAR2, PVR_FILTER_BILINEAR };
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;
    int b, c;

    pvr_list_begin(PVR_LIST_TR_POLY);
    if(page < 4) {
        for(b = 0; b < 15; b++) {
            cxt_textured(&cxt, PVR_LIST_TR_POLY, PVR_TXRFMT_RGB565, 16, 16, mip_txr, filter[page]);
            cxt.txr.mipmap = PVR_MIPMAP_ENABLE;
            cxt.txr.mipmap_bias = b + 1;
            pvr_poly_compile(&hdr, &cxt);
            for(c = 0; c < 10; c++)
                quad_uv(&hdr, cell_x(b * COLS + c), cell_y(b * COLS + c), sq[c], sq[c], 0.0f, 0.0f, 1.0f, 1.0f);
            for(c = 0; c < 8; c++)
                quad_uv(&hdr, cell_x(b * COLS + 10 + c), cell_y(b * COLS + 10 + c), rect[c][0], rect[c][1], 0.0f, 0.0f, 1.0f, 1.0f);
        }
    }
    else {
        float s = POINT_FIX / 8.0f;
        cxt_textured(&cxt, PVR_LIST_TR_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_VQ_ENABLE, 8, 8, vq_txr, PVR_FILTER_NONE);
        pvr_poly_compile(&hdr, &cxt);
        quad_uv(&hdr, cell_x(0), cell_y(0), 64.0f, 64.0f, s, s, 1.0f + s, 1.0f + s);
        cxt_textured(&cxt, PVR_LIST_TR_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_VQ_ENABLE, 8, 8, vq_txr, PVR_FILTER_BILINEAR);
        pvr_poly_compile(&hdr, &cxt);
        quad_uv(&hdr, cell_x(4), cell_y(4), 64.0f, 64.0f, 0.0f, 0.0f, 1.0f, 1.0f);

        PVR_SET(PVR_TEXTURE_MODULO, 1);         /* stride, in units of 32 texels */
        cxt_textured(&cxt, PVR_LIST_TR_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED | PVR_TXRFMT_STRIDE, 64, 8, stride_txr, PVR_FILTER_NONE);
        pvr_poly_compile(&hdr, &cxt);
        quad_uv(&hdr, cell_x(8), cell_y(8), 128.0f, 16.0f, POINT_FIX / 64.0f, POINT_FIX / 8.0f, 1.0f + POINT_FIX / 64.0f, 1.0f + POINT_FIX / 8.0f);
    }
    pvr_list_finish();
}
