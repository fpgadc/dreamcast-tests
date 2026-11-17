/* Shading: Gouraud interpolation, and how texture, base color and offset color are combined.

   page 0  Gouraud quads (two triangles, split along the top right - bottom left diagonal):
           cells 0-15   one cell each, corner colors per the label
           rows 1, 2    a 640 pixel wide ramp, 0 -> 255 and 255 -> 0
           rows 3-10    256 pixel tall ramps, 32 wide
   page 1  texture environment: every combination of
               texture env (replace, modulate, decal, modulate alpha) x texture alpha on/off x
               vertex alpha on/off x offset color on/off
           cells 0-31 in the translucent list, cells 40-71 in the opaque list. Each cell is four squares
           with different texel (uniform ARGB4444 texture), base and offset colors; flat shaded.
   page 2  the same, showing the alpha that reaches the framebuffer (white * destination alpha) */
#define NUM_PAGES 3
#include "pvrtest.h"

typedef struct { uint32_t c[4]; const char *name; } corners_t;
static const corners_t small[16] = {
    { { 0xff000000, 0xffffffff, 0xff000000, 0xffffffff }, "left 0, right 255" },
    { { 0xffffffff, 0xff000000, 0xffffffff, 0xff000000 }, "left 255, right 0" },
    { { 0xff000000, 0xff000000, 0xffffffff, 0xffffffff }, "top 0, bottom 255" },
    { { 0xffffffff, 0xffffffff, 0xff000000, 0xff000000 }, "top 255, bottom 0" },
    { { 0xffffffff, 0xff000000, 0xff000000, 0xff000000 }, "top left 255, rest 0" },
    { { 0xff000000, 0xffffffff, 0xff000000, 0xff000000 }, "top right 255, rest 0" },
    { { 0xff000000, 0xff000000, 0xffffffff, 0xff000000 }, "bottom left 255, rest 0" },
    { { 0xff000000, 0xff000000, 0xff000000, 0xffffffff }, "bottom right 255, rest 0" },
    { { 0xffff0000, 0xff00ff00, 0xff0000ff, 0xffffffff }, "red, green, blue, white" },
    { { 0xff000000, 0xff101010, 0xff000000, 0xff101010 }, "left 0, right 16" },
    { { 0xff000000, 0xff010101, 0xff000000, 0xff010101 }, "left 0, right 1" },
    { { 0xfffefefe, 0xffffffff, 0xfffefefe, 0xffffffff }, "left 254, right 255" },
    { { 0xff646464, 0xff656565, 0xff646464, 0xff656565 }, "left 100, right 101" },
    { { 0xff808080, 0xff808080, 0xff808080, 0xff808080 }, "all 128" },
    { { 0xff000000, 0xff0000ff, 0xff00ff00, 0xffff0000 }, "black, blue, green, red" },
    { { 0xff7f7f7f, 0xff808080, 0xff808080, 0xff7f7f7f }, "127, 128, 128, 127" },
};

/* texel, base and offset colors of the four squares of a cell */
static const uint16_t texel4444[4] = { 0xffff, 0x8c84, 0x4f0f, 0xc37b };
static const uint32_t base[4] = { 0x80c08040, 0xffffffff, 0xc0ff4080, 0x40a0e020 };
static const uint32_t offset[4] = { 0xff204060, 0x00404040, 0xffc0c0c0, 0x80ff00ff };
static pvr_ptr_t texel_txr[4];
static pvr_poly_hdr_t hdr_gouraud, hdr_alpha_view;

static void test_setup(void) {
    pvr_poly_cxt_t cxt;
    uint16_t t[64];
    int q, i;

    for(q = 0; q < 4; q++) {
        for(i = 0; i < 64; i++)
            t[i] = texel4444[q];
        texel_txr[q] = upload_raw(t, sizeof(t));
    }
    cxt_plain(&cxt, PVR_LIST_TR_POLY);
    cxt.gen.shading = PVR_SHADE_GOURAUD;
    pvr_poly_compile(&hdr_gouraud, &cxt);
    cxt_plain(&cxt, PVR_LIST_TR_POLY);
    cxt.blend.src = PVR_BLEND_DESTALPHA;
    pvr_poly_compile(&hdr_alpha_view, &cxt);
}

static const char *env_name[4] = { "replace", "modulate", "decal", "modulate alpha" };

static void test_describe(void) {
    int c, page;

    for(c = 0; c < 16; c++)
        label(0, c, "gouraud: %s", small[c].name);
    label_span(0, 20, 20, 1, "gouraud: 640 px ramp 0 -> 255");
    label_span(0, 40, 20, 1, "gouraud: 640 px ramp 255 -> 0");
    label_span(0, 60, 1, 8, "gouraud: 256 px tall ramp 0 -> 255");
    label_span(0, 61, 1, 8, "gouraud: 256 px tall ramp 255 -> 0");
    label_span(0, 62, 1, 8, "gouraud: 256 px tall ramp, red 0 -> 255, blue 255 -> 0");
    for(page = 1; page < 3; page++)
        for(c = 0; c < 32; c++) {
            label(page, c, "TR %s%s%s%s%s", page == 2 ? "(alpha) " : "", env_name[c & 3], (c & 4) ? ", texture alpha" : "",
                  (c & 8) ? ", vertex alpha" : "", (c & 16) ? ", offset" : "");
            label(page, 40 + c, "OP %s%s%s%s%s", page == 2 ? "(alpha) " : "", env_name[c & 3], (c & 4) ? ", texture alpha" : "",
                  (c & 8) ? ", vertex alpha" : "", (c & 16) ? ", offset" : "");
        }
}

static void env_cells(pvr_list_t list, int first) {
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;
    int c, q;

    for(c = 0; c < 32; c++) {
        for(q = 0; q < 4; q++) {
            uint32_t col[4] = { base[q], base[q], base[q], base[q] }, off[4] = { offset[q], offset[q], offset[q], offset[q] };
            cxt_textured(&cxt, list, PVR_TXRFMT_ARGB4444, 8, 8, texel_txr[q], PVR_FILTER_NONE);
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

static void test_page(int page) {
    int c;

    if(page == 0) {
        static const uint32_t up[4] = { 0xff000000, 0xffffffff, 0xff000000, 0xffffffff };
        static const uint32_t down[4] = { 0xffffffff, 0xff000000, 0xffffffff, 0xff000000 };
        static const uint32_t v_up[4] = { 0xff000000, 0xff000000, 0xffffffff, 0xffffffff };
        static const uint32_t v_down[4] = { 0xffffffff, 0xffffffff, 0xff000000, 0xff000000 };
        static const uint32_t v_mix[4] = { 0xff0000ff, 0xff0000ff, 0xffff0000, 0xffff0000 };
        pvr_list_begin(PVR_LIST_TR_POLY);
        for(c = 0; c < 16; c++)
            quad_full(&hdr_gouraud, cell_x(c), cell_y(c), 32.0f, 32.0f, 1.0f, 0, 0, 1, 1, small[c].c, NULL);
        quad_full(&hdr_gouraud, 0.0f, 32.0f, 640.0f, 32.0f, 1.0f, 0, 0, 1, 1, up, NULL);
        quad_full(&hdr_gouraud, 0.0f, 64.0f, 640.0f, 32.0f, 1.0f, 0, 0, 1, 1, down, NULL);
        quad_full(&hdr_gouraud, 0.0f, 96.0f, 32.0f, 256.0f, 1.0f, 0, 0, 1, 1, v_up, NULL);
        quad_full(&hdr_gouraud, 32.0f, 96.0f, 32.0f, 256.0f, 1.0f, 0, 0, 1, 1, v_down, NULL);
        quad_full(&hdr_gouraud, 64.0f, 96.0f, 32.0f, 256.0f, 1.0f, 0, 0, 1, 1, v_mix, NULL);
        pvr_list_finish();
        return;
    }
    pvr_list_begin(PVR_LIST_OP_POLY);
    env_cells(PVR_LIST_OP_POLY, 40);
    pvr_list_finish();
    pvr_list_begin(PVR_LIST_TR_POLY);
    env_cells(PVR_LIST_TR_POLY, 0);
    if(page == 2)
        quad(&hdr_alpha_view, 0.0f, 0.0f, 640.0f, 480.0f, 1.0f, 0xffffffff);
    pvr_list_finish();
}
