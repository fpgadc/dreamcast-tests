/* faredge_cmax: when does the edge unit saturate a tile's start, and to what (2026-11-19, after pvr-tests/faredge)?

   faredge left two strips the rule "the start saturates when dx y0 - dy x0 + c reaches 2^128" got wrong: page 4's
   first-column strip (its edge v1 -> v2 has c = 2^128.2 but a start of 2^127.7, and the console draws the strip as if
   that edge had saturated too) and page 1's control strip in the second column (c = 2^128.1, start 2^125.6: the
   console's edge is 9 pixels off, where a start of 2^127 puts it). The hypothesis: the constant c = xb ya - xa yb
   reaching 2^128 saturates the start by itself, as the first sum or the start do.
   Result (2026-11-19, one console): it is each product (xb ya, xa yb, dx y0, dy x0) reaching 2^128, not c: group A's
   755 tiles with a product of exactly 2^128 and c below it saturate; such values swamp the rest and two of one sign
   cancel (pages 11, 14, 15). And in the first column, an edge with a column step past 2^127 is drawn as if its row
   value R had 2^132 more in size whatever c is (groups B, D): pvr-tests/faredge_k132 measures when.

   One triangle per tile, each with the user tile clip of its tile, so that 300 edges are measured per page:
   a long edge from a near corner N to a far corner F (+-m 2^e along y, "V", or along x, "H"), the third corner T
   4096 pixels to the side (V: left, the triangle is left of the edge; H: up, the triangle is above it), so that only
   the edge N F crosses the tile. White, untextured, depth always; shaded black where the determinant passes 2^128
   (README "faredge"), against a background of 0x204060. The cases (agents/nulldc3/hunt/faredge/gen_cmax.cpp, which
   wrote cases.h and scores the pages against each hypothesis):
     B   column 0, |far y| in [2^127, 2^128) (the column step 2^127 or more): x_n swept in 1/16 pixel over the
         constant's threshold 2^128 / |far y| and on past the start's, half a pixel later (pages 0-5, down / up,
         N F / F N)
     BH  the same turned, tile row 0, columns 1-19 (pages 0-5): |dx| past 2^127, the column step small
     C   column 0, |far y| in [2^126, 2^127) (pages 6, 7)
     E   column 0, |far y| at 2^127 - 2^103, 2^127, 2^127 + 2^104, and c at 2^128 with the other product taking or
         adding 2^105 (page 8)
     D   column 0, the far corner's x large (2^117 .. 2^121), so that dx y0 - dy x0 changes from tile row to tile row:
         the value of the start past 2^128 (pages 9, 10, 15), c below 2^128 with the start past it (page 12), the
         other product near 2^128 taking c below it (page 11)
     G   column 0, |far y| below 2^127, c below 2^128 and the start past it (pages 13, 14)
     A   every other tile: the product x_n |far y| (V) or y_n |far x| (H) one step below 2^128, exactly 2^128 (the
         product cut to 24 bits) with the other product 0, taking 2^105 from it or adding 2^105, and past it */
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
