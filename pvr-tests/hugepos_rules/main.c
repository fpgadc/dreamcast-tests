/* hugepos_rules: the edge unit's rules for row values past 2^131, found from hugepos_sw1..4, hugepos_val1 and
   hugepos_cov (2026-11-19, round 2 of TODO C4), against their alternatives, and two sweeps of what was not modelled.

   One triangle per tile, each with the user tile clip of its tile (as faredge_cmax), white, depth always, against
   0x204060; 300 probes a page. The cases are written by agents/nulldc3/hunt/hugepos/r2/gen_rules.cpp, which keeps,
   per tile, a triangle whose pixels in that tile differ between the model of the time and one of its alternatives:
     H  pages 0-3: (-X, ya) (X, yb) T, |X| 0.25 .. 0.99 2^127, T 500 .. 3000 above or below, any corner order: the
        long edge's row value R = start + j dx leaves [-2^131, 2^131).
     B  pages 4-7: (-64, -64) (704, 0) (fx, fy) 2^127 and the same turned (x <-> y), fx -1.5 .. 1.5, fy 0.9 .. 1.95.
     S  page 8: coordinate differences at 2^128 (X = 2^127 (1 + k 2^-23), k -2 .. 2, in H, turned and the hugepos
        triangle a at M = 2^125 (1 + k 2^-23)).
     L  pages 9-10: (-64, -64) (0, 704) (4n, -3n) 2^125, n 1.2 .. 1.45 and 1.25 .. 1.375 finer, tiles in order: the
        one-pixel lines hugepos_sw3 pages 36, 37 show (n 1.3125, 1.34375).
     C  page 11: hugepos_val1 page 25's sliver (4, -37.25) (33.75, 10.75) (a, b) 2^127, a 1 .. 1.19, b 0.3 .. 0.95.
   The gen_rules.cpp labels name the model of round 2; what the console showed, as refsw3's pvr_isp_edge.h has it now
   (round 3, 2026-11-20, with hugepos_v3 and hugepos_v4; 199 of 3,686,400 pixels left):
     H: a row below -2^131 is R + 2^132 when the start is 0 or below (and the column step 0 or below); other rows past
        2^131 are outside, except the first row past +2^131 (R - 2^132). 0 pixels wrong. The winding is the sign of
        the determinant with its overflowing products swamping the rest (with the exact sign 29,045 pixels wrong).
     S: a difference past 2^128 swamps in the products of the start and is 2^127 as a step. 0 pixels wrong.
     L: rows past +2^131: the tile's first such row has R - 2^132 (again past 2^132), later ones too when the first
        one's value at pixel 32 lies in a later 2^131 block than 2^131 + 32 sx, else they are outside. 0 pixels wrong.
     B: the push of C past 2^131, the death of rows after D falls to 2^132 (only with a column step of 2^127 or
        more) and the rows past +2^131 as in L: 154 pixels in b-shaped slivers left, not modelled.
     C: the cut of rows whose D is below 239/16 2^127 (R below zero, a column step of 2^126 or more): 45 pixels of
        tie pixels left (they follow the zero band of E cut to 129, which faredge_m131 / faredge_ecap pin). */
#define NUM_PAGES 12
#include "pvrtest2.h"
#include "cases.h"

static void test_setup(void) {
}

static void test_describe(void) {
#ifndef _arch_dreamcast
    int p, i;
    for(p = 0; p < NUM_PAGES; p++)
        for(i = fc_first[p]; i < fc_first[p + 1]; i++)
            label(p, fc_cases[i].ty * 20 + fc_cases[i].tx, "%s", fc_label[i]);
#endif
}

static void test_page(int page) {
    int i, k;
    pvr_frame.bg_color = 0x204060;
    ta_list_begin(ListType_Opaque);
    for(i = fc_first[page]; i < fc_first[page + 1]; i++) {
        const fc_case_t *c = &fc_cases[i];
        PCW pcw = { .full = 0 };
        ISP_TSP isp = { .full = 0 };
        TSP tsp = { .full = 0 };
        TCW tcw = { .full = 0 };
        TA_UserTileClip clip;
        memset(&clip, 0, sizeof(clip));
        clip.pcw.ParaType = ParamType_User_Tile_Clip;
        clip.xmin = clip.xmax = c->tx;
        clip.ymin = clip.ymax = c->ty;
        ta_send(&clip, sizeof(clip));
        pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
        pcw.ListType = ListType_Opaque;
        pcw.Group_En = 1;
        pcw.Strip_Len = 1;
        pcw.User_Clip = User_Clip_Inside;
        isp.DepthMode = DepthMode_Always;
        isp.ZWriteDis = 1;
        isp.CullMode = CullMode_None;
        tsp.SrcInstr = BlendInstr_One;
        tsp.DstInstr = BlendInstr_Zero;
        tsp.FogCtrl = FogCtrl_None;
        tsp.UseAlpha = 1;
        ta_polygon(pcw, isp, tsp, tcw);
        for(k = 0; k < 3; k++)
            ta_vertex0(c->x[k], c->y[k], F32B_ONE, 0xffffffff, k == 2);
    }
    ta_list_end();
}
