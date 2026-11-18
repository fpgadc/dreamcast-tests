/* The tile's start cut to the slopes' grid (2026-11-18).

   The ISP steps a triangle's depth across a tile from the plane's value at the tile's first pixel, the start. Where
   the plane crosses zero next to that pixel the start is smaller than the slopes, and a console cuts it to the
   slopes' grid as well (21 bits under the larger slope's exponent), toward zero: a negative start comes out up to one
   step of that grid higher, and a pixel just under a stored depth step goes over it. Found in the depth buffers of
   polly2-data's dumps (sa2_ingame 34 depths, shenmue_intro 2; dumprender.sh z). Whether a positive start is cut the
   same way shows only at pixels whose depth is below about a quarter of a pixel's step (their stored step is finer
   than the slopes' grid): it is (page 1). The console = refsw3-offline on 49,152 of 49,152 pixels; the model with
   only a negative start cut misses 121, the model before 672.

   Each case is a triangle alone in a scene, in a tile of its own, opaque, depth compare always with depth write;
   the tile's depths read out by common/zexfil.h (27 renders a tile). The pages show the depths, one word per
   pixel in the tile's place (where the triangle does not cover, the background's depth), black elsewhere;
   compare2.py zstart hw off compares them word for word. gen.cpp makes the cases (cases.h) and says more.

   page 0  negative starts, where the rule changes covered pixels' depths (refsw3-core before it differs at them)
   page 1  small positive starts with the depth rising into the tile, where cutting a positive start too differs
           from cutting only a negative one (DepthConfig::startOnSlopes 1 and 2)
   page 2  starts of either sign as they come */
#define NUM_PAGES 3
#define TEST_DRAW_PAGE
#include "pvrtest2.h"
#include "zexfil.h"
#ifndef CASES_H
#define CASES_H "cases.h"
#endif
#include CASES_H

/* zexfil's area: free video memory above PARAM_BASE (as pvr-tests/zexfil) */
#define ZEXFIL_AT 0x580000u

static int cur_page, cur_case;

static void scene(void) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    int k;

    ta_list_begin(ListType_Opaque);
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Opaque;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    isp.DepthMode = DepthMode_Always;
    isp.CullMode = CullMode_None;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    ta_polygon(pcw, isp, tsp, tcw);
    for(k = 0; k < 3; k++)
        ta_vertex0(zs_case[cur_page][cur_case].x[k], zs_case[cur_page][cur_case].y[k], zs_case[cur_page][cur_case].z[k],
                   0xff000000u | (uint32_t)(cur_case + 1) * 0x0f0b07u, k == 2);
}

static void test_setup(void) {
    zexfil_use_pvrsetup(scene, ZEXFIL_AT);
}

static int case_cell(int n) {
    return (ZS_WIN_Y + n / ZS_WIN_W) * COLS + ZS_WIN_X + n % ZS_WIN_W;
}

static void test_describe(void) {
    static const char *what[NUM_PAGES] = { "negative start", "small positive start", "start of either sign" };
    int p, n;
    for(p = 0; p < NUM_PAGES; p++)
        for(n = 0; n < ZS_CASES; n++)
            label(p, case_cell(n), "%s: %d pixels covered; the start's cut changes %d of them, cutting a positive start too %d",
                  what[p], zs_case[p][n].covered, zs_case[p][n].d01, zs_case[p][n].d12);
}

static void test_page(int page) {
    (void)page;
}

static void test_draw_page(int page) {
    static uint32_t z[ZS_CASES][1024];
    char line[80];
    int n;

    cur_page = page;
    zexfil_foreign = 0;
    for(n = 0; n < ZS_CASES; n++) {
        cur_case = n;
        zexfil_tile(ZS_WIN_X + n % ZS_WIN_W, ZS_WIN_Y + n / ZS_WIN_W, z[n]);
    }
    pvrio_fill32(SETUP_FB, 640 * 480, 0);
    for(n = 0; n < ZS_CASES; n++)
        zexfil_show(ZS_WIN_X + n % ZS_WIN_W, ZS_WIN_Y + n / ZS_WIN_W, z[n]);
    if(zexfil_foreign) {
        snprintf(line, sizeof(line), "PVRIO zstart page %d: %d readings of another colour than the probe's", page, zexfil_foreign);
        pvrio_hw_say(line);
    }
}
