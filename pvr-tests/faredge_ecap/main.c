/* faredge_ecap: the edge unit's block exponent E at most 129, and held-out slivers for faredge_m131's rules
   (2026-11-20, after pvr-tests/faredge_m131).

   faredge_m131 showed rows of an edge with a column step past 2^127 that are not ZERO at a value of 2^112, where
   E = top(step) + 6 = 134 would make them so: E is cut, the ZERO band 2^(E - 21) is 2^108 for such edges as it is in
   their pushed rows (faredge_k132). Which cut: 129, 130, 131 or 132 gave the same pictures there.

   One triangle per tile, each with the user tile clip of its tile, white, untextured, depth always, against a
   background of 0x204060 (as faredge_cmax). The cases (agents/nulldc3/hunt/faredge/r2/gen_val.cpp wrote cases.h; evh.cpp
   scores the pages against the models of r2/hyps.h case 20):
     EC  pages 0-5, columns 4-19: near-vertical edges whose start is 0 (c's product and dy x0 overflow with opposite
         signs), |far y| in [2^123, 2^127) (E 130 .. 133 before a cut), rows R = j dx with dx = 2^105 .. 2^108 so that
         R crosses 2^107 .. 2^112 in row 4, 8 or 16: rows inside the ZERO band are decided by the column step (empty,
         or full), the others by R's sign (pixel 0 alone, or all but pixel 0)
     SV  pages 6-15: slivers between two corners near the tile and one at y +-2^127 (1 + k/256), x +-r 2^e, another
         seed than faredge_m131's, chosen where faredge_m131's model and its alternatives disagree (no rule at pixel 32,
         that rule on the row itself or strict, the plain determinant's winding, M's register [-2^131, 2^131], ...)
   Result (2026-11-20, one console; refsw3 3,393 of 3,397 tiles): the cut is at 129 (EC: 1,440 of 1,440; a cut at 130 has
   1,424 tiles wrong, 131, 132 or none 1,440); on the slivers faredge_m131's model holds (4 tiles wrong, of the kinds it
   left open), its alternatives do not (no rule at pixel 32: 515 wrong, the plain winding 1,009, M's register
   [-2^131, 2^131] 240). */
#define NUM_PAGES 16
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
