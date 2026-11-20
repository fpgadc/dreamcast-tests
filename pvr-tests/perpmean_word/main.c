/* The depth of a "perpendicular" triangle (FPU_PERP_VAL), as words (2026-11-19, TODO A2, README perpval).

   A triangle whose determinant is at most FPU_PERP_VAL in size is drawn as one of determinant zero: every pixel at
   the mean of its corners' depths. How the mean is computed (the order of the sum, its width, the factor 1/3, the
   cuts) was a guess in refsw3. Here every pixel of 16 tiles has a triangle of its own (corners (x + 1/4, y + 1/4),
   (x + 1, y + 1/4), (x + 1/4, y + 1): it covers that pixel's center only, determinant 9/16, mirrored: the second and
   third corner swapped), with a depth triple from gen.cpp (cases.h), opaque, DepthMode Always; FPU_PERP_VAL is 0.75,
   so every one of them is perpendicular and the probes of common/zexfil.h (determinants 1 and 2, in tile (0, 0))
   are not. zexfil.h reads every tile's depths out (27 renders a tile); the pages show them as words, one per pixel
   (the float's bits, 19 significant) in the tile's place, black elsewhere; compare2.py hw off compares them word
   for word. The families (gen.cpp): flat triangles (24 and 19 bit depths, 1/3 and 2/3 of powers of two), three
   depths in one binade (both windings), within 4, 8, 24, 40 binades, one or two depths 16 .. 40 binades below, a
   triple in all six orders, a depth below zero, two cancelling depths, pair sums near a power of two, zeros.
   Pages 0 .. 3: four tiles each. */
#define NUM_PAGES 4
#define TEST_DRAW_PAGE
#include "pvrtest2.h"
#include "zexfil.h"
#ifndef CASES_H
#define CASES_H "cases.h"
#endif
#include CASES_H

/* the probes' video memory: free (pvr-tests/zexfil) */
#define ZEXFIL_AT 0x580000u
#define PERP_VAL 0x3f400000u            /* 0.75 */

static int cur_tile;

static void scene(void);

static void test_setup(void) {
    pvrio_reg(FPU_PERP_VAL_addr, PERP_VAL);
    zexfil_use_pvrsetup(scene, ZEXFIL_AT);
}

static void test_describe(void) {
    int t;
    for(t = 0; t < NT; t++)
        label(t / 4, tiles[t].ty * COLS + tiles[t].tx, "tile %d: %s", t, tiles[t].name);
}

static void scene(void) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    const int t = cur_tile;
    int k;

    ta_list_begin(ListType_Opaque);
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Opaque;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    isp.DepthMode = DepthMode_Always;
    isp.CullMode = CullMode_None;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    ta_polygon(pcw, isp, tsp, tcw);
    for(k = 0; k < 1024; k++) {
        const int X = tiles[t].tx * 32 + k % 32, Y = tiles[t].ty * 32 + k / 32;
        const f32b x0 = f32b_scaled(4 * X + 1, -2), y0 = f32b_scaled(4 * Y + 1, -2);
        const f32b x1 = f32b_int(X + 1), y1 = f32b_int(Y + 1);
        ta_vertex0(x0, y0, tz[t][k][0], 0xff808080u, 0);
        if(tiles[t].mirror) {
            ta_vertex0(x0, y1, tz[t][k][1], 0xff808080u, 0);
            ta_vertex0(x1, y0, tz[t][k][2], 0xff808080u, 1);
        } else {
            ta_vertex0(x1, y0, tz[t][k][1], 0xff808080u, 0);
            ta_vertex0(x0, y1, tz[t][k][2], 0xff808080u, 1);
        }
    }
}

static void test_page(int page) {
    (void)page;
}

static void test_draw_page(int page) {
    static uint32_t z[4][1024];
    char line[80];
    int k;

    zexfil_foreign = 0;
    for(k = 0; k < 4; k++) {
        cur_tile = page * 4 + k;
        zexfil_tile(tiles[cur_tile].tx, tiles[cur_tile].ty, z[k]);
    }
    pvrio_fill32(SETUP_FB, 640 * 480, 0);
    for(k = 0; k < 4; k++)
        zexfil_show(tiles[page * 4 + k].tx, tiles[page * 4 + k].ty, z[k]);
    if(zexfil_foreign) {
        snprintf(line, sizeof(line), "PVRIO zexfil page %d: %d readings of another colour than the probe's", page, zexfil_foreign);
        pvrio_hw_say(line);
    }
}
