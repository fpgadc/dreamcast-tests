/* Translucent autosort (the translucent list is sorted per pixel by the hardware).

   Each cell has overlapping half-transparent quads (SRC_ALPHA, 1 - SRC_ALPHA; alpha 0x80) over black:
   red over the left 24 pixels, green over the right 24, blue over the middle 16 rows. They are always
   submitted red, green, blue; what changes is their depth (larger z = nearer).

   page 0
     cells 0-5     the six depth orders of 0.3, 0.5, 0.7
     cells 6-9     ties: all three at 0.5; red = green; green = blue; red = blue (the third at 0.7)
     cells 10-11   red and green intersect: their depths slope in opposite directions (0.3..0.7 across the cell)
     cells 12-13   the same six-order test with depth write off; with depth compare ALWAYS
     cells 20-33   the same as 0-13 on top of an opaque gray quad at 0.4 (it hides what is behind it)
   page 1  the same, with additive blending (ONE, ONE), where order must not matter

   The PVR is driven directly (../common/pvrtest2.h); main_kos.c is the earlier version on KallistiOS's driver. */
#define NUM_PAGES 2
#define TEST_AUTOSORT
#include "pvrtest2.h"

#define Z3 0x3e99999au     /* 0.3f */
#define Z5 0x3f000000u     /* 0.5f */
#define Z7 0x3f333333u     /* 0.7f */
#define Z4 0x3ecccccdu     /* 0.4f */
static const f32b orders[6][3] = { { Z3, Z5, Z7 }, { Z3, Z7, Z5 }, { Z5, Z3, Z7 }, { Z5, Z7, Z3 }, { Z7, Z3, Z5 }, { Z7, Z5, Z3 } };
static const f32b ties[4][3] = { { Z5, Z5, Z5 }, { Z5, Z5, Z7 }, { Z7, Z5, Z5 }, { Z5, Z7, Z5 } };
/* the same, for the labels */
static const char *order_str[6] = { "0.3 G 0.5 B 0.7", "0.3 G 0.7 B 0.5", "0.5 G 0.3 B 0.7", "0.5 G 0.7 B 0.3", "0.7 G 0.3 B 0.5", "0.7 G 0.5 B 0.3" };
static const char *tie_str[4] = { "0.5 G 0.5 B 0.5", "0.5 G 0.5 B 0.7", "0.7 G 0.5 B 0.5", "0.5 G 0.7 B 0.5" };

static void test_setup(void) {
}

static void test_describe(void) {
    int p, b, c;

    for(p = 0; p < 2; p++)
        for(b = 0; b < 2; b++) {
            const char *bg = b ? ", over opaque 0.4" : "";
            const char *bl = p ? "add" : "alpha";
            for(c = 0; c < 6; c++)
                label(p, b * 20 + c, "%s: R %s%s", bl, order_str[c], bg);
            for(c = 0; c < 4; c++)
                label(p, b * 20 + 6 + c, "%s: tie R %s%s", bl, tie_str[c], bg);
            label(p, b * 20 + 10, "%s: R and G intersect%s", bl, bg);
            label(p, b * 20 + 11, "%s: R and G intersect, submitted G first%s", bl, bg);
            label(p, b * 20 + 12, "%s: R 0.7 G 0.5 B 0.3, depth write off%s", bl, bg);
            label(p, b * 20 + 13, "%s: R 0.7 G 0.5 B 0.3, compare always%s", bl, bg);
        }
}

/* A polygon without culling, alpha from the vertex colour; the blend is source, destination. */
static void polygon(int list, int cmp, int write, int src, int dst) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };

    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = list;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Gouraud = 1;
    isp.DepthMode = cmp;
    isp.ZWriteDis = !write;
    isp.CullMode = CullMode_None;
    tsp.SrcInstr = src;
    tsp.DstInstr = dst;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    ta_polygon(pcw, isp, tsp, tcw);
}

/* R, G, B at the depths z[0..2] */
static void three(int cmp, int write, int src, int dst, int cell, const f32b z[3]) {
    int x = cell_px(cell), y = cell_py(cell);
    polygon(ListType_Translucent, cmp, write, src, dst);
    quad0(x, y, 24, 32, z[0], 0x80ff0000);
    polygon(ListType_Translucent, cmp, write, src, dst);
    quad0(x + 8, y, 24, 32, z[1], 0x8000ff00);
    polygon(ListType_Translucent, cmp, write, src, dst);
    quad0(x, y + 8, 32, 16, z[2], 0x800000ff);
}

static void sloped(int cell, uint32_t argb, f32b zl, f32b zr, int src, int dst) {
    int k;

    polygon(ListType_Translucent, DepthMode_GreaterEqual, 1, src, dst);
    for(k = 0; k < 4; k++)
        ta_vertex0(f32b_int(cell_px(cell) + (k & 1) * 32), f32b_int(cell_py(cell) + (k >> 1) * 32), (k & 1) ? zr : zl, argb, k == 3);
}

static void test_page(int page) {
    int src = page ? BlendInstr_One : BlendInstr_SrcAlpha;
    int dst = page ? BlendInstr_One : BlendInstr_InvSrcAlpha;
    int b, c;

    ta_list_begin(ListType_Opaque);
    polygon(ListType_Opaque, DepthMode_Always, 1, BlendInstr_One, BlendInstr_Zero);
    quad0(0, 32, 14 * 32, 32, Z4, 0xff404040);
    ta_list_end();

    ta_list_begin(ListType_Translucent);
    for(b = 0; b < 2; b++) {
        for(c = 0; c < 6; c++)
            three(DepthMode_GreaterEqual, 1, src, dst, b * 20 + c, orders[c]);
        for(c = 0; c < 4; c++)
            three(DepthMode_GreaterEqual, 1, src, dst, b * 20 + 6 + c, ties[c]);
        sloped(b * 20 + 10, 0x80ff0000, Z3, Z7, src, dst);
        sloped(b * 20 + 10, 0x8000ff00, Z7, Z3, src, dst);
        sloped(b * 20 + 11, 0x8000ff00, Z7, Z3, src, dst);
        sloped(b * 20 + 11, 0x80ff0000, Z3, Z7, src, dst);
        three(DepthMode_GreaterEqual, 0, src, dst, b * 20 + 12, orders[5]);
        three(DepthMode_Always, 1, src, dst, b * 20 + 13, orders[5]);
    }
    ta_list_end();
}
