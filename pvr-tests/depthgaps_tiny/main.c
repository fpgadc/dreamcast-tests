/* Depths at the bottom of the float range (2026-11-19, hunt depthgaps (d); after depthgaps_neggrid, depthgaps_edges).

   depthgaps_neggrid: zexfil's probes at negative depths of exponent field 1 tie with each other over 12 steps and
   with a probe at depth 0 (a field 0 corner is read as zero); field 2 is ordered on a grid of 2^-138.
   depthgaps_edges: a ramp stepping below 2^-126 above 0 stores depths between 0 and 2^-126, in order. Here flat
   32 x 32 quads (exact: products of the depth with multiples of 1024) at small depths, both signs, against the
   background (page: 0 Equal, 0 Greater, 0 Less, 2^-126 Equal (a background below 2^-20 is 0: bgdepth)).
   Row 0, cell k: flat quad at Q[k]. */
#define NUM_PAGES 4
#include "pvrtest2.h"
#include "texgen.h"

#define NQ 12
static const uint32_t Q[NQ] = { 0x00800000u, 0x00c00000u, 0x00800100u, 0x01000000u, 0x01400000u, 0x01800000u,
                                0x80800000u, 0x80c00000u, 0x80800100u, 0x81000000u, 0x81400000u, 0x81800000u };
static const uint32_t B[NUM_PAGES] = { 0, 0, 0, 0x00800000u };
static const int MD[NUM_PAGES] = { DepthMode_Equal, DepthMode_Greater, DepthMode_Less, DepthMode_Equal };
static const char *MN[8] = { "Never", "Less", "Equal", "LessEqual", "Greater", "NotEqual", "GreaterEqual", "Always" };
static uint32_t texG;

static uint32_t green(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0x07e0; }

static void test_setup(void)
{
    const tex_layout_t l = { TEX_TWIDDLED, 16, 8, 8, 0 };
    texG = tex_build(&l, green, NULL);
}

static void test_describe(void)
{
    int p, k;
    for(p = 0; p < NUM_PAGES; p++) {
        for(k = 0; k < NQ; k++)
            label(p, k, "flat %08x; background %08x, %s", (unsigned)Q[k], (unsigned)B[p], MN[MD[p]]);
    }
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

static void vtx(int x, int y, uint32_t z, int end)
{
    ta_vertex3(f32b_int(x), f32b_int(y), z, 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, end);
}

static void test_page(int page)
{
    int k;
    pvr_frame.bg_color = 0x000040;
    pvr_frame.bg_depth = B[page];
    ta_list_begin(ListType_Opaque);
    for(k = 0; k < NQ; k++) {
        int j;
        begin(MD[page], texG);
        for(j = 0; j < 4; j++)
            vtx(cell_px(k) + (j & 1) * 32, cell_py(k) + (j >> 1) * 32, Q[k], j == 3);
    }
    ta_list_end();
}
