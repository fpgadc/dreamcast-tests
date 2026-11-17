/* Texture environment, wrap modes and color clamp, without the texel expansion and sampling-position
   differences getting in the way: the textures are 8 bpp paletted with an ARGB8888 palette (exact on both
   sides) and the point-sampled pattern uses POINT_FIX.

   page 0  texture environment: env (replace, modulate, decal, modulate alpha) x texture alpha on/off x
           vertex alpha on/off x offset color on/off; cells 0-31 translucent list, 40-71 opaque list.
           Four squares per cell with different texel, base and offset colors (flat shaded).
   page 1  the same, showing the alpha that reaches the framebuffer (white * destination alpha)
   page 2  wrap modes: 64x64 quads with coordinates -1 .. 3 (2 pixels per texel) for the 16 combinations
           of flip and clamp. The texture is 8x8: red = column, green = row, blue = checkerboard.
   page 3  color clamp (max 0xc0a080, min 0x203040): 64 gray levels, 4 squares per cell.
           cells 0-15 untextured, clamp on; 20-35 untextured, clamp off;
           40-55 textured (modulate) + offset 0x404040, clamp on; 60-75 the same, clamp off */
#define NUM_PAGES 4
#include "pvrtest.h"

static const uint32_t texel[4] = { 0xffffffff, 0x80c08040, 0x40ff00ff, 0xc03070b0 };
static const uint32_t base[4] = { 0x80c08040, 0xffffffff, 0xc0ff4080, 0x40a0e020 };
static const uint32_t offset[4] = { 0xff204060, 0x00404040, 0xffc0c0c0, 0x80ff00ff };
static pvr_ptr_t texel_txr[4], pattern;
static pvr_poly_hdr_t hdr_alpha_view;
#define PAL8 (PVR_TXRFMT_PAL8BPP | PVR_TXRFMT_8BPP_PAL(0))

static void test_setup(void) {
    pvr_poly_cxt_t cxt;
    uint8_t t[64];
    int q, i;

    pvr_set_pal_format(PVR_PAL_ARGB8888);
    for(i = 0; i < 64; i++)
        pvr_set_pal_entry(i, 0xff000000 | ((i % 8) * 36 << 16) | ((i / 8) * 36 << 8) | ((((i % 8) ^ (i / 8)) & 1) ? 255 : 0));
    for(q = 0; q < 4; q++) {
        pvr_set_pal_entry(64 + q, texel[q]);
        memset(t, 64 + q, sizeof(t));
        texel_txr[q] = upload_raw(t, sizeof(t));
    }
    for(i = 0; i < 64; i++)
        t[i] = i;
    pattern = pvr_mem_malloc(64);
    pvr_txr_load_ex(t, pattern, 8, 8, PVR_TXRLOAD_8BPP);
    cxt_plain(&cxt, PVR_LIST_TR_POLY);
    cxt.blend.src = PVR_BLEND_DESTALPHA;
    pvr_poly_compile(&hdr_alpha_view, &cxt);
}

static const char *env_name[4] = { "replace", "modulate", "decal", "modulate alpha" };
static const char *uvmode[4] = { "none", "V", "U", "UV" };

static void test_describe(void) {
    int c, page;

    for(page = 0; page < 2; page++)
        for(c = 0; c < 32; c++) {
            label(page, c, "TR %s%s%s%s%s", page ? "(alpha) " : "", env_name[c & 3], (c & 4) ? ", texture alpha" : "",
                  (c & 8) ? ", vertex alpha" : "", (c & 16) ? ", offset" : "");
            label(page, 40 + c, "OP %s%s%s%s%s", page ? "(alpha) " : "", env_name[c & 3], (c & 4) ? ", texture alpha" : "",
                  (c & 8) ? ", vertex alpha" : "", (c & 16) ? ", offset" : "");
        }
    for(c = 0; c < 16; c++)
        label_span(2, (c / 8) * 80 + (c % 8) * 2, 2, 2, "wrap: flip %s clamp %s", uvmode[c / 4], uvmode[c % 4]);
    for(c = 0; c < 16; c++) {
        label(3, c, "clamp on, untextured, gray %d..", c * 16);
        label(3, 20 + c, "clamp off, untextured, gray %d..", c * 16);
        label(3, 40 + c, "clamp on, modulate + offset, gray %d..", c * 16);
        label(3, 60 + c, "clamp off, modulate + offset, gray %d..", c * 16);
    }
}

static void env_cells(pvr_list_t list, int first) {
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;
    int c, q;

    for(c = 0; c < 32; c++) {
        for(q = 0; q < 4; q++) {
            uint32_t col[4] = { base[q], base[q], base[q], base[q] }, off[4] = { offset[q], offset[q], offset[q], offset[q] };
            cxt_textured(&cxt, list, PAL8, 8, 8, texel_txr[q], PVR_FILTER_NONE);
            cxt.txr.env = c & 3;
            cxt.txr.alpha = (c & 4) ? PVR_TXRALPHA_ENABLE : PVR_TXRALPHA_DISABLE;
            cxt.gen.alpha = (c & 8) ? PVR_ALPHA_ENABLE : PVR_ALPHA_DISABLE;
            cxt.gen.specular = (c & 16) ? PVR_SPECULAR_ENABLE : PVR_SPECULAR_DISABLE;
            cxt.gen.shading = PVR_SHADE_FLAT;
            pvr_poly_compile(&hdr, &cxt);
            quad_full(&hdr, cell_x(first + c) + (q & 1) * 16.0f, cell_y(first + c) + (q >> 1) * 16.0f, 16.0f, 16.0f, 1.0f,
                      0.0f, 0.0f, 1.0f, 1.0f, col, off);
        }
    }
}

static void clamp_cells(int first, int textured, int clamp) {
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;
    int n;

    if(textured) {
        cxt_textured(&cxt, PVR_LIST_TR_POLY, PAL8, 8, 8, texel_txr[0], PVR_FILTER_NONE);
        cxt.txr.env = PVR_TXRENV_MODULATE;
        cxt.gen.specular = PVR_SPECULAR_ENABLE;
    }
    else
        cxt_plain(&cxt, PVR_LIST_TR_POLY);
    cxt.gen.shading = PVR_SHADE_FLAT;
    cxt.gen.color_clamp = clamp ? PVR_CLRCLAMP_ENABLE : PVR_CLRCLAMP_DISABLE;
    pvr_poly_compile(&hdr, &cxt);
    for(n = 0; n < 64; n++) {
        uint32_t g = n * 4 + (n == 63 ? 3 : 0), col[4], off[4];
        int k;
        for(k = 0; k < 4; k++) {
            col[k] = 0xff000000 | (g << 16) | (g << 8) | g;
            off[k] = 0x00404040;
        }
        quad_full(&hdr, cell_x(first + n / 4) + (n & 1) * 16.0f, cell_y(first + n / 4) + ((n >> 1) & 1) * 16.0f, 16.0f, 16.0f, 1.0f,
                  0.0f, 0.0f, 1.0f, 1.0f, col, off);
    }
}

static void test_page(int page) {
    static const int flips[4] = { PVR_UVFLIP_NONE, PVR_UVFLIP_V, PVR_UVFLIP_U, PVR_UVFLIP_UV };
    static const int clamps[4] = { PVR_UVCLAMP_NONE, PVR_UVCLAMP_V, PVR_UVCLAMP_U, PVR_UVCLAMP_UV };
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;
    int c;

    PVR_SET(PVR_COLOR_CLAMP_MAX, 0xffc0a080);
    PVR_SET(PVR_COLOR_CLAMP_MIN, 0x00203040);
    if(page < 2) {
        pvr_list_begin(PVR_LIST_OP_POLY);
        env_cells(PVR_LIST_OP_POLY, 40);
        pvr_list_finish();
    }
    pvr_list_begin(PVR_LIST_TR_POLY);
    if(page < 2) {
        env_cells(PVR_LIST_TR_POLY, 0);
        if(page == 1)
            quad(&hdr_alpha_view, 0.0f, 0.0f, 640.0f, 480.0f, 1.0f, 0xffffffff);
    }
    else if(page == 2) {
        float s = POINT_FIX / 8.0f;
        for(c = 0; c < 16; c++) {
            int cell = (c / 8) * 80 + (c % 8) * 2;
            cxt_textured(&cxt, PVR_LIST_TR_POLY, PAL8, 8, 8, pattern, PVR_FILTER_NONE);
            cxt.txr.uv_flip = flips[c / 4];
            cxt.txr.uv_clamp = clamps[c % 4];
            pvr_poly_compile(&hdr, &cxt);
            quad_uv(&hdr, cell_x(cell), cell_y(cell), 64.0f, 64.0f, -1.0f + s, -1.0f + s, 3.0f + s, 3.0f + s);
        }
    }
    else {
        clamp_cells(0, 0, 1);
        clamp_cells(20, 0, 0);
        clamp_cells(40, 1, 1);
        clamp_cells(60, 1, 0);
    }
    pvr_list_finish();
}
