/* A finite start in an overflowing depth plane (2026-11-19, after pvr-tests/nantr_pos, nantr_word).

   nantr_pos / nantr_word: in the depth unit's regime C (a slope overflows) the register starts one step on, s0 =
   +-1, where the plane's start (a x0 + b y0) + c is an infinity, and at 0 where the two slopes' terms cancel
   (+inf - inf) and c is a small finite number. Here c is a large finite number: the bad corner V0 = (1, 1/4) at S,
   the two others on the line y = 2 x through the origin, V1 = (1/8, 1/4), V2 = (1/2, 1) at Z, so that V0's term of c
   is S * 0, a x0 and b y0 are infinities of opposite sign, and the start is c / det = Z exactly (the products of
   c stay below 2^128 up to the largest float: |e| <= 7/8). The triangle covers one pixel center, (0.5, 0.5), the
   tile's first pixel: its word is the register's start. Read as zword reads words (a binary search with the
   background as the probe, one render a step, the pixel read back; results in rows 470, 471, word 0).
   Pages: S +inf with Z 0.5, 2^100, 2^120, 2^123, 2^124, 2^125, 2^126, 2^127, 1.5 2^127, the largest float, -2^124,
   -2^127, -the largest float; S -inf with Z 2^124, 2^127, -2^127. */
#define NUM_PAGES 16
#define TEST_DRAW_PAGE
#include "pvrtest2.h"
#include "texgen.h"

typedef struct { uint32_t s, z; const char *name; } variant_t;
static const variant_t V[NUM_PAGES] = {
    { 0x7f800000u, 0x3f000000u, "S +inf, Z 0.5" },
    { 0x7f800000u, 0x71800000u, "S +inf, Z 2^100" },
    { 0x7f800000u, 0x7b800000u, "S +inf, Z 2^120" },
    { 0x7f800000u, 0x7d000000u, "S +inf, Z 2^123" },
    { 0x7f800000u, 0x7d800000u, "S +inf, Z 2^124" },
    { 0x7f800000u, 0x7e000000u, "S +inf, Z 2^125" },
    { 0x7f800000u, 0x7e800000u, "S +inf, Z 2^126" },
    { 0x7f800000u, 0x7f000000u, "S +inf, Z 2^127" },
    { 0x7f800000u, 0x7f400000u, "S +inf, Z 1.5 2^127" },
    { 0x7f800000u, 0x7f7fffffu, "S +inf, Z max" },
    { 0x7f800000u, 0xfd800000u, "S +inf, Z -2^124" },
    { 0x7f800000u, 0xff000000u, "S +inf, Z -2^127" },
    { 0x7f800000u, 0xff7fffffu, "S +inf, Z -max" },
    { 0xff800000u, 0x7d800000u, "S -inf, Z 2^124" },
    { 0xff800000u, 0x7f000000u, "S -inf, Z 2^127" },
    { 0xff800000u, 0xff000000u, "S -inf, Z -2^127" },
};
#define NNEG (122u << 18)               /* negative probes: fields 133 .. 254, 2^18 each */
#define NPROBES (NNEG + (130u << 18))   /* and positive ones: fields 125 .. 254 */

static uint32_t texG;
static uint32_t green(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0x07e0; }

/* probe i in the ISP's order (zword's) */
static uint32_t probe(uint32_t i)
{
    if(i < NNEG)
        return 0x80000000u | (133u + (i >> 18)) << 23 | ((262143u - (i & 262143u)) << 5);
    i -= NNEG;
    return (125u + (i >> 18)) << 23 | ((i & 262143u) << 5);
}

static void test_setup(void)
{
    const tex_layout_t l = { TEX_TWIDDLED, 16, 8, 8, 0 };
    texG = tex_build(&l, green, NULL);
}

static void test_describe(void)
{
    int p;
    for(p = 0; p < NUM_PAGES; p++) {
        label(p, 280, "%s: the word of pixel (0, 0) (row 470, 471 word 0)", V[p].name);
        label(p, 0, "%s: the last render", V[p].name);
    }
}

/* x, y in 1/8 pixel */
static void vtx(int x8, int y8, uint32_t z, int end)
{
    ta_vertex3(f32b_scaled(x8, -3), f32b_scaled(y8, -3), z, 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, end);
}

/* one render with the background at b and the triangle compared by mode; pixel (0, 0) green? */
static int render(const variant_t *v, uint32_t b, int mode)
{
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    uint32_t w;
    pvr_frame.bg_color = 0x000040;
    pvr_frame.bg_depth = b;
    pvr_frame_begin();
    ta_list_begin(ListType_Opaque);
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
    vtx(8, 2, v->s, 0); vtx(1, 2, v->z, 0); vtx(4, 8, v->z, 1);
    ta_list_end();
    pvr_frame_render();
    pvrio_read32(SETUP_FB, 1, &w);
    return (w & 0xffffff) == 0x00ff00;
}

static void test_page(int page) { (void)page; }

static void test_draw_page(int page)
{
    const variant_t *v = &V[page];
    static uint32_t res[2][32];
    uint32_t lo = 0, hi = NPROBES;
    pvrio_unhashed = 1;
    while(lo < hi) {
        const uint32_t mid = lo + (hi - lo + 1) / 2;       /* is it above probe mid - 1? */
        if(render(v, probe(mid - 1), DepthMode_Greater)) lo = mid; else hi = mid - 1;
    }
    res[0][0] = lo;
    res[1][0] = lo < NPROBES ? (uint32_t)render(v, probe(lo), DepthMode_Equal) : 0;
    pvrio_w32(SETUP_FB + 470u * 640 * 4, res[0], 32);
    pvrio_w32(SETUP_FB + 471u * 640 * 4, res[1], 32);
    pvrio_unhashed = 0;
}
