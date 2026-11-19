/* ISP_BACKGND_D below zero, at zero of either sign, denormal, infinite and NaN (2026-11-19).

   pvr-tests/bgdepth measured the background's stored depth from 2^-20 to 1e6: a float of 19 significant bits whose
   smallest exponent is that of 0.25 (below 2^-20: 0). What a depth below zero, an infinity or a NaN becomes is not
   measured; refsw3 keeps such a value as given (one below 2^-20 in size: 0). One background depth a page (BG[]
   below); every cell a flat quad (all four corners at the probe depth P[col]) over the cell:
     rows 0-5   opaque, DepthMode Greater, GreaterEqual, Less, LessEqual, Equal, NotEqual
     row 6      translucent, autosort (drawn where its key is above the background's: its depth, then the tag)
     row 7      punch-through (the same walk, near to far, at or in front of the background)
   Probe depths along the row: -inf, -1e30, -2, -1, -0.5, -2^-20, -2^-21, -0, +0, 2^-149, 2^-21, 2^-20, 0.25,
   0.5, 1, 2, 1e30, the largest float, +inf, NaN. A probe at an infinity or a NaN cannot be set up as a plane;
   refsw3 stores the first corner's depth as it is. */
#define NUM_PAGES 16
#include "pvrtest2.h"

static const uint32_t BG[NUM_PAGES] = {
    0xbf800000u, 0xbf000000u, 0xb5000000u, 0xb6200000u,     /* -1, -0.5, -2^-21, -1.25 2^-19 */
    0x80000000u, 0x00000000u, 0x00000001u, 0x80000001u,     /* -0, +0, 2^-149, -2^-149 */
    0x7f800000u, 0xff800000u, 0x7fc00000u, 0xffc00000u,     /* +inf, -inf, NaN, -NaN */
    0x7f800001u, 0x7f7fffffu, 0x3f80001fu, 0xbf80001fu,     /* signalling NaN, the largest float, 1 and -1 with low bits */
};
static const uint32_t P[20] = {
    0xff800000u, 0xf149f2cau, 0xc0000000u, 0xbf800000u, 0xbf000000u, 0xb5800000u, 0xb5000000u, 0x80000000u,
    0x00000000u, 0x00000001u, 0x35000000u, 0x35800000u, 0x3e800000u, 0x3f000000u, 0x3f800000u, 0x40000000u,
    0x7149f2cau, 0x7f7fffffu, 0x7f800000u, 0x7fc00000u,
};
static const int MODE[6] = { DepthMode_Greater, DepthMode_GreaterEqual, DepthMode_Less, DepthMode_LessEqual,
                             DepthMode_Equal, DepthMode_NotEqual };

static void test_setup(void) { }

static void test_describe(void)
{
    static const char *row[8] = { "Greater", "GreaterEqual", "Less", "LessEqual", "Equal", "NotEqual",
                                  "translucent", "punch-through" };
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 160; c++)
            label(p, c, "background %08x, %s, probe %08x", (unsigned)BG[p], row[c / 20], (unsigned)P[c % 20]);
}

static void probe(int c, int list, int mode)
{
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    int k;
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = list;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    isp.DepthMode = mode;
    isp.CullMode = CullMode_None;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = list != ListType_Opaque;
    ta_polygon(pcw, isp, tsp, tcw);
    for(k = 0; k < 4; k++)
        ta_vertex0(f32b_int(cell_px(c) + (k & 1) * 32), f32b_int(cell_py(c) + (k >> 1) * 32), P[c % 20],
                   list == ListType_Opaque ? 0xffffffffu : list == ListType_Translucent ? 0xffffff40u : 0xff40ffffu, k == 3);
}

static void test_page(int page)
{
    int c;
    pvr_frame.bg_color = 0x000040;
    pvr_frame.bg_depth = BG[page];
    ta_list_begin(ListType_Opaque);
    for(c = 0; c < 120; c++)
        probe(c, ListType_Opaque, MODE[c / 20]);
    ta_list_end();
    ta_list_begin(ListType_Translucent);
    for(c = 120; c < 140; c++)
        probe(c, ListType_Translucent, DepthMode_GreaterEqual);
    ta_list_end();
    ta_list_begin(ListType_Punch_Through);
    for(c = 140; c < 160; c++)
        probe(c, ListType_Punch_Through, DepthMode_GreaterEqual);
    ta_list_end();
}
