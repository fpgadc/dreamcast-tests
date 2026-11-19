/* The words of an overflowing depth plane that starts one step on (2026-11-19, after pvr-tests/nantr_pos).

   nantr_pos: in the depth unit's regime C (a slope overflows: m = (s0 + sa i + sb j) mod 32, the words pvr_isp_depth.h
   kWrap, pvr-tests/zword) the register starts at s0 = +-1 instead of 0 where the plane's start, (a x0 + b y0) + c
   with the infinities' rules, is an infinity (zbad's NaN at TR at x = 0, where TR's term of c is NaN * 0). Here the
   words themselves, as zword reads them: a binary search with the background as the probe, one render a step, the
   pixel read back (zword's method and probes; results in rows 470, 471 of the page dump, zword_read.py's format).
   Pages: one triangle each; the targets: the 31 pixels of its top row (x + k, y) or, for the BL ones (whose
   register does not change along a row), of its left column (x, y + k). */
#define NUM_PAGES 8
#define TEST_DRAW_PAGE
#include "pvrtest2.h"
#include "texgen.h"

/* shape 0: (x, y), (x + 32, y), (x, y + 32), corner `bad` (1 TR, 2 BL) at s; shape 1: (x, y), (x + 32, y),
   (x + 32, y + 32), corner 1 (TR) at s; the others at o */
typedef struct { int x, y, shape, bad, col; uint32_t s, o; const char *name; } variant_t;
static const variant_t V[NUM_PAGES] = {
    { 0, 0, 0, 1, 0, 0x7fc00000u, 0x3f000000u, "TR NaN at (0, 0), others 0.5" },
    { 0, 64, 0, 1, 0, 0xffc00000u, 0x3f000000u, "TR -NaN at (0, 64), others 0.5" },
    { 0, 128, 0, 1, 0, 0x7f800001u, 0xbf000000u, "TR 7f800001 at (0, 128), others -0.5" },
    { 0, 192, 0, 1, 0, 0x7d000000u, 0x3f000000u, "TR 2^123 at (0, 192), others 0.5" },
    { 64, 0, 0, 2, 1, 0x7f800000u, 0x3f000000u, "BL +inf at (64, 0), others 0.5, left column" },
    { 128, 0, 0, 2, 1, 0xff800000u, 0x3f000000u, "BL -inf at (128, 0), others 0.5, left column" },
    { 320, 64, 1, 1, 0, 0x7f7fffffu, 0x3f000000u, "right angle TR, TR max at (320, 64), others 0.5" },
    { 320, 416, 1, 1, 0, 0x7f7fffffu, 0x3f000000u, "right angle TR, TR max at (320, 416), others 0.5" },
};
#define NNEG (122u << 18)               /* negative probes: fields 133 .. 254, 2^18 each */
#define NPROBES (NNEG + (130u << 18))   /* and positive ones: fields 125 .. 254 */

static uint32_t texG;
static uint32_t green(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0x07e0; }

/* probe i in the ISP's order */
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
        label(p, 280, "%s: the 31 targets' words (rows 470, 471)", V[p].name);
        label(p, (V[p].y / 32) * COLS + V[p].x / 32, "%s: the last render", V[p].name);
    }
}

static void vtx(int x, int y, uint32_t z, int end)
{
    ta_vertex3(f32b_int(x), f32b_int(y), z, 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, end);
}

/* one render with the background at b and the triangle compared by mode; the target pixel green? */
static int render(const variant_t *v, uint32_t b, int mode, int k)
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
    {
        static const int SH[2][3][2] = { { { 0, 0 }, { 32, 0 }, { 0, 32 } }, { { 0, 0 }, { 32, 0 }, { 32, 32 } } };
        int n;
        for(n = 0; n < 3; n++)
            vtx(v->x + SH[v->shape][n][0], v->y + SH[v->shape][n][1], n == v->bad ? v->s : v->o, n == 2);
    }
    ta_list_end();
    pvr_frame_render();
    pvrio_read32(SETUP_FB + ((uint32_t)(v->y + (v->col ? k : 0)) * 640 + (uint32_t)(v->x + (v->col ? 0 : k))) * 4, 1, &w);
    return (w & 0xffffff) == 0x00ff00;
}

static void test_page(int page) { (void)page; }

static void test_draw_page(int page)
{
    const variant_t *v = &V[page];
    static uint32_t res[2][32];
    int k;
    pvrio_unhashed = 1;
    for(k = 0; k < 31; k++) {
        /* the number of probes the depth is above: lo .. hi */
        uint32_t lo = 0, hi = NPROBES;
        while(lo < hi) {
            const uint32_t mid = lo + (hi - lo + 1) / 2;       /* is it above probe mid - 1? */
            if(render(v, probe(mid - 1), DepthMode_Greater, k)) lo = mid; else hi = mid - 1;
        }
        res[0][k] = lo;
        res[1][k] = lo < NPROBES ? (uint32_t)render(v, probe(lo), DepthMode_Equal, k) : 0;
    }
    res[0][31] = res[1][31] = 0;
    pvrio_w32(SETUP_FB + 470u * 640 * 4, res[0], 32);
    pvrio_w32(SETUP_FB + 471u * 640 * 4, res[1], 32);
    pvrio_unhashed = 0;
}
