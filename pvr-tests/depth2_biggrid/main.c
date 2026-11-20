/* The grid of depths of 2^128 and more (2026-11-20, hunt depth2; after depthgaps_bigpos, depthgaps_bigord).

   pvr_isp_depth.h GAP: a depth above zero of 2^128 or more (a start of -2^127 stepping up by about 2^123 a pixel:
   the constant overflows, regime B) is above every float word and ordered by value (depthgaps_bigord); on what grid
   was not pinned (13 to 23 significant bits fit; refsw3 takes 19, as below 2^128). Here pairs of small triangles in
   tile (0, 0), V0 = (X, Y) at 0, V1 = (X + 4, Y) at z1, V2 = (X, Y + 4) at z2: the pixels -2^127 + i A + j B with
   A = z1 / 4, B = z2 / 4, from 2^128 to about 1.7 2^129. P (DepthMode Always, green) and Q (the page's mode, white
   where it passes) differ in z1, z2 by 2^104 .. 2^112 or so: the pixels differ by about 2^104 .. 2^118. gen.cpp
   picked them so that 12 .. 26 bits, cut or rounded, or a fixed grid of 2^105 .. 2^118 give different pictures.
   Pages: set p / 3 of 26 pairs, mode p % 3 (Greater, Equal, Less). */
#define NUM_PAGES 30
#include "pvrtest2.h"
#include "texgen.h"

typedef struct { int x, y; uint32_t pz1, pz2, qz1, qz2; } pair_t;
#include "cases.h"

static const int MD[3] = { DepthMode_Greater, DepthMode_Equal, DepthMode_Less };
static const char *MN[3] = { "Greater", "Equal", "Less" };
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
        label(p, 0, "set %d: %d pairs of triangles in tile (0, 0), Q %s", p / 3, NSLOTS, MN[p % 3]);
}

static void tri(int x, int y, uint32_t z1, uint32_t z2, int mode, uint32_t tex)
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
    ta_vertex3(f32b_int(x), f32b_int(y), 0, 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, 0);
    ta_vertex3(f32b_int(x + 4), f32b_int(y), z1, 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, 0);
    ta_vertex3(f32b_int(x), f32b_int(y + 4), z2, 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, 1);
}

static void test_page(int page)
{
    const pair_t *s = PAIRS[page / 3];
    int k;
    pvr_frame.bg_color = 0x000040;
    pvr_frame.bg_depth = 0xff800000u;          /* -2^127: below every pixel */
    ta_list_begin(ListType_Opaque);
    for(k = 0; k < NSLOTS; k++) {
        tri(s[k].x, s[k].y, s[k].pz1, s[k].pz2, DepthMode_Always, texG);
        tri(s[k].x, s[k].y, s[k].qz1, s[k].qz2, MD[page % 3], texW);
    }
    ta_list_end();
}
