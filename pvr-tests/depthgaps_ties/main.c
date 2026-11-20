/* The words a depth below zero saturates to, against each other and the overflow register's words (2026-11-19, hunt
   depthgaps (a), (b), (c); after depthgaps_negconv).

   depthgaps_negconv: a depth below zero is stored in a window, the block's top binade [2^(E - 1), 2^E); smaller
   magnitudes all store one word UW(E) just above the window in the ISP's order, larger ones OW(E) just below it.
   Both lie between two probes (fraction above 0x7fffe0 in the word: UW(E) in exponent field E + 127, OW(E) in
   E + 126), as zword's words m = 17 .. 22, 24 .. 29 do, and as a negative background below 64 does (field 133).
   Here polygons against each other: the first DepthMode Always (textured green), the second over it with the
   page's mode (white), or against the background (-1: stored in field 133, bgneg) where there is no first.
     ramp(S, A, B): a quad over the cell, depth S at the tile's first pixel center, A, B a pixel
     UW6 = ramp(-1, -1, 0) (E 6: UW(6) for i <= 30), OW7 = ramp(-127.75, -1, 0) (E 7: OW(7) for i >= 1)
     flat c2ffffe0: the last word of field 133 (a negative depth there rounds up to it)
     C0: zword's triangle (legs 32, +inf at the top left corner, others 0.5: m = -(i + j));
     C1: the same with the others at 2^120 (m = -(i + j) - 1: depthgaps_phase)
     B(L, Z): a triangle with legs L, Z at the top left corner, others 0.5: only its constant overflows (a start
     of 0) and its slopes are -Z / L: B(8, 1.5 2^124) E 127, B(16, 1.5 2^123) E 125, B(16, 1.5 2^122) E 124,
     B(12, 1.2 2^124) E 126.
   Pages: Equal, Greater, Less. */
#define NUM_PAGES 3
#include "pvrtest2.h"
#include "texgen.h"

enum { NONE, RAMP, FLAT, C0, C1, BTRI };
typedef struct { int kind; int32_t s, a, b; int e; } draw_t;     /* RAMP: S, A, B = s, a, b 2^e; FLAT: s = word; BTRI: a = legs, s = Z word */
typedef struct { draw_t p1, p2; const char *name; } pair_t;
#define UW6 { RAMP, -4, -4, 0, -2 }
#define OW7 { RAMP, -511, -4, 0, -2 }
#define NP 14
static const pair_t P[NP] = {
    { UW6, OW7, "UW(6) ramp, then OW(7) ramp" },
    { OW7, UW6, "OW(7) ramp, then UW(6) ramp" },
    { { NONE }, UW6, "background -1, then UW(6) ramp" },
    { { NONE }, OW7, "background -1, then OW(7) ramp" },
    { { FLAT, (int32_t)0xc2ffffe0u }, UW6, "flat c2ffffe0, then UW(6) ramp" },
    { { FLAT, (int32_t)0xc2ffffe0u }, OW7, "flat c2ffffe0, then OW(7) ramp" },
    { { NONE }, { FLAT, (int32_t)0xc2ffffe0u }, "background -1, then flat c2ffffe0" },
    { { C0 }, { BTRI, 0x7f400000, 8 }, "C0, then B(8, 1.5 2^124)" },
    { { C0 }, { BTRI, 0x7ec00000, 16 }, "C0, then B(16, 1.5 2^123)" },
    { { C1 }, { BTRI, 0x7e400000, 16 }, "C1, then B(16, 1.5 2^122)" },
    { { C1 }, { BTRI, 0x7f19999a, 12 }, "C1, then B(12, 1.2 2^124)" },
    { { C0 }, { BTRI, 0x7f19999a, 12 }, "C0, then B(12, 1.2 2^124)" },
    { { C0 }, { BTRI, 0x7e400000, 16 }, "C0, then B(16, 1.5 2^122)" },
    { { C1 }, { BTRI, 0x7ec00000, 16 }, "C1, then B(16, 1.5 2^123)" },
};
static const char *MODE[3] = { "Equal", "Greater", "Less" };
static uint32_t texG, texW;

static uint32_t green(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0x07e0; }
static uint32_t white(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0xffff; }

static void test_setup(void)
{
    const tex_layout_t l = { TEX_TWIDDLED, 16, 8, 8, 0 };
    texG = tex_build(&l, green, NULL);
    texW = tex_build(&l, white, NULL);
}

static int cellof(int k) { return 21 + 2 * (k % 9) + 40 * (k / 9); }       /* every other cell, rows 1, 3 */

static void test_describe(void)
{
    int p, k;
    for(p = 0; p < NUM_PAGES; p++)
        for(k = 0; k < NP; k++)
            label(p, cellof(k), "%s, %s", P[k].name, MODE[p]);
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

static void vtx(int x, int y, uint32_t z, int end)
{
    ta_vertex3(f32b_int(x), f32b_int(y), z, 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, end);
}

static void draw(const draw_t *d, int x, int y, int mode, uint32_t tex)
{
    const uint32_t half = 0x3f000000u;
    int k;
    if(d->kind == NONE)
        return;
    begin(mode, tex);
    switch(d->kind) {
    case RAMP:      /* corner (dx, dy): S + A (dx - 0.5) + B (dy - 0.5) = (2 s + a (2 dx - 1) + b (2 dy - 1)) 2^(e - 1) */
        for(k = 0; k < 4; k++)
            vtx(x + (k & 1) * 32, y + (k >> 1) * 32,
                f32b_scaled(2 * d->s + d->a * (2 * 32 * (k & 1) - 1) + d->b * (2 * 32 * (k >> 1) - 1), d->e - 1), k == 3);
        break;
    case FLAT:
        for(k = 0; k < 4; k++)
            vtx(x + (k & 1) * 32, y + (k >> 1) * 32, (uint32_t)d->s, k == 3);
        break;
    case C0: case C1: {
        const uint32_t o = d->kind == C0 ? half : 0x7b800000u;
        vtx(x, y, 0x7f800000u, 0); vtx(x + 32, y, o, 0); vtx(x, y + 32, o, 1);
        break; }
    case BTRI:
        vtx(x, y, (uint32_t)d->s, 0); vtx(x + d->a, y, half, 0); vtx(x, y + d->a, half, 1);
        break;
    }
}

static void test_page(int page)
{
    const int mode = page == 0 ? DepthMode_Equal : page == 1 ? DepthMode_Greater : DepthMode_Less;
    int k;
    pvr_frame.bg_color = 0x000040;
    pvr_frame.bg_depth = 0xbf800000u;
    ta_list_begin(ListType_Opaque);
    for(k = 0; k < NP; k++) {
        const int c = cellof(k);
        draw(&P[k].p1, cell_px(c), cell_py(c), DepthMode_Always, texG);
        draw(&P[k].p2, cell_px(c), cell_py(c), mode, texW);
    }
    ta_list_end();
}
