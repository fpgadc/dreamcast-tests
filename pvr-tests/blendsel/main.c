/* Source and destination both buffer 2 (SrcSel and DstSel set): what a console does with the alpha
   channels, and whether it follows the order the polygons are sent in or their place in the tile.

   As ../blend: a cell is a tile of four 16x16 squares; in each square a colour goes into the framebuffer
   (ONE, ZERO), another into buffer 2 (ONE, ZERO, DstSel), then the test quad with the cell's setting.
   cell c: bits 0-2 source factor, bits 3-5 destination factor (SrcSel and DstSel always set), bits 6-7 the
   way the 12 quads of the tile are sent:
       0  square by square, 0 1 2 3 (top left, top right, bottom left, bottom right)
       1  square by square, 3 2 1 0
       2  the four framebuffer quads, the four buffer 2 quads, the four test quads
       3  square by square, but one test quad over the whole tile at the end

   page = set * 6 + view. Views 0..3 as in ../blend (framebuffer colour, buffer 2's colour, the framebuffer's
   alpha, buffer 2's alpha); 4 and 5: 255 - alpha of the framebuffer and of buffer 2 (the INV factors). */
#define NUM_CELLS 256
#define NUM_SETS 2
#define NUM_VIEWS 6
#define NUM_PAGES 12
#include "pvrtest2.h"

/* set 0: set 0 of ../blend. set 1: its set 3 with alphas that include 0, 1, 128 and 255 */
static uint32_t color(int set, int q, int layer) {
    static const uint8_t edge[6] = { 0, 1, 127, 128, 254, 255 };
    static const uint8_t alpha[3][4] = { { 255, 128, 255, 1 }, { 64, 255, 200, 255 }, { 255, 0, 128, 77 } };
    uint32_t s = (uint32_t)((set ? 3 : 0) * 12 + q * 3 + layer) * 2654435761u + 12345u, out = 0;
    int k;

    for(k = 0; k < 4; k++) {
        uint32_t b;
        s = s * 1664525u + 1013904223u;
        b = (s >> 16) & 0xff;
        if(b < 96)
            b = edge[b % 6];
        out = (out << 8) | b;
    }
    if(set)
        out = (out & 0x00ffffff) | (uint32_t)alpha[layer][q] << 24;
    return out;        /* ARGB */
}

static void plain(int src, int dst, int src_sel, int dst_sel) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };

    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Translucent;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Gouraud = 0;
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
    static const char *views[NUM_VIEWS] = { "fb color", "buf2 color", "fb alpha", "buf2 alpha", "fb 255-alpha", "buf2 255-alpha" };
    static const char *factors[8] = { "ZERO", "ONE", "OTHER", "1-OTHER", "SRC_A", "1-SRC_A", "DST_A", "1-DST_A" };
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < NUM_CELLS; c++)
            label(p, c, "set %d view %s: order %d, buf2 -> buf2 %s , %s", p / NUM_VIEWS, views[p % NUM_VIEWS], c >> 6,
                  factors[c & 7], factors[(c >> 3) & 7]);
}

/* part 0: the framebuffer's colour; 1: buffer 2's; 2: the test quad */
static void part(int set, int c, int q, int part_) {
    int x = cell_px(c) + (q & 1) * 16, y = cell_py(c) + (q >> 1) * 16;
    if(part_ == 0)
        plain(BlendInstr_One, BlendInstr_Zero, 0, 0);
    else if(part_ == 1)
        plain(BlendInstr_One, BlendInstr_Zero, 0, 1);
    else
        plain(c & 7, (c >> 3) & 7, 1, 1);
    quad0(x, y, 16, 16, F32B_ONE, color(set, q, part_));
}

static void test_page(int page) {
    int set = page / NUM_VIEWS, view = page % NUM_VIEWS;
    int c, q, k;

    ta_list_begin(ListType_Translucent);
    /* the 44 cells after the last are filled as well (not compared): a tile with nothing in it but the
       view's pass damages the picture of the tile before it (see ../blendpos) */
    for(c = 0; c < 300; c++) {
        switch((c >> 6) & 3) {
        case 0: for(q = 0; q < 4; q++) for(k = 0; k < 3; k++) part(set, c, q, k); break;
        case 1: for(q = 3; q >= 0; q--) for(k = 0; k < 3; k++) part(set, c, q, k); break;
        case 2: for(k = 0; k < 3; k++) for(q = 0; q < 4; q++) part(set, c, q, k); break;
        default:
            for(q = 0; q < 4; q++) for(k = 0; k < 2; k++) part(set, c, q, k);
            plain(c & 7, (c >> 3) & 7, 1, 1);
            quad0(cell_px(c), cell_py(c), 32, 32, F32B_ONE, color(set, 0, 2));
        }
    }
    if(view == 1) {
        plain(BlendInstr_One, BlendInstr_Zero, 1, 0);
        quad0(0, 0, 640, 480, F32B_ONE, 0xffffffff);
    } else if(view == 2 || view == 4) {
        plain(view == 2 ? BlendInstr_DstAlpha : BlendInstr_InvDstAlpha, BlendInstr_Zero, 0, 0);
        quad0(0, 0, 640, 480, F32B_ONE, 0xffffffff);
    } else if(view == 3 || view == 5) {
        plain(BlendInstr_One, BlendInstr_Zero, 0, 0);
        quad0(0, 0, 640, 480, F32B_ONE, 0xffffffff);
        plain(BlendInstr_Zero, view == 3 ? BlendInstr_SrcAlpha : BlendInstr_InvSrcAlpha, 1, 0);
        quad0(0, 0, 640, 480, F32B_ONE, 0xffffffff);
    }
    ta_list_end();
}
