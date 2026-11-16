/* What a polygon with source and destination both buffer 2 and (ONE or SRC_A, ZERO) does to the polygons
   that follow it in the tile (found with ../blendsel: they store alpha as 0 unless it is 255).

   A cell is a tile: a colour into the framebuffer and one into buffer 2 over the whole tile, then a
   "trigger" (the row), then a "probe" (the column) as four 16x16 squares with alphas 255, 254, 128 and 1,
   each its own polygon. Views as in ../blendsel (page = view).

   trigger (row):  0 none                          1 buf2 -> buf2 ONE, ZERO, whole tile
                   2 buf2 -> buf2 SRC_A, ZERO      3 buf2 -> fb ONE, ZERO          4 polygon -> buf2 ONE, ZERO
                   5 buf2 -> buf2 ZERO, ZERO       6 as 1, but one pixel           7 as 1, then three one pixel
                                                                                     polygons that change nothing
                   9 as 1, and nothing after it in this tile; row 10 below it has no trigger (tiles are
                     rendered column by column, so its probe follows that trigger in the next tile)
   probe (column): 0 polygon -> fb ONE, ZERO       1 polygon -> buf2 ONE, ZERO     2 polygon -> fb SRC_A, 1-SRC_A
                   3 polygon -> fb ONE, ONE        4 buf2 -> fb ONE, ZERO          5 polygon -> fb ZERO, ONE
                   6 polygon -> fb ONE, ZERO without UseAlpha                      7 polygon -> buf2 SRC_A, 1-SRC_A
                   8 polygon -> fb ONE, ZERO, opaque list in rows 0..7 does not exist: unused */
#define NUM_PAGES 6
#include "pvrtest2.h"

#define FB_COL   0x904080c0u
#define B2_COL   0x70c03050u
#define TRIG_COL 0x55112233u
static const uint32_t probe_col[4] = { 0xfff0e0d0u, 0xfe30a070u, 0x808090ffu, 0x0166ee22u };

static void plain(int src, int dst, int src_sel, int dst_sel, int use_alpha) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };

    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Translucent;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    isp.DepthMode = DepthMode_Always;
    isp.ZWriteDis = 1;
    isp.CullMode = CullMode_None;
    tsp.SrcInstr = src;
    tsp.DstInstr = dst;
    tsp.SrcSelect = src_sel;
    tsp.DstSelect = dst_sel;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = use_alpha;
    ta_polygon(pcw, isp, tsp, tcw);
}

static void test_setup(void) {
    pvr_frame.bg_color = 0x00000000;
}

static void test_describe(void) {
    static const char *views[NUM_PAGES] = { "fb color", "buf2 color", "fb alpha", "buf2 alpha", "fb 255-alpha", "buf2 255-alpha" };
    int p, r, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(r = 0; r < 11; r++)
            for(c = 0; c < 8; c++)
                if(r != 8)
                    label(p, r * 20 + c, "view %s: trigger %d probe %d", views[p], r, c);
}

static void trigger(int kind, int x, int y) {
    int k;
    switch(kind) {
    case 1: case 9: plain(BlendInstr_One, BlendInstr_Zero, 1, 1, 1);      quad0(x, y, 32, 32, F32B_ONE, TRIG_COL); break;
    case 2: plain(BlendInstr_SrcAlpha, BlendInstr_Zero, 1, 1, 1);         quad0(x, y, 32, 32, F32B_ONE, TRIG_COL); break;
    case 3: plain(BlendInstr_One, BlendInstr_Zero, 1, 0, 1);              quad0(x, y, 32, 32, F32B_ONE, TRIG_COL); break;
    case 4: plain(BlendInstr_One, BlendInstr_Zero, 0, 1, 1);              quad0(x, y, 32, 32, F32B_ONE, B2_COL); break;
    case 5: plain(BlendInstr_Zero, BlendInstr_Zero, 1, 1, 1);             quad0(x, y, 32, 32, F32B_ONE, TRIG_COL); break;
    case 6: plain(BlendInstr_One, BlendInstr_Zero, 1, 1, 1);              quad0(x, y, 1, 1, F32B_ONE, TRIG_COL); break;
    case 7:
        plain(BlendInstr_One, BlendInstr_Zero, 1, 1, 1);                  quad0(x, y, 32, 32, F32B_ONE, TRIG_COL);
        for(k = 0; k < 3; k++) {
            plain(BlendInstr_Zero, BlendInstr_One, 0, 0, 1);              quad0(x + 31, y + 31, 1, 1, F32B_ONE, TRIG_COL);
        }
        break;
    default: break;
    }
}

static void probe(int kind, int x, int y) {
    int q;
    for(q = 0; q < 4; q++) {
        switch(kind) {
        case 0: plain(BlendInstr_One, BlendInstr_Zero, 0, 0, 1); break;
        case 1: plain(BlendInstr_One, BlendInstr_Zero, 0, 1, 1); break;
        case 2: plain(BlendInstr_SrcAlpha, BlendInstr_InvSrcAlpha, 0, 0, 1); break;
        case 3: plain(BlendInstr_One, BlendInstr_One, 0, 0, 1); break;
        case 4: plain(BlendInstr_One, BlendInstr_Zero, 1, 0, 1); break;
        case 5: plain(BlendInstr_Zero, BlendInstr_One, 0, 0, 1); break;
        case 6: plain(BlendInstr_One, BlendInstr_Zero, 0, 0, 0); break;
        default: plain(BlendInstr_SrcAlpha, BlendInstr_InvSrcAlpha, 0, 1, 1); break;
        }
        quad0(x + (q & 1) * 16, y + (q >> 1) * 16, 16, 16, F32B_ONE, probe_col[q]);
    }
}

static void test_page(int page) {
    int r, c;

    ta_list_begin(ListType_Translucent);
    /* rows 8 and 11 are filled with probe 0 and no trigger (not compared): a tile with nothing in it but the
       view's pass damages the picture of the tile before it (see ../blendpos) */
    for(r = 0; r < 12; r++)
        for(c = 0; c < 8; c++) {
            int x = c * 32, y = r * 32;
            plain(BlendInstr_One, BlendInstr_Zero, 0, 0, 1);
            quad0(x, y, 32, 32, F32B_ONE, FB_COL);
            plain(BlendInstr_One, BlendInstr_Zero, 0, 1, 1);
            quad0(x, y, 32, 32, F32B_ONE, B2_COL);
            if(r <= 7 || r == 9)
                trigger(r, x, y);
            if(r != 9)
                probe(r == 8 || r == 11 ? 0 : c, x, y);
        }
    if(page == 1) {
        plain(BlendInstr_One, BlendInstr_Zero, 1, 0, 1);
        quad0(0, 0, 640, 480, F32B_ONE, 0xffffffff);
    } else if(page == 2 || page == 4) {
        plain(page == 2 ? BlendInstr_DstAlpha : BlendInstr_InvDstAlpha, BlendInstr_Zero, 0, 0, 1);
        quad0(0, 0, 640, 480, F32B_ONE, 0xffffffff);
    } else if(page == 3 || page == 5) {
        plain(BlendInstr_One, BlendInstr_Zero, 0, 0, 1);
        quad0(0, 0, 640, 480, F32B_ONE, 0xffffffff);
        plain(BlendInstr_Zero, page == 3 ? BlendInstr_SrcAlpha : BlendInstr_InvSrcAlpha, 1, 0, 1);
        quad0(0, 0, 640, 480, F32B_ONE, 0xffffffff);
    }
    ta_list_end();
}
