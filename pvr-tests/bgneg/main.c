/* What a background depth below zero is stored as (2026-11-19).

   pvr-tests/zsign: the ISP orders depths as the raw float bits read as a signed integer, with the fraction bits
   of a negative depth inverted (between binades the larger exponent is the larger depth, within one the float's
   own order). A negative background of magnitude 1024 and more is stored as itself (it ties with a probe at its
   value); one from -2 down to -1.5 2^-20 sits between the probes at -32 and -64, in the binade of -64 .. -128.
   If it is kept in that binade unnormalised (the way a positive background below 0.25 keeps 0.25's exponent,
   pvr-tests/bgdepth), it reads as -(64 + |bg|).
   Each page one background, BG[page]; cell c < 148: a flat opaque quad at -64 - c / 4 (-64 .. -100.75),
   DepthMode Greater; cell 150 + c: the same, Less. Neither drawn: equal. */
#define NUM_PAGES 24
#include "pvrtest2.h"

static const uint32_t BG[NUM_PAGES] = {
    0xbf800000u, 0xc0000000u, 0xbf000000u, 0xbfc00000u,     /* -1, -2, -0.5, -1.5 */
    0xbe800000u, 0xb6000000u, 0xb5c00000u, 0xb5800000u,     /* -0.25, -2^-19, -1.5 2^-20, -2^-20 */
    0xc0800000u, 0xc1000000u, 0xc1800000u, 0xc2000000u,     /* -4, -8, -16, -32 */
    0xc2050000u, 0xc27f0000u, 0xc2800000u, 0xc2820000u,     /* -33.25, -63.75, -64, -65 */
    0xc2c80000u, 0xc2ff8000u, 0xc3000000u, 0xc3010000u,     /* -100, -127.75, -128, -129 */
    0xbf80001fu, 0xbf8003e0u, 0xb8000000u, 0xbd800000u,     /* -1 - 31 2^-23, -1 - 31 2^-18, -2^-15, -2^-4 */
};

static void test_setup(void) { }

/* -64 - j / 4 as a float: (256 + j) 2^-2 */
static uint32_t probe(int j) { return 0x80000000u | f32b_scaled(256 + j, -2); }

static void test_describe(void)
{
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 300; c++)
            if(c % 150 < 148)
                label(p, c, "background %08x, probe %08x (-64 - %d/4), %s", (unsigned)BG[p], (unsigned)probe(c % 150), c % 150,
                      c < 150 ? "Greater" : "Less");
}

static void test_page(int page)
{
    int c, k;
    pvr_frame.bg_color = 0x000040;
    pvr_frame.bg_depth = BG[page];
    ta_list_begin(ListType_Opaque);
    for(c = 0; c < 300; c++) {
        PCW pcw = { .full = 0 };
        ISP_TSP isp = { .full = 0 };
        TSP tsp = { .full = 0 };
        TCW tcw = { .full = 0 };
        if(c % 150 >= 148)
            continue;
        pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
        pcw.ListType = ListType_Opaque;
        pcw.Group_En = 1;
        pcw.Strip_Len = 1;
        isp.DepthMode = c < 150 ? DepthMode_Greater : DepthMode_Less;
        isp.CullMode = CullMode_None;
        tsp.SrcInstr = BlendInstr_One;
        tsp.DstInstr = BlendInstr_Zero;
        tsp.FogCtrl = FogCtrl_None;
        ta_polygon(pcw, isp, tsp, tcw);
        for(k = 0; k < 4; k++)
            ta_vertex0(f32b_int(cell_px(c) + (k & 1) * 32), f32b_int(cell_py(c) + (k >> 1) * 32), probe(c % 150), 0xffffffffu, k == 3);
    }
    ta_list_end();
}
