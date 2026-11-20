/* faredge_k132: the edge start pushed 2^132 further out, and where that starts (2026-11-19, after pvr-tests/faredge_cmax).

   faredge_cmax showed two things about an edge whose column step dy is 2^127 or more (a corner past 2^127 along y):
   in the screen's first tile column (x0 = 0.5) the console draws the edge as if the row's value R (c + dx y - dy x0,
   the start of the row) were pushed 2^132 further from zero, R + sign(R) 2^132, whatever R is (cmax pages 0-4, 9, 10,
   12, 15: the crossing 2^132 / |dy| + R / |dy| pixels in); and that it does not do so for |dy| of 2^127 and
   2^127.007 (cmax page 5, page 8). And that every product of the setup (xb ya, xa yb, dx y0, dy x0) that reaches 2^128
   is replaced by a value that swamps the rest, so that two such products of one sign cancel exactly (cmax pages 11,
   14, 15: a row value of 0 in the tile's first row, j dx in the others). Where between 2^127.007 and 2^127.16 does the
   push start, and does it depend on R?

   One triangle per tile, user tile clip of its tile, white, against 0x204060 (as faredge_cmax). Cases
   (agents/nulldc3/hunt/faredge/gen_k132.cpp, which wrote cases.h and scores the pages against each hypothesis):
     K   column 0 of every page: N (0.5625, -4), F (2^119, m 2^104), T (-4096, -44) (N F T): the edge tilted so that R
         grows down the column from 0.05 |dy| to 1.84 |dy|, one |dy| a page: 2^132 / |dy| = 31.4, 31.2, 31.05, 30.95,
         30.8, 30.6, 30.4, 30.2, 30.0, 29.8, 29.6 (pages 0-10: is it 31 |dy| reaching 2^132, or R - 31 |dy|, or
         R - 32 |dy|?), 16.3, 20.6, 24.9 (pages 11-13: the crossing 2^132 / |dy| + R / |dy| row by row), 30.5 with the
         far corner up (F N T, page 14), 29.6 with T at +4096 (the triangle right of the edge, page 15)
     RV  every other tile: a near-vertical edge with |far y| in [2^127, 2^128) crossing the tile, both of c's product
         x_n |far y| and dy x0 past 2^128 with opposite signs (the start 0 in the model of the swamping products),
         the far corner's x 2^96 .. 2^118 from N's so that the rows below the first are j dx; chosen so that the old
         model (start saturated to 2^127 with the sign of what overflowed) and the new one differ */
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
