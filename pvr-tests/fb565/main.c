/* 16 bit framebuffers: how 8 bit colors are reduced, with and without dither.

   Built as fb565 (RGB565) and, through ../fb555/main.c, as fb555 (RGB555).

   1024 flat squares, four per cell (cells 0-255): gray levels 0..255, then red, green and blue alone 0..255.
   Each square is 16x16, so it holds 16 periods of the 4x4 dither pattern.
   Rows 13, 14: a 640 pixel wide Gouraud ramp, gray 0 -> 255 and 255 -> 0.
   page 0: dither off. page 1: dither on (bit 3 of FB_W_CTRL). */
#ifndef NUM_PAGES
#define NUM_PAGES 2
#endif
#ifndef TEST_FB_PACKMODE
#define TEST_FB_PACKMODE fb_packmode_565
#endif
#include "pvrtest2.h"

/* an opaque, untextured polygon header: the Gouraud bit is the only difference */
static void polygon(int gouraud) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };

    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Opaque;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Gouraud = gouraud;
    isp.DepthMode = DepthMode_Always;
    isp.ZWriteDis = 1;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    ta_polygon(pcw, isp, tsp, tcw);
}

static void test_setup(void) {
}

static void test_describe(void) {
    static const char *ch[4] = { "gray", "red", "green", "blue" };
    int p, c;

    for(p = 0; p < 2; p++) {
        for(c = 0; c < 256; c++)
            label(p, c, "dither %s, %s %d..%d", p ? "on" : "off", ch[c / 64], (c % 64) * 4, (c % 64) * 4 + 3);
        label_span(p, 260, 20, 1, "dither %s, ramp 0 -> 255", p ? "on" : "off");
        label_span(p, 280, 20, 1, "dither %s, ramp 255 -> 0", p ? "on" : "off");
    }
}

static void ramp(int y, const uint32_t c[4]) {
    int k;
    polygon(1);
    for(k = 0; k < 4; k++)
        ta_vertex0(f32b_int((k & 1) * 640), f32b_int(y + (k >> 1) * 32), F32B_ONE, c[k], k == 3);
}

static void test_page(int page) {
    static const uint32_t mask[4] = { 0xffffff, 0xff0000, 0x00ff00, 0x0000ff };
    static const uint32_t up[4] = { 0xff000000, 0xffffffff, 0xff000000, 0xffffffff };
    static const uint32_t down[4] = { 0xffffffff, 0xff000000, 0xffffffff, 0xff000000 };
    FB_W_CTRL_type fbw = { .full = 0 };
    int n;

    fbw.fb_packmode = TEST_FB_PACKMODE;
    fbw.fb_dither = page ? 1 : 0;           /* page 0: dither off, page 1: on */
    pvrio_reg(FB_W_CTRL_addr, fbw.full);
    ta_list_begin(ListType_Opaque);
    for(n = 0; n < 1024; n++) {
        uint32_t v = n & 255;
        polygon(0);
        quad0(cell_px(n / 4) + (n & 1) * 16, cell_py(n / 4) + ((n >> 1) & 1) * 16, 16, 16, F32B_ONE,
              0xff000000 | (((v << 16) | (v << 8) | v) & mask[n / 256]));
    }
    ramp(13 * 32, up);
    ramp(14 * 32, down);
    ta_list_end();
}
