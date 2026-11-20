/* faredge_far32: held-out check of the end of an edge's walk, round 3 of the faredge item (2026-11-20, after
   pvr-tests/faredge_walk). The model (refsw3 pvr_isp_edge.h EdgeTile::Walk; agents/nulldc3/hunt/faredge/r3/m/mechimpl.h
   mechanism 10), found on faredge_walk, faredge_m131, faredge_ecap and the hugepos_* sets: an edge whose block exponent
   was cut ends (the rows after are outside) when the row's largest value, R + 32 sx with a column step above zero, else
   R, with 32 sx kept below 2^131 (32 sx mod 2^131), falls from above 0 to 0 or below; counted from the polygon's first
   row in the tile; a polygon whose first row is the tile's last starts a row earlier.

   One triangle per tile, each with the user tile clip of its tile, white, untextured, depth always, against 0x204060
   (as faredge_walk), 16 pages of 300: slivers to a far corner, the H (-X, ya) (X, yb) T and B shaped families, a third
   of them starting inside the tile, from a new seed. Written by agents/nulldc3/hunt/faredge/r3/m/gen4.cpp (SEED=31337),
   which keeps per tile the candidate whose pixels differ most between the model and its alternatives: 32 sx kept whole;
   0 counting as above 0; the end test counted from row 0; the plain row range; the levels of round 3's first model. */
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
