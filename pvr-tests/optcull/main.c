/* Which punch-through content of a tile stops the opaque runs from ending at modifier volume edges (2026-11-19).

   pvr-tests/optrun: in a tile whose first punch-through pass draws something, opaque runs do not end at volume edges;
   a punch-through list that draws nothing changes nothing (polly2-data sa2_ingame, 18wheel_select). polly2-data
   ff2_fight, tile (10, 12): the console's opaque run crosses a volume edge (its colour fits a run from column 29 or
   before, not one from 30; a shadow bit poked in shows the volume state changing there on the console as in refsw3),
   in a tile whose punch-through list holds two triangles that are culled (CullMode, facing away). Read as optrun
   (uvrun's steep opaque triangle with the shadow bit, uvmeas's texture; a cheap shadow volume over columns 18-23 of
   every cell); the punch-through list of every cell's tile holds:
     kind 0  a triangle culled facing away (CullMode negative)
     kind 1  the same triangle wound the other way, CullMode positive (kinds 0-2 draw with CullMode none)
     kind 2  a triangle culled as small (CullMode small, legs of 3/4 pixel over a pixel centre, FPU_CULL_VAL as set up)
     kind 3  a quad behind the opaque triangle (depth 0.5: the depth test fails), over its columns 2-5, rows 0-3
     kind 4  a degenerate triangle (three corners on a line) in the corner
     kind 5  a thin triangle in the corner that covers no pixel centre
     kind 6  optrun's corner quad that passes (draws: the runs do not end at the volume)
     kind 7  no punch-through list (the runs end at the volume)
     kind 8  the corner quad failing the alpha test
   Two pages of triangles per kind (page = 2 kind + 0 or 1). */
#define KINDS 9
#define NUM_PAGES (2 * KINDS)
#define TEST_MODIFIERS
#include "pvr_params.h"
#include "../uvrun/cases.h"
#include "pvrtest2.h"

static uint32_t texture;        /* 64 bit area address */

static void test_setup(void) {
    static uint32_t words[1024 * 8 / 2];
    int i, j;
    for(j = 0; j < 8; j++)
        for(i = 0; i < 1024; i += 2)
            words[(j * 1024 + i) / 2] = (uint32_t)i | (uint32_t)(i + 1) << 16;
    texture = pvr_tex_alloc(sizeof(words));
    pvrio_w64(texture, words, sizeof(words) / 4);
}

static void test_describe(void) {
    static const char *what[KINDS] = { "PT culled facing away (negative)", "PT culled facing away (positive)", "PT culled small",
                                       "PT behind the opaque triangle", "PT degenerate", "PT covering no pixel centre",
                                       "PT corner quad passes", "no PT list", "PT corner quad fails" };
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 300; c++)
            label(p, c, "kind %d: %s, volume over 18-23", p / 2, what[p / 2]);
}

static void triangle(const tri_t *t) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    int k;
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Opaque;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Texture = 1;
    pcw.Shadow = 1;
    isp.DepthMode = DepthMode_GreaterEqual;
    isp.Texture = 1;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    tsp.ShadInstr = ShadInstr_Decal;
    tsp.FilterMode = FilterMode_Point;
    tsp.MipMapD = 4;
    tsp.TexU = 7;                               /* 1024 */
    tsp.TexV = 0;                               /* 8 */
    tcw.PixelFmt = PixelFmt_565;
    tcw.ScanOrder = 1;
    tcw.TexAddr = texture >> 3;
    ta_polygon(pcw, isp, tsp, tcw);
    for(k = 0; k < 3; k++)                      /* v: x, y, u, z */
        ta_vertex3(t->v[k][0], t->v[k][1], t->v[k][3], t->v[k][2], F32B_HALF, 0xffffffff, 0, k == 2);
}

/* an untextured punch-through quad over columns c0 .. c1 - 1 and rows r0 .. r1 - 1 of the cell at (x, y) */
static void pt_quad(int x, int y, int c0, int c1, int r0, int r1, f32b z, int passes) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Punch_Through;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    isp.DepthMode = DepthMode_GreaterEqual;
    tsp.SrcInstr = BlendInstr_SrcAlpha;
    tsp.DstInstr = BlendInstr_InvSrcAlpha;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    ta_polygon(pcw, isp, tsp, tcw);
    quad0(x + c0, y + r0, c1 - c0, r1 - r0, z, passes ? 0xffff0000u : 0x0000ff00u);
}

static void vol_tri(int last, f32b x0, f32b y0, f32b x1, f32b y1, f32b x2, f32b y2) {
    TA_ModVolParam m;
    TA_ModVol v;
    memset(&m, 0, sizeof(m));
    m.pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    m.pcw.ListType = ListType_Opaque_Modifier_Volume;
    m.isp.modvol.VolumeMode = last ? VolumeMode_Inside_Last : VolumeMode_Normal;
    m.isp.modvol.CullMode = CullMode_None;
    ta_send(&m, sizeof(m));
    memset(&v, 0, sizeof(v));
    v.pcw = pcw_vertex(1);
    v.x0 = x0; v.y0 = y0; v.z0 = F(3.0f);
    v.x1 = x1; v.y1 = y1; v.z1 = F(3.0f);
    v.x2 = x2; v.y2 = y2; v.z2 = F(3.0f);
    ta_send(&v, sizeof(v));
}

static void pt_tri(int cull, f32b z, f32b x0, f32b y0, f32b x1, f32b y1, f32b x2, f32b y2) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Punch_Through;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    isp.DepthMode = DepthMode_GreaterEqual;
    isp.CullMode = (uint32_t)cull;
    tsp.SrcInstr = BlendInstr_SrcAlpha;
    tsp.DstInstr = BlendInstr_InvSrcAlpha;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    ta_polygon(pcw, isp, tsp, tcw);
    ta_vertex0(x0, y0, z, 0xffff0000u, 0);
    ta_vertex0(x1, y1, z, 0xffff0000u, 0);
    ta_vertex0(x2, y2, z, 0xffff0000u, 1);
}

static void test_page(int page) {
    const int kind = page / 2;
    int i, n;
    pvrio_reg(FPU_SHAD_SCALE_addr, 0x180u);
    pvrio_reg(PT_ALPHA_REF_addr, 0x80);
    pvr_frame.bg_color = 0x000040;
    ta_list_begin(ListType_Opaque);
    for(i = 0; i < NUM_TRIS; i++)
        if(tris[i].page == (page & 1))
            triangle(&tris[i]);
    ta_list_end();
    if(kind != 7) {
        ta_list_begin(ListType_Punch_Through);
        for(i = 0, n = 0; i < NUM_TRIS; i++) {
            const int x = cell_px(n), y = cell_py(n);
            const f32b X = f32b_int(x + 28), Y = f32b_int(y + 28), X4 = f32b_int(x + 32), Y4 = f32b_int(y + 32);
            const f32b Xq = f32b_scaled(4 * (x + 28) + 1, -2), Yq = f32b_scaled(4 * (y + 28) + 1, -2);
            if(tris[i].page != (page & 1))
                continue;
            n++;
            switch(kind) {
            case 0: pt_tri(CullMode_Negative, F(2.0f), X, Y, X, Y4, X4, Y); break;
            case 1: pt_tri(CullMode_Positive, F(2.0f), X, Y, X4, Y, X, Y4); break;
            case 2: pt_tri(CullMode_Small, F(2.0f), Xq, Yq, f32b_int(x + 29), Yq, Xq, f32b_int(y + 29)); break;
            case 3: pt_quad(x, y, 2, 6, 0, 4, F(0.5f), 1); break;
            case 4: pt_tri(CullMode_None, F(2.0f), X, Y, f32b_int(x + 30), f32b_int(y + 30), X4, Y4); break;
            case 5: pt_tri(CullMode_None, F(2.0f), f32b_scaled(4 * (x + 28) + 1, -2), Y, f32b_scaled(4 * (x + 28) + 1, -2), Y4, f32b_scaled(4 * (x + 28) + 3, -2), Y4); break;
            case 6: pt_quad(x, y, 28, 32, 28, 32, F(2.0f), 1); break;
            case 8: pt_quad(x, y, 28, 32, 28, 32, F(2.0f), 0); break;
            }
        }
        ta_list_end();
    }
    ta_list_begin(ListType_Opaque_Modifier_Volume);
    for(i = 0, n = 0; i < NUM_TRIS; i++) {
        const int x = cell_px(n), y = cell_py(n);
        if(tris[i].page != (page & 1))
            continue;
        n++;
        vol_tri(0, f32b_int(x + 18), f32b_int(y), f32b_int(x + 24), f32b_int(y), f32b_int(x + 24), f32b_int(y + 32));
        vol_tri(1, f32b_int(x + 18), f32b_int(y), f32b_int(x + 24), f32b_int(y + 32), f32b_int(x + 18), f32b_int(y + 32));
    }
    ta_list_end();
}
