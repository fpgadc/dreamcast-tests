/* edge2_mult: which column steps below zero give the end of a cut edge the far value R + 2^131 (2026-11-20, after
   pvr-tests/edge2_end: -2^126, -2^127 and -1.5 2^127 do, every multiple of 2^126 so far; the end with R alone
   everywhere else measured). MULT126 (the model): multiples of 2^126; MULT125: of 2^125; MULT124: of 2^124.

   One triangle per tile, each with the user tile clip of its tile (as faredge_cmax), white, depth always, against
   0x204060. The cases are written by gen.cpp (hyp.h: pvr_isp_edge.h with switches): a far corner at y = +-k 2^125
   (page 0, k 1, 3, 5, 7) or +-k 2^124 (page 1, k odd, 1 .. 15) exactly, its x at +-2^124 .. 2^128, two near corners,
   kept per tile where MULT126 and the alternative differ (the tiles where none was found are empty).
   Result (2026-11-20, one console; 58 tiles found): MULT126 every tile, refsw3 has it; MULT125 29 wrong (page 0),
   MULT124 58 (both pages): only multiples of 2^126 (32 sx = 0 mod 2^131) have the far value R + 2^131.
 */
#define NUM_PAGES 2
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
