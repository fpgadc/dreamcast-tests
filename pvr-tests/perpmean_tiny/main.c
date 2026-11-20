/* The mean depth of a perpendicular triangle (FPU_PERP_VAL) at the bottom of the float range (2026-11-19, after
   pvr-tests/perpmean_word).

   The mean is (z1 + z2) + z3 divided by 3; a corner below 2^-126 in size is zero already (README "The TSP's numbers
   at their limits"), but the quotient of a sum of 2^-126 .. 3 2^-126 is below 2^-126. Is it zero, as the setup's
   numbers and the tile's start are below 2^-126 (zspec), or stored as something between 0 and 2^-126 (a sloped plane
   stores a float's denormals there: depthgaps_edges)? 24 corner triples (u = 2^-126: means u/3 .. 2u, cancellations,
   zeros) and the same negated. Each case is a block of 16 x 16 pixels, every pixel a perpendicular triangle of its
   own (corners (x + 1/4, y + 1/4), (x + 1, y + 1/4), (x + 1/4, y + 1): determinant 9/16, FPU_PERP_VAL 0.75) with the
   case's corner depths, textured green (decal: a depth below zero shows). Case k at cell k (positive) and 40 + k
   (negated), the block at (8, 8) in the cell.
     pages 0 .. 2   background at 0, the targets DepthMode Greater, Less, Equal
     pages 3, 4     a flat 16 x 16 quad at +2^-126 (determinant 256: exact; textured white) first, DepthMode Always,
                    then the targets Less, Equal
     pages 5, 6     the quad at -2^-120, then the targets Greater, Less
   (A first version had the second quad at -2^-126: every negative mean below 2^-126 was above it, as 0 is; the mean
   -2^-125 (case 15) was below it, which a float's order does not say: depths below zero of exponent fields 1, 2 are
   the depthgaps hunt's item, depthgaps_neggrid.) */
#define NUM_PAGES 7
#include "pvrtest2.h"
#include "texgen.h"

#define PERP_VAL 0x3f400000u            /* 0.75 */
#define U1 0x00800000u                  /* 2^-126 */
#define NC 24
static const struct { uint32_t z[3]; const char *name; } C[NC] = {
    { { U1, U1, U1 }, "(u, u, u): u" },
    { { U1, U1, 0 }, "(u, u, 0): 2u/3" },
    { { U1, 0, 0 }, "(u, 0, 0): u/3" },
    { { 0, 0, U1 }, "(0, 0, u): u/3" },
    { { 0x00c00000u, 0, 0 }, "(1.5u, 0, 0): u/2" },
    { { 0x01000000u, 0, 0 }, "(2u, 0, 0): 2u/3" },
    { { 0x01000000u, U1, 0 }, "(2u, u, 0): u" },
    { { 0x013fffffu, 0, 0 }, "(3u - ulp, 0, 0): just under u" },
    { { 0x01400000u, 0, 0 }, "(3u, 0, 0): u" },
    { { 0x01800000u, 0x80800000u, 0 }, "(4u, -u, 0): u" },
    { { 0x01800000u, 0x80c00000u, 0 }, "(4u, -1.5u, 0): 5u/6" },
    { { 0x0d800000u, 0x8d800000u, U1 }, "(2^-100, -2^-100, u): u/3" },
    { { 0x0d800000u, 0x8d800000u, 0x01400000u }, "(2^-100, -2^-100, 3u): u" },
    { { 0x00c00000u, 0x80a00000u, 0 }, "(1.5u, -1.25u, 0): first sum u/4" },
    { { 0x03800000u, 0, 0 }, "(2^-120, 0, 0): 2^-120/3" },
    { { 0x01c00000u, 0, 0 }, "(6u, 0, 0): 2u" },
    { { 0x01a00000u, 0, 0 }, "(5u, 0, 0): 5u/3" },
    { { U1, U1, 0x00c00000u }, "(u, u, 1.5u): 7u/6" },
    { { 0x01000000u, U1, U1 }, "(2u, u, u): 4u/3" },
    { { 0x00e00000u, 0x00a00000u, 0 }, "(1.75u, 1.25u, 0): u" },
    { { 0x00800001u, U1, U1 }, "(u + ulp, u, u): u + ulp" },
    { { 0, 0, 0 }, "(0, 0, 0)" },
    { { U1, 0x80800000u, 0 }, "(u, -u, 0): 0" },
    { { 0x00c00000u, 0, 0x00a00000u }, "(1.5u, 0, 1.25u): 11u/12" },
};
#define NEGP 0x83800000u                /* -2^-120 */
static const int MODE[NUM_PAGES] = { DepthMode_Greater, DepthMode_Less, DepthMode_Equal, DepthMode_Less, DepthMode_Equal,
                                     DepthMode_Greater, DepthMode_Less };
static const uint32_t PROBE[NUM_PAGES] = { 0, 0, 0, U1, U1, NEGP, NEGP };
static const char *MN[8] = { "Never", "Less", "Equal", "LessEqual", "Greater", "NotEqual", "GreaterEqual", "Always" };
static uint32_t texG, texW;

static uint32_t green(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0x07e0; }
static uint32_t white(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0xffff; }

static void test_setup(void)
{
    const tex_layout_t l = { TEX_TWIDDLED, 16, 8, 8, 0 };
    texG = tex_build(&l, green, NULL);
    texW = tex_build(&l, white, NULL);
    pvrio_reg(FPU_PERP_VAL_addr, PERP_VAL);
}

static void test_describe(void)
{
    int p, k, s;
    for(p = 0; p < NUM_PAGES; p++)
        for(s = 0; s < 2; s++)
            for(k = 0; k < NC; k++)
                label(p, 40 * s + k, "%s%s; %s %s, %s", s ? "negated " : "", C[k].name, PROBE[p] ? "after a quad at" : "background",
                      PROBE[p] == U1 ? "+2^-126" : PROBE[p] ? "-2^-120" : "0", MN[MODE[p]]);
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

static void vtx(f32b x, f32b y, uint32_t z, int end)
{
    ta_vertex3(x, y, z, 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, end);
}

static void test_page(int page)
{
    int s, k, i, j;
    pvr_frame.bg_color = 0x000040;
    pvr_frame.bg_depth = 0;
    ta_list_begin(ListType_Opaque);
    for(s = 0; s < 2; s++)
        for(k = 0; k < NC; k++) {
            const int c = 40 * s + k, X0 = cell_px(c) + 8, Y0 = cell_py(c) + 8;
            if(PROBE[page]) {
                begin(DepthMode_Always, texW);
                for(i = 0; i < 4; i++)
                    vtx(f32b_int(X0 + (i & 1) * 16), f32b_int(Y0 + (i >> 1) * 16), PROBE[page], i == 3);
            }
            begin(MODE[page], texG);
            for(j = 0; j < 16; j++)
                for(i = 0; i < 16; i++) {
                    const int X = X0 + i, Y = Y0 + j;
                    uint32_t z[3];
                    int n;
                    for(n = 0; n < 3; n++)
                        z[n] = C[k].z[n] ? C[k].z[n] ^ (s ? 0x80000000u : 0) : 0;
                    vtx(f32b_scaled(4 * X + 1, -2), f32b_scaled(4 * Y + 1, -2), z[0], 0);
                    vtx(f32b_int(X + 1), f32b_scaled(4 * Y + 1, -2), z[1], 0);
                    vtx(f32b_scaled(4 * X + 1, -2), f32b_int(Y + 1), z[2], 1);
                }
        }
    ta_list_end();
}
