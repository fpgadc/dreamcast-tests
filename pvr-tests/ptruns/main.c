/* Is a punch-through pixel that failed the alpha test shaded again when a later pass draws elsewhere? (2026-11-18)

   polly2-data jgr_tree_shadows2: refsw2 shaded such a pixel again in the next pass, from another run start, where
   it passed; the console leaves it as it was (the pixel shows what is behind it, if anything). Here every cell has
   one triangle of uvmeas's steep mix (cases.h, gen.py: slopes along x with 24 bits, so that the texel a pixel reads
   depends on where its run starts) in the punch-through list, with pvr-tests/uvmeas's texture in 1555: texel i holds
   the number i (green i >> 5, blue i & 31) and alpha 1, except where i % 5 == 2 (alpha 0: the pixel fails, PT_ALPHA_REF
   0x80). A failed pixel shaded again from another run start reads another texel and can pass: it would show a texel
   where the console shows what is behind. Two pages of triangles per kind (page = 2 kind + 0 or 1):
     kind 0  the triangle alone (no later pass happens)
     kind 1  a red untextured punch-through quad behind it over the cell's columns 0-15: a later pass draws there, and
             the failed pixels at 16-31 have nothing new
     kind 2  the red quad behind the whole cell
     kind 3  bands in front over columns 8-11 and 18-23 with alpha 0 (pvr-tests/uvrun kind 4) as well
   Blend SRC_ALPHA, INV_SRC_ALPHA: a pixel that fails leaves what was there. The PVR is driven directly
   (../../common/pvrtest2.h). */
#define KINDS 4
#define NUM_PAGES (2 * KINDS)
#include "pvr_params.h"
#include "cases.h"
#include "pvrtest2.h"

static uint32_t texture;        /* 64 bit area address */
static const int bands[2][2] = { { 8, 4 }, { 18, 6 } };     /* first column, width */

static void test_setup(void) {
    static uint32_t words[1024 * 8 / 2];
    int i, j;
    for(j = 0; j < 8; j++)
        for(i = 0; i < 1024; i += 2) {
            const uint32_t a = (i % 5 == 2) ? 0 : 0x8000u, b = ((i + 1) % 5 == 2) ? 0 : 0x8000u;
            words[(j * 1024 + i) / 2] = (a | (uint32_t)(i >> 5) << 5 | (uint32_t)(i & 31))
                                      | (b | (uint32_t)((i + 1) >> 5) << 5 | (uint32_t)((i + 1) & 31)) << 16;
        }
    texture = pvr_tex_alloc(sizeof(words));
    pvrio_w64(texture, words, sizeof(words) / 4);
}

static void test_describe(void) {
    static const char *what[KINDS] = { "alone", "red behind columns 0-15", "red behind the cell", "alpha 0 bands in front" };
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
    pcw.ListType = ListType_Punch_Through;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Texture = 1;
    isp.DepthMode = DepthMode_GreaterEqual;
    isp.Texture = 1;
    tsp.SrcInstr = BlendInstr_SrcAlpha;
    tsp.DstInstr = BlendInstr_InvSrcAlpha;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    tsp.ShadInstr = ShadInstr_Decal;
    tsp.FilterMode = FilterMode_Point;
    tsp.MipMapD = 4;
    tsp.TexU = 7;                               /* 1024 */
    tsp.TexV = 0;                               /* 8 */
    tcw.PixelFmt = PixelFmt_1555;
    tcw.ScanOrder = 1;
    tcw.TexAddr = texture >> 3;
    ta_polygon(pcw, isp, tsp, tcw);
    for(k = 0; k < 3; k++)                      /* v: x, y, u, z */
        ta_vertex3(t->v[k][0], t->v[k][1], t->v[k][3], t->v[k][2], F32B_HALF, 0xffffffff, 0, k == 2);
}

/* an untextured punch-through quad: columns x0 .. x0 + w - 1 of the cell at (x, y), depth z, colour argb */
static void quad(int x, int y, int x0, int w, f32b z, uint32_t argb) {
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
    quad0(x + x0, y, w, 32, z, argb);
}

static void test_page(int page) {
    const int kind = page / 2;
    int i, n, b;
    pvrio_reg(PT_ALPHA_REF_addr, 0x80);
    pvr_frame.bg_color = 0x000040;
    ta_list_begin(ListType_Punch_Through);
    for(i = 0, n = 0; i < NUM_TRIS; i++) {
        const tri_t *t = &tris[i];
        const int x = cell_px(n), y = cell_py(n);
        if(t->page != page)
            continue;
        triangle(t);
        if(kind == 1)
            quad(x, y, 0, 16, F32B_HALF, 0xffff0000u);
        if(kind == 2)
            quad(x, y, 0, 32, F32B_HALF, 0xffff0000u);
        if(kind == 3)
            for(b = 0; b < 2; b++)
                quad(x, y, bands[b][0], bands[b][1], F(2.0f), 0x00ff00ffu);
        n++;
    }
    ta_list_end();
}
