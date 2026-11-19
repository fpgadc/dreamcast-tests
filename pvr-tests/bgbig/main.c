/* What a large positive background depth is stored as (2026-11-19, after pvr-tests/bgspec).

   bgspec: a background of +infinity (and 0x7f800001) ties with probe polygons whose corners are at the largest
   float, +infinity or a NaN (their plane cannot be set up: what they store is one word, V), while a background at
   the largest float (0x7f7fffff) is above them: not its bits cut to 19 significant bits (0x7f7fffe0, below
   +infinity). bgdepth measured backgrounds up to 1e6 only. One background a page (BG[]); cell j < 104: a flat
   opaque quad at 2^j (j <= 100: the plane's constant, about z x y, stays below 2^128 in every cell) or, for
   j = 101 .. 103, at the largest float, +infinity, a NaN, DepthMode Greater; cell 150 + j: the same, Less.
   Neither drawn: equal. */
#define NUM_PAGES 20
#include "pvrtest2.h"

static const uint32_t BG[NUM_PAGES] = {
    0x71800000u, 0x78000000u, 0x78800000u, 0x7b800000u,     /* 2^100, 2^113, 2^114, 2^120 */
    0x7e800000u, 0x7ec00000u, 0x7f000000u, 0x7f400000u,     /* 2^126, 1.5 2^126, 2^127, 1.5 2^127 */
    0x7f700000u, 0x7f7fffc0u, 0x7f7fffe0u, 0x7f7ffff0u,     /* 1.875 2^127, the largest less 63, 31, 15 ulps */
    0x7f7fffffu, 0x7f800000u, 0x7f800001u, 0x7fc00000u,     /* the largest float, +inf, 0x7f800001, NaN */
    0x7fffffffu, 0x71800001u, 0x7d800000u, 0x7e000000u,     /* NaN all ones, 2^100 + 1 ulp, 2^124, 2^125 */
};
static const uint32_t SPECIAL[3] = { 0x7f7fffffu, 0x7f800000u, 0x7fc00000u };

static void test_setup(void) { }

static uint32_t probe(int j) { return j < 101 ? (uint32_t)(j + 127) << 23 : SPECIAL[j - 101]; }

static void test_describe(void)
{
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 300; c++)
            if(c % 150 < 104)
                label(p, c, "background %08x, probe %08x, %s", (unsigned)BG[p], (unsigned)probe(c % 150), c < 150 ? "Greater" : "Less");
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
        if(c % 150 >= 104)
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
