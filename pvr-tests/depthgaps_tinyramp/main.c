/* Ramps across zero with slopes about 2^-126 a pixel (2026-11-20, hunt depthgaps round 2; after depthgaps_tiny).

   zspec: the setup's numbers have the range of a float without denormals; a slope below 2^-126 is zero. Round 1 of
   hunt depthgaps saw ramps with slopes of exactly +-2^-126 (1 + 2^-19) differ from refsw3 at 23 pixels (those cells
   were left out of depthgaps_tiny). Here such ramps, one 32 x 32 quad per tile, z = (n0 + nx X + ny Y) 2^-146 at
   the quad's corners (X, Y in 0, 32 from its top left; corners rounded to floats), against the background at 0
   with DepthMode Equal, Greater, Less (pages 0 .. 2): the sign of every pixel's stored depth. Cells 12 and 13 are
   round 1's: corners at (64, 64) + (0 .. 32), n = 2 (2^19 + o) - (2^19 + 1) (2 X - 1) + (2^19 + 3) (2 Y - 1),
   o = 0, 1. */
#define NUM_PAGES 3
#include "pvrtest2.h"
#include "texgen.h"

typedef struct { int64_t n0, nx, ny; } ramp_t;
#define NR 12
#define S1 ((1 << 20) + 2)                      /* 2^-126 (1 + 2^-19) in units of 2^-146 */
static const ramp_t R[NR] = {
    { -16LL * S1, S1, 0 },
    { 16LL * S1, -S1, 0 },
    { -16LL * (1 << 20) + (1 << 19), 1 << 20, 0 },
    { -32LL * S1, S1, S1 },
    { 0, -S1, S1 + 4 },
    { -16LL * 3 * (1 << 19) + 7, 3 * (1 << 19), 0 },
    { -16LL * (1 << 21), (1 << 19) + 2, 1 << 21 },
    { 5, (1 << 21) + 2, -S1 },
    { -16LL * S1 + 3, S1, 64 },
    { 16LL * S1 - 3, -S1, -64 },
    { -16LL * S1 - (1 << 19), S1, 1 << 15 },
    { -(1LL << 19), S1, -S1 },
};
static const int MD[NUM_PAGES] = { DepthMode_Equal, DepthMode_Greater, DepthMode_Less };
static const char *MN[NUM_PAGES] = { "Equal", "Greater", "Less" };
static uint32_t texW;

static uint32_t white(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0xffff; }

/* n 2^e as a float: n to 24 bits, to nearest, ties to even; below 2^-126 on the denormal grid, cut */
static uint32_t fbits(int64_t n, int e)
{
    const uint32_t sign = n < 0 ? 0x80000000u : 0;
    uint64_t m = n < 0 ? (uint64_t)-n : (uint64_t)n;
    int sh = 0, f;
    if(!m)
        return sign;
    while((m >> sh) >= (1ull << 24))
        sh++;
    if(sh) {
        const uint64_t rest = m & ((1ull << sh) - 1), half = 1ull << (sh - 1);
        m >>= sh;
        e += sh;
        if(rest > half || (rest == half && (m & 1)))
            m++;
        if(m >= (1ull << 24)) {
            m >>= 1;
            e++;
        }
    }
    while(m < (1ull << 23)) {
        m <<= 1;
        e--;
    }
    f = e + 150;
    if(f <= 0)
        return sign | (uint32_t)(m >> (1 - f));
    return sign | (uint32_t)f << 23 | (uint32_t)(m & 0x7fffffu);
}

static void test_setup(void)
{
    const tex_layout_t l = { TEX_TWIDDLED, 16, 8, 8, 0 };
    texW = tex_build(&l, white, NULL);
}

static void test_describe(void)
{
    int p, t;
    for(p = 0; p < NUM_PAGES; p++) {
        for(t = 0; t < NR; t++)
            label(p, t, "ramp n0 %lld, nx %lld, ny %lld (2^-146) against the background 0, %s", (long long)R[t].n0, (long long)R[t].nx, (long long)R[t].ny, MN[p]);
        label(p, 42, "round 1's ramp, o = 0, %s", MN[p]);
        label(p, 43, "round 1's ramp, o = 1 (at (96, 64)), %s", MN[p]);
    }
}

static void begin(int mode)
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
    tcw.TexAddr = texW >> 3;
    ta_polygon(pcw, isp, tsp, tcw);
}

static void test_page(int page)
{
    int t, k;
    pvr_frame.bg_color = 0x000040;
    pvr_frame.bg_depth = 0;
    ta_list_begin(ListType_Opaque);
    for(t = 0; t < NR; t++) {
        begin(MD[page]);
        for(k = 0; k < 4; k++) {
            const int X = (k & 1) * 32, Y = (k >> 1) * 32;
            ta_vertex3(f32b_int(32 * t + X), f32b_int(Y), fbits(R[t].n0 + R[t].nx * X + R[t].ny * Y, -146),
                       0x3e800000u, 0x3e800000u, 0xffffffffu, 0, k == 3);
        }
    }
    for(t = 0; t < 2; t++) {
        begin(MD[page]);
        for(k = 0; k < 4; k++) {
            const int X = (k & 1) * 32, Y = (k >> 1) * 32;
            const int64_t n = 2 * ((1 << 19) + t) - ((1LL << 19) + 1) * (2 * X - 1) + ((1LL << 19) + 3) * (2 * Y - 1);
            ta_vertex3(f32b_int(64 + 32 * t + X), f32b_int(64 + Y), fbits(n, -146), 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, k == 3);
        }
    }
    ta_list_end();
}
