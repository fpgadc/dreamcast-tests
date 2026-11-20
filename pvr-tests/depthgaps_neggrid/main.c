/* The grid of a stored negative depth (2026-11-19, hunt depthgaps (c); after depthgaps_negpp).

   depthgaps_negpp: flat polygons at negative depths one and two steps of the 19 bit grid apart (0x20 in the word)
   tie, positive ones do not; the ties fit a magnitude rounded up to a multiple of 0x100 in the word. Here the grid
   is measured with zexfil's probe triangles (common/zexfil_probes.h: one per pixel of tile (0, 0), whose depth the
   setup computes exactly for any depth of 19 significant bits): pixel p = 25 w + k (p < 1000): the probe at
   A = W[w] (DepthMode Always, textured green), then the same at B = A + 32 (k - 12) (textured white) with DepthMode
   Equal (page 0), Greater (page 1), Less (page 2). White where B passes. (Depths of exponent field 1 are left
   out: the probes' products fall below 2^-126 there and the setup's arithmetic, not the store, decides.) */
#define NUM_PAGES 3
#include "pvrtest2.h"
#include "texgen.h"
#include "zexfil_probes.h"

#define NW 40
static const uint32_t W[NW] = {
    0xbf800000u, 0xbf800020u, 0xbf800080u, 0xbf8000e0u, 0xbf800100u, 0xbf812340u, 0xbf812300u, 0xbf812400u,
    0xbf8123e0u, 0xbf812800u, 0xbf813000u, 0xbf8fffe0u, 0xbfffff00u, 0xbfffffe0u, 0xbfc00000u, 0xbfc00200u,
    0xc2c80000u, 0xc2c80100u, 0xc2c80180u, 0xc9812340u, 0xc9ffff00u, 0xf1812340u, 0xf1800400u, 0xa0812340u,
    0xa0800400u, 0x81012340u, 0x82800100u, 0x8a812340u, 0xb5812340u, 0xb5800400u, 0xd0812340u, 0xd0800800u,
    0x3f800000u, 0x3f812340u, 0x3f800100u, 0x42c80100u, 0xbf800800u, 0xbf801000u, 0xbf802000u, 0xbf804000u,
};
static const char *MODE[3] = { "Equal", "Greater", "Less" };
static uint32_t texG, texW;

static uint32_t green(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0x07e0; }
static uint32_t white(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0xffff; }

static void test_setup(void)
{
    const tex_layout_t l = { TEX_TWIDDLED, 16, 8, 8, 0 };
    texG = tex_build(&l, green, NULL);
    texW = tex_build(&l, white, NULL);
}

static void test_describe(void)
{
    int p;
    for(p = 0; p < NUM_PAGES; p++)
        label(p, 0, "tile (0, 0): pixel 25 w + k: probe at W[w] Always, then at W[w] + 32 (k - 12) %s", MODE[p]);
}

static void begin(int mode, uint32_t tex)
{
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Opaque;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Texture = 1;
    isp.DepthMode = mode;
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
    tcw.TexAddr = tex >> 3;
    ta_polygon(pcw, isp, tsp, tcw);
}

static void probe(int i, uint32_t z)
{
    int k;
    for(k = 0; k < 3; k++)
        ta_vertex3(zexfil_probe[i].x[k], zexfil_probe[i].y[k], z, 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, k == 2);
}

static void test_page(int page)
{
    int i;
    pvr_frame.bg_color = 0x000040;
    pvr_frame.bg_depth = 0;
    ta_list_begin(ListType_Opaque);
    for(i = 0; i < 1024; i++) {
        const int p = zexfil_probe[i].pixel, w = p / 25, k = p % 25;
        if(p >= 25 * NW)
            continue;
        begin(DepthMode_Always, texG);
        probe(i, W[w]);
        begin(page == 0 ? DepthMode_Equal : page == 1 ? DepthMode_Greater : DepthMode_Less, texW);
        probe(i, W[w] + (uint32_t)(32 * (k - 12)));
    }
    ta_list_end();
}
