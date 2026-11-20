/* hugepos_v4: the edge unit's rules for row values past 2^131, round 3 of TODO C4, second console run (2026-11-20):
   the rules found from hugepos_v3 (rows past +2^131: the tile's first such row wraps by 2^132, later ones only when
   the value at pixel 32 of that first row, R + 32 sx, lies in a later 2^131 block than 2^131 + 32 sx; rows past
   +2^132 wrap as often as it takes; rows below -2^131 wrap only for a start of 0 or below and a column step of 0 or
   below; death only with a column step of 2^127 or more; the zero band of E cut to 129 without negative shifts) held
   fixed, on new cases.

   One triangle per tile, each with the user tile clip of its tile (as faredge_cmax), white, depth always, against
   0x204060; 300 probes a page. The cases are written by agents/nulldc3/hunt/hugepos/r3/gen4.cpp, which keeps, per
   tile, a triangle whose pixels in that tile differ between the model and some of its alternatives (the label lists
   which, and by how many pixels):
     V  pages 0, 1, 10: random triangles with one or two corners at 2^122 .. 2^127.97; 2: slivers towards a far corner;
        3: b-shaped triangles; 9: long near-horizontal or vertical edges; 11: two long edges through one far corner
     C  pages 4-5: rows past +2^131 (the first one, later ones, the carry; the column step near 2^126, 2^127, 1.5 2^127
        on page 4), rows past +2^132
     N  pages 6-7: rows below -2^131 with a column step above zero, starts of 0, below 0, saturated
     D  page 8: death with column steps either side of 2^127; the sliver cut
   Result (2026-11-20, one console, the rules fixed before it ran; 2,084 of 3,686,400 pixels left with the final
   model, the base a16bcc78 219,476): pages 4-5 (rows past +2^131, the carry) and 9 exact, page 3 3 pixels, page 8 4;
   each rule's alternative in agents/nulldc3/hunt/hugepos/r3/ablation_final.txt is worse. The saturated differences
   (pages 0, 1, 10) were found from this run and checked with hugepos_v5. Left: page 7 (1,286) and 6 (135): rows
   below -2^131 the console wraps with a column step above zero; pages 0, 1, 2, 10, 11 (51, 302, 83, 183, 37).
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
