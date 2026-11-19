/* The ISP's stored depth at the edges of its number format (2026-11-19): infinities, NaNs, zeros of either sign
   and values below 2^-126, and how the depth compares treat them.

   refsw3 keeps a depth as the bits of a float compared as floats compare (a NaN with nothing, -0 = +0); a polygon
   whose plane cannot be set up (an infinity or a NaN among its depths) stores its first corner's depth as given;
   a plane's value below 2^-126 is rounded to a float's denormal grid. None of it measured.
   Part A, special depths (S[] below, ten of them):
     page 0  a quad over the cell, DepthMode Always, its first corner's depth S (the other three at 0.5); then a
             white quad over the cell at 0.5 (columns 0-9) or -0.5 (10-19), DepthMode by row: Greater,
             GreaterEqual, Less, LessEqual, Equal, NotEqual; rows 6, 7: the first quad with all four corners at S,
             the probe Greater, Less
     page 1  the quad alone, DepthMode by row (rows 0-5 as above) against the background at 0.5: columns 0-9 the
             first corner at S, 10-19 all four
     page 2  page 1 with the background at 1e-4 and the quad's other corners at -0.5
   Part B, depths around 2^-126: every cell a quad sloped along x, depth zl at the cell's left edge and zr at its
   right (ZB[] by row, DepthMode Always); then a flat quad over the cell at the depth T[col], DepthMode Greater
   (page 3), Less (4), Equal (5), GreaterEqual (6). Where the probe is drawn tells the stored depth against T pixel
   by pixel. A slope of 2^-124 over 32 pixels is a coefficient of about 2^-130 a pixel, below a float's range too. */
#define NUM_PAGES 7
#include "pvrtest2.h"

static const uint32_t S[10] = { 0x7fc00000u, 0xffc00000u, 0x7f800001u, 0x7f800000u, 0xff800000u,
                                0x80000000u, 0x00000001u, 0x7f7fffffu, 0x7149f2cau, 0x3f800000u };
static const char *SNAME[10] = { "NaN", "-NaN", "sNaN", "+inf", "-inf", "-0", "2^-149", "largest", "1e30", "1" };
/* (zl, zr) by row */
static const uint32_t ZB[8][2] = {
    { 0x01800000u, 0x81800000u },       /* 2^-124 .. -2^-124 */
    { 0x01000000u, 0x00000000u },       /* 2^-125 .. 0 */
    { 0x00800000u, 0x80800000u },       /* 2^-126 .. -2^-126 */
    { 0x81000000u, 0x00000000u },       /* -2^-125 .. 0 */
    { 0x00c00000u, 0x00c00000u },       /* 1.5 2^-126, flat */
    { 0x08800000u, 0x88800000u },       /* 2^-110 .. -2^-110 */
    { 0x0d800000u, 0x0d802000u },       /* 2^-100 .. 2^-100 (1 + 2^-10) */
    { 0x3f800000u, 0x3f800008u },       /* 1 .. 1 + 2^-20 */
};
static const uint32_t T[20] = {
    0x00000000u, 0x80000000u,
    0x00800000u, 0x80800000u, 0x00a00000u, 0x80a00000u, 0x00c00000u, 0x80c00000u,    /* 2^-126 (1, 1.25, 1.5) */
    0x01000000u, 0x81000000u, 0x01400000u, 0x81400000u, 0x01800000u, 0x81800000u,    /* 2^-125, 1.5 2^-125, 2^-124 */
    0x08800000u, 0x88800000u, 0x0d800000u, 0x8d800000u,                              /* 2^-110, 2^-100 */
    0x3f800000u, 0x3f800004u,                                                        /* 1, 1 + 2^-21 */
};
static const int MODE[6] = { DepthMode_Greater, DepthMode_GreaterEqual, DepthMode_Less, DepthMode_LessEqual,
                             DepthMode_Equal, DepthMode_NotEqual };
static const char *MODENAME[6] = { "Greater", "GreaterEqual", "Less", "LessEqual", "Equal", "NotEqual" };

static void test_setup(void) { }

static void test_describe(void)
{
    static const char *bmode[4] = { "Greater", "Less", "Equal", "GreaterEqual" };
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 160; c++) {
            const int row = c / 20, col = c % 20;
            if(p == 0)
                label(p, c, "first %s at %s, probe %s at %s", row < 6 ? "corner" : "quad", SNAME[col % 10],
                      row < 6 ? MODENAME[row] : row == 6 ? "Greater" : "Less", col < 10 ? "0.5" : "-0.5");
            else if(p <= 2) {
                if(row < 6)
                    label(p, c, "%s at %s, %s against the background %s", col < 10 ? "first corner" : "quad",
                          SNAME[col % 10], MODENAME[row], p == 1 ? "0.5" : "1e-4");
            } else
                label(p, c, "slope %08x .. %08x, probe %08x, %s", (unsigned)ZB[row][0], (unsigned)ZB[row][1],
                      (unsigned)T[col], bmode[p - 3]);
        }
}

/* a quad over cell c, depth z[k] at its corners in strip order (top left, top right, bottom left, bottom right) */
static void quad(int c, int mode, const uint32_t z[4], uint32_t colour)
{
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    int k;
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Opaque;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    isp.DepthMode = mode;
    isp.CullMode = CullMode_None;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    ta_polygon(pcw, isp, tsp, tcw);
    for(k = 0; k < 4; k++)
        ta_vertex0(f32b_int(cell_px(c) + (k & 1) * 32), f32b_int(cell_py(c) + (k >> 1) * 32), z[k], colour, k == 3);
}

static void test_page(int page)
{
    int c;
    pvr_frame.bg_color = 0x000040;
    pvr_frame.bg_depth = page == 1 ? 0x3f000000u : 0x38d1b717u;        /* 0.5; 1e-4 */
    ta_list_begin(ListType_Opaque);
    for(c = 0; c < 160; c++) {
        const int row = c / 20, col = c % 20;
        if(page == 0) {
            const uint32_t s = S[col % 10], probe = col < 10 ? 0x3f000000u : 0xbf000000u;
            const uint32_t z[4] = { s, row < 6 ? 0x3f000000u : s, row < 6 ? 0x3f000000u : s, row < 6 ? 0x3f000000u : s };
            const uint32_t pz[4] = { probe, probe, probe, probe };
            quad(c, DepthMode_Always, z, 0xff40c040u);
            quad(c, row < 6 ? MODE[row] : row == 6 ? DepthMode_Greater : DepthMode_Less, pz, 0xffffffffu);
        } else if(page <= 2) {
            const uint32_t s = S[col % 10], o = page == 1 ? 0x3f000000u : 0xbf000000u;
            const uint32_t z[4] = { s, col < 10 ? o : s, col < 10 ? o : s, col < 10 ? o : s };
            if(row < 6)
                quad(c, MODE[row], z, 0xffffffffu);
        } else {
            static const int bmode[4] = { DepthMode_Greater, DepthMode_Less, DepthMode_Equal, DepthMode_GreaterEqual };
            const uint32_t z[4] = { ZB[row][0], ZB[row][1], ZB[row][0], ZB[row][1] };
            const uint32_t pz[4] = { T[col], T[col], T[col], T[col] };
            quad(c, DepthMode_Always, z, 0xff40c040u);
            quad(c, bmode[page - 3], pz, 0xffffffffu);
        }
    }
    ta_list_end();
}
