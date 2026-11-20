/* tsp2_mipdz: the mipmap level of a triangle whose TSP determinant is zero (2026-11-20).

   refsw3's MipSelect took the reciprocal of the setup's determinant Sdet, which divides by zero when Sdet is 0
   (polly2-rtl docs/PLAN.md probe F1, decision D4). Here such triangles with a mipmapped texture whose every level
   is one colour (mipdump's method), so that each pixel shows the level the TSP chose there, and pages of white
   levels in a trilinear pass, which show the fraction it blends with (255 k / 256).
     page 0, 1  three equal corners (a point at y = row + 1/4 and x = the cell's middle + 1/4: the ISP draws the
                whole width of the tile on that row; slivers_detzero), 16 per cell on rows 0, 2, .. 30: every sum
                of the setup is zero, the numerators of the level too
     page 2, 3  slivers on rows 0, 2, .. 30 (two corners 2^-14 .. 2^-12 to one side of the row's pixel centres,
                the third as far to the other, each moved by a few last places; gen.cpp) whose determinant in the
                TSP's setup is exactly zero (its products cut to 24 bits cancel) while the level's numerators are
                not (1/w z, 1.5 z, 1.25 z and U 0, 1, 1/2 at the corners; row 13: U 0, 2^20, 2^-20): 1/det = +0
                and 1/det = infinity give levels one apart on 1435 of them; 1078 slots where no sliver was found
                are points
   Pages 0, 2: the level (point sampled level colours, rows[] as listed); pages 1, 3: trilinear pass B of white
   levels (the fraction), the same rows otherwise.
   Columns: the 1/w (points: every corner's; slivers: the base z) 2^-(56 + c / 2) (1 + (16 (c & 1) + j) / 32)
   for the cell's j-th triangle (row 14: below zero); V 1/4.
   Results (one console, 2026-11-20): every pixel of every such triangle, points and slivers alike, on the 1x1
   level, with the fraction of K's 8 bits 0xFF (times the D adjust) times w * w's: 1,200 of 1,200 regions with
   that (refsw3 pvr_tsp_plane.h MipSelect). 1/det = +0 (K = 2^(-127 + size), polly2-rtl's D4) 59, 1/det = an
   infinity 36: both the largest map at small 1/w. refsw3 before: a division by zero (SIGFPE).
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

static void test_describe(void)
{
    static const char *what[NUM_PAGES] = { "points, level", "points, trilinear B: fraction", "slivers, level", "slivers, trilinear B: fraction" };
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 300; c++)
            label(p, c, "%s, %s, 1/w %s2^-%d (1 + (%d + j) / 32)", what[p], rows[c / 20].name, rows[c / 20].neg ? "-" : "", 56 + c % 20 / 2, 16 * (c & 1));
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
