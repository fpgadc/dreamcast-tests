/* Measure the depth the ISP stores for a triangle. See gen.cpp.

   Every page draws all the polygons A (red; depth always passes and is written), in order, and then one
   probe B per measured cell (green, compare GREATER): a flat quad over the whole cell at the value this page
   takes from the ladder of that cell. A pixel is green when the stored depth of B is greater than that of
   the A on top there.

   The PVR is driven directly (../../common/pvrtest2.h); main_kos.c is the earlier version on KallistiOS's
   driver, and the parameter words set here are the ones it sent. */
#define NUM_PAGES 16
#include "pvr_params.h"
#ifndef CASES_H
#define CASES_H "cases.h"       /* run2.sh zmeas@<set> gives the tables of an archived set */
#endif
#include CASES_H
#include "pvrtest2.h"

static void test_setup(void) {
}

static void test_describe(void) {
    int p, i;
    for(p = 0; p < NUM_PAGES; p++)
        for(i = 0; i < NUM_PROBES; i++)
            label(p, probes[i].cell, "probe %d", i);
}

/* translucent list, flat, drawn in the order sent */
static void polygon(int DepthMode, int ZWriteDis) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };

    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Translucent;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    isp.DepthMode = DepthMode;
    isp.ZWriteDis = ZWriteDis;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    ta_polygon(pcw, isp, tsp, tcw);
}

static void test_page(int page) {
    int i, k;

    ta_list_begin(ListType_Translucent);
    for(i = 0; i < NUM_TRIS; i++) {
        const tri_t *t = &tris[i];
        polygon(DepthMode_Always, 0);
        if(t->is_quad)
            quad0(cell_px(t->cell), cell_py(t->cell), 32, 32, t->v[0][2], 0xffff0000);
        else
            for(k = 0; k < 3; k++)
                ta_vertex0(t->v[k][0], t->v[k][1], t->v[k][2], 0xffff0000, k == 2);
    }
    for(i = 0; i < NUM_PROBES; i++) {
        polygon(DepthMode_Greater, 1);
        quad0(cell_px(probes[i].cell), cell_py(probes[i].cell), 32, 32, probes[i].bz[page], 0xff00ff00);
    }
    ta_list_end();
}
