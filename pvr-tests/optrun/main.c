/* The runs of opaque polygons in a tile with punch-through polygons (2026-11-18).

   polly2-data jgr_car: in a tile whose first punch-through pass draws something, the opaque polygons' runs do not
   end at modifier volume edges (where the same polygons' runs end in a tile without punch-through: pvr-tests/volrun,
   polly2-data sa2_ingame), and an opaque run starts anew after punch-through pixels in front of it on the line;
   in mvsc2_ingame it does not, after one punch-through polygon in front of it over 11 pixels.

   Read as pvr-tests/uvrun: every cell has, opaque, one textured triangle of uvrun's steep mix (../uvrun/cases.h:
   the cell's upper left half, depth 1, slopes along x with 24 bits; uvmeas's texture: texel i holds the number i,
   so a pixel shows the coordinate the TSP computed there), and punch-through polygons in front of it (depth 2,
   untextured, alpha 0xff: they pass, 0: they fail; PT_ALPHA_REF 0x80) over columns of the cell, each its own
   polygon. A tile with punch-through has one more over the cell's last 4 columns of its last 4 rows, outside the
   triangle, so that the first pass draws something in the tile even where no band is.
     kind 0   that corner polygon only
     kind 1   one polygon over columns 8-11, passes
     kind 2   one polygon over columns 8-11, fails
     kind 3   two polygons, 8-9 and 10-11, pass
     kind 4   three, 8, 9-10 and 11, pass
     kind 5   four, 8, 9, 10 and 11, pass
     kind 6   five, 8 to 12, pass
     kind 7   four, 8 to 11, fail
     kind 8   one over 8-11 that fails, in front of one over 8-11 that passes (depth 1.5)
     kind 9   kind 1 with a modifier volume over columns 18-23 (cheap shadow; the triangle has the shadow bit)
     kind 10  kind 0 with that volume
     kind 11  that volume, no punch-through in the tile (the volume edges end the runs: uvrun kind 1)
   Two pages of triangles per kind (page = 2 kind + 0 or 1). */
#define KINDS 12
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
    static const char *what[KINDS] = { "corner only", "one passes 8-11", "one fails 8-11", "two pass 8-11", "three pass 8-11",
                                       "four pass 8-11", "five pass 8-12", "four fail 8-11", "fails over passes 8-11",
                                       "one passes 8-11, volume 18-23", "corner, volume 18-23", "volume 18-23, no punch-through" };
    int p;
    for(p = 0; p < NUM_PAGES; p++)
        label_span(p, 0, 20, 15, "kind %d (%s) page %d", p / 2, what[p / 2], p);
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

static void test_page(int page) {
    const int kind = page / 2;
    int i, n, b;
    pvrio_reg(FPU_SHAD_SCALE_addr, 0x180u);
    pvrio_reg(PT_ALPHA_REF_addr, 0x80);
    pvr_frame.bg_color = 0x000040;
    ta_list_begin(ListType_Opaque);
    for(i = 0; i < NUM_TRIS; i++)
        if(tris[i].page == (page & 1))
            triangle(&tris[i]);
    ta_list_end();
    if(kind != 11) {
        ta_list_begin(ListType_Punch_Through);
        for(i = 0, n = 0; i < NUM_TRIS; i++) {
            const int x = cell_px(n), y = cell_py(n);
            if(tris[i].page != (page & 1))
                continue;
            n++;
            pt_quad(x, y, 28, 32, 28, 32, F(2.0f), 1);
            switch(kind) {
            case 1: case 9: pt_quad(x, y, 8, 12, 0, 32, F(2.0f), 1); break;
            case 2: pt_quad(x, y, 8, 12, 0, 32, F(2.0f), 0); break;
            case 3: pt_quad(x, y, 8, 10, 0, 32, F(2.0f), 1); pt_quad(x, y, 10, 12, 0, 32, F(2.0f), 1); break;
            case 4: pt_quad(x, y, 8, 9, 0, 32, F(2.0f), 1); pt_quad(x, y, 9, 11, 0, 32, F(2.0f), 1); pt_quad(x, y, 11, 12, 0, 32, F(2.0f), 1); break;
            case 5: case 7: for(b = 8; b < 12; b++) pt_quad(x, y, b, b + 1, 0, 32, F(2.0f), kind == 5); break;
            case 6: for(b = 8; b < 13; b++) pt_quad(x, y, b, b + 1, 0, 32, F(2.0f), 1); break;
            case 8: pt_quad(x, y, 8, 12, 0, 32, F(2.0f), 0); pt_quad(x, y, 8, 12, 0, 32, F(1.5f), 1); break;
            }
        }
        ta_list_end();
    }
    if(kind >= 9) {
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
}
