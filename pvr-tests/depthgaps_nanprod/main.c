/* A NaN's fraction in the depth's slopes and constant (2026-11-20, hunt depthgaps round 3; after depthgaps_dzinf).

   depthgaps_dzinf: in the mean of a triangle of determinant zero, two corners of exponent field 255 of opposite
   signs add as their fractions say (the larger one's infinity; equal ones +0). Do the products of the slopes and
   the constant, z * (y1 - y2) .. z * e, keep a NaN's fraction too? Here triangles with two such corners whose terms
   of a slope come out of opposite signs: V0 = (0, 0), V1 = (4, 0), V2 = (0, 4) at z0, z1, z2 (a = 4 (z1 - z0), b =
   4 (z2 - z0), c = 16 z0, each a sum of products): with infinities only, the terms cancel to +0; with the NaN's
   fraction, the NaN's term wins, and the plane's register (regime C: steps of 2^127) starts and steps otherwise.
   Every pixel inside (i + j < 3) read as depthgaps_bigslope reads it (a binary search with the background as the
   probe; row 470 word k: the number of probes below target k's depth; row 471: 1 where it equals the next one).
   Page p: P[p]. */
#define NUM_PAGES 8
#define TEST_DRAW_PAGE
#include "pvrtest2.h"
#include "texgen.h"

typedef struct { int l; uint32_t z[3]; const char *name; } tri_t;
static const tri_t P[NUM_PAGES] = {
    { 4, { 0x7fc00000u, 0x7f800000u, 0x3f800000u }, "NaN 7fc00000, +inf, 1" },
    { 4, { 0xffc00000u, 0xff800000u, 0x3f800000u }, "NaN ffc00000, -inf, 1" },
    { 4, { 0x7f800000u, 0x7fc00000u, 0x3f800000u }, "+inf, NaN 7fc00000, 1" },
    { 4, { 0x7fc00000u, 0x3f800000u, 0x7f800000u }, "NaN 7fc00000, 1, +inf" },
    { 4, { 0x7f800000u, 0x3f800000u, 0x7fc00000u }, "+inf, 1, NaN 7fc00000" },
    { 4, { 0x7f800001u, 0x7f800000u, 0x3f800000u }, "NaN 7f800001, +inf, 1" },
    { 4, { 0x7fc00000u, 0x7f800001u, 0x3f800000u }, "NaN 7fc00000, NaN 7f800001, 1" },
    { 4, { 0x7f800000u, 0x7f800000u, 0x3f800000u }, "+inf, +inf, 1" },
};
#define NNEG (122u * 0x8001u)           /* negative probes: fields 133 .. 254, 0x7fffe0 then the 0x100 grid */
#define NPROBES (NNEG + 1u + (130u << 18))      /* 0, positive ones: fields 125 .. 254 */

static uint32_t texG;
static uint32_t green(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0x07e0; }

/* probe i in the ISP's order */
static uint32_t probe(uint32_t i)
{
    if(i < NNEG) {
        const uint32_t f = 133u + i / 0x8001u, g = i % 0x8001u;
        return 0x80000000u | f << 23 | (g ? 0x800000u - 0x100u * g : 0x7fffe0u);
    }
    i -= NNEG;
    if(!i)
        return 0;
    i--;
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
        label(p, 280, "%s: the depths of the pixels inside, (0, 0), (1, 0), .. by rows (rows 470, 471)", P[p].name);
        label(p, 0, "%s: the last render", P[p].name);
    }
}

/* one render with the background at b and the triangle compared by mode; pixel (x, y) green? */
static int render(const tri_t *t, uint32_t b, int mode, int x, int y)
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
    ta_vertex3(f32b_int(0), f32b_int(0), t->z[0], 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, 0);
    ta_vertex3(f32b_int(t->l), f32b_int(0), t->z[1], 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, 0);
    ta_vertex3(f32b_int(0), f32b_int(t->l), t->z[2], 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, 1);
    ta_list_end();
    pvr_frame_render();
    pvrio_read32(SETUP_FB + ((uint32_t)y * 640 + (uint32_t)x) * 4, 1, &w);
    return (w & 0xffffff) == 0x00ff00;
}

static void test_page(int page) { (void)page; }

static void test_draw_page(int page)
{
    const tri_t *t = &P[page];
    static uint32_t res[2][32];
    int i, j, k = 0;
    pvrio_unhashed = 1;
    for(j = 0; j < t->l - 1; j++)
        for(i = 0; i + j < t->l - 1; i++, k++) {
            uint32_t lo = 0, hi = NPROBES;
            while(lo < hi) {
                const uint32_t mid = lo + (hi - lo + 1) / 2;       /* is it above probe mid - 1? */
                if(render(t, probe(mid - 1), DepthMode_Greater, i, j)) lo = mid; else hi = mid - 1;
            }
            res[0][k] = lo;
            res[1][k] = lo < NPROBES ? (uint32_t)render(t, probe(lo), DepthMode_Equal, i, j) : 0;
        }
    pvrio_w32(SETUP_FB + 470u * 640 * 4, res[0], 32);
    pvrio_w32(SETUP_FB + 471u * 640 * 4, res[1], 32);
    pvrio_unhashed = 0;
}
