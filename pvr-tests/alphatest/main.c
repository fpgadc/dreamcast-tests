/* Punch-through alpha test: which alpha values pass for a given reference value.

   The background is a red opaque quad. In the punch-through list, 256 green squares (four per cell) have
   alpha 0..255. Page p uses the reference value refs[p] (register 0x11c).
       cells 0-63     textured: the alpha comes from an 8 bpp texture with an ARGB8888 palette
       cells 80-143   untextured: the alpha is the vertex alpha
       cells 160-223  textured, modulate alpha env: texel alpha 255 x vertex alpha */
#define NUM_PAGES 5
#include "pvrtest2.h"

static const int refs[NUM_PAGES] = { 0, 1, 127, 128, 255 };
static uint32_t txr[256];                   /* 64 bit area addresses */

static void test_setup(void) {
    uint8_t t[64];
    int i;

    pvr_pal_format(PAL_RAM_CTRL_ARGB8888);
    for(i = 0; i < 256; i++) {
        pvr_pal_entry(i, ((uint32_t)i << 24) | 0x00ff00);
        memset(t, i, sizeof(t));
        txr[i] = pvr_tex_alloc(sizeof(t));
        pvr_tex_load(txr[i], t, sizeof(t));
    }
}

static void test_describe(void) {
    int p, c;

    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 64; c++) {
            label(p, c, "ref %d, texture alpha %d..%d", refs[p], c * 4, c * 4 + 3);
            label(p, 80 + c, "ref %d, vertex alpha %d..%d", refs[p], c * 4, c * 4 + 3);
            label(p, 160 + c, "ref %d, texel 255 x vertex alpha %d..%d", refs[p], c * 4, c * 4 + 3);
        }
}

/* a 16x16 square of the punch-through list; env: ShadInstr_Decal (replace) or ShadInstr_ModulateAlpha; at == 0: untextured */
static void square(int x, int y, uint32_t at, int ShadInstr, uint32_t BaseCol) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };

    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Punch_Through;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Gouraud = 1;
    pcw.Texture = at != 0;
    isp.DepthMode = DepthMode_Always;
    isp.ZWriteDis = 1;
    isp.CullMode = CullMode_None;
    isp.Texture = at != 0;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    if(at) {
        tsp.ShadInstr = ShadInstr;
        tsp.FilterMode = FilterMode_Point;
        tsp.MipMapD = 4;
        tsp.TexU = 0;                           /* 8 texels */
        tsp.TexV = 0;
        tcw.PixelFmt = PixelFmt_Pal8;
        tcw.PalSelect = 0;
        tcw.TexAddr = at >> 3;
    }
    ta_polygon(pcw, isp, tsp, tcw);
    if(at)
        quad3(x, y, 16, 16, F32B_ONE, F32B_ZERO, F32B_ZERO, F32B_ONE, F32B_ONE, BaseCol, 0);
    else
        quad0(x, y, 16, 16, F32B_ONE, BaseCol);
}

static void test_page(int page) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    int n;

    pvrio_reg(PT_ALPHA_REF_addr, refs[page]);
    ta_list_begin(ListType_Opaque);
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Opaque;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Gouraud = 1;
    isp.DepthMode = DepthMode_Always;
    isp.ZWriteDis = 1;
    isp.CullMode = CullMode_None;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    ta_polygon(pcw, isp, tsp, tcw);
    quad0(0, 0, 640, 480, F32B_HALF, 0xffff0000);
    ta_list_end();

    ta_list_begin(ListType_Punch_Through);
    for(n = 0; n < 256; n++) {
        int qx = (n & 1) * 16, qy = ((n >> 1) & 1) * 16;
        square(cell_px(n / 4) + qx, cell_py(n / 4) + qy, txr[n], ShadInstr_Decal, 0xffffffff);
        square(cell_px(80 + n / 4) + qx, cell_py(80 + n / 4) + qy, 0, 0, ((uint32_t)n << 24) | 0x00ff00);
        square(cell_px(160 + n / 4) + qx, cell_py(160 + n / 4) + qy, txr[255], ShadInstr_ModulateAlpha, ((uint32_t)n << 24) | 0xffffff);
    }
    ta_list_end();
}
