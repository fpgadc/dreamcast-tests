/* edge2_end: the end of an edge whose block exponent was cut, with a column step of exactly -2^127 (2026-11-20,
   after pvr-tests/edge2_walk): edge2_walk pages 0-2 (661 tiles) showed no end with that step, saturated or not;
   faredge_ecap pages 7 and 9 tiles (0,4) and (0,9) end where the row value reaches -2^131 exactly. FAR131: with a
   column step of -2^127 the far value of the end is R + 2^131 (it ends only when R falls to -2^131 + ZERO or below).

   One triangle per tile, each with the user tile clip of its tile (as faredge_cmax), white, depth always, against
   0x204060; 300 probes a page. The cases are written by gen.cpp (hyp.h: pvr_isp_edge.h with switches), which keeps,
   per tile, a triangle whose pixels in that tile differ between FAR131 and another hypothesis (the label names the
   pair and lists every variant's pixels against FAR131):
     page 0  a far corner at y = +-2^127 exactly, x 2^124 .. 2^128: END127 (the end with R alone), NOEND127 (none)
     page 1  the same with x = (16 m - s) / j 2^127: rows that reach +-2^131 exactly or nearly (FAR131 / NOEND127)
     page 2  y = +-2^126, 3 2^126, 2^125, 3, 5, 7 2^125: MULT126, R + 2^131 for every column step below zero that
             is a multiple of 2^126
     page 3  y = +-2^127 (1 +- 2^-19, 3 2^-19, 2^-13, 2^-8): NEAR127, R + 2^131 within 2^117 of -2^127
     page 4  y = +-2^127: POS127, R + 2^131 with +2^127 as well
   Result (2026-11-20, one console; 280 tiles found, the rest of each page empty): MULT126 every tile, refsw3 has it.
   FAR131 55 wrong (page 2: -2^126 and -1.5 2^127 have R + 2^131 as well), NOEND127 92 (page 1: rows reaching -2^131
   end), END127 123, NEAR127 121, POS127 173. Odd multiples of 2^125 and 2^124: R alone (pvr-tests/edge2_mult).
 */
#define NUM_PAGES 5
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
