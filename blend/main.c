/* PowerVR2 blend unit test: every blend setting, to be compared between real hardware and an emulator.

   The screen is a grid of 32x32 cells, one tile each. Cell number c tests one setting:
       bits 0-2  source blend factor       (PVR_BLEND_ZERO .. PVR_BLEND_INVDESTALPHA)
       bits 3-5  destination blend factor
       bit 6     SrcSel: source = secondary accumulation buffer ("buffer 2")
       bit 7     DstSel: destination = buffer 2
   Each cell is four 16x16 squares, and in each square three flat-colored, untextured quads are drawn, in
   the translucent list in submission order:
       1. a color into the framebuffer            (ONE, ZERO)
       2. another color into buffer 2             (ONE, ZERO, DstSel)
       3. the test quad with the cell's setting
   The colors (with alpha) depend on the square and on the color set (SET, 0..NUM_SETS-1), not on the cell.
   Nothing is textured or interpolated, so the only thing under test is the blend unit.

   A full-screen pass then brings what is to be inspected into the framebuffer's color (VIEW):
       0  nothing: the framebuffer's color
       1  buffer 2's color          (source = buffer 2, ONE, ZERO)
       2  the framebuffer's alpha   (white, DESTALPHA, ZERO)
       3  buffer 2's alpha          (white, then source = buffer 2 with ZERO, SRCALPHA)

   Pages (pvrtest2.h): page = set * 4 + view, 16 pages. */

#define NUM_CELLS 256
#define NUM_SETS 4
#define NUM_VIEWS 4
#define NUM_PAGES 16               /* NUM_SETS * NUM_VIEWS */
#include "pvrtest2.h"

/* The color of layer 0 (framebuffer), 1 (buffer 2) or 2 (test quad) in square q of a cell. A third of the
   bytes are edge values. The same function is in compare.py. */
static uint32_t color(int set, int q, int layer) {
    static const uint8_t edge[6] = { 0, 1, 127, 128, 254, 255 };
    uint32_t s = (uint32_t)(set * 12 + q * 3 + layer) * 2654435761u + 12345u, out = 0;
    int k;

    for(k = 0; k < 4; k++) {
        uint32_t b;
        s = s * 1664525u + 1013904223u;
        b = (s >> 16) & 0xff;
        if(b < 96)
            b = edge[b % 6];
        out = (out << 8) | b;
    }
    return out;        /* ARGB */
}

/* A translucent flat-shaded untextured polygon, alpha used, no depth test or write, no culling. */
static void plain(int src, int dst, int src_sel, int dst_sel) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };

    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Translucent;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Gouraud = 0;                    /* PVR_SHADE_FLAT */
    isp.DepthMode = DepthMode_Always;
    isp.ZWriteDis = 1;
    isp.CullMode = CullMode_None;
    tsp.SrcInstr = src;
    tsp.DstInstr = dst;
    tsp.SrcSelect = src_sel;
    tsp.DstSelect = dst_sel;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    ta_polygon(pcw, isp, tsp, tcw);
}

static void test_setup(void) {
    pvr_frame.bg_color = 0x00000000;
}

static void test_describe(void) {
    static const char *views[NUM_VIEWS] = { "fb color", "buf2 color", "fb alpha", "buf2 alpha" };
    static const char *factors[8] = { "ZERO", "ONE", "OTHER", "1-OTHER", "SRC_A", "1-SRC_A", "DST_A", "1-DST_A" };
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)          /* one region per blend setting (cell): set, view, setting */
        for(c = 0; c < NUM_CELLS; c++)
            label(p, c, "set %d view %s: setting %d src %s -> dst %s, %s , %s", p / NUM_VIEWS, views[p % NUM_VIEWS], c,
                  (c & 64) ? "buf2" : "poly", (c & 128) ? "buf2" : "fb", factors[c & 7], factors[(c >> 3) & 7]);
}

static void test_page(int page) {
    int set = page / NUM_VIEWS, view = page % NUM_VIEWS;
    int c, q;

    ta_list_begin(ListType_Translucent);
    /* The 44 cells after the last setting are filled too (settings 0..43 again, not compared). A tile that
       has nothing in it but the pass that reads buffer 2 reads it while the tile before it is still being
       written out, and that tile's picture comes out damaged: with empty cells there, the last 20 settings
       looked broken on a console (see ../blendpos). */
    for(c = 0; c < 300; c++) {
        int setting = c < NUM_CELLS ? c : c - NUM_CELLS;
        for(q = 0; q < 4; q++) {
            int x = cell_px(c) + (q & 1) * 16, y = cell_py(c) + (q >> 1) * 16;
            plain(BlendInstr_One, BlendInstr_Zero, 0, 0);
            quad0(x, y, 16, 16, F32B_ONE, color(set, q, 0));
            plain(BlendInstr_One, BlendInstr_Zero, 0, 1);
            quad0(x, y, 16, 16, F32B_ONE, color(set, q, 1));
            plain(setting & 7, (setting >> 3) & 7, (setting >> 6) & 1, (setting >> 7) & 1);
            quad0(x, y, 16, 16, F32B_ONE, color(set, q, 2));
        }
    }
    if(view == 1) {
        plain(BlendInstr_One, BlendInstr_Zero, 1, 0);
        quad0(0, 0, 640, 480, F32B_ONE, 0xffffffff);
    } else if(view == 2) {
        plain(BlendInstr_DstAlpha, BlendInstr_Zero, 0, 0);
        quad0(0, 0, 640, 480, F32B_ONE, 0xffffffff);
    } else if(view == 3) {
        plain(BlendInstr_One, BlendInstr_Zero, 0, 0);
        quad0(0, 0, 640, 480, F32B_ONE, 0xffffffff);
        plain(BlendInstr_Zero, BlendInstr_SrcAlpha, 1, 0);
        quad0(0, 0, 640, 480, F32B_ONE, 0xffffffff);
    }
    ta_list_end();
}
