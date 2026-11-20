/* faredge_m131: the edge unit's row value at pixel 16 against 2^131, and the rows a far edge drops after it
   (2026-11-19, after pvr-tests/faredge_k132 and the hugepos_sw* tests).

   faredge_k132 found that a row whose value at pixel 16, M = R + 16 sx (R the row's value at the tile's first column,
   sx the column step), is 2^131 or more in size is evaluated as if R had 2^132 more in size. Left open: M exactly at
   -2^131 or +2^131 (hugepos_sw4 page 1 row 14 says -2^131 is not pushed, page 13 row 18 that +2^131 is: a register
   of [-2^131, 2^131)), the precision of M, and rows the hugepos_sw3 / sw4 slivers drop although M is in range.

   One triangle per tile, each with the user tile clip of its tile, white, untextured, depth always, against a
   background of 0x204060 (as faredge_cmax). The cases (agents/nulldc3/hunt/faredge/r2/gen_m131.cpp wrote cases.h;
   evh.cpp scores the pages against the hypotheses of r2/hyps.h):
     RV  pages 0-5, columns 1-19: a near-vertical edge whose start is 0 (c's product and dy x0 overflow with opposite
         signs), the column step -(2^127 + d 2^108), the row step chosen so that M = -2^131 + delta exactly in row 8 or
         16 of the tile, delta 0, +-2^96, +-2^100, +-2^104, +-2^108; the rows before are pushed (whole rows), after not
         (pixel 0 or nothing)
     S0  pages 0-5, column 0: the same with the start saturated to 2^127 by c's product and the column step
         -(2^127 + 2^123 + k 2^108)
     SL  pages 6-15: slivers between two corners near the tile and one at y +-2^127 (1 + k / 256), x +-r 2^e, chosen
         where the hypotheses disagree: the M rule alone, with the rows after the value at pixel 32 of the previous row
         falls to 2^132 (from past it in row 0) pushed as well (XOR / OR), with the row value past 2^131 wrapped, ...
   Result (2026-11-20, one console; refsw3 pvr_isp_edge.h has it, 3,673 of 3,678 tiles): M's register is [-2^131, 2^131)
   (-2^131 not pushed, +2^131 pushed); rows of an edge with a column step past 2^127 are not ZERO at 2^112 (E is cut to
   129, ZERO 2^108: pvr-tests/faredge_ecap pins the cut); with a column step above zero, the rows after the one whose
   value at pixel 32 fell to 2^132 or below (from above it in the tile's first row) are pushed too (9,694 of 9,708 rows);
   the winding is the determinant's sign with its overflowing products swamping the rest (294 tiles turned); a row whose
   value is past 2^131 with a column step of the other sign is outside. Not modelled: 5 tiles (README / the GAP in
   pvr_isp_edge.h). */
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
