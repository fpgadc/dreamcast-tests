/* What depth does the ISP store for a triangle whose determinant is an infinity? (pvr-tests/hugepos: triangle a,
   corners (-4M, -3M) (4M, 3M) (-3M, 4M), has an infinite determinant from M = 2^62 on and is still drawn in the
   right place up to 2^116.)

   Every page draws that triangle first, depth compare ALWAYS, writing its depth; it covers the screen below the
   diagonal 3x = 4y. Then one 24x24 probe quad per cell in rows 10-14, columns 0-9 (all inside the triangle), each
   at its own depth probes[], compare GREATER, not writing: a probe is drawn when its depth is greater than the one
   the triangle stored there. Cells 0-9 of row 9 repeat probes 0-9 with compare LESS. The probes are black, which
   they stay at any depth (a numerator of 0 divides to 0); the triangle is white wherever its determinant is an
   infinity (its colour planes are equal zeros or infinities) and red in the control.

   pages: see kinds[]. Page 7 is the control: M = 2^30, an ordinary determinant.

   Result (2026-11-17): refsw2-offline, which stores a depth of 0 for an infinite determinant
   (pvr_isp_depth.h), gives the console's picture for 478 of the 480 probes, the grey levels of the probes whose
   own colour planes underflow included. Every probe from 2^-125 up passes GREATER and none from 2^-100 down
   passes LESS: the stored depth is below 2^-125. Probes of 2^-126 and below come out white when drawn, like the
   triangle, so they tell nothing, except two: the probes of 2^-126 and of 2^-126 plus one unit on page 4
   (corner depths 1, 0.5, 0.25) are drawn in part with a colour of their own (00ffff) on the console, and not
   on page 0 (flat 0.5). So the stored
   depth is not the same 0 for every such triangle; what it is below 2^-125 is not known. */
#define NUM_PAGES 8
#include "pvrtest2.h"

#define NPROBE 50
static const uint32_t probes[NPROBE] = {
    0x80000000, 0x00000000, 0x00000001, 0x00000100, 0x00010000, 0x00400000, 0x007fffff, 0x00800000, 0x00800001, 0x00c00000,
    0x01000000, 0x01800000, 0x02000000, 0x02800000, 0x03000000, 0x03800000, 0x04000000, 0x05000000, 0x06000000, 0x07000000,
    0x08000000, 0x09000000, 0x0a000000, 0x0b000000, 0x0c000000, 0x0d000000, 0x0d800000, 0x1f800000, 0x3f000000, 0x3f000001,
    0x80000001, 0x80800000, 0xbf000000, 0xff800000, 0x7f800000, 0x7fc00000, 0xffc00000, 0x7f7fffff, 0x7f000000, 0x00000002,
    0x00000004, 0x00000010, 0x00001000, 0x00100000, 0x00200000, 0x00600000, 0x00a00000, 0x00e00000, 0x01400000, 0x01c00000,
};
typedef struct { const char *name; int m; uint32_t z[3]; } kind_t;
static const kind_t kinds[NUM_PAGES] = {
    { "M = 2^64, z = 0.5",            64,  { 0x3f000000, 0x3f000000, 0x3f000000 } },
    { "M = 2^80, z = 0.5",            80,  { 0x3f000000, 0x3f000000, 0x3f000000 } },
    { "M = 2^100, z = 0.5",           100, { 0x3f000000, 0x3f000000, 0x3f000000 } },
    { "M = 2^64, z = 0.25, 0.5, 1",   64,  { 0x3e800000, 0x3f000000, 0x3f800000 } },
    { "M = 2^64, z = 1, 0.5, 0.25",   64,  { 0x3f800000, 0x3f000000, 0x3e800000 } },
    { "M = 2^64, z = 2^100",          64,  { 0x71800000, 0x71800000, 0x71800000 } },
    { "M = 2^64, z = 2^-100",         64,  { 0x0d800000, 0x0d800000, 0x0d800000 } },
    { "M = 2^30, z = 0.5 (control)",  30,  { 0x3f000000, 0x3f000000, 0x3f000000 } },
};

static void test_setup(void) { }

static void test_describe(void) {
    int p, i;
    for(p = 0; p < NUM_PAGES; p++)
        for(i = 0; i < NPROBE; i++) {
            label(p, (10 + i / 10) * COLS + i % 10, "%s: probe %08x GREATER", kinds[p].name, (unsigned)probes[i]);
            if(i < 10)
                label(p, 9 * COLS + i, "%s: probe %08x LESS", kinds[p].name, (unsigned)probes[i]);
        }
}

static void polygon(int DepthMode, int ZWriteDis) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Translucent;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    isp.DepthMode = DepthMode;
    isp.ZWriteDis = ZWriteDis;
    isp.CullMode = CullMode_None;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    ta_polygon(pcw, isp, tsp, tcw);
}

static void test_page(int page) {
    const kind_t *k = &kinds[page];
    const uint32_t M = (uint32_t)(k->m + 127) << 23;
    const f32b p4 = F(4.0f), p3 = F(3.0f);
    const f32b xy[3][2] = { { sf_neg(sf_mul(p4, M)), sf_neg(sf_mul(p3, M)) }, { sf_mul(p4, M), sf_mul(p3, M) }, { sf_neg(sf_mul(p3, M)), sf_mul(p4, M) } };
    int i;

    ta_list_begin(ListType_Translucent);
    polygon(DepthMode_Always, 0);
    for(i = 0; i < 3; i++)
        ta_vertex0(xy[i][0], xy[i][1], k->z[i], 0xffff0000, i == 2);
    for(i = 0; i < NPROBE; i++) {
        const int c = (10 + i / 10) * COLS + i % 10;
        polygon(DepthMode_Greater, 1);
        quad0(cell_px(c) + 4, cell_py(c) + 4, 24, 24, probes[i], 0xff000000);       /* black: 0 / anything is 0 */
        if(i < 10) {
            const int d = 9 * COLS + i;
            polygon(DepthMode_Less, 1);
            quad0(cell_px(d) + 4, cell_py(d) + 4, 24, 24, probes[i], 0xff000000);
        }
    }
    ta_list_end();
}
