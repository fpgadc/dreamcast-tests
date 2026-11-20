/* The stored depth of a sloped polygon below zero (2026-11-19, hunt depthgaps (c); after depthgaps_neggrid).

   depthgaps_neggrid: a flat polygon at a negative depth is stored on a coarser grid than a positive one: its
   magnitude rounded up to a multiple of 0x100 in the word (0x20 is the stored depth's last bit). Is a sloped one's
   depth first cut to the 19 bits and then rounded up, or rounded up from all its bits? A ramp whose setup is exact:
   corners (0.5, 0.5) at depth 0, (64.5, 0.5) at 64 A, (0.5, 64.5) at 64 B (A = sa a 2^-20, B = sb b 2^-20, a and b
   of 20 and 16 bits): every product and sum of the setup is exact, the start at the tile's first pixel is exactly 0,
   and the pixel (i, j) of tile (0, 0) has the depth v = A i + B j exactly (the steps are fixed point). The ramp is
   drawn first (DepthMode Always, textured green), then at every pixel zexfil's probe triangle (common/zexfil_probes.h:
   exact for any depth of 19 bits) at a depth P(v), textured white, DepthMode Equal (or Greater). White where the probe
   passes. P(v), the word of v's magnitude with its sign:
     negative v:  F = the word cut to a multiple of 0x100 (toward zero), or F + 0x100 (at the top of the binade the
                  word with fraction 0x7fffe0)
     positive v:  the word cut to 19 bits (as stored: zexfil, zmeas)
   Pages: 0 A, B < 0, F, Equal; 1 the same, F + 0x100, Equal; 2 F, Greater; 3 A, B > 0, Equal; 4 A < 0 < B, F or
   the positive word, Equal; 5 the same with F + 0x100. */
#define NUM_PAGES 6
#include "pvrtest2.h"
#include "texgen.h"
#include "zexfil_probes.h"

static const int32_t RA = 0x0b69d3, RB = 54321;         /* a, b */
static const int SA[NUM_PAGES] = { -1, -1, -1, 1, -1, -1 }, SB[NUM_PAGES] = { -1, -1, -1, 1, 1, 1 };
static const char *WHAT[NUM_PAGES] = { "A, B < 0, probe F, Equal", "A, B < 0, probe F + 0x100, Equal", "A, B < 0, probe F, Greater",
                                       "A, B > 0, probe the 19 bit word, Equal", "A < 0 < B, probe F, Equal", "A < 0 < B, probe F + 0x100, Equal" };
static uint32_t texG, texW;

static uint32_t green(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0x07e0; }
static uint32_t white(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0xffff; }

static void test_setup(void)
{
    const tex_layout_t l = { TEX_TWIDDLED, 16, 8, 8, 0 };
    texG = tex_build(&l, green, NULL);
    texW = tex_build(&l, white, NULL);
}

static void test_describe(void)
{
    int p;
    for(p = 0; p < NUM_PAGES; p++)
        label(p, 0, "ramp a %d, b %d: %s", (int)RA, (int)RB, WHAT[p]);
}

static void begin(int mode, uint32_t tex)
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
    tcw.TexAddr = tex >> 3;
    ta_polygon(pcw, isp, tsp, tcw);
}

/* the probe's word for pixel (i, j) of page p */
static uint32_t probe_word(int p, int i, int j)
{
    const int64_t n = (int64_t)SA[p] * RA * i + (int64_t)SB[p] * RB * j;
    uint32_t w, m;
    int64_t a = n < 0 ? -n : n;
    int e = 0;
    if(!a)
        return 0;
    while(a >= 0x01000000) { a >>= 1; e++; }     /* cut toward zero to 24 bits */
    w = f32b_scaled((int32_t)a, e - 20);
    if(n > 0)
        return w & ~0x1fu;
    w = (w & ~0xffu) | 0x80000000u;
    if(p == 1 || p == 5) {
        m = w & 0x7fffffu;
        w = m == 0x7fff00u ? (w | 0x7fffe0u) : w + 0x100u;
    }
    return w;
}

static void test_page(int page)
{
    int i, k;
    const uint32_t hx = 0x3f000000u, fx = 0x42810000u;        /* 0.5, 64.5 */
    pvr_frame.bg_color = 0x000040;
    pvr_frame.bg_depth = 0;
    ta_list_begin(ListType_Opaque);
    begin(DepthMode_Always, texG);
    ta_vertex3(hx, hx, 0, 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, 0);
    ta_vertex3(fx, hx, f32b_scaled(SA[page] * RA, 6 - 20), 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, 0);
    ta_vertex3(hx, fx, f32b_scaled(SB[page] * RB, 6 - 20), 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, 1);
    for(i = 0; i < 1024; i++) {
        const int p = zexfil_probe[i].pixel;
        const uint32_t z = probe_word(page, p % 32, p / 32);
        begin(page == 2 ? DepthMode_Greater : DepthMode_Equal, texW);
        for(k = 0; k < 3; k++)
            ta_vertex3(zexfil_probe[i].x[k], zexfil_probe[i].y[k], z, 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, k == 2);
    }
    ta_list_end();
}
