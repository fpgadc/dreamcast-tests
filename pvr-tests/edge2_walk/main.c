/* edge2_walk: the edge unit's walk of an edge whose block exponent was cut (a step of 2^124 or more), three open
   questions of refsw3's pvr_isp_edge.h (README "refsw3 without float", faredge / hugepos GAPs), 2026-11-20.

   One triangle per tile, each with the user tile clip of its tile (as faredge_cmax), white, depth always, against
   0x204060; 300 probes a page. The cases are written by gen.cpp (hyp.h: pvr_isp_edge.h with switches), which keeps,
   per tile, a triangle whose pixels in that tile differ between two hypotheses (the label names the pair and lists
   every variant's pixels against U27):
     S  pages 0-1: two far corners on opposite sides in y (|dy| from 2^128: a saturated column step of 2^127) and a
        near one, where the end of the edge with that step below zero matters (hugepos_v5 page 0 tile (0,7): its rows
        drawn after the row value fell from +14.6 2^127 to -15.75 2^127): SATNOEND no end with a saturated column
        step below zero, SATNOEND0 only with a start of 0, SATNOENDW only with a row step above zero
     E  page 2: a far corner at y = +-2^127 exactly (a column step of exactly 2^127, not saturated: faredge_ecap
        pages 7 and 9 end there), EXNOEND no end with a column step of -2^127
     H  pages 3-5: a near-horizontal edge to a far corner at x 2^124 .. 2^128 whose column step has bits below the
        walk's units, near zero in a row of the tile: the units of such an edge (hugepos_rules page 11's ties):
        U27 2^(E - 27) for the ordinary rows and the walk alike (E cut to 129: 2^102), U26, U28, EP25-27 2^(ep - k),
        OLD the ordinary rows in 2^(ep - 21) and the walk in 2^(ep - 27), FLOOR the steps floored
     P  page 6: points (three equal corners: every edge inside, the row range filled), corners 1/1024 apart and small
        ordinary triangles, the top corner in rows 28 .. 31 of the tile: whether an ordinary polygon whose first row
        is the tile's last starts a row earlier (ROW31ALL), as one with a cut edge does (faredge_walk)
   Result (2026-11-20, one console; refsw3 pvr_isp_edge.h / refsw_tile.cpp have it, 2,100 of 2,100 tiles):
     S, E: no end with a column step of exactly -2^127, saturated or not (661 tiles; the end with R alone has them all
        wrong, SATNOEND0 252 + 206, SATNOENDW all): see edge2_end, which found R + 2^131 as the far value
     H: U27, the start cut toward zero (5 tiles in row 0 wrong floored), and the end's far value against ZERO, not 0
        (12 tiles: a start of 0 and a column step of one unit); U26 240 tiles wrong, U28 428, EP25 435, EP26 478, EP27
        557, FLOOR 135, OLD 339
     P: ROW31ALL: every polygon whose first row is the tile's last starts a row earlier (25 points, 18 tiny triangles;
        43 tiles wrong without), none in rows 28 .. 30
 */
#define NUM_PAGES 7
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
