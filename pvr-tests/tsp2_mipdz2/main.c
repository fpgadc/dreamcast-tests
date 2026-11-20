/* tsp2_mipdz2: the mipmap level of a triangle whose TSP determinant is zero, at every size of w (2026-11-20).

   tsp2_mipdz (1/w 2^-65 .. 2^-56): every pixel of such a triangle, three equal corners or a sliver whose products
   cancel, is on the 1x1 level, with a fraction as if the polygon's length were all ones (0xFF in 8 bits, times
   the D adjust) and huge. Here the same pages with 1/w from 2^-127 to 2^126, to see where the level comes down.
     page 0, 1  three equal corners (a point at y = row + 1/4, x = the cell's middle + 1/4), 16 per cell on rows
                0, 2, .. 30
     page 2, 3  slivers on rows 0, 2, .. 30 whose determinant in the TSP's setup is exactly zero (gen.cpp; points
                where none was found)
   Pages 0, 2: the level (point sampled level colours, rows[] as listed); pages 1, 3: trilinear pass B of white
   levels (the fraction), the same rows otherwise.
   Columns: the 1/w (points: every corner's; slivers: the base z of z, 1.5 z, 1.25 z) 2^E (1 + j / 16) for the
   cell's j-th triangle, E = -127, -100, -80, -64, -56, -40, -20, -8, -4, -2, 0, 2, 4, 8, 16, 32, 48, 64, 96, 126
   (row 14: below zero); U 0, 1, 1/2 at the corners of a sliver (row 13: 0, 2^20, 2^-20), 1/4 at a point; V 1/4.
   Results (one console, 2026-11-20): the 1x1 level at every 1/w, 2^-127 to 2^126, the fraction that of 0xFF (times
   the D adjust) times w * w's 8 bits: 1,200 of 1,200 regions. A length of the largest number, 1.FFFFFF 2^127,
   comes down from the last level from 1/w = 2^64 on (3 columns of every row wrong). Row 14 column 0 (corners of
   -2^-127 (1 + j / 16), denormals) shows the mean's (-0 + -0) + -0 is +0 (with -0: both fraction regions wrong).
   Texel (x, y) of the level 2^l wide: 565, red 3 l, green 5 l, blue 31 - 2 l. Opaque list, depth ALWAYS. */
#define NUM_PAGES 4
#include "pvrtest2.h"
#include "texgen.h"
#include "cases.h"

typedef struct { const char *name; int texU, mipD, dcalc, filter, uExp, neg; } row_t;
static const row_t rows[15] = {
    { "64, D 4/4",                    3, 4, 0, 0, 0, 0 },
    { "64, D 4/4, DCalcCtrl 1",       3, 4, 1, 0, 0, 0 },
    { "64, D 1/4",                    3, 1, 0, 0, 0, 0 },
    { "64, D 15/4",                   3, 15, 0, 0, 0, 0 },
    { "64, D 2/4, bilinear",          3, 2, 0, 1, 0, 0 },
    { "8, D 4/4",                     0, 4, 0, 0, 0, 0 },
    { "8, D 4/4, DCalcCtrl 1",        0, 4, 1, 0, 0, 0 },
    { "8, D 15/4",                    0, 15, 0, 0, 0, 0 },
    { "256, D 4/4",                   5, 4, 0, 0, 0, 0 },
    { "256, D 4/4, DCalcCtrl 1",      5, 4, 1, 0, 0, 0 },
    { "256, D 1/4",                   5, 1, 0, 0, 0, 0 },
    { "16, D 4/4",                    1, 4, 0, 0, 0, 0 },
    { "32, D 8/4",                    2, 8, 0, 0, 0, 0 },
    { "64, D 4/4, U 2^20",            3, 4, 0, 0, 20, 0 },
    { "64, D 4/4, 1/w below zero",    3, 4, 0, 0, 0, 1 },
};
static uint32_t tex_col[6], tex_white;

static uint32_t c565(int r, int g, int b) { return (uint32_t)(r & 31) << 11 | (uint32_t)(g & 63) << 5 | (uint32_t)(b & 31); }
static uint32_t level_colour(const void *ctx, int size, int x, int y)
{
    int l = 0;
    (void)ctx; (void)x; (void)y;
    while((1 << l) < size) l++;
    return c565(3 * l, 5 * l, 31 - 2 * l);
}
static uint32_t white(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0xffff; }

static void test_setup(void)
{
    int k;
    for(k = 0; k <= 5; k++) {
        const tex_layout_t l = { TEX_MIP, 16, 8 << k, 8 << k, 0 };
        tex_col[k] = tex_build(&l, level_colour, NULL);
    }
    {
        const tex_layout_t l = { TEX_MIP, 16, 256, 256, 0 };
        tex_white = tex_build(&l, white, NULL);
    }
}

static const int E[20] = { -127, -100, -80, -64, -56, -40, -20, -8, -4, -2, 0, 2, 4, 8, 16, 32, 48, 64, 96, 126 };

static void test_describe(void)
{
    static const char *what[NUM_PAGES] = { "points, level", "points, trilinear B: fraction", "slivers, level", "slivers, trilinear B: fraction" };
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 300; c++)
            label(p, c, "%s, %s, 1/w %s2^%d (1 + j / 16)", what[p], rows[c / 20].name, rows[c / 20].neg ? "-" : "", E[c % 20]);
}

static void test_page(int page)
{
    const int lines = page >= 2, fraction = page & 1;
    int c, j, k;
    pvr_frame.bg_color = 0x000040;
    ta_list_begin(ListType_Opaque);
    for(c = 0; c < 300; c++) {
        const row_t *r = &rows[c / 20];
        for(j = 0; j < 16; j++) {
            PCW pcw = { .full = 0 };
            ISP_TSP isp = { .full = 0 };
            TSP tsp = { .full = 0 };
            TCW tcw = { .full = 0 };
            pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
            pcw.ListType = ListType_Opaque;
            pcw.Group_En = 1;
            pcw.Strip_Len = 1;
            pcw.Texture = 1;
            isp.DepthMode = DepthMode_Always;
            isp.CullMode = CullMode_None;
            isp.Texture = 1;
            isp.DCalcCtrl = (uint32_t)r->dcalc;
            tsp.SrcInstr = BlendInstr_One;
            tsp.DstInstr = BlendInstr_Zero;
            tsp.FogCtrl = FogCtrl_None;
            tsp.UseAlpha = 1;
            tsp.ShadInstr = ShadInstr_Decal;
            tsp.FilterMode = fraction ? FilterMode_TrilinearB : r->filter ? FilterMode_Bilinear : FilterMode_Point;
            tsp.MipMapD = (uint32_t)r->mipD;
            tsp.TexU = (uint32_t)r->texU;
            tsp.TexV = (uint32_t)r->texU;
            tcw.PixelFmt = PixelFmt_565;
            tcw.MipMapped = 1;
            tcw.TexAddr = (fraction ? tex_white : tex_col[r->texU]) >> 3;
            ta_polygon(pcw, isp, tsp, tcw);
            for(k = 0; k < 3; k++) {
                const dzslot_t *sl = &slots[lines][16 * c + j];
                ta_vertex3(sl->x[k], sl->y[k], sl->z[k], sl->u[k], 0x3e800000u, 0xffffffffu, 0, k == 2);
            }
        }
    }
    ta_list_end();
}
