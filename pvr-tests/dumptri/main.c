/* Triangles of game frames alone, untextured, Gouraud, at their place on the screen: the colour the TSP
   interpolates, read exactly (0888 framebuffer). tris.h (made from refsw2-offline's log of a polly2 dump: corners
   in rasterizer order, exact, with their packed colours) gives them, one page each. Opaque, compare always,
   decal, no fog. (2026-11-18) */
#include "tris.h"
#define NUM_PAGES NUM_DTRIS
#include "pvrtest2.h"

static void test_setup(void) {}

static void test_describe(void)
{
    int p;
    for(p = 0; p < NUM_PAGES; p++)
        label_span(p, 0, 20, 15, "%s", dtris[p].name);
}

static void test_page(int page)
{
    const dtri_t *t = &dtris[page];
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    int k;
    pvr_frame.bg_color = 0x000000;
    ta_list_begin(ListType_Opaque);
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Opaque;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Gouraud = 1;
    isp.DepthMode = DepthMode_Always;
    isp.CullMode = CullMode_None;
    isp.Gouraud = 1;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    ta_polygon(pcw, isp, tsp, tcw);
    for(k = 0; k < 3; k++)
        ta_vertex0(t->v[k][0], t->v[k][1], t->v[k][2], t->col[k], k == 2);
    ta_list_end();
}
