/* Depths of blocks of exponent above 128 against each other (2026-11-20, hunt depthgaps round 2; after
   depthgaps_bigslope).

   depthgaps_bigslope: triangles near the origin with slopes of 2^123 a pixel and more (the block's exponent E =
   the larger slope's + 5: 129 .. 131) store their positive depths as floats, and every depth below zero between
   -2^127 and 0, where a block of E 128 stores the depths above its window (|v| < 2^127) as well; the background
   probes cannot tell them apart. Here two such triangles at the same pixels, the first DepthMode Always (green), the
   second compared Greater, Equal, Less (white where it passes). Triangles as depthgaps_bigslope's: V0 = (0, 0),
   V1 = (L, 0), V2 = (0, L) at z0, z1, z2. Page p: pair p / 3, mode p % 3. */
#define NUM_PAGES 27
#include "pvrtest2.h"
#include "texgen.h"

typedef struct { int l; uint32_t z[3]; } tri_t;
static const tri_t T[] = {
    { 4, { 0x00000000u, 0xfd800000u, 0x00000000u } },     /* 0: A -2^122: E 128, depths -2^121 (2i + 1) */
    { 4, { 0x00000000u, 0xfe000000u, 0x00000000u } },     /* 1: A -2^123: E 129 */
    { 4, { 0x00000000u, 0xfe600000u, 0x00000000u } },     /* 2: A -1.75 2^123: E 129 */
    { 3, { 0x00000000u, 0xfea00000u, 0x00000000u } },     /* 3: A -1.25 2^126 / 3: E 130 */
    { 2, { 0x00000000u, 0xfef80000u, 0x00000000u } },     /* 4: A -1.9375 2^125: E 131 */
    { 2, { 0x00000000u, 0xfe800000u, 0xfe800000u } },     /* 5: A B -2^125: E 131 */
    { 4, { 0x00000000u, 0xfe000000u, 0xfe000000u } },     /* 6: A B -2^123: E 129 */
    { 5, { 0x00000000u, 0xfe400000u, 0x00000000u } },     /* 7: A -1.5 2^125 / 5: E 129 */
    { 4, { 0x00000000u, 0xfdc00000u, 0x00000000u } },     /* 8: A -1.5 2^122: E 128 */
};
static const int PAIR[NUM_PAGES / 3][2] = { { 0, 1 }, { 1, 0 }, { 1, 3 }, { 3, 4 }, { 1, 2 }, { 4, 5 }, { 6, 1 }, { 7, 1 }, { 8, 0 } };
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
        label(p, 0, "triangle %d, then triangle %d %s", PAIR[p / 3][0], PAIR[p / 3][1], MN[p % 3]);
}

static void tri(const tri_t *t, int mode, uint32_t tex)
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
    ta_vertex3(f32b_int(0), f32b_int(0), t->z[0], 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, 0);
    ta_vertex3(f32b_int(t->l), f32b_int(0), t->z[1], 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, 0);
    ta_vertex3(f32b_int(0), f32b_int(t->l), t->z[2], 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, 1);
}

static void test_page(int page)
{
    pvr_frame.bg_color = 0x000040;
    pvr_frame.bg_depth = 0xff800000u;          /* -2^127: below both */
    ta_list_begin(ListType_Opaque);
    tri(&T[PAIR[page / 3][0]], DepthMode_Always, texG);
    tri(&T[PAIR[page / 3][1]], MD[page % 3], texW);
    ta_list_end();
}
