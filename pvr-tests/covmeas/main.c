/* Which pixels are inside a polygon whose edge passes through pixel centers? See gen.py.
   Every polygon is white, flat, depth 1, drawn alone over the black background; as a triangle, a strip of two
   triangles or a sprite (COV_PRIM 0, 1, 2), in the list COV_LIST. With COV_PRIM 3 the triangles are modifier
   volumes, one each, in front of a white polygon over the whole screen that has cheap shadow: what a triangle
   covers is darker. With COV_PRIM 4 the triangles are red and write their depth, and a green polygon over
   the whole screen at COV_PROBE_Z follows that is drawn where it is nearer.

   The PVR is driven directly (../../common/pvrtest2.h): the same source renders in refsw2-offline, on a console
   and in an emulator. main_kos.c is the earlier version on KallistiOS's driver; the parameter words set
   here field by field are the ones it sent. */
#define NUM_PAGES 8
#include "pvr_params.h"
#ifndef CASES_H
#define CASES_H "cases.h"       /* run2.sh covmeas@<set> gives the tables of an archived set */
#endif
#include CASES_H
#if COV_PRIM == 3
#define TEST_MODIFIERS
#endif
#include "pvrtest2.h"

static const uint32_t WHITE = 0xffffffff, RED = 0xffff0000, GREEN = 0xff00ff00, BLUE = 0xff0000ff;

/* A polygon in `list`: nothing that could hide it but what is asked for. Blending: source as it is. */
static void polygon(int list, int Gouraud, int Shadow, int CullMode, int DepthMode, int ZWriteDis) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };

    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = list;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Col_Type = Col_Type_Packed;
    pcw.Gouraud = Gouraud;
    pcw.Shadow = Shadow;
    isp.DepthMode = DepthMode;
    isp.CullMode = CullMode;
    isp.ZWriteDis = ZWriteDis;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    ta_polygon(pcw, isp, tsp, tcw);
}

static void test_setup(void) {
    pvrio_reg(HALF_OFFSET_addr, COV_HALFOFS);
#if COV_PRIM == 3
    pvrio_reg(FPU_SHAD_SCALE_addr, 0x180);      /* intensity shadow, scale 128/256 */
#endif
}

static void test_describe(void) {
    int p;
    for(p = 0; p < NUM_PAGES; p++)
        label_span(p, 0, 20, 15, "page %d", p);
}

/* a triangle with a depth per corner; COV_VCOL: corners red, green, blue */
static void tri3(const f32b xy[4][2], uint32_t argb) {
    static const f32b zs[3] = COV_ZS;
    const uint32_t col[3] = { RED, GREEN, BLUE };
    int k;

    polygon(COV_LIST, COV_VCOL == 2, 0, COV_CULL, DepthMode_Always, COV_PRIM != 4);
    for(k = 0; k < 3; k++)
        ta_vertex0(xy[k][0], xy[k][1], zs[k], COV_VCOL ? col[k] : argb, k == 2);
}

static void strip4(const f32b xy[4][2]) {
    int k;

    polygon(COV_LIST, COV_VCOL == 2, 0, COV_CULL, DepthMode_Always, 1);
    for(k = 0; k < 4; k++)
        ta_vertex0(xy[k][0], xy[k][1], COV_Z, WHITE, k == 3);
}

static void sprite4(const f32b p[4][2]) {
    TA_SpriteParam s;
    TA_Sprite0 v;

    memset(&s, 0, sizeof(s));
    s.pcw.ParaType = ParamType_Sprite;
    s.pcw.ListType = COV_LIST;
    s.pcw.UV_16bit = 1;
    s.isp.DepthMode = DepthMode_Always;
    s.isp.CullMode = COV_CULL;
    s.isp.ZWriteDis = 1;
    s.tsp.FogCtrl = FogCtrl_None;
    if(COV_LIST == ListType_Opaque) {           /* as KallistiOS's sprite context: blended unless opaque */
        s.tsp.SrcInstr = BlendInstr_One;
        s.tsp.DstInstr = BlendInstr_Zero;
    } else {
        s.tsp.SrcInstr = BlendInstr_SrcAlpha;
        s.tsp.DstInstr = BlendInstr_InvSrcAlpha;
        s.tsp.UseAlpha = 1;
    }
    s.BaseCol = WHITE;
    ta_send(&s, sizeof(s));
    memset(&v, 0, sizeof(v));
    v.pcw = pcw_vertex(1);
    v.x0 = p[0][0]; v.y0 = p[0][1]; v.z0 = COV_Z;
    v.x1 = p[1][0]; v.y1 = p[1][1]; v.z1 = COV_Z;
    v.x2 = p[2][0]; v.y2 = p[2][1]; v.z2 = COV_Z;
    v.x3 = p[3][0]; v.y3 = p[3][1];
    ta_send(&v, sizeof(v));
}

static void volume(const f32b p[4][2]) {
    TA_ModVolParam m;
    TA_ModVol v;

    memset(&m, 0, sizeof(m));
    m.pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    m.pcw.ListType = ListType_Opaque_Modifier_Volume;
    m.isp.modvol.VolumeMode = VolumeMode_Inside_Last;
    m.isp.modvol.CullMode = CullMode_None;
    ta_send(&m, sizeof(m));
    memset(&v, 0, sizeof(v));
    v.pcw = pcw_vertex(1);
    v.x0 = p[0][0]; v.y0 = p[0][1]; v.z0 = F32B_ONE;
    v.x1 = p[1][0]; v.y1 = p[1][1]; v.z1 = F32B_ONE;
    v.x2 = p[2][0]; v.y2 = p[2][1]; v.z2 = F32B_ONE;
    ta_send(&v, sizeof(v));
}

static void test_page(int page) {
    int i;

    if(COV_PRIM == 3) {
        ta_list_begin(ListType_Opaque);
        polygon(ListType_Opaque, 0, 1, CullMode_None, DepthMode_Always, 0);
        quad0(0, 0, 640, 480, F32B_HALF, WHITE);
        ta_list_end();
        ta_list_begin(ListType_Opaque_Modifier_Volume);
        for(i = 0; i < NUM_TRIS; i++)
            if(tris[i].page == page)
                volume(tris[i].v);
        ta_list_end();
        return;
    }
    ta_list_begin(COV_LIST);
    for(i = 0; i < NUM_TRIS; i++)
        if(tris[i].page == page) {
            if(COV_PRIM == 0)
                tri3(tris[i].v, WHITE);
            else if(COV_PRIM == 4)
                tri3(tris[i].v, RED);
            else if(COV_PRIM == 1)
                strip4(tris[i].v);
            else
                sprite4(tris[i].v);
        }
    if(COV_PRIM == 4) {
        polygon(COV_LIST, 0, 0, CullMode_None, DepthMode_Greater, 1);
        quad0(0, 0, 640, 480, COV_PROBE_Z, GREEN);
    }
    ta_list_end();
}
