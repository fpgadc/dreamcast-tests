/* The one depth of a small negative background, pinned to the grid (2026-11-20, hunt depthgaps round 2; after
   depthgaps_ties).

   bgneg: a background from -2^-20 to -63.75 is one depth; depthgaps_ties: it ties with the -1 of a ramp whose block
   exponent is 6 (E 6: stored as the block's M = -1024 on the grid 2^-10), the ramp's -2 .. -31 are below it.
   Here a ramp of E 6 (slope 1 a row: E = 1 + 5) with steps of 2^-10 along row 0 of tile (0, 0): pixel (i, 0) at
   -1 + (i - 16) 2^-10 (M = -1040 .. -1009), row 1 from -16 2^-10 to 15 2^-10 (between -1 and 0, then above 0): a
   quad (0, 0) (32, 0) (0, 32) (32, 32) at -(1.5 + 2^-6 + 2^-11) + 2^-10 x + y, compared with the background by
   Greater, Equal, Less (white where it passes). Page p: background B[p / 3], mode p % 3. */
#define NUM_PAGES 24
#include "pvrtest2.h"
#include "texgen.h"

static const uint32_t B[NUM_PAGES / 3] = { 0xb5800000u, 0xbf000000u, 0xbf800000u, 0xbf800001u, 0xc1fffff0u,
                                           0xc2000000u, 0xc27f0000u, 0xc27fffffu };
static const int MD[3] = { DepthMode_Greater, DepthMode_Equal, DepthMode_Less };
static const char *MN[3] = { "Greater", "Equal", "Less" };
static uint32_t texW;

static uint32_t white(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0xffff; }

static void test_setup(void)
{
    const tex_layout_t l = { TEX_TWIDDLED, 16, 8, 8, 0 };
    texW = tex_build(&l, white, NULL);
}

static void test_describe(void)
{
    int p;
    for(p = 0; p < NUM_PAGES; p++)
        label(p, 0, "ramp of E 6 against the background %08x, %s", (unsigned)B[p / 3], MN[p % 3]);
}

static void test_page(int page)
{
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    int k;
    pvr_frame.bg_color = 0x000040;
    pvr_frame.bg_depth = B[page / 3];
    ta_list_begin(ListType_Opaque);
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Opaque;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Texture = 1;
    isp.DepthMode = MD[page % 3];
    isp.CullMode = CullMode_None;
    isp.Texture = 1;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    tsp.ShadInstr = ShadInstr_Decal;
    tsp.FilterMode = FilterMode_Point;
    tsp.MipMapD = 4;
    tcw.PixelFmt = PixelFmt_565;
    tcw.TexAddr = texW >> 3;
    ta_polygon(pcw, isp, tsp, tcw);
    /* z = (-(3 2^10 + 2^5 + 1) + 2 x + 2^11 y) 2^-11 */
    for(k = 0; k < 4; k++)
        ta_vertex3(f32b_int((k & 1) * 32), f32b_int((k >> 1) * 32),
                   f32b_scaled(-(3 * 1024 + 32 + 1) + 2 * 32 * (k & 1) + 2048 * 32 * (k >> 1), -11),
                   0x3e800000u, 0x3e800000u, 0xffffffffu, 0, k == 3);
    ta_list_end();
}
