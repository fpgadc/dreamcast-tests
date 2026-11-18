/* A polygon at exactly the background's stored depth, the background's tag below the polygons' (2026-11-18).

   pvr-tests/bgdepth: the background's depth keeps no bits below 2^-20; as a fixed point with 20 fraction bits, or as a
   float of 19 significant bits whose smallest step is 2^-20 (a denormal below 0.25)? Polygons' depths are on the 19
   bit grid, so the two differ only for a background D above 0.5 with its 2^-20 bit set: the float stores the 19 bit
   value below D (0.5 for 0.5 + 2^-20), the fixed point D. A polygon at exactly that 19 bit value then ties with the
   float (the tags decide: pvr-tests/trtie) and is behind the fixed point. Here the background's parameters come first
   (pvr_frame.bg_first), so its tag is below the polygons' and a tie draws the polygon.
     page 0  background 0.5 + 2^-20, translucent (sorted) at 0.5
     page 1  background 0.75 + 2^-20, translucent at 0.75
     page 2  background 1 + 2^-20, translucent at 1
     page 3  page 0 with punch-through
     page 4  background 0.5, translucent at 0.5 (control: a tie either way, drawn)
     page 5  background 0.5 + 2^-20, translucent at 0.5 + 2^-19 (control: above either way, drawn) */
#define NUM_PAGES 6
#include "pvrtest2.h"

static const uint32_t BGZ[NUM_PAGES] = { 0x3f000010u, 0x3f400010u, 0x3f800008u, 0x3f000010u, 0x3f000000u, 0x3f000010u };
static const uint32_t PZ[NUM_PAGES]  = { 0x3f000000u, 0x3f400000u, 0x3f800000u, 0x3f000000u, 0x3f000000u, 0x3f000020u };

static void test_setup(void) { pvr_frame.bg_first = 1; }

static void test_describe(void)
{
    static const char *what[NUM_PAGES] = { "bg 0.5 + 2^-20, translucent at 0.5", "bg 0.75 + 2^-20, translucent at 0.75",
                                           "bg 1 + 2^-20, translucent at 1", "bg 0.5 + 2^-20, punch-through at 0.5",
                                           "bg 0.5, translucent at 0.5", "bg 0.5 + 2^-20, translucent at 0.5 + 2^-19" };
    int p;
    for(p = 0; p < NUM_PAGES; p++)
        label_span(p, 0, 20, 15, "%s", what[p]);
}

static void test_page(int page)
{
    const int list = page == 3 ? ListType_Punch_Through : ListType_Translucent;
    int c;
    pvr_cfg.pre_sort = 0;
    pvr_region_write(0);
    pvr_frame.bg_color = 0x000040;
    pvr_frame.bg_depth = BGZ[page];
    ta_list_begin(list);
    for(c = 0; c < 300; c++) {
        PCW pcw = { .full = 0 };
        ISP_TSP isp = { .full = 0 };
        TSP tsp = { .full = 0 };
        TCW tcw = { .full = 0 };
        pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
        pcw.ListType = list;
        pcw.Group_En = 1;
        pcw.Strip_Len = 1;
        isp.DepthMode = DepthMode_GreaterEqual;
        isp.CullMode = CullMode_None;
        tsp.SrcInstr = BlendInstr_SrcAlpha;
        tsp.DstInstr = BlendInstr_InvSrcAlpha;
        tsp.FogCtrl = FogCtrl_None;
        tsp.UseAlpha = 1;
        ta_polygon(pcw, isp, tsp, tcw);
        quad0(cell_px(c), cell_py(c), 32, 32, PZ[page], list == ListType_Translucent ? 0x80ff0000u : 0xff00ff00u);
    }
    ta_list_end();
}
