/* tsp2_zadd: how the TSP's setup adds a zero, read as the magnitude of a U / w slope (2026-11-20). See gen.cpp.

   Every case is a sliver on one row of a tile (rows 0, 2, .. 30 of every cell: 16 per cell, 4800 per page): two
   corners (x = cell + 2, cell + 10) on a line 2^-5 dy above or below the row's pixel centres, the third (x = cell +
   6.25) dy away on the other side, dy = 2^-3 (page 1: 2^-2 .. 2^-4), so that the determinant is about 1 and the x
   slope of U / w, a sum whose terms are about 2^-126, stays in range; 8 pixels drawn. 1/w is z (page 0: 2^-116 ..
   2^-112, page 1: 2^-118 .. 2^-108, a quarter below zero) at every corner. The difference shows as the slope times
   the screen's x (the plane's constant is its own sum). Read: U through the 1024 x 8 texture whose
   texel i holds the number i (RGB565: green i >> 5, blue i & 31), point sampled, repeating, decal (uvmeas).
   Results (one console, 2026-11-20): the TSP's setup adds a zero as the ISP's depth setup does (refsw3
   pvr_tsp_plane.h Setup): +0 against 2^-126 .. 1.5 2^-126 of the other sign, any other number exactly, every zero
   sum +0, in both additions of a sum: 9,600 of 9,600 cases (600 of 600 regions). The rule refsw3 had (2^-127 in a
   sum's last addition, a zero a zero in the first, -0 + -0 = -0): 1,463; -0 + -0 = -0 alone: 7,203; 2^-127 kept
   in the last addition: 6,207; no rule in the first addition: 7,253; 2^-127 in both: 3,810.
   The background is red. Translucent list, in the order sent, depth compare ALWAYS, no depth write. */
#define NUM_PAGES 2
#include "pvr_params.h"
#include "cases.h"
#include "pvrtest2.h"

static uint32_t texture;        /* 64 bit area address */

static void test_setup(void) {
    static uint32_t words[1024 * 8 / 2];        /* two 16 bit texels per word, the first in the low half */
    int i, j;

    for(j = 0; j < 8; j++)
        for(i = 0; i < 1024; i += 2)
            words[(j * 1024 + i) / 2] = (uint32_t)i | (uint32_t)(i + 1) << 16;
    texture = pvr_tex_alloc(sizeof(words));
    pvrio_w64(texture, words, sizeof(words) / 4);
}

static void test_describe(void) {
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 300; c++)
            label(p, c, "page %d, cell %d (rows 0, 2, .. 30: cases %d ..)", p, c, 16 * (300 * p + c));
}

static void test_page(int page) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    int i, k;

    pvr_frame.bg_color = 0x00ff0000;
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Translucent;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Texture = 1;
    isp.DepthMode = DepthMode_Always;
    isp.CullMode = CullMode_None;
    isp.ZWriteDis = 1;
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

    ta_list_begin(ListType_Translucent);
    for(i = 300 * 16 * page; i < 300 * 16 * (page + 1); i++) {
        const zcase_t *c = &cases[i];
        ta_polygon(pcw, isp, tsp, tcw);
        for(k = 0; k < 3; k++)
            ta_vertex3(c->x[k], c->y[k], c->z, c->u[k], 0x3f000000u, 0xffffffffu, 0, k == 2);
    }
    ta_list_end();
}
