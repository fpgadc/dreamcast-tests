/* SupSample's points half a line down, the rule by itself (2026-11-19). README ssrare: at a point half a line down the
   evaluator has floor(C + B / 2) where whole points have floor(C) + floor(B / 2), one grid unit more where the
   fractions of C and B / 2 below the grid carry; C has a fraction there only when another term (A or B times the
   tile's position) sets the grid, which ssrare's cells had on 48 of 200,000 points. Here every cell's planes are
   made from targets with a constant near zero at the screen's origin (A x and B y set the grid):
     1/w  = aw x + bw y + cw,   U/w = au x + bu y + cu,   V/w = av x + bv y + cv     (x, y on the screen)
   and each corner gets u = (U/w) / (1/w), v = (V/w) / (1/w), 1/w. One volume, SupSample, bilinear, decal, 64x64
   4444 textures of all-zero / all-one channels (every weight step shows, as ssrare's). 300 cells a page:
     pages  0 ..  7  all three constants near zero: c = a (x + 16) rho, rho = +-2^-7 .. 2^-13 (C off the grid)
     pages  8 .. 11  only U/w's and V/w's (1/w's constant ordinary)
     pages 12 .. 15  controls: every constant ordinary (C on the grid: the rule changes nothing)
   Offline with the old rule (floor(C) + floor(B / 2)) against the new one: the pixels where they differ are this
   test's readings. */
#define NUM_PAGES 16
#include <math.h>
#include "pvrtest2.h"
#include "texgen.h"

static uint32_t tex;

static uint32_t hsh(uint32_t v) { v *= 2654435761u; v ^= v >> 15; v *= 2246822519u; v ^= v >> 13; v *= 3266489917u; v ^= v >> 16; return v; }
static double unit01(uint32_t h) { return (h >> 8) / 16777216.0; }

static uint32_t bw_texel(const void *ctx, int size, int x, int y)
{
    const uint32_t h = hsh((uint32_t)(y * 64 + x) ^ 0x51ab0c7u);
    (void)ctx; (void)size;
    return (h & 1 ? 0xf000u : 0) | (h & 2 ? 0x0f00u : 0) | (h & 4 ? 0x00f0u : 0) | (h & 8 ? 0x000fu : 0);
}

static void test_setup(void)
{
    const tex_layout_t l = { TEX_TWIDDLED, 16, 64, 64, 0 };
    tex = tex_build(&l, bw_texel, NULL);
}

static void test_describe(void)
{
    static const char *d[3] = { "all constants near zero", "U/w and V/w constants near zero", "controls: ordinary constants" };
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 300; c++)
            label(p, c, "%s", d[p < 8 ? 0 : p < 12 ? 1 : 2]);
}

/* a constant near zero against the term a (x + 16), or (control) an ordinary one */
static double small_c(uint32_t h, double a, double xt, int near0)
{
    const double rho = ldexp(1.0 + unit01(h), -7 - (int)(h % 7)) * ((h >> 3) & 1 ? -1 : 1);
    if(!near0)
        return a * xt * 0.37 + (unit01(hsh(h)) - 0.5) * 3.0;
    return a * (xt + 16) * rho;
}

static void test_page(int page)
{
    int c, k;
    pvr_frame.bg_color = 0x000040;
    ta_list_begin(ListType_Opaque);
    for(c = 0; c < 300; c++) {
        PCW pcw = { .full = 0 };
        ISP_TSP isp = { .full = 0 };
        TSP tsp = { .full = 0 };
        TCW tcw = { .full = 0 };
        const int x = cell_px(c), y = cell_py(c);
        const uint32_t s = (uint32_t)c * 7919u + (uint32_t)page * 1000003u;
        const int wnear = page < 8, uvnear = page < 12;
        /* 1/w: about 0.5 .. 2 over the cell, rising toward the lower right */
        const double xc = x + 16, yc = y + 16, w0 = 0.5 + 1.5 * unit01(hsh(s + 1));
        const double gw = w0 / (xc + yc) * (0.6 + 0.8 * unit01(hsh(s + 2)));
        const double aw = gw, bw = gw * (0.5 + unit01(hsh(s + 3)));
        const double cw = wnear ? small_c(hsh(s + 4), aw, xc, 1) : w0 - aw * xc - bw * yc;
        /* U/w, V/w: 1/4 .. 2 texels a pixel (64 texels), signs and the minor slope at random */
        const double tu = (0.25 + 1.75 * unit01(hsh(s + 5))) / 64 * (hsh(s + 6) & 1 ? -1 : 1);
        const double tv = (0.25 + 1.75 * unit01(hsh(s + 7))) / 64 * (hsh(s + 8) & 1 ? -1 : 1);
        const double au = tu * w0, bu = tu * w0 * (unit01(hsh(s + 9)) - 0.5) * 0.5;
        const double av = tv * w0 * (unit01(hsh(s + 10)) - 0.5) * 0.5, bv = tv * w0;
        const double cu = small_c(hsh(s + 11), au, xc, uvnear), cv = small_c(hsh(s + 12), bv, yc, uvnear);
        pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
        pcw.ListType = ListType_Opaque;
        pcw.Group_En = 1;
        pcw.Strip_Len = 1;
        pcw.Texture = 1;
        isp.DepthMode = DepthMode_Always;
        isp.CullMode = CullMode_None;
        isp.Texture = 1;
        tsp.SrcInstr = BlendInstr_One;
        tsp.DstInstr = BlendInstr_Zero;
        tsp.FogCtrl = FogCtrl_None;
        tsp.UseAlpha = 1;
        tsp.ShadInstr = ShadInstr_Decal;
        tsp.FilterMode = FilterMode_Bilinear;
        tsp.SupSample = 1;
        tsp.MipMapD = 4;
        tsp.TexU = 3;
        tsp.TexV = 3;
        tcw.PixelFmt = PixelFmt_4444;
        tcw.TexAddr = tex >> 3;
        ta_polygon(pcw, isp, tsp, tcw);
        for(k = 0; k < 4; k++) {
            const double vx = x + (k & 1) * 32, vy = y + (k >> 1) * 32;
            const double w = aw * vx + bw * vy + cw;
            const double u = (au * vx + bu * vy + cu) / w, v = (av * vx + bv * vy + cv) / w;
            ta_vertex3(f32b_int((int)vx), f32b_int((int)vy), F((float)w), F((float)u), F((float)v), 0xffffffffu, 0, k == 3);
        }
    }
    ta_list_end();
}
