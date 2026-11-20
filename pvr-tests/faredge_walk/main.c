/* faredge_walk: the walk of an edge with a cut block exponent down its tile, round 3 of the faredge item (TODO C4)
   (2026-11-20, after pvr-tests/faredge_m131, faredge_ecap and hugepos_rules / hugepos_v3).

   The model under test (agents/nulldc3/hunt/faredge/r3/m/mechimpl.h, mechanism 4): an edge whose block exponent was
   cut to 129 is walked down the tile from the polygon's first row in it; its row value R wraps into [-2^131, 2^131)
   (but a row whose R rose to 2^131 or past after the first row is outside); R and the column step of one sign decide
   the row, else M = R + 16 sx wraps (the push of 2^132); and the row's far value (pixel 32 for a column step above
   zero, else pixel 0) falling through 0, 2^131 or 2^132 below where it was ends the edge for the rest of the tile.
   A polygon whose first row is the tile's last (31) starts a row earlier.

   One triangle per tile, each with the user tile clip of its tile, white, untextured, depth always, against a
   background of 0x204060 (as faredge_cmax). The cases are written by agents/nulldc3/hunt/faredge/r3/m/gen3.cpp, which
   keeps per tile the candidate whose pixels differ most between the model and its alternatives:
     pages 0-3: slivers (two corners near the tile, one at y (1 + k/256) 2^127, x +-r 2^e) whose top corner is in row
       28 .. 31 of the tile: the row range at the tile's last rows (plain; rows 30-31 a row earlier; a top corner in the
       last quarter of its row a row earlier)
     pages 4-7: slivers, some starting inside the tile: the levels (2^131 strictly; tracked from row 0; no level kill
       for a column step of 0 or below)
     pages 8-11: H (-X, ya) (X, yb) T and B shaped triangles, some starting inside the tile: rows past 2^131 (dead
       when R starts there; every such row wrapped; wrapped with a column step below zero)
     pages 12-15: all three families against every alternative (validation)
   Result (2026-11-20, one console): the first model had 109 tiles wrong (the alternatives 162 .. 1,425: the row range
   at row 31, the first row as reference and R wrapped at the first row confirmed). What explains every tile (3,537 of
   3,537, refsw3 pvr_isp_edge.h EdgeTile::Walk): the edge ends when R + 32 sx (column step above zero, else R) falls to
   0 or below from above, with 32 sx kept below 2^131 (32 sx mod 2^131); the levels and the separate end after R rose
   past 2^131 of the first model are that rule. Held out: pvr-tests/faredge_far32 (4,800 of 4,800). */
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
