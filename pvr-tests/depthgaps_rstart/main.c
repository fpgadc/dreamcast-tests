/* A finite start below zero in an overflowing depth plane: its rounding onto the register's grid (2026-11-20, hunt
   depthgaps round 2; after pvr-tests/nantr_fin2).

   nantr_fin, nantr_fin2: in the depth unit's regime C (a slope overflows, the register steps by 2^127 a pixel, its
   sign at 2^131) a finite start is kept, its bits from 2^102 up; a start below zero was taken toward zero onto that
   grid by refsw3 without a test (GAP). Here nantr_fin2's triangle, V0 = (16, 0) at S (+-inf), V1 = (0, 0) and V2 =
   (16, 16) at Z, whose start is Z exactly and whose register at pixel (k, 0) is Z + sign(S) k steps, with Z below
   zero and bits under 2^102: toward zero, -0.5 2^102 is 0 and the pixel one step down -2^127 (0xff000000), one step
   up 2^127; down (floor), it is -2^102 (above -2^127, below 0), one step down 0xff000100, one step up 2^127 - 2^102
   (0x7effffe0 to 19 bits). Pixels (0, 0), (1, 0), (2, 0) read as depthgaps_bigslope reads them (a binary search with
   the background as the probe; row 470 word k: the number of probes below the depth, row 471: 1 where it equals the next one).
   Page p: Z[p / 2], S -inf (p even) or +inf (p odd). */
#define NUM_PAGES 20
#define TEST_DRAW_PAGE
#include "pvrtest2.h"
#include "texgen.h"

typedef struct { uint32_t z; const char *name; } start_t;
static const start_t Z[NUM_PAGES / 2] = {
    { 0xf2000000u, "Z -2^101 (-0.5 of 2^102)" },
    { 0xbf000000u, "Z -0.5" },
    { 0xfb000010u, "Z -(2^119 + 2^100) (-(2^17 + 0.25) 2^102)" },
    { 0xfb000020u, "Z -(2^119 + 2^101) (-(2^17 + 0.5) 2^102)" },
    { 0xfb000030u, "Z -(2^119 + 3 2^100) (-(2^17 + 0.75) 2^102)" },
    { 0xf7800010u, "Z -(2^112 + 2^93) (-(2^10 + 2^-9) 2^102)" },
    { 0xfb000040u, "Z -(2^119 + 2^102) (exact: -(2^17 + 1) 2^102)" },
    { 0x7b000020u, "Z 2^119 + 2^101 ((2^17 + 0.5) 2^102)" },
    { 0xec800000u, "Z -2^90" },
    { 0xf2c00000u, "Z -1.5 2^102" },
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
        label(p, 280, "%s, S %s: the depths of pixels (0, 0), (1, 0), (2, 0) (rows 470, 471)", Z[p / 2].name, p & 1 ? "+inf" : "-inf");
        label(p, 0, "%s, S %s: the last render", Z[p / 2].name, p & 1 ? "+inf" : "-inf");
    }
}

static void vtx(int x, int y, uint32_t z, int end)
{
    ta_vertex3(f32b_int(x), f32b_int(y), z, 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, end);
}

/* one render with the background at b and the triangle compared by mode; pixel (x, 0) green? */
static int render(int page, uint32_t b, int mode, int x)
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
    vtx(16, 0, page & 1 ? 0x7f800000u : 0xff800000u, 0); vtx(0, 0, Z[page / 2].z, 0); vtx(16, 16, Z[page / 2].z, 1);
    ta_list_end();
    pvr_frame_render();
    pvrio_read32(SETUP_FB + (uint32_t)x * 4, 1, &w);
    return (w & 0xffffff) == 0x00ff00;
}

static void test_page(int page) { (void)page; }

static void test_draw_page(int page)
{
    static uint32_t res[2][32];
    int k;
    pvrio_unhashed = 1;
    for(k = 0; k < 3; k++) {
        uint32_t lo = 0, hi = NPROBES;
        while(lo < hi) {
            const uint32_t mid = lo + (hi - lo + 1) / 2;       /* is it above probe mid - 1? */
            if(render(page, probe(mid - 1), DepthMode_Greater, k)) lo = mid; else hi = mid - 1;
        }
        res[0][k] = lo;
        res[1][k] = lo < NPROBES ? (uint32_t)render(page, probe(lo), DepthMode_Equal, k) : 0;
    }
    pvrio_w32(SETUP_FB + 470u * 640 * 4, res[0], 32);
    pvrio_w32(SETUP_FB + 471u * 640 * 4, res[1], 32);
    pvrio_unhashed = 0;
}
