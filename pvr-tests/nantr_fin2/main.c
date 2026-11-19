/* A finite start in an overflowing depth plane, with the register's steps (2026-11-19, after pvr-tests/nantr_fin).

   nantr_fin: in the depth unit's regime C (a slope overflows, the register steps by one unit a pixel and wraps every
   32, pvr_isp_depth.h kWrap) a finite start of the plane is not dropped: the tile's first pixel shows it (the
   positive ones as themselves from 2^102 on). Here the steps are added to it: the triangle V0 = (16, 0) at S (+-inf),
   V1 = (0, 0) and V2 = (16, 16) at Z, so that a x0 and b y0 are infinities of opposite sign, V0's and V2's terms of
   c are S * 0 and Z * 0, and the start is c / det = Z exactly (Z below 2^120: c = -256 Z); the register at pixel
   (i, j) is Z + sign(S) (i - j) steps. The words are read as zword reads them (a binary search with the background
   as the probe, one render a step, the pixel read back; results in rows 470, 471): targets k = 0 .. 15 the top
   row (k, 0), k = 16 .. 30 the right column (15, k - 15).
   Pages: S +inf with Z 2^119, 1.5 2^119, -2^119, 2^110; S -inf with Z 2^119, -2^119, -2^110, 1.5 2^119. */
#define NUM_PAGES 8
#define TEST_DRAW_PAGE
#include "pvrtest2.h"
#include "texgen.h"

typedef struct { uint32_t s, z; const char *name; } variant_t;
static const variant_t V[NUM_PAGES] = {
    { 0x7f800000u, 0x7b000000u, "S +inf, Z 2^119" },
    { 0x7f800000u, 0x7b400000u, "S +inf, Z 1.5 2^119" },
    { 0x7f800000u, 0xfb000000u, "S +inf, Z -2^119" },
    { 0x7f800000u, 0x76800000u, "S +inf, Z 2^110" },
    { 0xff800000u, 0x7b000000u, "S -inf, Z 2^119" },
    { 0xff800000u, 0xfb000000u, "S -inf, Z -2^119" },
    { 0xff800000u, 0xf6800000u, "S -inf, Z -2^110" },
    { 0xff800000u, 0x7b400000u, "S -inf, Z 1.5 2^119" },
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
        label(p, 280, "%s: the 31 targets' words (rows 470, 471)", V[p].name);
        label(p, 0, "%s: the last render", V[p].name);
    }
}

static void vtx(int x, int y, uint32_t z, int end)
{
    ta_vertex3(f32b_int(x), f32b_int(y), z, 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, end);
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
    vtx(16, 0, v->s, 0); vtx(0, 0, v->z, 0); vtx(16, 16, v->z, 1);
    ta_list_end();
    pvr_frame_render();
    pvrio_read32(SETUP_FB + (uint32_t)(k < 16 ? k : (k - 15) * 640 + 15) * 4, 1, &w);
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
