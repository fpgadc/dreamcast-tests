/* The word a negative background of magnitude below 64 is stored as (2026-11-19, after pvr-tests/bgneg).

   bgneg: a negative background of magnitude 64 and more is stored as itself; every one below 64 (-2^-20 .. -63.75)
   ties with none of the probes -64 .. -100.75 and is below all of them in the ISP's order (raw float bits as a
   signed integer, a negative depth's fraction bits inverted: pvr-tests/zsign), and above -32: so one word near the
   bottom of the binade -64 .. -128. Two sweeps find it, for four backgrounds (BG[page / 2]):
     even pages  probes -100.75 - j / 4, j = 0 .. 147 (to -137.75, across -128)
     odd pages   probes with the raw bits 0xc2ffffe0 - 32 j (-127.99976 and down, one step of the stored depth's
                 19 bits a cell)
   Cell j: DepthMode Greater; cell 150 + j: Less. Neither drawn: equal. */
#define NUM_PAGES 8
#include "pvrtest2.h"

static const uint32_t BG[4] = { 0xbf800000u, 0xc2000000u, 0xc27f0000u, 0xb5800000u };   /* -1, -32, -63.75, -2^-20 */

static void test_setup(void) { }

static uint32_t probe(int page, int j)
{
    if(page & 1)
        return 0xc2ffffe0u - 32u * (uint32_t)j;
    return 0x80000000u | f32b_scaled(403 + j, -2);       /* -(100.75 + j / 4) = -(403 + j) 2^-2 */
}

static void test_describe(void)
{
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 300; c++)
            if(c % 150 < 148)
                label(p, c, "background %08x, probe %08x, %s", (unsigned)BG[p / 2], (unsigned)probe(p, c % 150), c < 150 ? "Greater" : "Less");
}

static void test_page(int page)
{
    int c, k;
    pvr_frame.bg_color = 0x000040;
    pvr_frame.bg_depth = BG[page / 2];
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
            ta_vertex0(f32b_int(cell_px(c) + (k & 1) * 32), f32b_int(cell_py(c) + (k >> 1) * 32), probe(page, c % 150), 0xffffffffu, k == 3);
    }
    ta_list_end();
}
