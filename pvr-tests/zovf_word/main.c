/* The stored depth words of a tile whose start is an infinity on one side only (2026-11-19, after pvr-tests/zovf_sat;
   hunt zovf), by pvr-tests/zword's method (a binary search per pixel with the background as the probe, one render a
   step, the results written into rows 470, 471 of the framebuffer; see zword for the probes and the result words).

   zovf_sat: such a tile (the constant c 2^128 or more, a x0 + b y0 below) stores exactly +2^127 at its first pixel and
   less than that at the others when c > 0 (the slopes below zero); when c < 0 (slopes above zero) -2^127 near the
   first pixel and above it (not Equal, not Less) further on. Here the words of the top row: the triangle (corners
   (x, y), (x + 32, y), (x, y + 32), the right-angle corner at S, the others at 0.5; zovf's and zovf_sat's triangles),
   pixels (x + k, y), k = 0 .. 30. */
#define NUM_PAGES 4
#define TEST_DRAW_PAGE
#include "pvrtest2.h"
#include "texgen.h"

typedef struct { int x, y, tr; uint32_t s; const char *name; } variant_t;
static const variant_t V[NUM_PAGES] = {
    { 448, 32, 0, 0x78800000u, "zovf cell 34: right angle 2^114 at (448, 32), c = 2^128" },
    { 0, 160, 0, 0x792aaaaeu, "zovf_sat cell 100: c = 2^128 + 2^106 at (0, 160)" },
    { 0, 192, 0, 0xf9124927u, "zovf_sat cell 120: c = -(2^128 + 2^106) at (0, 192)" },
    { 64, 192, 0, 0xf8e71c73u, "zovf_sat cell 122: c = -(1 + 2^-6) 2^128 at (64, 192)" },
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
    int p, k;
    for(p = 0; p < NUM_PAGES; p++)
        for(k = 0; k < 31; k++)
            label(p, k, "%s, pixel %d of its top row", V[p].name, k);
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
    const uint32_t half = 0x3f000000u;
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
    if(v->tr) { vtx(v->x, v->y, half, 0); vtx(v->x + 32, v->y, v->s, 0); vtx(v->x, v->y + 32, half, 1); }
    else { vtx(v->x, v->y, v->s, 0); vtx(v->x + 32, v->y, half, 0); vtx(v->x, v->y + 32, half, 1); }
    ta_list_end();
    pvr_frame_render();
    pvrio_read32(SETUP_FB + ((uint32_t)v->y * 640 + (uint32_t)(v->x + k)) * 4, 1, &w);
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
