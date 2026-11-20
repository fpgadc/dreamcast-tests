/* A FINITE slope below zero, off the 2^102 grid, beside an overflowed one (regime C) (2026-11-20, hunt depth2; after
   zovf_creg, zovf_wrap).

   pvr_isp_depth.h GAP: in regime C the slope that did not overflow steps the register as a number, cut to 20 bits and
   to the register's grid of 2^102 toward zero (zovf_wrap W2: a slope of -2^-6 a pixel adds 0, not -2^102); for slopes
   below zero of 2^102 or more off that grid toward zero and down differ by one unit (2^102) a pixel, which the words
   below zero (a block of E 128: a grid of 2^112) hide. Here the register above zero: zovf_creg's C0 (B (0, 0) at
   -2^118, R (32, 0) at 2^123: a = +inf, one step of 2^127 a pixel along x from a start of 0; D (0, 32) at -2^118 +
   32 F: the finite slope F a row), the pixels (1, j) at 2^127 + j F, stored to 19 bits (2^108 there): toward zero
   and down differ by a grid step at most rows. The words as zword reads them (a binary search with the background as
   the probe, one render a step, the pixel read back; rows 470, 471 of the page: the index of the last probe the depth
   is above, + 1; 1 when it equals the next probe). Pages: F = -(8192 + 1/2), -(8192 + 3/4), -(256 + 1/4),
   -(2^18 + 1/2), +(8192 + 1/2), -(127 + 1/2) units of 2^102; targets (1, j), j = 1, 2, 3, 4, 6, 8, 11, 15, 20, 25,
   30, then (0, 1), (2, 1). */
#define NUM_PAGES 6
#define TEST_DRAW_PAGE
#include "pvrtest2.h"
#include "texgen.h"

static const uint32_t ZD[NUM_PAGES] = { 0xfba00200u, 0xfba00300u, 0xfa900400u, 0xfe010010u, 0x7b400400u, 0xfa87f800u };
static const char *FNAME[NUM_PAGES] = { "-(8192 + 1/2)", "-(8192 + 3/4)", "-(256 + 1/4)", "-(2^18 + 1/2)", "+(8192 + 1/2)", "-(127 + 1/2)" };
#define NT 13
static const int TX[NT] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 2 }, TY[NT] = { 1, 2, 3, 4, 6, 8, 11, 15, 20, 25, 30, 1, 1 };
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
        label(p, 280, "F %s 2^102: the %d targets' words (rows 470, 471)", FNAME[p], NT);
        label(p, 0, "F %s 2^102: the last render", FNAME[p]);
    }
}

static void vtx(int x, int y, uint32_t z, int end)
{
    ta_vertex3(f32b_int(x), f32b_int(y), z, 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, end);
}

/* one render with the background at b and the triangle compared by mode; the target pixel green? */
static int render(int page, uint32_t b, int mode, int k)
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
    vtx(0, 0, 0xfa800000u, 0); vtx(32, 0, 0x7d000000u, 0); vtx(0, 32, ZD[page], 1);
    ta_list_end();
    pvr_frame_render();
    pvrio_read32(SETUP_FB + ((uint32_t)TY[k] * 640 + (uint32_t)TX[k]) * 4, 1, &w);
    return (w & 0xffffff) == 0x00ff00;
}

static void test_page(int page) { (void)page; }

static void test_draw_page(int page)
{
    static uint32_t res[2][32];
    int k;
    pvrio_unhashed = 1;
    for(k = 0; k < NT; k++) {
        uint32_t lo = 0, hi = NPROBES;
        while(lo < hi) {
            const uint32_t mid = lo + (hi - lo + 1) / 2;       /* is it above probe mid - 1? */
            if(render(page, probe(mid - 1), DepthMode_Greater, k)) lo = mid; else hi = mid - 1;
        }
        res[0][k] = lo;
        res[1][k] = lo < NPROBES ? (uint32_t)render(page, probe(lo), DepthMode_Equal, k) : 0;
    }
    for(k = NT; k < 32; k++)
        res[0][k] = res[1][k] = 0;
    pvrio_w32(SETUP_FB + 470u * 640 * 4, res[0], 32);
    pvrio_w32(SETUP_FB + 471u * 640 * 4, res[1], 32);
    pvrio_unhashed = 0;
}
