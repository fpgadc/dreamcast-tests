/* The overflow register's words against other planes' depths between the same two float words (2026-11-20, hunt
   depth2; after zword, depthgaps_mm, depthgaps_ties, nantr_*, zovf_creg).

   pvr_isp_depth.h GAP: when a slope overflows (regime C) a pixel stores a word of the register alone (RegDepth):
   zword, depthgaps_mm and nantr_* pin those words against float words and against each other; where they lie among
   OTHER planes' depths between the same two float words (an ordinary block's depths outside its top binade, ordered
   by its E and fixed point M, StoredDepth::Neg; depths of 2^128 and more, StoredDepth::Big) is refsw3's choice: the
   register's word as a block of E = p + 100 with M its bits shifted left past the zeros under its sign. Here pairs
   in one tile each, P (DepthMode Always, green) then Q (the page's mode, white where it passes):
     register triangles: zword's TL +-inf (legs 32; others 0.5: m = -+(i + j); others 2^120: C1, m = -(i + j) - 1),
       zovf_creg's C0 (B (x, y) at -2^118, R (x + 32, y) at 2^123, D (x, y + 32) at -2^118 + 32 F: r = i steps + j F,
       F from +-2^100 to +-2^122: between whole steps)
     blocks: depthgaps_ties' B(L, Z) (TL at z0 of 2^122 .. 2^127, TR, BL at z1, z2, legs L: a start of 0, the
       depths -(i A + j B), E 122 .. 128 from the larger slope: the depths above its window), or another C0
     tile (0, 0): zword's TL +inf with legs 64 (Q) over up to 25 small triangles (P) whose start is -2^127
       (depthgaps_bigpos: V0 (X, Y) at 0, V1 (X + L, Y) at z1, V2 (X, Y + L) at z2): depths of 2^128 and more
       against m = 2 .. 15, and below -2^128 (a block of E 128 below its window) against m = 24 .. 29
   gen.cpp picked them so that their pixels lie between the same two float words, as many kinds as it found.
   Pages: set p / 3 (4), mode p % 3 (Greater, Equal, Less). */
#define NUM_PAGES 12
#include "pvrtest2.h"
#include "texgen.h"

typedef struct { int kind; int x[3], y[3]; uint32_t z[3]; } tri_t;
typedef struct { int tx, ty; tri_t p, q; } pair_t;
#include "cases.h"

static const int MD[3] = { DepthMode_Greater, DepthMode_Equal, DepthMode_Less };
static const char *MN[3] = { "Greater", "Equal", "Less" };
static const char *KN[4] = { "zword", "C0", "B(L, Z)", "start -2^127" };
static const tri_t ZW64 = { 0, { 0, 64, 0 }, { 0, 0, 64 }, { 0x7f800000u, 0x3f000000u, 0x3f000000u } };
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
        label(p, 0, "set %d: %d triangles of start -2^127, then zword TL +inf legs 64, %s", s, NSLOTS[s], MN[p % 3]);
        label(p, 1, "set %d: zword TL +inf legs 64 from (0, 0), %s", s, MN[p % 3]);
        label(p, COLS, "set %d: zword TL +inf legs 64 from (0, 0), %s", s, MN[p % 3]);
        for(k = 0; k < NPAIRS[s]; k++) {
            const pair_t *q = &PAIRS[s][k];
            label(p, q->ty * COLS + q->tx, "set %d: %s %08x %08x %08x legs %d, then %s %08x %08x %08x legs %d, %s", s, KN[q->p.kind], q->p.z[0],
                  q->p.z[1], q->p.z[2], q->p.x[1] - q->p.x[0], KN[q->q.kind], q->q.z[0], q->q.z[1], q->q.z[2], q->q.x[1] - q->q.x[0], MN[p % 3]);
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
        ta_vertex3(f32b_int(t->x[k]), f32b_int(t->y[k]), t->z[k], 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, k == 2);
}

static void test_page(int page)
{
    const int s = page / 3, mode = MD[page % 3];
    int k;
    pvr_frame.bg_color = 0x000040;
    pvr_frame.bg_depth = 0;
    ta_list_begin(ListType_Opaque);
    for(k = 0; k < NSLOTS[s]; k++)
        tri(&SLOTS[s][k], DepthMode_Always, texG);
    tri(&ZW64, mode, texW);
    for(k = 0; k < NPAIRS[s]; k++) {
        tri(&PAIRS[s][k].p, DepthMode_Always, texG);
        tri(&PAIRS[s][k].q, mode, texW);
    }
    ta_list_end();
}
