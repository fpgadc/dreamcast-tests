/* Bump maps: the intensity computed from a bump texel (S, R) and the light parameters in the offset color.

   256 squares (four per cell, cells 0-63): S = 17 * (n / 16), R = 17 * (n % 16), from uniform 8x8 bump
   textures, drawn with the replace environment so the result shows directly.
   Page p uses the light parameters K1, K2, K3, Q of lights[p] (offset color bytes A, R, G, B).
   cells 80-143: the same with the modulate environment and base color 0x80c0ff40. */
#define NUM_PAGES 5
#include "pvrtest2.h"

static const uint32_t lights[NUM_PAGES] = { 0x00ff0000, 0x0000ff00, 0x80408040, 0x0080ffc8, 0xff000000 };
static uint32_t txr[256];                   /* 64 bit area addresses */

static void test_setup(void) {
    uint16_t t[64];
    int n, i;

    for(n = 0; n < 256; n++) {
        for(i = 0; i < 64; i++)
            t[i] = (uint16_t)((17 * (n / 16)) << 8 | (17 * (n % 16)));
        txr[n] = pvr_tex_alloc(sizeof(t));
        pvr_tex_load(txr[n], t, sizeof(t));
    }
}

static void test_describe(void) {
    int p, c;

    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 64; c++) {
            label(p, c, "K1 K2 K3 Q = %08x, replace, S %d, R %d..", (unsigned)lights[p], 17 * (c / 4), 17 * ((c * 4) % 16));
            label(p, 80 + c, "K1 K2 K3 Q = %08x, modulate, S %d, R %d..", (unsigned)lights[p], 17 * (c / 4), 17 * ((c * 4) % 16));
        }
}

static void test_page(int page) {
    int n, m;

    ta_list_begin(ListType_Translucent);
    for(m = 0; m < 2; m++) {
        for(n = 0; n < 256; n++) {
            PCW pcw = { .full = 0 };
            ISP_TSP isp = { .full = 0 };
            TSP tsp = { .full = 0 };
            TCW tcw = { .full = 0 };
            int c = m * 80 + n / 4;

            pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
            pcw.ListType = ListType_Translucent;
            pcw.Group_En = 1;
            pcw.Strip_Len = 1;
            pcw.Gouraud = 0;                    /* flat */
            pcw.Texture = 1;
            pcw.Offset = 1;
            isp.DepthMode = DepthMode_Always;
            isp.ZWriteDis = 1;
            isp.CullMode = CullMode_None;
            isp.Texture = 1;
            isp.Offset = 1;
            tsp.SrcInstr = BlendInstr_One;
            tsp.DstInstr = BlendInstr_Zero;
            tsp.FogCtrl = FogCtrl_None;
            tsp.UseAlpha = 1;
            tsp.ShadInstr = m ? ShadInstr_Modulate : ShadInstr_Decal;
            tsp.FilterMode = FilterMode_Point;
            tsp.MipMapD = 4;
            tsp.TexU = 0;                       /* 8 texels */
            tsp.TexV = 0;
            tcw.PixelFmt = PixelFmt_BumpMap;
            tcw.TexAddr = txr[n] >> 3;
            ta_polygon(pcw, isp, tsp, tcw);
            quad3(cell_px(c) + (n & 1) * 16, cell_py(c) + ((n >> 1) & 1) * 16, 16, 16, F32B_ONE,
                  F32B_ZERO, F32B_ZERO, F32B_ONE, F32B_ONE, m ? 0x80c0ff40 : 0xffffffff, lights[page]);
        }
    }
    ta_list_end();
}
