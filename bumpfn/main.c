/* The bump map function: the intensity the console makes of a bump texel (S, R) and the light K1, K2, K3, Q
   in the offset colour (bytes A, R, G, B).

   A 256x256 bump map holds every texel value: S = y, R = x. It is drawn one texel per pixel, point sampled,
   through (SRC_ALPHA, ZERO) with a white base colour, which shows the intensity (a bump texel's result is
   its alpha; its colour is white). Two lights per page: light 2p at (0, 0), light 2p + 1 at (256, 0).
   Under the first, a strip (0, 256) 256x32 of a palette texture whose texel x has alpha x: what the same
   blend makes of every alpha, to read the intensities back from the pictures. */
#define NUM_LIGHTS 14
#define NUM_PAGES (NUM_LIGHTS / 2)
#include "pvrtest2.h"
#include "texgen.h"

static const uint32_t lights[NUM_LIGHTS] = {
    0x00ff0000, 0x0000ff00, 0x80000000, 0x00800000, 0x00008000, 0x0000ff55,
    0x40605020, 0x00110000, 0x00001100, 0xffffff00, 0x80408040, 0x00ffff80,
    0x10ff0000, 0x1000ff00,         /* K1 above 0: the smallest table entries show */
};
static uint32_t tex_bump, tex_alpha;

static uint32_t texel(const void *c, int size, int x, int y) {
    (void)size;
    return c ? (uint32_t)x : (uint32_t)((y << 8) | x);
}

static void test_setup(void) {
    static const tex_layout_t lb = { TEX_TWIDDLED, 16, 256, 256, 0 }, la = { TEX_TWIDDLED, 8, 256, 256, 0 };
    int i;

    pvr_pal_format(PAL_RAM_CTRL_ARGB8888);
    for(i = 0; i < 256; i++)
        pvr_pal_entry(i, ((uint32_t)i << 24) | 0xffffffu);
    tex_bump = tex_build(&lb, texel, NULL);
    tex_alpha = tex_build(&la, texel, &la);
}

static void test_describe(void) {
    int p, k, c;
    for(p = 0; p < NUM_PAGES; p++) {
        for(k = 0; k < 2; k++)
            for(c = 0; c < 64; c++)
                label(p, (c / 8) * COLS + k * 8 + c % 8, "K1 K2 K3 Q = %08x: S %d.., R %d..", (unsigned)lights[2 * p + k], (c / 8) * 32, (c % 8) * 32);
        for(c = 0; c < 8; c++)
            label(p, 8 * COLS + c, "alpha %d.. through the same blend", c * 32);
    }
}

static void textured(int PixelFmt, uint32_t txr, int offset) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };

    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Translucent;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Texture = 1;
    pcw.Gouraud = 1;
    pcw.Offset = offset;
    isp.DepthMode = DepthMode_Always;
    isp.ZWriteDis = 1;
    isp.Texture = 1;
    isp.Offset = offset;
    tsp.SrcInstr = BlendInstr_SrcAlpha;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    tsp.ShadInstr = ShadInstr_Decal;
    tsp.FilterMode = FilterMode_Point;
    tsp.MipMapD = 4;
    tsp.TexU = 5;                               /* 256 texels */
    tsp.TexV = 5;
    tcw.PixelFmt = PixelFmt;
    tcw.TexAddr = txr >> 3;
    ta_polygon(pcw, isp, tsp, tcw);
}

static void test_page(int page) {
    int k;

    ta_list_begin(ListType_Translucent);
    for(k = 0; k < 2; k++) {
        textured(PixelFmt_BumpMap, tex_bump, 1);
        quad3(k * 256, 0, 256, 256, F32B_ONE, F32B_ZERO, F32B_ZERO, F32B_ONE, F32B_ONE, 0xffffffff, lights[2 * page + k]);
    }
    textured(PixelFmt_Pal8, tex_alpha, 0);
    quad3(0, 256, 256, 32, F32B_ONE, F32B_ZERO, F32B_ZERO, F32B_ONE, F(0.125f), 0xffffffff, 0);
    ta_list_end();
}
