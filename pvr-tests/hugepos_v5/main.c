/* hugepos_v5: the edge unit's rules for row values past 2^131, round 3 of TODO C4, third console run (2026-11-20):
   every rule of refsw3's pvr_isp_edge.h of round 3 held fixed, chiefly the coordinate differences past 2^128 that
   hugepos_v4 showed (a difference that reaches 2^128 swamps in the products of the start, dx y0 and dy x0, and is
   2^127 with its sign as a step), on new cases.

   One triangle per tile, each with the user tile clip of its tile (as faredge_cmax), white, depth always, against
   0x204060; 300 probes a page. The cases are written by agents/nulldc3/hunt/hugepos/r3/gen5.cpp, which keeps, per
   tile, a triangle whose pixels in that tile differ between the model and some of its alternatives (the label lists
   which, and by how many pixels):
     X  pages 0-1: two far corners (2^126.6 .. 2^127.97) on opposite sides and a near one; page 2: H, V and a-type
        triangles with differences at 2^128 (1 + k 2^-23) .. 1.3 2^128; page 3: random triangles; all kept where the
        differences swamped as steps too (round 2), saturated in the products too, or not at all disagree
     V  pages 4-5: random triangles with one or two corners at 2^122 .. 2^127.97
     C  page 6: rows past +2^131 (the first one, later ones by the carry), column steps near k / 2 2^127
     N  page 7: rows below -2^131 with a column step above zero
   Result (2026-11-20, one console; 1,590 of 2,457,600 pixels left with the final model, the base a16bcc78 130,192):
   the differences past 2^128 hold (page 2 exact; swamped as steps too 68,397 pixels of the test wrong, saturated in
   the products too 3,365, not at all 83,864), page 6 exact; pages 0-1 showed that a column step of exactly 2^127
   (a saturated dy) lets rows below -2^131 wrap as a step of 0 does (checked with hugepos_v6). Left: pages 0, 1, 3, 4,
   5, 7 (397, 181, 202, 168, 177, 465): rows below -2^131 the console wraps with other column steps above zero.
 */
#define NUM_PAGES 8
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
