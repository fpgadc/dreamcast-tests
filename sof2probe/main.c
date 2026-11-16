/* Is FB_W_SOF2 ever written? The display is put into interlaced mode (SPG_CONTROL 0x150), and every page
   waits a different number of vertical blanks before it renders with the interlace bit of SCALER_CTL set, so
   that the renders fall into both fields of the display. FB_W_SOF2 points at a second buffer that is filled
   with magenta before each render and dumped as page<n>b.

   Result on a console: nothing is ever written to it. Every render goes to FB_W_SOF1, whatever the field bit
   of SCALER_CTL and the display's field are. */
#define NUM_PAGES 12
#include "pvrtest2.h"

#define SOF2 0x200000u

static const uint32_t scaler[4] = { 0x60800, 0x20800, 0x60400, 0x20400 };

static void test_setup(void) {
    pvrio_reg(FB_W_SOF2_addr, SOF2);
    pvrio_reg(SCALER_CTL_addr, 0x60800);        /* set before the first render: see scalermeas */
    pvrio_reg(SPG_CONTROL_addr, 0x150);
}

static void test_describe(void) {
    int p;
    for(p = 0; p < NUM_PAGES; p++)
        label_span(p, 0, 20, 15, "SCALER_CTL %05x after %d vertical blanks", (unsigned)scaler[p % 4], p % 3 + 1);
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

static void picture(void) {
    int n;
    ta_list_begin(ListType_Opaque);
    for(n = 0; n < 480; n++) {
        flat();
        quad0(0, n, 640, 1, F32B_ONE, 0xff000000 | ((n & 1) ? 0 : 0xff0000) | (uint32_t)(n & 255) << 8 | (uint32_t)(n >> 1));
    }
    ta_list_end();
}

static void test_page(int page) {
    static uint32_t magenta[640];
    char name[32];
    int y;

    for(y = 0; y < 640; y++)
        magenta[y] = 0x00ff00ff;
    for(y = 0; y < 480; y++)
        pvrio_w32(SOF2 + (uint32_t)y * 640 * 4, magenta, 640);
    pvrio_reg(SOFTRESET_addr, 2);
    pvrio_reg(SOFTRESET_addr, 0);
    pvrio_reg(SCALER_CTL_addr, scaler[page % 4]);
    for(y = 0; y < page % 3 + 1; y++)
        pvrio_vblank();
    picture();
    pvr_frame_render();
    snprintf(name, sizeof(name), "page%db", page);
    pvrio_dump32(SOF2, 640 * 480, name, ".raw");
    pvr_frame_begin();
    picture();
}
