/* Where does the TSP start a line of a triangle when a nearer polygon hides part of it? See gen.cpp.
   The texture is that of uvmeas: 1024x8, texel i holds the number i. Every cell has one textured triangle
   (depth 1) and up to two red bands (depth 2, nearer) over whole columns of the cell. With LIST_TR 0 all of
   it is in the opaque list with depth test and write; with LIST_TR 1 in the translucent list.

   The PVR is driven directly (../common/pvrtest2.h); main_kos.c is the earlier version on KallistiOS's
   driver, and the parameter words set here are the ones it sent (the texture is at another address). */
#define NUM_PAGES 8
#include "pvr_params.h"
#ifndef CASES_H
#define CASES_H "cases.h"       /* run2.sh uvspan@<set> gives the tables of an archived set */
#endif
#include CASES_H
#include "pvrtest2.h"

#define LIST (LIST_TR ? ListType_Translucent : ListType_Opaque)

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
    int p;
    for(p = 0; p < NUM_PAGES; p++)
        label_span(p, 0, 20, 15, "page %d", p);
}

static void test_page(int page) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 }, tsp_band;
    TCW tcw = { .full = 0 }, none = { .full = 0 };
    int i, k;

    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = LIST;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    isp.DepthMode = DepthMode_GreaterEqual;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    tsp_band = tsp;
    tsp.MipMapD = 4;
    tsp.TexU = 7;                               /* 1024 */
    tsp.TexV = 0;                               /* 8 */
    tcw.PixelFmt = PixelFmt_565;
    tcw.ScanOrder = 1;
    tcw.TexAddr = texture >> 3;

    ta_list_begin(LIST);
    pcw.Texture = isp.Texture = 1;
    for(i = 0; i < NUM_TRIS; i++) {
        if(tris[i].page != page)
            continue;
        ta_polygon(pcw, isp, tsp, tcw);
        for(k = 0; k < 3; k++)                  /* v: x, y, u, z */
            ta_vertex3(tris[i].v[k][0], tris[i].v[k][1], tris[i].v[k][3], tris[i].v[k][2], F32B_HALF, 0xffffffff, 0, k == 2);
    }
    pcw.Texture = isp.Texture = 0;
    for(i = 0; i < NUM_BANDS; i++)
        if(bands[i].page == page) {
            ta_polygon(pcw, isp, tsp_band, none);
            quad0(cell_px(bands[i].cell) + bands[i].a, cell_py(bands[i].cell), bands[i].b - bands[i].a, 32, 0x40000000 /* 2.0 */, 0xffff0000);
        }
    ta_list_end();
}
