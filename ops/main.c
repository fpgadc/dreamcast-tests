/* One operation at a time: the cases of cases.h (made by gen, from gen.cpp) are inputs on which changing a
   single number of the fitted arithmetic changes the result. See gen.cpp for the four kinds of case and
   analyze.py for the verdicts.
   depth   red triangle, then a green quad over the whole cell with compare EQUAL
   fog     black triangle with table fog (white, every table pair (0, 255), density 1)
   color   Gouraud gray triangle
   cover   white triangle

   The PVR is driven directly (../common/pvrtest2.h); main_kos.c is the earlier version on KallistiOS's
   driver, and the parameter words set here are the ones it sent. */
#define NUM_PAGES 12            /* cases.h uses OPS_PAGES of them */
#include "pvrtest2.h"
#include "cases.h"

static void test_setup(void) {
    int i;
    for(i = 0; i < 128; i++)
        pvrio_reg(FOG_TABLE_START_addr + 4 * i, 0x00ff);
    pvrio_reg(FOG_DENSITY_addr, 0x8000);
    pvrio_reg(FOG_COL_RAM_addr, 0x00ffffff);
}

static void test_describe(void) {
    static const char *kind[4] = { "depth", "fog", "color", "cover" };
    int i;
    for(i = 0; i < NUM_CASES; i++)
        label(cases[i].page, cases[i].cell, "case %d (%s)", i, kind[cases[i].kind]);
}

/* translucent list, drawn in the order sent */
static void polygon(int Gouraud, int DepthMode, int ZWriteDis, int FogCtrl) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };

    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Translucent;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Gouraud = Gouraud;
    isp.DepthMode = DepthMode;
    isp.ZWriteDis = ZWriteDis;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl;
    tsp.UseAlpha = 1;
    ta_polygon(pcw, isp, tsp, tcw);
}

static void triangle(const case_t *c, const uint32_t col[3]) {
    int k;
    for(k = 0; k < 3; k++)
        ta_vertex0(c->v[k][0], c->v[k][1], c->v[k][2], col[k], k == 2);
}

static void test_page(int page) {
    int i, k;

    ta_list_begin(ListType_Translucent);
    for(i = 0; i < NUM_CASES; i++) {
        const case_t *c = &cases[i];
        uint32_t col[3];
        if(c->page != page)
            continue;
        if(c->kind == 0) {
            col[0] = col[1] = col[2] = 0xffff0000;
            polygon(0, DepthMode_Always, 0, FogCtrl_None);
            triangle(c, col);
            polygon(0, DepthMode_Equal, 1, FogCtrl_None);
            quad0(cell_px(c->cell), cell_py(c->cell), 32, 32, c->bz, 0xff00ff00);
        }
        else if(c->kind == 1) {
            col[0] = col[1] = col[2] = 0xff000000;
            polygon(0, DepthMode_Always, 1, FogCtrl_Table);
            triangle(c, col);
        }
        else if(c->kind == 2) {
            for(k = 0; k < 3; k++)
                col[k] = 0xff000000 | ((uint32_t)c->col[k] << 16) | ((uint32_t)c->col[k] << 8) | c->col[k];
            polygon(1, DepthMode_Always, 1, FogCtrl_None);
            triangle(c, col);
        }
        else {
            col[0] = col[1] = col[2] = 0xffffffff;
            polygon(0, DepthMode_Always, 1, FogCtrl_None);
            triangle(c, col);
        }
    }
    ta_list_end();
}
