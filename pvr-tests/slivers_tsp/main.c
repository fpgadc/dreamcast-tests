/* slivers_tsp: the TSP's numbers on triangles whose determinant is a few units of the last bit, zero or of the
   wrong sign (dreamcast-tests TODO C3), read as numbers (2026-11-19). See gen.cpp.

   The triangles are covmeas's (fit/covmeas_data: slivers, almost-points and degenerate triangles); the ISP draws
   them on pixels near their edges, and the TSP's planes over them are the setup's sums divided by a determinant
   of almost nothing. Every triangle is drawn three times, on three pages, each read another way:
     mode 0  U: textured with a 1024 x 8 texture whose texel i holds the number i (RGB565: green i >> 5, blue
             i & 31), point sampled, repeating, decal (uvmeas): a pixel shows floor(U * 1024) mod 1024
     mode 1  1/w: black, table fog, every fog table pair (0, 255), fog white (wmeas): the grey level is bits 11..18
             of the mantissa of the TSP's 1/w
     mode 2  colour: Gouraud, a colour per corner
   The background is red (no texel and no fog grey has red in it). Translucent list, in the order sent, depth
   compare ALWAYS, no depth write.

   Results (one console, 2026-11-19): the slivers and almost-points are refsw3's on every pixel, U, 1/w and colour
   alike (so are covmeas's 27 sliver sets, colours exact: the 870 + 4 pixels of TODO C3 were pvr-tests/ops/model.py's).
   The degenerate triangles (determinant zero) were not: refsw3 took the third corner's depth for their 1/w, a
   console the mean of the three (42 of 3600 cells, every drawn degenerate one, in 1/w and U; colours right either
   way). pvr-tests/slivers_detzero pins the mean, slivers_dzzero a zero depth in it. */
#define NUM_PAGES 12
#include "pvr_params.h"
#ifndef CASES_H
#define CASES_H "cases.h"       /* run2.sh slivers_tsp@<set> gives the tables of another set */
#endif
#include CASES_H
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
    for(i = 0; i < 128; i++)
        pvrio_reg(FOG_TABLE_START_addr + 4 * i, 0x00ff);    /* every pair is (0, 255) */
    pvrio_reg(FOG_DENSITY_addr, 0x8000);                    /* density 1.0 */
    pvrio_reg(FOG_COL_RAM_addr, 0x00ffffff);
}

static void test_describe(void) {
    static const char *mode[3] = { "U", "1/w (fog)", "colour" };
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 300; c++)
            label(p, c, "page %d (%s), %s, cell %d", p, PAGE_WHAT[p], mode[PAGE_MODE[p]], c);
}

static void test_page(int page) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    const int mode = PAGE_MODE[page];
    int i, k;

    pvr_frame.bg_color = 0x00ff0000;
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Translucent;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    isp.DepthMode = DepthMode_Always;
    isp.CullMode = CullMode_None;
    isp.ZWriteDis = 1;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = mode == 1 ? FogCtrl_Table : FogCtrl_None;
    tsp.UseAlpha = 1;
    if(mode == 0) {
        pcw.Texture = 1;
        isp.Texture = 1;
        tsp.ShadInstr = ShadInstr_Decal;
        tsp.FilterMode = FilterMode_Point;
        tsp.MipMapD = 4;
        tsp.TexU = 7;                           /* 1024 */
        tsp.TexV = 0;                           /* 8 */
        tcw.PixelFmt = PixelFmt_565;
        tcw.ScanOrder = 1;
        tcw.TexAddr = texture >> 3;
    }
    if(mode == 2) {
        pcw.Gouraud = 1;
        isp.Gouraud = 1;
    }

    ta_list_begin(ListType_Translucent);
    for(i = 0; i < NUM_TRIS; i++) {
        if(tris[i].page != page)
            continue;
        ta_polygon(pcw, isp, tsp, tcw);
        for(k = 0; k < 3; k++) {
            if(mode == 0)
                ta_vertex3(tris[i].v[k][0], tris[i].v[k][1], tris[i].v[k][2], tris[i].v[k][3], 0x3f000000u, 0xffffffffu, 0, k == 2);
            else
                ta_vertex0(tris[i].v[k][0], tris[i].v[k][1], tris[i].v[k][2], mode == 1 ? 0xff000000u : tris[i].col[k], k == 2);
        }
    }
    ta_list_end();
}
