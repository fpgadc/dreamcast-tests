/* A sorted translucent polygon at exactly the depth of the opaque or punch-through layer under it (2026-11-18).

   polly2-data shenmue_intro2 (522,170): a translucent polygon whose ISP depth equals the punch-through pixel's
   (0x1.0d788p-3 both) is not drawn on the console; refsw2 (and polly2-rtl) start the sort from a reference below
   every tag at that depth, so it was drawn. dumprules page 1 cells 4-6 (drawn at exactly the opaque depth) sent the
   opaque list first: their opaque tags were below the translucent ones. Tags grow with the order the parameters are
   written, so the order the lists are sent in decides which is below.

   Every cell has a quad (32x32) under a translucent quad (red, alpha 0x80, SRC_ALPHA / INV_SRC_ALPHA, sorted per
   pixel) with the same corners in the same order: the same depth at every pixel. Drawn, the cell is a red mix.
     kind 0  opaque grey, then the translucent list            (translucent tags above)
     kind 1  the translucent list, then opaque grey            (translucent tags below)
     kind 2  punch-through green (alpha 0xff: passes), then the translucent list
     kind 3  the translucent list, then punch-through green
     kind 4-7  the same with a sloped depth: 0.3 at the top left, 0.7 at the bottom right (flat: 0.5)
     kind 8  no polygon under it: the translucent quad at exactly the background's depth (ISP_BACKGND_D 0.5)
   Punch-through at exactly the opaque depth (green over grey, no translucent):
     kind 9  opaque, then punch-through          kind 10  punch-through, then opaque
     kind 11 punch-through at exactly the background's depth (the background's parameters are written last: its
             tag is above the polygons')
     kind 12-15  translucent at exactly the background's depth, flat, the depth 0x3727c5a0 (polly2-data rez_ingame: a
             translucent polygon there is drawn on the console), 0.75, 0.3 (0x3e99999a), 0x3727c5a0 with punch-through
   The background is at depth 0.5 on pages 8 and 11, at the polygons' depth on 12-15, 0.001 elsewhere. One page per
   kind. */
#define NUM_PAGES 16
#include "pvrtest2.h"

static void test_setup(void) {}

static void test_describe(void)
{
    static const char *what[NUM_PAGES] = { "opaque, then translucent", "translucent, then opaque", "punch-through, then translucent",
                                           "translucent, then punch-through", "sloped: opaque, then translucent",
                                           "sloped: translucent, then opaque", "sloped: punch-through, then translucent",
                                           "sloped: translucent, then punch-through", "translucent at the background's depth",
                                           "opaque, then punch-through", "punch-through, then opaque", "punch-through at the background's depth",
                                           "translucent at the background's depth 0x3727c5a0", "translucent at the background's depth 0.75",
                                           "translucent at the background's depth 0x3e99999a", "punch-through at the background's depth 0x3727c5a0" };
    int p;
    for(p = 0; p < NUM_PAGES; p++)
        label_span(p, 0, 20, 15, "%s", what[p]);
}

static void poly(int list)
{
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
    tsp.SrcInstr = list == ListType_Opaque ? BlendInstr_One : BlendInstr_SrcAlpha;
    tsp.DstInstr = list == ListType_Opaque ? BlendInstr_Zero : BlendInstr_InvSrcAlpha;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    ta_polygon(pcw, isp, tsp, tcw);
}

static uint32_t flat_z = 0x3f000000u;          /* the flat depth (0.5 unless a page says) */

/* the cell's quad: corners in strip order, depth flat 0.5 or 0.3 + 0.2 x / 32 + 0.2 y / 32 */
static void cell_quad(int x, int y, int sloped, uint32_t argb)
{
    static const uint32_t zs[4] = { 0x3e99999au, 0x3f000000u, 0x3f000000u, 0x3f333333u };      /* 0.3, 0.5, 0.5, 0.7 */
    int k;
    for(k = 0; k < 4; k++)
        ta_vertex0(f32b_int(x + (k & 1) * 32), f32b_int(y + (k >> 1) * 32), sloped ? zs[k] : flat_z, argb, k == 3);
}

static void layer(int list, int sloped)
{
    const uint32_t argb = list == ListType_Translucent ? 0x80ff0000u : list == ListType_Opaque ? 0xff808080u : 0xff00ff00u;
    int c;
    ta_list_begin(list);
    for(c = 0; c < 300; c++) {
        poly(list);
        cell_quad(cell_px(c), cell_py(c), sloped, argb);
    }
    ta_list_end();
}

static void test_page(int page)
{
    const int sloped = page >= 4 && page < 8, k = page & 3;
    pvr_cfg.pre_sort = 0;                       /* sorted per pixel (the harness's default is the order sent) */
    pvr_region_write(0);
    pvr_frame.bg_color = 0x000040;
    static const uint32_t zpage[4] = { 0x3727c5a0u, 0x3f400000u, 0x3e99999au, 0x3727c5a0u };
    flat_z = page >= 12 ? zpage[page - 12] : 0x3f000000u;
    pvr_frame.bg_depth = page == 8 || page == 11 ? F32B_HALF : page >= 12 ? flat_z : 0x3a83126fu;   /* ISP_BACKGND_D (written by the harness) */
    if(page == 8 || page == 11 || page >= 12) {
        layer(page == 11 || page == 15 ? ListType_Punch_Through : ListType_Translucent, 0);
        return;
    }
    if(page >= 9) {
        layer(page == 9 ? ListType_Opaque : ListType_Punch_Through, 0);
        layer(page == 9 ? ListType_Punch_Through : ListType_Opaque, 0);
        return;
    }
    {
        const int under = k < 2 ? ListType_Opaque : ListType_Punch_Through;
        if(k == 0 || k == 2) {
            layer(under, sloped);
            layer(ListType_Translucent, sloped);
        } else {
            layer(ListType_Translucent, sloped);
            layer(under, sloped);
        }
    }
}
