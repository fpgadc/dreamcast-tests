/* hugepos_v3: the edge unit's rules for row values past 2^131, round 3 of TODO C4 (2026-11-20): the merged model of
   the faredge and hugepos hunts (pvr_isp_edge.h: the push of R by 2^132 when R + 16 sx leaves [-2^131, 2^131), the
   death of rows after the value at pixel 32 falls to 2^132, E cut to 129, rows past 2^131, swamped coordinate
   differences, the sliver cut) checked on cases made after those rules were fixed, cases where the alternatives the
   earlier tests leave open disagree, and two sweeps of what is not modelled yet.

   One triangle per tile, each with the user tile clip of its tile (as faredge_cmax), white, depth always, against
   0x204060; 300 probes a page. The cases are written by agents/nulldc3/hunt/hugepos/r3/gen3.cpp, which keeps, per
   tile, a triangle whose pixels in that tile differ between the model and some of its alternatives (the label lists
   which, and by how many pixels):
     V  pages 0-3: validation: random triangles with one or two corners at 2^122 .. 2^127.97 (0, 1), long
        near-horizontal or vertical edges (2), slivers towards a far corner (3)
     D  page 4: the wrap of a row below -2^131 (a start of 0 or below; only 0; every start)
        page 5: the death of rows after the value at pixel 32 falls to 2^132 (only with a column step of 2^127 or more)
        page 6: the sliver cut (the bound 239/16 2^127, only with a column step of 2^126 or more)
        page 7: swamped coordinate differences, the rows below an edge whose dx swamps, the cut of E to 129
     X  pages 8-9: two long edges through one far corner, n (cos t, sin t) 2^127, n 0.9 .. 1.95: rows past +2^131
        that the console draws as if wrapped by 2^132 (hugepos_rules pages 9-10, hugepos_sw3 pages 36-37); not modelled
        pages 10-11: b-shaped triangles (-64, -64) (704, 0) (fx, fy) 2^127, varied, turned and mirrored: slivers between
        two edges whose rows were pushed (hugepos_rules pages 4-7); not modelled
   Result (2026-11-20, one console, refsw3 pvr_isp_edge.h of round 3; 2,169 of 3,686,400 pixels left, the base
   a16bcc78 277,588): page 7 showed that differences past 2^128 are 2^127 as steps (swamped only in the products of
   the start), pages 0-4 that rows below -2^131 with a column step above zero are outside (mostly; the GAP in
   pvr_isp_edge.h), pages 8-9 the rule for rows past +2^131 (the first row of the tile wraps, later ones by the carry
   of its value at pixel 32), page 5 that the death of rows needs a column step of 2^127 or more; and that the zero
   band of E cut to 129 must not take a negative shift. Left: pages 0, 1, 3, 4, 6 (733, 589, 146, 495, 206 pixels):
   rows below -2^131 the console wraps with a column step above zero, sliver rows it cuts with D up to 15.9 2^127.
 */
#define NUM_PAGES 12
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
