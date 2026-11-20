/* The stored depth of a sloped polygon below zero, by a ladder of probes (2026-11-19, hunt depthgaps (c); after
   depthgaps_negramp).

   depthgaps_negramp's ramp (corners (0.5, 0.5) at 0, (64.5, 0.5) at 64 A, (0.5, 64.5) at 64 B, A = -a 2^-20,
   B = -b 2^-20: the depth of pixel (i, j) of tile (0, 0) is v = A i + B j exactly) stores neither F (v's word cut
   to a multiple of 0x100) nor F + 0x100 at two thirds of its pixels. Here the probes (zexfil's exact triangles) are
   at F + 0x100 (k - 8), k = 0 .. 9 (pages 0 - 9, DepthMode Equal), and F - 0x800 and F + 0x100 with Greater
   (pages 10, 11). A probe word whose fraction leaves the binade is clamped to it (0 .. 0x7fff00). */
#define NUM_PAGES 12
#include "pvrtest2.h"
#include "texgen.h"
#include "zexfil_probes.h"

static const int32_t RA = 0x0b69d3, RB = 54321;         /* a, b */
static const int OFS[NUM_PAGES] = { -8, -7, -6, -5, -4, -3, -2, -1, 0, 1, -8, 1 };
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
        label(p, 0, "ramp a %d, b %d, both below zero: probe F %+d x 0x100, %s", (int)RA, (int)RB, OFS[p], p < 10 ? "Equal" : "Greater");
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
    const int64_t n = -(int64_t)RA * i - (int64_t)RB * j;
    uint32_t w;
    int32_t f;
    int64_t a = -n;
    int e = 0;
    if(!a)
        return 0;
    while(a >= 0x01000000) { a >>= 1; e++; }     /* cut toward zero to 24 bits */
    w = f32b_scaled((int32_t)a, e - 20);
    f = (int32_t)(w & 0x7fff00u) + 0x100 * OFS[p];
    if(f < 0) f = 0;
    if(f > 0x7fff00) f = 0x7fff00;
    return 0x80000000u | (w & 0x7f800000u) | (uint32_t)f;
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
    ta_vertex3(fx, hx, f32b_scaled(-RA, 6 - 20), 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, 0);
    ta_vertex3(hx, fx, f32b_scaled(-RB, 6 - 20), 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, 1);
    for(i = 0; i < 1024; i++) {
        const int p = zexfil_probe[i].pixel;
        const uint32_t z = probe_word(page, p % 32, p / 32);
        begin(page >= 10 ? DepthMode_Greater : DepthMode_Equal, texW);
        for(k = 0; k < 3; k++)
            ta_vertex3(zexfil_probe[i].x[k], zexfil_probe[i].y[k], z, 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, k == 2);
    }
    ta_list_end();
}
