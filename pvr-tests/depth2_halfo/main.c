/* Ordinary planes with HALF_OFFSET bit 0 off: where the depth unit starts (2026-11-20, hunt depth2; after
   depth2_halfc).

   depth2_halfc: with HALF_OFFSET 6 (the ISP samples at pixel corners) an overflowing plane's start is evaluated at
   the tile's first pixel corner (in tile (0, 0) an infinite slope times x0 = 0 is 0 there). For an ordinary plane
   refsw3 started at the centre and went half steps back; evaluated at the corner the start is rounded differently
   (the setup's 24 bit sums, its 20 bits, the slopes' grid), and most triangles then store other words at some
   pixels (in random ones 39 % of the pixels, by 1 unit of the 19 bits mostly). Here 8 random ordinary triangles
   over tile (0, 0) (gen.cpp: corners within 16 pixels of it, depths 0.5 .. 8), 5 target pixels each where the two
   differ; the words as zword reads them (a binary search with the background as the probe, one render a step, the
   pixel read back), with HALF_OFFSET 6 for every render: rows 470, 471 of page p (triangle p): the index of the
   last probe the depth is above, + 1 (the probes: the positive words of fields 125 .. 140, low 5 bits clear); 1
   when it equals the next probe. */
#define NUM_PAGES 8
#define TEST_DRAW_PAGE
#include "pvrtest2.h"
#include "texgen.h"

typedef struct { int x, y; uint32_t corner, centre; } target_t;
typedef struct { int x16[3], y16[3]; uint32_t z[3]; target_t t[5]; } case_t;
#include "cases.h"

#define NPROBES (16u << 18)             /* positive probes: fields 125 .. 140 */

static uint32_t texG;
static uint32_t green(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0x07e0; }

static uint32_t probe(uint32_t i) { return (125u + (i >> 18)) << 23 | ((i & 262143u) << 5); }

/* n / 16 as a float (|n| < 2^24) */
static uint32_t f16(int n)
{
    const uint32_t sign = n < 0 ? 0x80000000u : 0;
    uint32_t m = (uint32_t)(n < 0 ? -n : n);
    int e = -4;
    if(!m)
        return sign;
    while(m < (1u << 23)) {
        m <<= 1;
        e--;
    }
    return sign | (uint32_t)(e + 150) << 23 | (m & 0x7fffffu);
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
        label(p, 280, "triangle %d, HALF_OFFSET 6: the %d targets' words (rows 470, 471)", p, NTGT);
        label(p, 0, "triangle %d, HALF_OFFSET 6: the last render", p);
    }
}

static int render(const case_t *c, uint32_t b, int mode, int k)
{
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    uint32_t w;
    int n;
    pvr_frame.bg_color = 0x000040;
    pvr_frame.bg_depth = b;
    pvr_frame_begin();
    pvrio_reg(HALF_OFFSET_addr, 6);
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
    for(n = 0; n < 3; n++)
        ta_vertex3(f16(c->x16[n]), f16(c->y16[n]), c->z[n], 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, n == 2);
    ta_list_end();
    pvr_frame_render();
    pvrio_read32(SETUP_FB + ((uint32_t)c->t[k].y * 640 + (uint32_t)c->t[k].x) * 4, 1, &w);
    return (w & 0xffffff) == 0x00ff00;
}

static void test_page(int page) { (void)page; }

static void test_draw_page(int page)
{
    const case_t *c = &CASES[page];
    static uint32_t res[2][32];
    int k;
    pvrio_unhashed = 1;
    for(k = 0; k < NTGT; k++) {
        uint32_t lo = 0, hi = NPROBES;
        while(lo < hi) {
            const uint32_t mid = lo + (hi - lo + 1) / 2;       /* is it above probe mid - 1? */
            if(render(c, probe(mid - 1), DepthMode_Greater, k)) lo = mid; else hi = mid - 1;
        }
        res[0][k] = lo;
        res[1][k] = lo < NPROBES ? (uint32_t)render(c, probe(lo), DepthMode_Equal, k) : 0;
    }
    for(k = NTGT; k < 32; k++)
        res[0][k] = res[1][k] = 0;
    pvrio_w32(SETUP_FB + 470u * 640 * 4, res[0], 32);
    pvrio_w32(SETUP_FB + 471u * 640 * 4, res[1], 32);
    pvrio_unhashed = 0;
    pvrio_reg(HALF_OFFSET_addr, 7);
}
