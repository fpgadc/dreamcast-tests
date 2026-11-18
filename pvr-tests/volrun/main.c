/* Where runs of pixels end with a modifier volume whose state changes by depth, not at a triangle's edge
   (2026-11-18).

   polly2-data jgr_car, jgr_tree_shadows*: the shadow's edge on the ground is where a face of the shadow volume goes
   through the ground (the depth test changes), and the console's runs there do not fit refsw2's "a run ends where
   the volume state changes" (pvr-tests/uvrun kind 1, where the volumes were rectangles in front of everything:
   every change of state was a triangle's edge). Outside the volume the console's runs go on from the polygon's
   first pixel; inside, some start later. Volume triangles behind the ground come and go there too.

   Read as pvr-tests/uvrun: every cell has one textured triangle of uvrun's steep mix (../uvrun/cases.h: the cell's
   upper left half, depth 1, slopes along x with 24 bits; uvmeas's texture: texel i holds the number i, so a pixel
   shows the coordinate the TSP computed), opaque, shadow bit, cheap shadow (FPU_SHAD_SCALE 0x180, decal: the
   reading is exact inside too). Every row of the triangle starts at column 0. The volumes are rectangles over
   columns of the cell, at depth 2 (in front of the triangle), 0.5 (behind) or sloped from left to right:
     kind 0  in front over the whole cell (control: inside everywhere, no edge)
     kind 1  sloped 2 -> 0 over the whole cell: inside on columns 0-15, outside from 16 (left by depth)
     kind 2  sloped 0 -> 2: outside on columns 0-15, inside from 16 (entered by depth)
     kind 3  in front over the whole cell and behind over columns 12-31, one volume: inside everywhere
     kind 4  behind over columns 12-31 only: outside everywhere
     kind 5  in front over columns 0-11 and in front over 12-31, one volume: inside everywhere
     kind 6  kind 1 and behind over columns 20-31, one volume
     kind 7  kind 3 as two volumes (the one in front, the one behind)
   With a second volume (jgr_car's tile 16,7: a volume of two triangles a billion units across, depth 100000, in
   front of every pixel twice, so outside by itself):
     kind 8  kind 1, then a volume in front over the whole cell twice (two rectangles)
     kind 9  the same, the second volume sent first
     kind 10 kind 1, then a volume in front over the whole cell once (inside everywhere: no edge left)
     kind 11 kind 1, then jgr_car's two huge triangles as a volume (depth 100000)
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
    static const char *what[KINDS] = { "in front, whole cell", "sloped 2 -> 0", "sloped 0 -> 2", "in front + behind 12-31",
                                       "behind 12-31", "in front 0-11 + in front 12-31", "sloped 2 -> 0 + behind 20-31",
                                       "in front + behind 12-31, two volumes", "sloped, then in front twice",
                                       "in front twice, then sloped", "sloped, then in front once", "sloped, then huge triangles" };
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

static void vol_tri(int last, f32b x0, f32b y0, f32b z0, f32b x1, f32b y1, f32b z1, f32b x2, f32b y2, f32b z2) {
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
    v.x0 = x0; v.y0 = y0; v.z0 = z0;
    v.x1 = x1; v.y1 = y1; v.z1 = z1;
    v.x2 = x2; v.y2 = y2; v.z2 = z2;
    ta_send(&v, sizeof(v));
}

/* a rectangle over columns c0 .. c1 - 1 of the cell at (x, y), depth zl at its left edge and zr at its right edge;
   last: its second triangle closes the volume */
static void vol_rect(int x, int y, int c0, int c1, f32b zl, f32b zr, int last) {
    const f32b X0 = f32b_int(x + c0), X1 = f32b_int(x + c1), Y0 = f32b_int(y), Y1 = f32b_int(y + 32);
    vol_tri(0, X0, Y0, zl, X1, Y0, zr, X1, Y1, zr);
    vol_tri(last, X0, Y0, zl, X1, Y1, zr, X0, Y1, zl);
}

static void test_page(int page) {
    const int kind = page / 2;
    int i, n;
    pvrio_reg(FPU_SHAD_SCALE_addr, 0x180u);
    pvr_frame.bg_color = 0x000040;
    ta_list_begin(ListType_Opaque);
    for(i = 0; i < NUM_TRIS; i++)
        if(tris[i].page == (page & 1))
            triangle(&tris[i]);
    ta_list_end();
    ta_list_begin(ListType_Opaque_Modifier_Volume);
    for(i = 0, n = 0; i < NUM_TRIS; i++) {
        const int x = cell_px(n), y = cell_py(n);
        if(tris[i].page != (page & 1))
            continue;
        n++;
        switch(kind) {
        case 0: vol_rect(x, y, 0, 32, F(2.0f), F(2.0f), 1); break;
        case 1: vol_rect(x, y, 0, 32, F(2.0f), F(0.0f), 1); break;
        case 2: vol_rect(x, y, 0, 32, F(0.0f), F(2.0f), 1); break;
        case 3: vol_rect(x, y, 0, 32, F(2.0f), F(2.0f), 0); vol_rect(x, y, 12, 32, F(0.5f), F(0.5f), 1); break;
        case 4: vol_rect(x, y, 12, 32, F(0.5f), F(0.5f), 1); break;
        case 5: vol_rect(x, y, 0, 12, F(2.0f), F(2.0f), 0); vol_rect(x, y, 12, 32, F(2.0f), F(2.0f), 1); break;
        case 6: vol_rect(x, y, 0, 32, F(2.0f), F(0.0f), 0); vol_rect(x, y, 20, 32, F(0.5f), F(0.5f), 1); break;
        case 7: vol_rect(x, y, 0, 32, F(2.0f), F(2.0f), 1); vol_rect(x, y, 12, 32, F(0.5f), F(0.5f), 1); break;
        case 8: vol_rect(x, y, 0, 32, F(2.0f), F(0.0f), 1); vol_rect(x, y, 0, 32, F(2.0f), F(2.0f), 0); vol_rect(x, y, 0, 32, F(3.0f), F(3.0f), 1); break;
        case 9: vol_rect(x, y, 0, 32, F(2.0f), F(2.0f), 0); vol_rect(x, y, 0, 32, F(3.0f), F(3.0f), 1); vol_rect(x, y, 0, 32, F(2.0f), F(0.0f), 1); break;
        case 10: vol_rect(x, y, 0, 32, F(2.0f), F(0.0f), 1); vol_rect(x, y, 0, 32, F(2.0f), F(2.0f), 1); break;
        case 11:
            vol_rect(x, y, 0, 32, F(2.0f), F(0.0f), 1);
            /* jgr_car's 0004AE90 and 0004AEF0 (the first two of its huge volume), closed */
            vol_tri(0, 0xce6babe2u, 0x4de9da19u, F(100000.0f), 0x4debe5b1u, 0xceeb4c8fu, F(100000.0f), 0xce83e6b2u, 0x4de0e79cu, F(100000.0f));
            vol_tri(1, 0x4debe5b1u, 0xceeb4c8fu, F(100000.0f), 0x4db50ef1u, 0xceefb8a5u, F(100000.0f), 0xce83e6b2u, 0x4de0e79cu, F(100000.0f));
            break;
        }
    }
    ta_list_end();
}
