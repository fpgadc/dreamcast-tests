/* The depth the background is stored with: ISP_BACKGND_D against flat opaque probes (2026-11-18).

   polly2-data rez_ingame: a translucent polygon at exactly the background's depth (0x3727c5a0, about 1e-5), whose
   tag is below the background's, is drawn on the console; at 0.5, 0.75 or 0.3 it is not (pvr-tests/trtie: there
   the tags decide a tie). A flat polygon at 0x3727c5a0 is stored as exactly that depth (zmeas), so the background's
   depth is stored lower. Here every page has one background depth D (BG[] below) and every cell a flat opaque quad
   with compare GREATER at a depth around D: it is drawn where its depth is above the background's stored depth.
   Cells 0-149: D + (c - 75) steps of the 19 bit grid (D's exponent); cells 150-299: D (1 + (c - 225) 2^-12).
   The probes' depths are on the 19 bit grid, which a flat quad keeps exactly (zmeas).
   Pages 24-29: D on the 2^-20 grid but not on the 19 bit one (0.5 + 2^-20 ...), and the probes 300 consecutive
   floats around D (D + (c - 150) ulps): a float of 19 significant bits with a smallest step of 2^-20 below 0.25
   (a denormal) stores 0.5 for 0.5 + 2^-20, a fixed point with 20 fraction bits stores D.
   Pages 30-31: D = 0.5 + 2^-20 and 1 + 2^-20, the probes sloped along x from D - 2^-19 (at the cell's left edge) to
   D + 2^-19 (right edge), cell c shifted by c ulps of D: a flat probe's depth is cut by the setup's arithmetic, a
   sloped one's goes through the values between. */
#include <math.h>
#define NUM_PAGES 32
#include "pvrtest2.h"

static const uint32_t BG[NUM_PAGES] = {
    0x3727c5a0u, 0x3727c5a8u, 0x3727c5bfu, 0x372fffffu,       /* rez's, and with low bits set (around 2^-17) */
    0x36800000u, 0x36aaaaabu, 0x35ffffffu, 0x35a00001u,       /* 2^-18 .. 2^-20 */
    0x38d1b717u, 0x3a83126fu, 0x3c23d70au, 0x3dcccccdu,       /* 1e-4, 1e-3, 0.01, 0.1 */
    0x3e99999au, 0x3f2aaaabu, 0x3f7fffffu, 0x3f000001u,       /* 0.3, 2/3, below 1, above 0.5 */
    0x3fa66666u, 0x406ccccdu, 0x42c8999au, 0x44fffccdu,       /* 1.3, 3.7, 100.3, 2047.9 */
    0x457ffe66u, 0x459c459au, 0x47800026u, 0x49742400u,       /* 4095.9, 5000.7, 65536.3, 1e6 */
    0x3f000010u, 0x3f400030u, 0x3f800008u, 0x3fc00028u,       /* on the 2^-20 grid, off the 19 bit one */
    0x40400004u, 0x3f000030u,
    0x3f000010u, 0x3f800008u,                                 /* the same, probes sloped along x */
};

static double bgval(int p) { union { uint32_t u; float f; } c; c.u = BG[p]; return c.f; }

/* the probe depth of cell c, cut to the 19 bit grid */
static float probe(int p, int c)
{
    const double d = bgval(p);
    int e;
    double v, m;
    frexp(d, &e);
    if(p >= 24)
        return (float)(d + (c - 150) * ldexp(1.0, e - 24));         /* consecutive floats around D */
    if(c < 150)
        v = d + (c - 75) * ldexp(1.0, e - 19);
    else
        v = d * (1.0 + (c - 225) / 4096.0);
    m = frexp(v, &e);
    return (float)ldexp(floor(m * 524288.0) / 524288.0, e);
}

static void test_setup(void) {}

static void test_describe(void)
{
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 300; c++)
            label(p, c, "background %08x, probe %a", (unsigned)BG[p], probe(p, c));
}

static void test_page(int page)
{
    int c;
    pvr_frame.bg_color = 0x000040;
    pvr_frame.bg_depth = BG[page];
    ta_list_begin(ListType_Opaque);
    for(c = 0; c < 300; c++) {
        PCW pcw = { .full = 0 };
        ISP_TSP isp = { .full = 0 };
        TSP tsp = { .full = 0 };
        TCW tcw = { .full = 0 };
        union { float f; uint32_t u; } z;
        pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
        pcw.ListType = ListType_Opaque;
        pcw.Group_En = 1;
        pcw.Strip_Len = 1;
        isp.DepthMode = DepthMode_Greater;
        isp.CullMode = CullMode_None;
        tsp.SrcInstr = BlendInstr_One;
        tsp.DstInstr = BlendInstr_Zero;
        tsp.FogCtrl = FogCtrl_None;
        ta_polygon(pcw, isp, tsp, tcw);
        if(page >= 30) {                        /* sloped: z = D - 2^-19 + x 2^-18 / 32 (+ c ulps), corners in strip order */
            int e, k;
            const double d = bgval(page);
            frexp(d, &e);
            for(k = 0; k < 4; k++) {
                union { float f; uint32_t u; } zk;
                zk.f = (float)(d - ldexp(1.0, e - 20) + (k & 1) * ldexp(1.0, e - 19) + c * ldexp(1.0, e - 24));
                ta_vertex0(f32b_int(cell_px(c) + (k & 1) * 32), f32b_int(cell_py(c) + (k >> 1) * 32), zk.u, 0xffffffffu, k == 3);
            }
        } else {
            z.f = probe(page, c);
            quad0(cell_px(c), cell_py(c), 32, 32, z.u, 0xffffffffu);
        }
    }
    ta_list_end();
}
