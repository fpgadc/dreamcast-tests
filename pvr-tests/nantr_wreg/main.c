/* The word of the overflowing plane's register between whole steps, where the model's formula is not measured yet
   (2026-11-19, after pvr-tests/nantr_fin, nantr_fin2).

   In the depth unit's regime C the register is the plane's start plus +-2^127 a pixel, two's complement with its sign
   at 2^131; pvr_isp_depth.h WrapWord gives its word from zword's words at whole steps and nantr_fin / nantr_fin2's
   between them. Below zero the formula has a carry: from -8.5 steps to -8 (and -12.25 to -12) it gives words above
   those of -8 (-12), not between those of -9 and -8. Here: a spike from the edge V1 = (7/16, 7/16), V2 = (9/16, 9/16)
   at Z (on the line y = x through the origin: V0's term of c is S * 0, a x0 and b y0 are infinities of opposite
   sign, the start is Z) to V0 = (X, 1/2) at S (+-inf): it covers the pixel centers (k + 1/2, 1/2), k = 0 .. X - 1,
   at Z +- k steps; |Z| <= 2^124 keeps every finite product below 2^128. Words as zword reads them (a binary search
   with the background as the probe, one render a step, the pixel read back; results in rows 470, 471): targets
   k = 0 .. 13, the pixels (k, 0).
   Pages: X 9, S -inf, Z -2^124; X 9, S -inf, Z -2^123; X 13, S -inf, Z 2^124; X 13, S -inf, Z -2^124; X 13, S +inf,
   Z -2^124; X 9, S +inf, Z 2^124; X 13, S -inf, Z 1.5 2^123; X 13, S -inf, Z -1.5 2^123. */
#define NUM_PAGES 8
#define TEST_DRAW_PAGE
#include "pvrtest2.h"
#include "texgen.h"

typedef struct { int x; uint32_t s, z; const char *name; } variant_t;
static const variant_t V[NUM_PAGES] = {
    { 9, 0xff800000u, 0xfd800000u, "X 9, S -inf, Z -2^124" },
    { 9, 0xff800000u, 0xfd000000u, "X 9, S -inf, Z -2^123" },
    { 13, 0xff800000u, 0x7d800000u, "X 13, S -inf, Z 2^124" },
    { 13, 0xff800000u, 0xfd800000u, "X 13, S -inf, Z -2^124" },
    { 13, 0x7f800000u, 0xfd800000u, "X 13, S +inf, Z -2^124" },
    { 9, 0x7f800000u, 0x7d800000u, "X 9, S +inf, Z 2^124" },
    { 13, 0xff800000u, 0x7d400000u, "X 13, S -inf, Z 1.5 2^123" },
    { 13, 0xff800000u, 0xfd400000u, "X 13, S -inf, Z -1.5 2^123" },
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
        label(p, 280, "%s: the 14 targets' words (rows 470, 471)", V[p].name);
        label(p, 0, "%s: the last render", V[p].name);
    }
}

/* x, y in 1/16 pixel */
static void vtx(int x16, int y16, uint32_t z, int end)
{
    ta_vertex3(f32b_scaled(x16, -4), f32b_scaled(y16, -4), z, 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, end);
}

/* one render with the background at b and the triangle compared by mode; target k green? */
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
    vtx(16 * v->x, 8, v->s, 0); vtx(7, 7, v->z, 0); vtx(9, 9, v->z, 1);
    ta_list_end();
    pvr_frame_render();
    pvrio_read32(SETUP_FB + (uint32_t)k * 4, 1, &w);
    return (w & 0xffffff) == 0x00ff00;
}

static void test_page(int page) { (void)page; }

static void test_draw_page(int page)
{
    const variant_t *v = &V[page];
    static uint32_t res[2][32];
    int k;
    pvrio_unhashed = 1;
    for(k = 0; k < 14; k++) {
        uint32_t lo = 0, hi = NPROBES;
        while(lo < hi) {
            const uint32_t mid = lo + (hi - lo + 1) / 2;       /* is it above probe mid - 1? */
            if(render(v, probe(mid - 1), DepthMode_Greater, k)) lo = mid; else hi = mid - 1;
        }
        res[0][k] = lo;
        res[1][k] = lo < NPROBES ? (uint32_t)render(v, probe(lo), DepthMode_Equal, k) : 0;
    }
    pvrio_w32(SETUP_FB + 470u * 640 * 4, res[0], 32);
    pvrio_w32(SETUP_FB + 471u * 640 * 4, res[1], 32);
    pvrio_unhashed = 0;
}
