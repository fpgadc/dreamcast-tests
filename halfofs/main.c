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
#include "pvrtest2.h"

static uint32_t pattern, ramp_h, ramp_v;

/* the four parameter words of a polygon */
typedef struct { PCW pcw; ISP_TSP isp; TSP tsp; TCW tcw; } hdr_t;
static hdr_t hdr_white, hdr_gouraud, hdr_a, hdr_gt;

/* a translucent-list polygon, replace / one,zero blend, depth never written unless ZWriteDis = 0; txr = 0
   untextured, else an 8x8 8 bpp paletted texture (palette 0), twiddled */
static hdr_t make_hdr(uint32_t txr, int filter, int DepthMode, int ZWriteDis) {
    hdr_t h;

    h.pcw.full = 0;
    h.isp.full = 0;
    h.tsp.full = 0;
    h.tcw.full = 0;
    h.pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    h.pcw.ListType = ListType_Translucent;
    h.pcw.Group_En = 1;
    h.pcw.Strip_Len = 1;
    h.pcw.Gouraud = 1;
    h.isp.DepthMode = DepthMode;
    h.isp.ZWriteDis = ZWriteDis;
    h.tsp.SrcInstr = BlendInstr_One;
    h.tsp.DstInstr = BlendInstr_Zero;
    h.tsp.FogCtrl = FogCtrl_None;
    h.tsp.UseAlpha = 1;
    if(txr) {
        h.pcw.Texture = 1;
        h.isp.Texture = 1;
        h.tsp.ShadInstr = ShadInstr_Decal;
        h.tsp.FilterMode = filter;
        h.tsp.MipMapD = 4;
        h.tsp.TexU = 0;                         /* 8 texels */
        h.tsp.TexV = 0;
        h.tcw.PixelFmt = PixelFmt_Pal8;
        h.tcw.PalSelect = 0;
        h.tcw.TexAddr = txr >> 3;
    }
    return h;
}

static void test_setup(void) {
    uint8_t t[64];
    int i;

    pvr_pal_format(PAL_RAM_CTRL_ARGB8888);
    for(i = 0; i < 64; i++) {
        pvr_pal_entry(i, 0xff000000 | ((i % 8) * 36 << 16) | ((i / 8) * 36 << 8) | ((((i % 8) ^ (i / 8)) & 1) ? 255 : 0));
        t[i] = i;
    }
    pvr_pal_entry(64, 0xff000000);
    pvr_pal_entry(65, 0xffffffff);
    pattern = pvr_tex_alloc(64);
    pvr_tex_load_twiddled8(pattern, t, 8, 8);
    for(i = 0; i < 64; i++) t[i] = 64 + ((i % 8) >= 4);
    ramp_h = pvr_tex_alloc(64);
    pvr_tex_load_twiddled8(ramp_h, t, 8, 8);
    for(i = 0; i < 64; i++) t[i] = 64 + ((i / 8) >= 4);
    ramp_v = pvr_tex_alloc(64);
    pvr_tex_load_twiddled8(ramp_v, t, 8, 8);

    hdr_white = make_hdr(0, 0, DepthMode_Always, 1);
    hdr_gouraud = make_hdr(0, 0, DepthMode_Always, 1);
    hdr_a = make_hdr(0, 0, DepthMode_Always, 0);        /* writes depth */
    hdr_gt = make_hdr(0, 0, DepthMode_Greater, 1);
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

/* a quad with per-corner depth, color and texture coordinates; corners TL, TR, BL, BR. x, y, w, h are floats
   (bits): x + (k & 1) * w is x or x + w. */
static void quad_z(const hdr_t *h, f32b x, f32b y, f32b w, f32b ht, const f32b z[4], const uint32_t col[4],
                   f32b u0, f32b v0, f32b u1, f32b v1) {
    int k;

    ta_polygon(h->pcw, h->isp, h->tsp, h->tcw);
    for(k = 0; k < 4; k++) {
        f32b vx = (k & 1) ? sf_add(x, w) : x, vy = (k >> 1) ? sf_add(y, ht) : y;
        if(h->pcw.Texture)
            ta_vertex3(vx, vy, z[k], (k & 1) ? u1 : u0, (k >> 1) ? v1 : v0, col[k], 0, k == 3);
        else
            ta_vertex0(vx, vy, z[k], col[k], k == 3);
    }
}

/* f32b of a whole number of pixels */
#define PX(n) f32b_int(n)

static hdr_t textured(uint32_t txr, int filter) {
    return make_hdr(txr, filter, DepthMode_Always, 1);
}

static void test_page(int page) {
    static const f32b one[4] = { 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000 };
    static const f32b zx[4] = { 0x3f800000, 0x40800000, 0x3f800000, 0x40800000 }, zy[4] = { 0x3f800000, 0x3f800000, 0x40800000, 0x40800000 };     /* 1, 4 */
    static const uint32_t white[4] = { 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff };
    static const uint32_t rx[4] = { 0xff000000, 0xffffffff, 0xff000000, 0xffffffff };
    static const uint32_t ry[4] = { 0xff000000, 0xff000000, 0xffffffff, 0xffffffff };
    const f32b Z = F32B_ZERO, O = F32B_ONE;
    hdr_t hp, hb, hh, hv;
    int c, r;

    pvrio_reg(HALF_OFFSET_addr, page);
    hp = textured(pattern, FilterMode_Point);
    hb = textured(pattern, FilterMode_Bilinear);
    hh = textured(ramp_h, FilterMode_Bilinear);
    hv = textured(ramp_v, FilterMode_Bilinear);

    ta_list_begin(ListType_Translucent);
    for(c = 0; c < 64; c++) {
        int cell = (c / 8) * COLS + c % 8;
        /* the quad at (8 + i/8, 8 + j/8): 8 * (8 + ...) + i is exact */
        static const uint32_t w4[4] = { 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff };
        quad_z(&hdr_white, f32b_scaled((cell_px(cell) + 8) * 8 + c % 8, -3), f32b_scaled((cell_py(cell) + 8) * 8 + c / 8, -3), PX(8), PX(8), one, w4, Z, Z, O, O);
    }
    for(c = 0; c < 8; c++) {
        int cell = 8 * COLS + c, k = 2 + 2 * (c % 4);
        /* hi = 1 + 2^-k, mid = 1 + 2^-(k+1): exact */
        f32b hi = sf_add(O, f32b_scaled(1, -k)), mid = sf_add(O, f32b_scaled(1, -k - 1));
        f32b z[4] = { O, c < 4 ? hi : O, c < 4 ? O : hi, hi };
        static const uint32_t red[4] = { 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000 };
        static const uint32_t green[4] = { 0xff00ff00, 0xff00ff00, 0xff00ff00, 0xff00ff00 };
        f32b zm[4] = { mid, mid, mid, mid };
        quad_z(&hdr_a, PX(cell_px(cell)), PX(cell_py(cell)), PX(32), PX(32), z, red, Z, Z, O, O);
        quad_z(&hdr_gt, PX(cell_px(cell)), PX(cell_py(cell)), PX(32), PX(32), zm, green, Z, Z, O, O);
    }
    /* Gouraud */
    quad_z(&hdr_gouraud, PX(cell_px(9) + 8), PX(cell_py(9)), PX(8), PX(32), one, rx, Z, Z, O, O);
    quad_z(&hdr_gouraud, PX(cell_px(10)), PX(cell_py(10) + 8), PX(32), PX(8), one, ry, Z, Z, O, O);
    quad_z(&hdr_gouraud, PX(cell_px(11)), PX(cell_py(11)), PX(32), PX(32), one, rx, Z, Z, O, O);
    quad_z(&hdr_gouraud, PX(cell_px(12)), PX(cell_py(12)), PX(32), PX(32), one, ry, Z, Z, O, O);
    quad_z(&hdr_gouraud, PX(cell_px(13) + 8), PX(cell_py(13)), PX(8), PX(32), zx, rx, Z, Z, O, O);
    quad_z(&hdr_gouraud, PX(cell_px(14)), PX(cell_py(14) + 8), PX(32), PX(8), zy, ry, Z, Z, O, O);
    /* texture position */
    for(c = 0; c < 9; c++) {
        /* d = (c - 4) / 8, s = d / 32, t = d / 8: exact */
        f32b s = f32b_scaled(c - 4, -8), t = f32b_scaled(c - 4, -6);
        f32b one_s = sf_add(O, s), one_t = sf_add(O, t);
        int cell;
        cell = 1 * COLS + 9 + c; quad_z(&hp, PX(cell_px(cell)), PX(cell_py(cell)), PX(32), PX(32), one, white, s, Z, one_s, O);
        cell = 2 * COLS + 9 + c; quad_z(&hp, PX(cell_px(cell)), PX(cell_py(cell)), PX(32), PX(32), one, white, Z, s, O, one_s);
        cell = 3 * COLS + 9 + c; quad_z(&hb, PX(cell_px(cell)), PX(cell_py(cell)), PX(32), PX(32), one, white, s, Z, one_s, O);
        cell = 4 * COLS + 9 + c; quad_z(&hb, PX(cell_px(cell)), PX(cell_py(cell)), PX(32), PX(32), one, white, Z, s, O, one_s);
        cell = 5 * COLS + 9 + c; quad_z(&hp, PX(cell_px(cell) + 8), PX(cell_py(cell) + 8), PX(8), PX(8), one, white, t, Z, one_t, O);
    }
    for(c = 0; c < 4; c++) {
        /* a = (3.5 + c/128) / 8, b = a + 1/8: exact */
        f32b a = f32b_scaled(448 + c, -10), b = sf_add(a, F(0.125f));
        int cell = 6 * COLS + 9 + c;
        quad_z(&hh, PX(cell_px(cell)), PX(cell_py(cell)), PX(32), PX(32), one, white, a, F(0.25f), b, F(0.25f));
        cell += 4;
        quad_z(&hv, PX(cell_px(cell)), PX(cell_py(cell)), PX(32), PX(32), one, white, F(0.25f), a, F(0.25f), b);
    }
    r = 7 * COLS + 9;
    quad_z(&hp, PX(cell_px(r)), PX(cell_py(r)), PX(32), PX(32), zx, white, Z, Z, O, O);
    quad_z(&hp, PX(cell_px(r + 1)), PX(cell_py(r + 1)), PX(32), PX(32), zy, white, Z, Z, O, O);
    quad_z(&hb, PX(cell_px(r + 2)), PX(cell_py(r + 2)), PX(32), PX(32), zx, white, Z, Z, O, O);
    quad_z(&hb, PX(cell_px(r + 3)), PX(cell_py(r + 3)), PX(32), PX(32), zy, white, Z, Z, O, O);
    ta_list_end();
}
