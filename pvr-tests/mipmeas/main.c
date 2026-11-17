/* The mipmap level and the trilinear fraction, read as numbers. See gen.py.

   The texture is 64x64, RGB565, mipmapped, and every level is one colour:
       64x64 red, 32x32 green, 16x16 blue, 8x8 yellow, 4x4 cyan, 2x2 magenta, 1x1 white
   Every cell is a 32x32 quad of it (or a right triangle at its top left corner), point of view straight on, with the texture coordinates of cases.h; each
   polygon is drawn with ONE trilinear pass (FilterMode 2 or 3) and replaces what is there, so a pixel shows
   one level's colour times the pass's blend coefficient. */
#define NUM_PAGES 8
#include "pvr_params.h"
#ifndef CASES_H
#define CASES_H "cases.h"       /* run2.sh mipmeas@<set> gives the tables of an archived set */
#endif
#include CASES_H
#include "pvrtest2.h"

static uint32_t texture;        /* 64 bit area address */

static void test_setup(void) {
    /* levels 1x1 .. 64x64, each a flat colour; the 1x1 level is the fourth 16 bit word */
    static const uint16_t level_color[7] = { 0xffff, 0xf81f, 0x07ff, 0xffe0, 0x001f, 0x07e0, 0xf800 };
    static uint16_t mip[3 + 5461];
    int i, k, n = 3, size = 1;

    for(k = 0; k < 7; k++, size *= 4)
        for(i = 0; i < size; i++)
            mip[n++] = level_color[k];
    texture = pvr_tex_alloc(sizeof(mip));
    pvr_tex_load(texture, mip, sizeof(mip));
}

static void test_describe(void) {
    int p;
    for(p = 0; p < NUM_PAGES; p++)
        label_span(p, 0, 20, 15, "page %d", p);
}

static void test_page(int page) {
    int i, k;

    ta_list_begin(ListType_Translucent);
    for(i = 0; i < NUM_QUADS; i++) {
        const quad_t *q = &quads[i];
        PCW pcw = { .full = 0 };
        ISP_TSP isp = { .full = 0 };
        TSP tsp = { .full = 0 };
        TCW tcw = { .full = 0 };
        int x = cell_px(q->cell), y = cell_py(q->cell);

        if(q->page != page)
            continue;
        pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
        pcw.ListType = ListType_Translucent;
        pcw.Group_En = 1;
        pcw.Strip_Len = 1;
        pcw.Texture = 1;
        isp.DepthMode = DepthMode_Always;
        isp.ZWriteDis = 1;
        isp.Texture = 1;
        isp.DCalcCtrl = q->dcalc;               /* where the level's numerators are taken */
        tsp.SrcInstr = BlendInstr_One;
        tsp.DstInstr = BlendInstr_Zero;
        tsp.FogCtrl = FogCtrl_None;
        tsp.UseAlpha = 1;
        tsp.IgnoreTexA = 1;
        tsp.ShadInstr = ShadInstr_Decal;
        tsp.FilterMode = q->mode;               /* 2: trilinear pass A, 3: pass B */
        tsp.MipMapD = q->bias;                  /* in quarters */
        tsp.TexU = 3;                           /* 64 */
        tsp.TexV = 3;
        tcw.PixelFmt = PixelFmt_565;
        tcw.MipMapped = 1;
        tcw.TexAddr = texture >> 3;
        ta_polygon(pcw, isp, tsp, tcw);
        if(q->tri[1][0] | q->tri[1][1] | q->tri[2][0] | q->tri[2][1])      /* a triangle */
            for(k = 0; k < 3; k++)
                ta_vertex3(f32b_int(x + q->tri[k][0]), f32b_int(y + q->tri[k][1]), q->z[k], q->uv[k][0], q->uv[k][1], 0xffffffff, 0, k == 2);
        else
            for(k = 0; k < 4; k++)
                ta_vertex3(f32b_int(x + (k & 1) * 32), f32b_int(y + (k >> 1) * 32), q->z[k], q->uv[k][0], q->uv[k][1], 0xffffffff, 0, k == 3);
    }
    ta_list_end();
}
