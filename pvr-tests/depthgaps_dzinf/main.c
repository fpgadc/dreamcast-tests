/* The mean depth of a triangle of determinant zero, or a perpendicular one, with an infinite or NaN corner
   (2026-11-20, hunt depthgaps round 3; after depthgaps_detneg).

   The ISP's depth of a triangle of determinant zero is the mean of its corners ((z1 + z2) + z3 times 1/3,
   perpmean_word). With an infinity or a NaN among them refsw3 had the first corner's depth as given (a GAP:
   detinf's flat probe quads at +infinity do not tell it from +-2^127), depthgaps_detneg put five such means above
   2^118 or between -2^118 and 0. Here 31 corner triples (C[r]), each in row r of tile (0, 0) as three corners at
   one point (16.25, r + 1/4): determinant 0, the ISP draws the whole row (depthgaps_detneg), and in tile (1, 0) as
   one pixel's perpendicular triangle (corners (48 + 1/4, r + 1/4), (49, r + 1/4), (48 + 1/4, r + 1): determinant
   9/16, FPU_PERP_VAL 0.75). Textured green (decal), drawn with DepthMode Equal, Greater, Less against a background
   at B[page / 3]: 2^127, -2^127, 1.5 2^127, -1.5 2^126 (the background's words are exact there: bgbig, bgspec,
   depthgaps_bglow). Tile (2, 0): 31 more triples (C2[r]: NaNs against infinities of the other sign) at one point
   (80.25, r + 1/4); tile (3, 0): C3[r], NaNs of other fractions against each other, at (112.25, r + 1/4). */
#define NUM_PAGES 12
#include "pvrtest2.h"
#include "texgen.h"

#define PERP_VAL 0x3f400000u            /* 0.75 */
#define PI 0x7f800000u
#define NI 0xff800000u
#define QN 0x7fc00000u
#define ONE 0x3f800000u
#define NC 31
static const struct { uint32_t z[3]; const char *name; } C[NC] = {
    { { PI, ONE, ONE }, "+inf, 1, 1" },
    { { ONE, PI, ONE }, "1, +inf, 1" },
    { { ONE, ONE, PI }, "1, 1, +inf" },
    { { NI, ONE, ONE }, "-inf, 1, 1" },
    { { ONE, ONE, NI }, "1, 1, -inf" },
    { { QN, ONE, ONE }, "NaN 7fc00000, 1, 1" },
    { { 0xffc00000u, ONE, ONE }, "NaN ffc00000, 1, 1" },
    { { 0x7f800001u, ONE, ONE }, "NaN 7f800001, 1, 1" },
    { { PI, NI, ONE }, "+inf, -inf, 1" },
    { { NI, PI, ONE }, "-inf, +inf, 1" },
    { { PI, ONE, NI }, "+inf, 1, -inf" },
    { { ONE, PI, NI }, "1, +inf, -inf" },
    { { QN, NI, ONE }, "NaN 7fc00000, -inf, 1" },
    { { 0x7f000000u, 0x7f000000u, 0 }, "2^127, 2^127, 0" },
    { { 0xff7fffffu, 0xff7fffffu, ONE }, "-max, -max, 1" },
    { { PI, PI, PI }, "+inf, +inf, +inf" },
    { { NI, NI, NI }, "-inf, -inf, -inf" },
    { { QN, QN, QN }, "NaN x3" },
    { { 0xffffffffu, ONE, ONE }, "NaN ffffffff, 1, 1" },
    { { PI, 0, 0 }, "+inf, 0, 0" },
    { { 0, 0, NI }, "0, 0, -inf" },
    { { 0xc2c80000u, 0, PI }, "-100, 0, +inf" },
    { { 0x42c80000u, 0xffc00000u, 0 }, "100, NaN ffc00000, 0" },
    { { PI, 0xffc00000u, ONE }, "+inf, NaN ffc00000, 1" },
    { { 0x7149f2cau, 0x7149f2cau, PI }, "1e30, 1e30, +inf" },
    { { 0x7f000000u, 0x7f000000u, 0xff000000u }, "2^127, 2^127, -2^127" },
    { { 0xbf800000u, 0xbf800000u, 0xbf800000u }, "-1, -1, -1" },
    { { ONE, ONE, ONE }, "1, 1, 1" },
    { { 0x7f7fffffu, 0, 0 }, "max, 0, 0" },
    { { 0x7f7fffffu, 0x7f7fffffu, 0x7f7fffffu }, "max x3" },
    { { NI, QN, 0 }, "-inf, NaN 7fc00000, 0" },
};
static const struct { uint32_t z[3]; const char *name; } C2[NC] = {
    { { QN, ONE, NI }, "NaN 7fc00000, 1, -inf" },
    { { ONE, QN, NI }, "1, NaN 7fc00000, -inf" },
    { { 0xffc00000u, ONE, PI }, "NaN ffc00000, 1, +inf" },
    { { QN, 0xffc00000u, ONE }, "NaN 7fc00000, NaN ffc00000, 1" },
    { { 0xffc00000u, QN, ONE }, "NaN ffc00000, NaN 7fc00000, 1" },
    { { PI, NI, 0xffc00000u }, "+inf, -inf, NaN ffc00000" },
    { { NI, PI, QN }, "-inf, +inf, NaN 7fc00000" },
    { { QN, PI, NI }, "NaN 7fc00000, +inf, -inf" },
    { { 0xffc00000u, NI, PI }, "NaN ffc00000, -inf, +inf" },
    { { 0x7f800001u, NI, ONE }, "NaN 7f800001, -inf, 1" },
    { { 0xff800001u, PI, ONE }, "NaN ff800001, +inf, 1" },
    { { NI, 0x7fffffffu, ONE }, "-inf, NaN 7fffffff, 1" },
    { { PI, 0xff800001u, ONE }, "+inf, NaN ff800001, 1" },
    { { QN, NI, NI }, "NaN 7fc00000, -inf, -inf" },
    { { 0xffc00000u, PI, PI }, "NaN ffc00000, +inf, +inf" },
    { { NI, ONE, QN }, "-inf, 1, NaN 7fc00000" },
    { { PI, ONE, 0xffc00000u }, "+inf, 1, NaN ffc00000" },
    { { QN, 0xffc00000u, NI }, "NaN 7fc00000, NaN ffc00000, -inf" },
    { { QN, 0xffc00000u, PI }, "NaN 7fc00000, NaN ffc00000, +inf" },
    { { 0xffc00000u, QN, NI }, "NaN ffc00000, NaN 7fc00000, -inf" },
    { { 0x7f7fffffu, 0x7f7fffffu, NI }, "max, max, -inf" },
    { { 0x7f7fffffu, 0x7f7fffffu, QN }, "max, max, NaN 7fc00000" },
    { { 0xff7fffffu, 0xff7fffffu, QN }, "-max, -max, NaN 7fc00000" },
    { { 0x7f7fffffu, 0x7f7fffffu, 0xffc00000u }, "max, max, NaN ffc00000" },
    { { QN, QN, NI }, "NaN 7fc00000 x2, -inf" },
    { { 0xffc00000u, 0xffc00000u, PI }, "NaN ffc00000 x2, +inf" },
    { { ONE, NI, QN }, "1, -inf, NaN 7fc00000" },
    { { 0x7fbfffffu, NI, ONE }, "NaN 7fbfffff, -inf, 1" },
    { { 0xffffffffu, PI, 0 }, "NaN ffffffff, +inf, 0" },
    { { PI, NI, QN }, "+inf, -inf, NaN 7fc00000" },
    { { NI, PI, 0xffc00000u }, "-inf, +inf, NaN ffc00000" },
};
#define NC3 12
static const struct { uint32_t z[3]; const char *name; } C3[NC3] = {
    { { QN, 0xff800001u, ONE }, "NaN 7fc00000, NaN ff800001, 1" },
    { { 0xff800001u, QN, ONE }, "NaN ff800001, NaN 7fc00000, 1" },
    { { 0xffc00000u, 0x7f800001u, ONE }, "NaN ffc00000, NaN 7f800001, 1" },
    { { 0x7fffffffu, 0xffc00000u, ONE }, "NaN 7fffffff, NaN ffc00000, 1" },
    { { 0xffffffffu, QN, ONE }, "NaN ffffffff, NaN 7fc00000, 1" },
    { { 0x7f800001u, 0xff800001u, ONE }, "NaN 7f800001, NaN ff800001, 1" },
    { { 0x7f800002u, 0xff800001u, ONE }, "NaN 7f800002, NaN ff800001, 1" },
    { { 0xff800002u, 0x7f800001u, 0xbf800000u }, "NaN ff800002, NaN 7f800001, -1" },
    { { QN, 0xffc00001u, ONE }, "NaN 7fc00000, NaN ffc00001, 1" },
    { { 0x7f800001u, 0xffffffffu, ONE }, "NaN 7f800001, NaN ffffffff, 1" },
    { { QN, NI, 0xffc00000u }, "NaN 7fc00000, -inf, NaN ffc00000" },
    { { PI, 0xffc00000u, QN }, "+inf, NaN ffc00000, NaN 7fc00000" },
};
static const uint32_t B[4] = { 0x7f000000u, 0xff000000u, 0x7f400000u, 0xfec00000u };
static const char *BN[4] = { "2^127", "-2^127", "1.5 2^127", "-1.5 2^126" };
static const int MODE[3] = { DepthMode_Equal, DepthMode_Greater, DepthMode_Less };
static const char *MN[3] = { "Equal", "Greater", "Less" };
static uint32_t texG;

static uint32_t green(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0x07e0; }

static void test_setup(void)
{
    const tex_layout_t l = { TEX_TWIDDLED, 16, 8, 8, 0 };
    texG = tex_build(&l, green, NULL);
    pvrio_reg(FPU_PERP_VAL_addr, PERP_VAL);
}

static void test_describe(void)
{
    int p;
    for(p = 0; p < NUM_PAGES; p++) {
        label(p, 0, "rows 0 .. 30: C[r] at one point (determinant 0), background %s, %s", BN[p / 3], MN[p % 3]);
        label(p, 1, "rows 0 .. 30: C[r] as one perpendicular triangle at x 48, background %s, %s", BN[p / 3], MN[p % 3]);
        label(p, 2, "rows 0 .. 30: C2[r] at one point (determinant 0), background %s, %s", BN[p / 3], MN[p % 3]);
        label(p, 3, "rows 0 .. 11: C3[r] at one point (determinant 0), background %s, %s", BN[p / 3], MN[p % 3]);
    }
}

static void begin(int mode)
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
    tcw.TexAddr = texG >> 3;
    ta_polygon(pcw, isp, tsp, tcw);
}

static void vtx(f32b x, f32b y, uint32_t z, int end)
{
    ta_vertex3(x, y, z, 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, end);
}

static void test_page(int page)
{
    int r, k;
    pvr_frame.bg_color = 0x000040;
    pvr_frame.bg_depth = B[page / 3];
    ta_list_begin(ListType_Opaque);
    for(r = 0; r < NC; r++) {
        begin(MODE[page % 3]);
        for(k = 0; k < 3; k++)
            vtx(f32b_scaled(65, -2), f32b_scaled(4 * r + 1, -2), C[r].z[k], k == 2);
        begin(MODE[page % 3]);
        vtx(f32b_scaled(193, -2), f32b_scaled(4 * r + 1, -2), C[r].z[0], 0);
        vtx(f32b_int(49), f32b_scaled(4 * r + 1, -2), C[r].z[1], 0);
        vtx(f32b_scaled(193, -2), f32b_int(r + 1), C[r].z[2], 1);
        begin(MODE[page % 3]);
        for(k = 0; k < 3; k++)
            vtx(f32b_scaled(321, -2), f32b_scaled(4 * r + 1, -2), C2[r].z[k], k == 2);
        if(r < NC3) {
            begin(MODE[page % 3]);
            for(k = 0; k < 3; k++)
                vtx(f32b_scaled(449, -2), f32b_scaled(4 * r + 1, -2), C3[r].z[k], k == 2);
        }
    }
    ta_list_end();
}
