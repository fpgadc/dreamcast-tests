/* The TSP's 1/w of a perpendicular triangle (FPU_PERP_VAL), read to 16 bits through a texture coordinate (2026-11-19,
   after pvr-tests/perpmean_fogw; uvmeas's method).

   perpmean_fogw: a perpendicular triangle's 1/w plane is flat at the mean of the corners' 1/w, (w1 + w2) + w3
   divided by 3 (12 bits of it, through the fog table), and an attribute plane is the third corner's attribute times
   that, so that the attribute comes out as the third corner's: U = (u3 X) / X, X the mean. That quotient depends on X
   to the last bit of the 16 bit planes (the reciprocal table, the cut products: README uvmeas), so a U of 64 .. 128
   read to half a unit of its 16th bit says which X it was. The texture is uvmeas's: 1024 x 8 texels, RGB565, texel i
   holds the number i (green = i >> 5, blue = i & 31), point sampled, repeating, decal: a pixel shows floor(1024 U)
   mod 1024. Every pixel of 8 tiles has a perpendicular triangle of its own (corners (x + 1/4, y + 1/4), (x + 1, y +
   1/4), (x + 1/4, y + 1): determinant 9/16, FPU_PERP_VAL 0.75), opaque, DepthMode Always; tiles 4 .. 7 the same
   triangles mirrored (the other winding). Corner 1/w (rng() below):
     tiles 0, 4   three of 1 .. 128          tiles 1, 5   three in one binade
     tiles 2, 6   one of them 12 .. 30 binades below the others (where the adder drops it)
     tiles 3, 7   three of 2^-20 .. 2^20
   u: all three corners 64 .. 128 (any 24 bit fraction), v 0.5. */
#define NUM_PAGES 1
#include "pvrtest2.h"

#define PERP_VAL 0x3f400000u            /* 0.75 */
static const struct { int tx, ty; } AT[8] = { { 4, 3 }, { 6, 3 }, { 8, 3 }, { 10, 3 }, { 4, 5 }, { 6, 5 }, { 8, 5 }, { 10, 5 } };
static uint32_t tw[8][1024][3], tu[8][1024][3];
static uint32_t texture;

static uint32_t rf(int e)               /* 2^e .. 2^(e + 1), any 23 bit fraction */
{
    return (uint32_t)(127 + e) << 23 | (rng() & 0x7fffffu);
}

static void test_setup(void)
{
    static uint32_t words[1024 * 8 / 2];
    int t, k, i, j;
    for(j = 0; j < 8; j++)
        for(i = 0; i < 1024; i += 2)
            words[(j * 1024 + i) / 2] = (uint32_t)i | (uint32_t)(i + 1) << 16;
    texture = pvr_tex_alloc(sizeof(words));
    pvrio_w64(texture, words, sizeof(words) / 4);
    for(t = 0; t < 4; t++)
        for(k = 0; k < 1024; k++) {
            const int e = (int)(rng() % 7), low = (int)(rng() % 3), d = 12 + (int)(rng() % 19);
            for(i = 0; i < 3; i++) {
                switch(t) {
                case 0: tw[t][k][i] = rf((int)(rng() % 7)); break;
                case 1: tw[t][k][i] = rf(e); break;
                case 2: tw[t][k][i] = rf(i == low ? e - d : e); break;
                default: tw[t][k][i] = rf((int)(rng() % 41) - 20); break;
                }
                tu[t][k][i] = rf(6);
            }
            for(i = 0; i < 3; i++) {
                tw[t + 4][k][i] = tw[t][k][i];
                tu[t + 4][k][i] = tu[t][k][i];
            }
        }
    pvrio_reg(FPU_PERP_VAL_addr, PERP_VAL);
}

static void test_describe(void)
{
    static const char *what[4] = { "1/w three of 1 .. 128", "1/w three in one binade", "1/w one 12 .. 30 binades below",
                                    "1/w three of 2^-20 .. 2^20" };
    int t;
    for(t = 0; t < 8; t++)
        label(0, AT[t].ty * COLS + AT[t].tx, "%s, u 64 .. 128%s", what[t % 4], t >= 4 ? ", mirrored" : "");
}

static void test_page(int page)
{
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    int t, k;
    (void)page;
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Opaque;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Texture = 1;
    isp.DepthMode = DepthMode_Always;
    isp.CullMode = CullMode_None;
    isp.Texture = 1;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    tsp.ShadInstr = ShadInstr_Decal;
    tsp.FilterMode = FilterMode_Point;
    tsp.MipMapD = 4;
    tsp.TexU = 7;                               /* 1024 */
    tsp.TexV = 0;                               /* 8 */
    tcw.PixelFmt = PixelFmt_565;
    tcw.ScanOrder = 1;
    tcw.TexAddr = texture >> 3;
    ta_list_begin(ListType_Opaque);
    ta_polygon(pcw, isp, tsp, tcw);
    for(t = 0; t < 8; t++)
        for(k = 0; k < 1024; k++) {
            const int X = AT[t].tx * 32 + k % 32, Y = AT[t].ty * 32 + k / 32;
            const f32b x0 = f32b_scaled(4 * X + 1, -2), y0 = f32b_scaled(4 * Y + 1, -2);
            const f32b x1 = f32b_int(X + 1), y1 = f32b_int(Y + 1), v = 0x3f000000u;
            ta_vertex3(x0, y0, tw[t][k][0], tu[t][k][0], v, 0xffffffffu, 0, 0);
            if(t >= 4) {
                ta_vertex3(x0, y1, tw[t][k][1], tu[t][k][1], v, 0xffffffffu, 0, 0);
                ta_vertex3(x1, y0, tw[t][k][2], tu[t][k][2], v, 0xffffffffu, 0, 1);
            } else {
                ta_vertex3(x1, y0, tw[t][k][1], tu[t][k][1], v, 0xffffffffu, 0, 0);
                ta_vertex3(x0, y1, tw[t][k][2], tu[t][k][2], v, 0xffffffffu, 0, 1);
            }
        }
    ta_list_end();
}
