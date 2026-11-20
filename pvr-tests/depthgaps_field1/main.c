/* Flat probes at depths of exponent fields 0 .. 5, mostly field 1 (2026-11-20, hunt depthgaps round 2; after
   depthgaps_neggrid, depthgaps_tiny).

   depthgaps_neggrid left out the depths of exponent field 1: there zexfil's probe triangles (common/zexfil_probes.h,
   flat, exact for any depth of 19 bits from field 3 up) have products z * (y1 - y2) and z * e below 2^-126, which the
   setup takes as zero, and on a console they tie with each other and with a probe at 0, where refsw3 had about 20
   of them otherwise. Here every probe of tile (0, 0): pixel p = 25 w + k (p < 1000), the probe at
   B = W[w] + 32 (k - 12): pages 0 .. 2 against the background at 0 (Equal, Greater, Less); pages 3 .. 5 drawn over
   the probe at A = W[w] (DepthMode Always, textured green) with DepthMode Equal, Greater, Less. White where B
   passes. */
#define NUM_PAGES 6
#include "pvrtest2.h"
#include "texgen.h"
#include "zexfil_probes.h"

#define NW 40
static const uint32_t W[NW] = {
    0x80812340u, 0x80800000u, 0x80c00000u, 0x80fffe00u, 0x80a00000u, 0x80800100u, 0x80e00000u, 0x80900000u,
    0x80ffffe0u, 0x80b00000u, 0x80d55540u, 0x80f00000u, 0x80812300u, 0x80812400u, 0x80888880u, 0x80f0f0e0u,
    0x80c0c0c0u, 0x80ffff00u, 0x80800020u, 0x80fe0000u, 0x00812340u, 0x00800000u, 0x00c00000u, 0x00fffe00u,
    0x00a00000u, 0x00e00000u, 0x81012340u, 0x81000000u, 0x81400000u, 0x817fff00u, 0x01012340u, 0x01400000u,
    0x81812340u, 0x81c00000u, 0x80400000u, 0x80000020u, 0x00400000u, 0x82012340u, 0x82812340u, 0x80820000u,
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
        label(p, 0, p < 3 ? "tile (0, 0): pixel 25 w + k: probe at W[w] + 32 (k - 12) against the background 0, %s" : "tile (0, 0): pixel 25 w + k: probe at W[w] Always, then at W[w] + 32 (k - 12) %s", MODE[p % 3]);
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
        if(page >= 3) {
            begin(DepthMode_Always, texG);
            probe(i, W[w]);
        }
        begin(page % 3 == 0 ? DepthMode_Equal : page % 3 == 1 ? DepthMode_Greater : DepthMode_Less, texW);
        probe(i, W[w] + (uint32_t)(32 * (k - 12)));
    }
    ta_list_end();
}
