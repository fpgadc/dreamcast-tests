/* Depths of 2^128 and more against each other (2026-11-20, hunt depthgaps round 3; after depthgaps_bigpos).

   depthgaps_bigpos: a triangle whose constant overflows (the tile's start -2^127) with slopes of about 2^124 a pixel
   stores the depths it steps to past 2^128 above every background probe, the largest float's word 0x7f7fffe0
   included. Here two such triangles at the same pixels (tile (0, 0), row 0 .. 2, x 26 .. 28), the first DepthMode
   Always (green), the second compared Greater, Equal, Less (white where it passes): are those depths one word, or
   ordered, and on what grid. Triangles as depthgaps_bigpos's: V0 = (X, 0), V1 = (X + L, 0), V2 = (X, L) at z0, z1,
   z2. Page p: pair p / 3, mode p % 3. */
#define NUM_PAGES 33
#include "pvrtest2.h"
#include "texgen.h"

typedef struct { int x, l; uint32_t z[3]; } tri_t;
static const tri_t T[] = {
    { 26, 4, { 0x00000000u, 0x7e780000u, 0x00000000u } },     /* 0: A 1.9375 2^123: x 26 .. 28 at 1.074, 1.135, 1.195 2^128 */
    { 26, 4, { 0x00000000u, 0x7e700000u, 0x00000000u } },     /* 1: A 1.875 2^123: 1.023, 1.082, 1.141 2^128 */
    { 26, 4, { 0x00000000u, 0x7e780000u, 0x7e780000u } },     /* 2: A B 1.9375 2^123: row 0 as 0, rows 1, 2 higher */
    { 26, 2, { 0x00000000u, 0x7effffe0u, 0x00000000u } },     /* 3: A 2^126 - 2^107: (26, 0) at 12 2^127 */
    { 26, 2, { 0x00000000u, 0x7ec00000u, 0x00000000u } },     /* 4: A 1.5 2^125: (26, 0) at 9.25 2^127 */
    { 26, 4, { 0x7f7fffffu, 0x7f7fffffu, 0x7f7fffffu } },     /* 5: flat at the largest float (0x7f7fffe0) */
    { 26, 4, { 0x7f800000u, 0x7f800000u, 0x7f800000u } },     /* 6: flat at +infinity (2^127) */
    { 26, 4, { 0x00000000u, 0x7e780001u, 0x00000000u } },     /* 7: as 0 with z1 one ulp (2^102) more: A 2^100 more */
    { 26, 4, { 0x00000000u, 0x7e781000u, 0x00000000u } },     /* 8: as 0 with z1 2^114 more: A 2^112 more */
};
static const int PAIR[NUM_PAGES / 3][2] = { { 1, 0 }, { 0, 1 }, { 0, 0 }, { 5, 0 }, { 6, 0 }, { 0, 3 }, { 3, 4 }, { 4, 3 }, { 0, 2 }, { 0, 7 }, { 0, 8 } };
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
        label(p, 0, "triangle %d (Always), then triangle %d %s", PAIR[p / 3][0], PAIR[p / 3][1], MN[p % 3]);
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
    ta_vertex3(f32b_int(t->x), f32b_int(0), t->z[0], 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, 0);
    ta_vertex3(f32b_int(t->x + t->l), f32b_int(0), t->z[1], 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, 0);
    ta_vertex3(f32b_int(t->x), f32b_int(t->l), t->z[2], 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, 1);
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
