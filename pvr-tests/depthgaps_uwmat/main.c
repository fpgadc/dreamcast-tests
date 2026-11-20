/* The words below a negative depth's window, against each other (2026-11-19, hunt depthgaps (c); after
   depthgaps_ties).

   depthgaps_ties: in field 133 the words lie UW(6) (a ramp of E 6 at -2 .. -31) < the background -1 < OW(7) (a ramp
   of E 7 beyond -128) < 0xc2ffffe0, and the UW(6) ramp's first pixel (-1) ties with the background: so the word
   under the window depends on the depth. Here ramps that vary along x against ramps that vary along y, every pair
   of depths at one pixel: pixel (i, j) compares the first ramp's depth at i with the second's at j.
     ramp(S, A, B): a quad over the cell, depth S at the tile's first pixel center, A, B a pixel
   Pages: Equal, Greater, Less (the second ramp against the first). */
#define NUM_PAGES 3
#include "pvrtest2.h"
#include "texgen.h"

enum { NONE, RAMP, FLAT, C0, C1, BTRI };
typedef struct { int kind; int32_t s, a, b; int e; } draw_t;     /* RAMP: S, A, B = s, a, b 2^e; FLAT: s = word; BTRI: a = legs, s = Z word */
typedef struct { draw_t p1, p2; const char *name; } pair_t;
#define NP 10
static const pair_t P[NP] = {
    { { RAMP, -1, -1, 0, 0 }, { RAMP, -1, 0, -1, 0 }, "ramp(-1, -1, 0) E 6, then ramp(-1, 0, -1) E 6" },
    { { RAMP, -1, -1, 0, -2 }, { RAMP, -1, 0, -1, -2 }, "ramp(-0.25, -0.25, 0) E 4, then ramp(-0.25, 0, -0.25) E 4" },
    { { RAMP, -511, -4, 0, -2 }, { RAMP, -511, 0, -4, -2 }, "ramp(-127.75, -1, 0) E 7, then ramp(-127.75, 0, -1) E 7" },
    { { RAMP, -1, -1, 0, 0 }, { RAMP, -511, 0, -4, -2 }, "ramp(-1, -1, 0) E 6, then ramp(-127.75, 0, -1) E 7" },
    { { RAMP, -1, -1, 0, 0 }, { RAMP, -1, 0, -3, -1 }, "ramp(-1, -1, 0) E 6, then ramp(-0.5, 0, -1.5) E 6" },
    { { RAMP, -1, 1, 0, 0 }, { RAMP, -1, 0, 1, 0 }, "ramp(-1, +1, 0) E 6, then ramp(-1, 0, +1) E 6 (crossing 0)" },
    { { RAMP, -1, -1, 0, 4 }, { RAMP, -1, 0, -1, 4 }, "ramp(-16, -16, 0) E 10, then ramp(-16, 0, -16) E 10" },
    { { RAMP, -1, -1, 0, -40 }, { RAMP, -1, 0, -1, -40 }, "ramp(-2^-40, -2^-40, 0), then the same along y" },
    { { RAMP, -129, -1, 0, -7 }, { RAMP, -129, 0, -1, -7 }, "ramp(-1.0078, -2^-7, 0) E 1, then along y" },
    { { RAMP, -255, -2, 0, -1 }, { RAMP, -255, 0, -2, -1 }, "ramp(-127.5, -1, 0) E 7, then along y" },

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
