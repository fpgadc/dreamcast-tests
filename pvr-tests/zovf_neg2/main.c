/* More negative depths in the block form (2026-11-19, hunt zovf round 2, after pvr-tests/zovf_neg), the same method
   and layout (zword's binary search with the background as the probe; target n of a page at x = 32 n: row 470 the
   index of the last probe the depth is above, + 1, row 471 1 when it equals the next probe, row 472 1 when it equals
   a background of 0).

   zovf_neg: a positive start stepped below zero reads as the block's negative full scale, -(2^(pair + 6) - ulp)
   (pair the larger slope's exponent), above every negative probe once that is past the float range; zovf_satw2: -2^127
   and below read as themselves, -2^127 + 2^110 as -2^127. Here negative starts of exponents 8 .. 128 stepped toward
   zero and away (and pixel 0, the start itself), starts of 0, positive starts stepped to -2^127 and below from far
   down, and the grid of the snap to -2^127 and -1.5 2^127 (steps of 2^109 .. 1.5 2^112). Fat slivers in tile 0 around
   pixel (k, 0); page 0 targets 0 - 19, page 1 20 - 39. Triangles and labels: agents/nulldc3/hunt/zovf/zgen.cpp
   (zgen3.h), mode neg2 (cases.h). */
#define NUM_PAGES 2
#define TEST_DRAW_PAGE
#include "pvrtest2.h"
#include "texgen.h"
#include "cases.h"

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
#ifndef _arch_dreamcast
    int i;
    for(i = 0; i < NUM_LABELS; i++)
        label(labels[i].group, labels[i].cell, "%s", labels[i].text);
#endif
}

/* the target pixel: the column of the third corner (k + 0.5), row 0 */
static int target_x(int n)
{
    union { uint32_t u; float f; } c;
    c.u = cases[n].v[2][0];
    return (int)c.f;
}

/* one render with the background at b and the target's sliver compared by mode; the target pixel green? */
static int render(int n, uint32_t b, int mode)
{
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    uint32_t w;
    int k;
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
    for(k = 0; k < 3; k++)
        ta_vertex3(cases[n].v[k][0], cases[n].v[k][1], cases[n].v[k][2], 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, k == 2);
    ta_list_end();
    pvr_frame_render();
    pvrio_read32(SETUP_FB + (uint32_t)target_x(n) * 4, 1, &w);
    return (w & 0xffffff) == 0x00ff00;
}

static void test_page(int page) { (void)page; }

static void test_draw_page(int page)
{
    static uint32_t res[3][640];
    int n, r;
    pvrio_unhashed = 1;
    for(r = 0; r < 3; r++)
        for(n = 0; n < 640; n++)
            res[r][n] = 0;
    for(n = 0; n < NUM_CASES; n++) {
        const int x = 32 * (cases[n].cell - 280);
        if(cases[n].group != page)
            continue;
        /* the number of probes the depth is above: lo .. hi */
        uint32_t lo = 0, hi = NPROBES;
        while(lo < hi) {
            const uint32_t mid = lo + (hi - lo + 1) / 2;       /* is it above probe mid - 1? */
            if(render(n, probe(mid - 1), DepthMode_Greater)) lo = mid; else hi = mid - 1;
        }
        res[0][x] = lo;
        res[1][x] = lo < NPROBES ? (uint32_t)render(n, probe(lo), DepthMode_Equal) : 0;
        res[2][x] = (uint32_t)render(n, 0, DepthMode_Equal);
    }
    for(r = 0; r < 3; r++)
        pvrio_w32(SETUP_FB + (470u + (uint32_t)r) * 640 * 4, res[r], 640);
    pvrio_unhashed = 0;
}
