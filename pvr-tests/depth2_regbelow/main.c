/* The overflow register's values against an ordinary block's depths below its window, between the same two float
   words (2026-11-20, hunt depth2; after depth2_regmix).

   pvr_isp_depth.h GAP (RegDepth): depth2_regmix ordered the register's words and the values between them against
   blocks whose depths lie ABOVE their window (from a start of 0) and against blocks of E 128; an ordinary block of E
   125 .. 127 below its window (beyond -2^E, between the words of fields E + 125 and E + 126, where the register's m
   = 17 .. 23 lie: an ordinary sliver of E 127 there falls between m = 22 and 23) against them was not measured:
   refsw3 stores both as StoredDepth::Neg(E, M), ordered by M on the grid 2^(E - 16), the register's value rounded
   down onto it. Such a block needs a corner's depth there, and the setup keeps the products of such a corner in
   range only for a tiny triangle near the origin: per pixel a triangle 2^-10 .. 2^-4 pixel around its centre
   (that pixel the only one inside), corners on a plane through the wanted value whose start, at the tile's first
   pixel, is in the window of E and whose slopes are below 2^(E - 5) (gen.cpp). Against, in the same tile:
     tile (0, 0): zword's TL +inf with legs 64 (corners (0, 0) +inf, (64, 0), (0, 64) 0.5: m = -(i + j) mod 32),
       m = 22 at i + j = 42 (E 127, M = -2^17) and m = 19 at i + j = 45 (E 126, M = -2^17)
     the other tiles: zovf_creg's C0 (B (x, y) at -2^118, R (x + 32, y) at 2^123, D (x, y + 32) at -2^118 + 32 F)
       with a small finite F < 0 a row, off the block's grid: just below m = 23 (E 127 below its window, the block's
       M the register's rounded down + d, d mostly -2 .. 2), and just below m = 20, 18, 17 (the register above the
       window of E 126, 125, 124, against blocks of E 127, 126, 125 below theirs: the same two float words)
   P (DepthMode Always, green) then Q (the page's mode, white where it passes): the register triangle first or the
   tiny ones first, per tile. Pages: set p / 3 (4), mode p % 3 (Greater, Equal, Less). */
#define NUM_PAGES 12
#include "pvrtest2.h"
#include "texgen.h"

typedef struct { uint32_t x[3], y[3], z[3]; } tri_t;
typedef struct { int tx, ty, regFirst; tri_t reg; int ntiny, first; } tile_t;
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
    int p, k;
    for(p = 0; p < NUM_PAGES; p++) {
        const int s = p / 3;
        for(k = 0; k < NTILES[s]; k++) {
            const tile_t *t = &TILES[s][k];
            const char *reg = t->tx == 0 && t->ty == 0 ? "zword TL +inf legs 64" : "C0";
            if(t->regFirst)
                label(p, t->ty * COLS + t->tx, "set %d: %s (D %08x), then %d tiny triangles below the window, %s", s, reg, t->reg.z[2], t->ntiny, MN[p % 3]);
            else
                label(p, t->ty * COLS + t->tx, "set %d: %d tiny triangles below the window, then %s (D %08x), %s", s, t->ntiny, reg, t->reg.z[2], MN[p % 3]);
            if(t->tx == 0 && t->ty == 0) {
                label(p, 1, "set %d: zword TL +inf legs 64 from (0, 0) (%s), %s", s, t->regFirst ? "P" : "Q", MN[p % 3]);
                label(p, COLS, "set %d: zword TL +inf legs 64 from (0, 0) (%s), %s", s, t->regFirst ? "P" : "Q", MN[p % 3]);
            }
        }
    }
}

static void tri(const tri_t *t, int mode, uint32_t tex)
{
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    int k;
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
    for(k = 0; k < 3; k++)
        ta_vertex3(t->x[k], t->y[k], t->z[k], 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, k == 2);
}

static void test_page(int page)
{
    const int s = page / 3, mode = MD[page % 3];
    int k, q;
    pvr_frame.bg_color = 0x000040;
    pvr_frame.bg_depth = 0;
    ta_list_begin(ListType_Opaque);
    for(k = 0; k < NTILES[s]; k++) {
        const tile_t *t = &TILES[s][k];
        if(t->regFirst) {
            tri(&t->reg, DepthMode_Always, texG);
            for(q = 0; q < t->ntiny; q++)
                tri(&TINY[t->first + q], mode, texW);
        } else {
            for(q = 0; q < t->ntiny; q++)
                tri(&TINY[t->first + q], DepthMode_Always, texG);
            tri(&t->reg, mode, texW);
        }
    }
    ta_list_end();
}
