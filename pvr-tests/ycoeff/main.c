/* Y_COEFF: the vertical filter of the scaler. When SCALER_CTL's factor shrinks the picture (above 0x400) every line
   is filtered with the lines above and below it before the scaler takes it; Y_COEFF bits 15..8 weight the line
   itself, bits 7..0 the two others. KallistiOS and almost every game leave 0x8040, (above + 2 * line + below) / 4;
   Under Defeat sets 0x6532 and its frames are darker on a console (polly2-data underdefeat_*). refsw3 (2026-11-18):
   ((c0 * above) >> 4) + ((c1 * line) >> 4) + ((c0 * below) >> 4), all >> 4, from those dumps.

   Every page renders the same picture into a 32 bit framebuffer (no dither) with the page's SCALER_CTL and
   Y_COEFF. The picture: one quad per line in four bands of 160 pixels, line y of band b in the colour code(b, y):
   two patterns, flat white (the weights' sum, and what happens above 255) and an arbitrary one.
   Pages: weights whose sum is 256, below 256, above 256 (one or both at 255), zero; the bits above 15; factor
   0x400 (no filter) and 0x800 (two lines into one) with Under Defeat's value. The core is reset before
   SCALER_CTL is changed (scalermeas: a console otherwise never finishes the next render). */
#define NUM_PAGES 18
#include "pvrtest2.h"

static const struct { uint32_t scaler, ycoeff; } pages[NUM_PAGES] = {
    { 0x401, 0x8040 }, { 0x401, 0x6532 }, { 0x401, 0x4020 }, { 0x401, 0x0101 }, { 0x401, 0x1234 },     /* sum <= 256 */
    { 0x401, 0x8000 }, { 0x401, 0x0080 }, { 0x401, 0x0000 }, { 0x401, 0xff00 }, { 0x401, 0x00ff },     /* one weight */
    { 0x401, 0xa060 }, { 0x401, 0xc041 }, { 0x401, 0xffff }, { 0x401, 0x7f7f },                        /* sum > 256 */
    { 0x401, 0xffff6532 }, { 0x400, 0x6532 }, { 0x800, 0x6532 }, { 0x800, 0xffff },                    /* 14: bits 31..16; factors */
};

static void test_setup(void) {
}

static void test_describe(void) {
    int p;
    for(p = 0; p < NUM_PAGES; p++)
        label_span(p, 0, 20, 15, "SCALER_CTL %05x Y_COEFF %08x", (unsigned)pages[p].scaler, (unsigned)pages[p].ycoeff);
}

/* the colour of line n in band b; read.py / the comparison need nothing else */
static uint32_t code(int b, int n) {
    uint32_t r, g, bl, x;
    switch(b) {
    case 0: r = (uint32_t)(n * 97 + 13) & 255; g = (uint32_t)(n * 57 + 101) & 255; bl = (uint32_t)n & 255; break;
    case 1: r = (n & 1) ? 0 : 255; g = (uint32_t)(n * 31 + 7) & 255; bl = (uint32_t)(n >> 1) & 255; break;
    case 2: r = g = bl = 255; break;
    default: x = (uint32_t)n * 2654435761u; x ^= x >> 13; x *= 2246822519u; x ^= x >> 16;
             r = x & 255; g = (x >> 8) & 255; bl = (x >> 16) & 255; break;
    }
    return 0xff000000 | r << 16 | g << 8 | bl;
}

static void flat(void) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Opaque;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    isp.DepthMode = DepthMode_Always;
    isp.ZWriteDis = 1;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    ta_polygon(pcw, isp, tsp, tcw);
}

static void test_page(int page) {
    int n, b;
    pvrio_reg(SOFTRESET_addr, 2);
    pvrio_reg(SOFTRESET_addr, 0);
    pvrio_reg(SCALER_CTL_addr, pages[page].scaler);
    pvrio_reg(Y_COEFF_addr, pages[page].ycoeff);
    ta_list_begin(ListType_Opaque);
    for(b = 0; b < 4; b++)
        for(n = 0; n < 480; n++) {
            flat();
            quad0(b * 160, n, 160, 1, F32B_ONE, code(b, n));
        }
    ta_list_end();
}
