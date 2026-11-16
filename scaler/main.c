/* The scaler (SCALER_CTL): vertical scaling / flicker filter and horizontal halving.

   The picture is the same on every page; page p writes scaler[p] to the register:
       0x00400  no scaling (reference)        0x00401  slight vertical scale (what KOS uses for interlace)
       0x00800  vertical scale 2:1            0x00600  vertical scale 1.5:1
       0x10400  horizontal halving
   Content: rows 0-1 single-pixel horizontal lines (white, then alternating red / green / blue);
   rows 2-3 single-pixel vertical lines; rows 4-5 a one-pixel checkerboard; rows 6-7 a Gouraud ramp;
   rows 8-14 flat blocks of 16 colors, one cell each, repeated. */
#define NUM_PAGES 5
#include "pvrtest2.h"

static const uint32_t scaler[NUM_PAGES] = { 0x00400, 0x00401, 0x00800, 0x00600, 0x10400 };

/* opaque polygon, no culling, depth always passes and is not written, replace (one, zero); flat or Gouraud */
static void poly(int Gouraud) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };

    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Opaque;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Gouraud = Gouraud;
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
    static const char *rows[5] = { "horizontal lines", "vertical lines", "checkerboard", "ramp", "color blocks" };
    int p, r;

    for(p = 0; p < NUM_PAGES; p++)
        for(r = 0; r < 15; r++)
            label_span(p, r * COLS, COLS, 1, "scaler %05x, %s, row %d", (unsigned)scaler[p], rows[r < 8 ? r / 2 : 4], r);
}

static void flat(int x, int y, int w, int h, uint32_t argb) {
    poly(0);
    quad0(x, y, w, h, F32B_ONE, argb);
}

static void test_page(int page) {
    static const uint32_t rgb[3] = { 0xffff0000, 0xff00ff00, 0xff0000ff };
    static const uint32_t up[4] = { 0xff000000, 0xffffffff, 0xff000000, 0xffffffff };
    int i, j, k;

    pvrio_reg(SCALER_CTL_addr, scaler[page]);
    ta_list_begin(ListType_Opaque);
    for(i = 0; i < 32; i += 2)
        flat(0, i, 640, 1, 0xffffffff);
    for(i = 0; i < 32; i += 2)
        flat(0, 32 + i, 640, 1, rgb[(i / 2) % 3]);
    for(i = 0; i < 640; i += 2)
        flat(i, 64, 1, 64, i < 320 ? 0xffffffff : rgb[(i / 2) % 3]);
    for(j = 0; j < 64; j += 2)
        for(i = 0; i < 640; i += 4) {
            flat(i, 128 + j, 1, 1, 0xffffffff);
            flat(i + 2, 128 + j, 1, 1, 0xffffffff);
            flat(i + 1, 129 + j, 1, 1, 0xffffffff);
            flat(i + 3, 129 + j, 1, 1, 0xffffffff);
        }
    poly(1);                                    /* the ramp: a colour per corner */
    for(k = 0; k < 4; k++)
        ta_vertex0(f32b_int((k & 1) * 640), f32b_int(192 + (k >> 1) * 64), F32B_ONE, up[k], k == 3);
    for(j = 8; j < 15; j++)
        for(i = 0; i < COLS; i++) {
            int c = (i + j * 5) % 16;
            flat(i * 32, j * 32, 32, 32,
                 0xff000000 | ((c & 1) ? 0xff0000 : 0) | ((c & 2) ? 0x00ff00 : 0) | ((c & 4) ? 0x0000ff : 0) | ((c & 8) ? 0x404040 : 0));
        }
    ta_list_end();
}
