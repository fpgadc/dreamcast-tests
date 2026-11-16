/* Gouraud colors on triangles with exactly known planes. See gen.py.

   The PVR is driven directly (../common/pvrtest2.h); main_kos.c is the earlier version on KallistiOS's
   driver, and the parameter words set here are the ones it sent. */
#define NUM_PAGES 8
#include "pvr_params.h"
#ifndef CASES_H
#define CASES_H "cases.h"       /* run2.sh cmeas@<set> gives the tables of an archived set */
#endif
#include CASES_H
#include "pvrtest2.h"

static void test_setup(void) {
}

static void test_describe(void) {
    int p;
    for(p = 0; p < NUM_PAGES; p++)
        label_span(p, 0, 20, 15, "page %d", p);
}

static void test_page(int page) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    int i, k;

    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Translucent;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Gouraud = 1;
    isp.DepthMode = DepthMode_Always;
    isp.ZWriteDis = 1;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;

    ta_list_begin(ListType_Translucent);
    for(i = 0; i < NUM_TRIS; i++) {
        if(tris[i].page != page)
            continue;
        ta_polygon(pcw, isp, tsp, tcw);
        for(k = 0; k < 3; k++)
            ta_vertex0(tris[i].xy[k][0], tris[i].xy[k][1], F32B_ONE, tris[i].argb[k], k == 2);
    }
    ta_list_end();
}
