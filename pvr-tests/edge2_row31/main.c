/* edge2_row31: a polygon whose first row is its tile's last (row 31) starts a row earlier there, with and without a
   user tile clip (2026-11-20). faredge_walk and edge2_walk page 6 measured that rule (refsw3 refsw_tile.cpp) with
   every probe drawn inside the user tile clip of its own tile; refsw3 applies it to every polygon. This test draws
   the same kind of probes with no user tile clip, with the clip of the probe's own tile and with a clip of the 3x3
   tiles around it, to see whether the clip has a part in it.

   One polygon per tile (gen.cpp writes cases.h), white, depth always, against 0x204060: a point (three equal
   corners: its edges are inside everywhere, so it fills its tile's width on the rows of its row range) or a tiny
   triangle (corners 1/1024 pixel apart), its top corner at y = 32 ty + r + f, r = 28 .. 31, f = 0, 1/2, 3/4.
   Result (2026-11-20, one console; refsw3 with the rule for every polygon: 300 of 300 tiles): the clip has no part in
   it. In each of the three clip modes the 13 points whose top corner is in row 31 fill rows 30 and 31, and 9, 9, 10
   of the 12 tiny triangles there draw in row 30 (67 tiles that the plain row range never draws there); none of the 75
   probes per mode in rows 28 .. 30 starts a row earlier.
 */
#define NUM_PAGES 1
#include "pvrtest2.h"
#include "cases.h"

static void test_setup(void) {
}

static void test_describe(void) {
#ifndef _arch_dreamcast
    int i;
    for(i = 0; i < 300; i++)
        label(0, fc_cases[i].ty * 20 + fc_cases[i].tx, "%s", fc_label[i]);
#endif
}

static void test_page(int page) {
    int i, k;
    (void)page;
    pvr_frame.bg_color = 0x204060;
    ta_list_begin(ListType_Opaque);
    for(i = 0; i < 300; i++) {
        const fc_case_t *c = &fc_cases[i];
        PCW pcw = { .full = 0 };
        ISP_TSP isp = { .full = 0 };
        TSP tsp = { .full = 0 };
        TCW tcw = { .full = 0 };
        if(c->mode != 0) {
            TA_UserTileClip clip;
            memset(&clip, 0, sizeof(clip));
            clip.pcw.ParaType = ParamType_User_Tile_Clip;
            if(c->mode == 1) {
                clip.xmin = clip.xmax = c->tx;
                clip.ymin = clip.ymax = c->ty;
            } else {
                clip.xmin = c->tx > 0 ? c->tx - 1 : 0;
                clip.xmax = c->tx < 19 ? c->tx + 1 : 19;
                clip.ymin = c->ty > 0 ? c->ty - 1 : 0;
                clip.ymax = c->ty < 14 ? c->ty + 1 : 14;
            }
            ta_send(&clip, sizeof(clip));
        }
        pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
        pcw.ListType = ListType_Opaque;
        pcw.Group_En = 1;
        pcw.Strip_Len = 1;
        pcw.User_Clip = c->mode != 0 ? User_Clip_Inside : User_Clip_Disable;
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
