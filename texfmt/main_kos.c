/* Texel formats: how each format's bits become 8 bits per channel.

   Every square (16x16 pixels, four per cell) is a quad textured with an 8x8 texture whose texels are all
   the same value, so where the texture is sampled does not matter.

   page 0  RGB565 (each channel swept alone), ARGB1555, ARGB4444; then alpha of 1555 and 4444, shown as
           white * alpha through a (SRC_ALPHA, ZERO) blend
   page 1  YUV422: Y swept with U = V = 128, then a grid of U, V at Y = 0, 128, 255
   page 2  YUV422: U swept, then V swept, at Y = 128
   page 3..6  8 bpp paletted, palette format ARGB1555, RGB565, ARGB4444, ARGB8888 */
#define NUM_PAGES 7
#include "pvrtest.h"

enum { F565, F1555, F4444, FYUV, FPAL };
typedef struct { uint8_t fmt, alpha_view; uint16_t raw, raw2; pvr_ptr_t txr; char tag[20]; } item_t;

#define MAX_ITEMS 520
static item_t items[3][MAX_ITEMS];
static int num_items[3];
static pvr_ptr_t pal_txr[256];
static uint32_t pal_entries[4][256];

static void add(int page, int fmt, int raw, int raw2, int alpha_view, const char *tag) {
    item_t *it = &items[page][num_items[page]++];
    uint16_t t[64];
    int i;

    it->fmt = fmt;
    it->raw = raw;
    it->raw2 = raw2;
    it->alpha_view = alpha_view;
    strncpy(it->tag, tag, sizeof(it->tag) - 1);
    for(i = 0; i < 64; i++)
        t[i] = (fmt == FYUV && (i & 1)) ? raw2 : raw;      /* YUV422: words are Y0:U, Y1:V */
    it->txr = upload_raw(t, sizeof(t));
}

static void add_yuv(int page, int y, int u, int v, const char *tag) {
    add(page, FYUV, (y << 8) | u, (y << 8) | v, 0, tag);
}

static void test_setup(void) {
    static const int uv[5] = { 0, 64, 128, 192, 255 };
    static const int ys[3] = { 0, 128, 255 };
    int i, j, k, f;

    for(i = 0; i < 32; i++) add(0, F565, i << 11, 0, 0, "565 R");
    for(i = 0; i < 64; i++) add(0, F565, i << 5, 0, 0, "565 G");
    for(i = 0; i < 32; i++) add(0, F565, i, 0, 0, "565 B");
    for(i = 0; i < 32; i++) add(0, F1555, 0x8000 | (i << 10), 0, 0, "1555 R");
    for(i = 0; i < 32; i++) add(0, F1555, 0x8000 | (i << 5), 0, 0, "1555 G");
    for(i = 0; i < 32; i++) add(0, F1555, 0x8000 | i, 0, 0, "1555 B");
    for(i = 0; i < 16; i++) add(0, F4444, 0xf000 | (i << 8), 0, 0, "4444 R");
    for(i = 0; i < 16; i++) add(0, F4444, 0xf000 | (i << 4), 0, 0, "4444 G");
    for(i = 0; i < 16; i++) add(0, F4444, 0xf000 | i, 0, 0, "4444 B");
    for(i = 0; i < 2; i++) add(0, F1555, (i << 15) | 0x7fff, 0, 1, "1555 A");
    for(i = 0; i < 2; i++) add(0, F1555, (i << 15) | 0x7fff, 0, 0, "1555 A, no blend");
    for(i = 0; i < 16; i++) add(0, F4444, (i << 12) | 0xfff, 0, 1, "4444 A");

    for(i = 0; i < 256; i++) add_yuv(1, i, 128, 128, "YUV Y");
    for(k = 0; k < 3; k++)
        for(i = 0; i < 5; i++)
            for(j = 0; j < 5; j++) {
                char tag[20];
                sprintf(tag, "YUV Y%d U%d V..", ys[k], uv[i]);
                add_yuv(1, ys[k], uv[i], uv[j], tag);
            }
    for(i = 0; i < 256; i++) add_yuv(2, 128, i, 128, "YUV U");
    for(i = 0; i < 256; i++) add_yuv(2, 128, 128, i, "YUV V");

    for(i = 0; i < 256; i++) {
        uint8_t t[64];
        memset(t, i, sizeof(t));
        pal_txr[i] = upload_raw(t, sizeof(t));
    }
    rng_state = 77;
    for(f = 0; f < 4; f++) {
        for(i = 0; i < 256; i++) {
            uint32_t r = rng(), e;
            if(f == 0)      e = 0x8000 | (i < 32 ? i << 10 : i < 64 ? (i - 32) << 5 : i < 96 ? i - 64 : (r & 0x7fff));
            else if(f == 1) e = i < 32 ? i << 11 : i < 96 ? (i - 32) << 5 : i < 128 ? i - 96 : (r & 0xffff);
            else if(f == 2) e = 0xf000 | (i < 16 ? i << 8 : i < 32 ? (i - 16) << 4 : i < 48 ? i - 32 : (r & 0xfff));
            else            e = 0xff000000 | (i << 16) | ((255 - i) << 8) | (i ^ 0x55);
            pal_entries[f][i] = e;
        }
    }
}

static void test_describe(void) {
    static const char *pal_name[4] = { "ARGB1555", "RGB565", "ARGB4444", "ARGB8888" };
    int page, n, f;

    for(page = 0; page < 3; page++)
        for(n = 0; n < num_items[page]; n += 4)
            label(page, n / 4, "%s, raw %04x.. (4 squares)", items[page][n].tag, items[page][n].raw);
    for(f = 0; f < 4; f++)
        for(n = 0; n < 256; n += 4)
            label(3 + f, n / 4, "PAL8 %s entries %d..%d, first %08x", pal_name[f], n, n + 3, (unsigned)pal_entries[f][n]);
}

static void square(int n, int format, pvr_ptr_t txr, int alpha_view) {
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;
    int c = n / 4, q = n % 4;

    cxt_textured(&cxt, PVR_LIST_TR_POLY, format, 8, 8, txr, PVR_FILTER_NONE);
    if(alpha_view)
        cxt.blend.src = PVR_BLEND_SRCALPHA;
    pvr_poly_compile(&hdr, &cxt);
    quad_uv(&hdr, cell_x(c) + (q & 1) * 16.0f, cell_y(c) + (q >> 1) * 16.0f, 16.0f, 16.0f, 0.0f, 0.0f, 1.0f, 1.0f);
}

static void test_page(int page) {
    static const int fmt[4] = { PVR_TXRFMT_RGB565, PVR_TXRFMT_ARGB1555, PVR_TXRFMT_ARGB4444,
                                PVR_TXRFMT_YUV422 | PVR_TXRFMT_NONTWIDDLED };
    static const int pal_fmt[4] = { PVR_PAL_ARGB1555, PVR_PAL_RGB565, PVR_PAL_ARGB4444, PVR_PAL_ARGB8888 };
    int n;

    pvr_list_begin(PVR_LIST_TR_POLY);
    if(page < 3) {
        for(n = 0; n < num_items[page]; n++)
            square(n, fmt[items[page][n].fmt], items[page][n].txr, items[page][n].alpha_view);
    }
    else {
        pvr_set_pal_format(pal_fmt[page - 3]);
        for(n = 0; n < 256; n++)
            pvr_set_pal_entry(n, pal_entries[page - 3][n]);
        for(n = 0; n < 256; n++)
            square(n, PVR_TXRFMT_PAL8BPP | PVR_TXRFMT_8BPP_PAL(0), pal_txr[n], 0);
    }
    pvr_list_finish();
}
