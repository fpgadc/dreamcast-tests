/* faredge_cull: which determinant the culling by facing uses for triangles whose overflowing products swamp the rest
   (2026-11-20, round 3 of the faredge item; the verifier's open point: refsw3 turns the edges by the determinant with
   the swamp, pvrisp::EdgeWinding, but culls by the plain one, pvrisp::Winding).

   300 triangles, one per tile with the user tile clip of its tile (as faredge_walk), white, untextured, depth always,
   against 0x204060: in 4 tiles of 5 the two determinants have opposite signs (far corners at +-2^127 .. 2^128.95, or
   the H family (-X, ya) (X, yb) T), in the fifth they agree (controls); each has 8 pixels or more in its tile. The
   same triangles on every page, the page being the CullMode: 0 none, 1 small, 2 negative, 3 positive. Written by
   agents/nulldc3/hunt/faredge/r3/m/gen_cull.cpp.
   Result (2026-11-20, one console): the culling uses the swamped determinant, its sign for the facing (12 + 14 of the
   240) and its size: the 214 whose swamped terms cancel to 0 are culled by size in modes 1-3. refsw3 culls by it now
   (pvrisp::EdgeWinding's det): 1,200 of 1,200 regions; with the plain determinant 716. */
#define NUM_PAGES 4
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
        isp.CullMode = page;                /* page 0 none, 1 small, 2 negative, 3 positive */
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
