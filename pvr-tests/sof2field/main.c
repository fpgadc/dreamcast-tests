/* Does the core ever write to FB_W_SOF2 when the display is interlaced and the render is started in a
   known field? (sof2probe set the same interlaced display but never looked at the field.) Every page
   waits for the field it wants (SPG_STATUS bit 10) before it writes STARTRENDER.

   page = field * 4 + setting; SCALER_CTL = settings[setting]. Dumps per page:
       page<n>a   the buffer at FB_W_SOF1 after the render (it was magenta before)
       page<n>b   the buffer at FB_W_SOF2 after the render (magenta before)
       page<n>s   8 words: SPG_STATUS when the wait ended, vertical blanks waited, SPG_STATUS after the
                  render, SPG_CONTROL, SPG_LOAD, FB_W_SOF1, FB_W_SOF2, SCALER_CTL (zeros on a PC) */
#define NUM_PAGES 8
#include "pvrtest2.h"

#define SOF2 0x200000u
#define NOTES 0x3f0000u         /* 32 bit area, unused */

static const uint32_t settings[4] = { 0x20800, 0x60800, 0x20400, 0x60400 };

static void test_setup(void) {
    /* what KallistiOS writes for 640x480 NTSC interlaced: the line and clock counts are those of VGA
       (SPG_LOAD 0x020c0359 either way), plus the interlace and NTSC bits here. Its vid_set_mode cannot be
       used: with a VGA cable it clears the interlace flag. */
    pvrio_reg(SPG_CONTROL_addr, 0x150);
    pvrio_reg(FB_W_SOF2_addr, SOF2);
}

static void test_describe(void) {
    int p;
    for(p = 0; p < NUM_PAGES; p++)
        label_span(p, 0, 20, 15, "SCALER_CTL %05x, render started in field %d", (unsigned)settings[p % 4], p / 4);
}

static uint32_t code(int band, int n) {
    uint32_t r, g, b;
    if(band == 0) { r = (uint32_t)(n * 97 + 13) & 255; g = (uint32_t)(n * 57 + 101) & 255; b = (uint32_t)n & 255; }
    else          { r = (n & 1) ? 0 : 255;             g = (uint32_t)(n * 31 + 7) & 255;   b = (uint32_t)(n >> 1) & 255; }
    return 0xff000000 | r << 16 | g << 8 | b;
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

static void set_scaler(uint32_t value) {
    pvrio_reg(SOFTRESET_addr, 2);
    pvrio_reg(SOFTRESET_addr, 0);
    pvrio_reg(SCALER_CTL_addr, value);
}

static void picture(void) {
    int n, band;

    ta_list_begin(ListType_Opaque);
    for(band = 0; band < 2; band++) {
        for(n = 0; n < 480; n++) {
            flat();
            quad0(band * 160, n, 160, 1, F32B_ONE, code(band, n));
        }
        for(n = 0; n < 320; n++) {
            flat();
            quad0(320 + n, band * 240, 1, 240, F32B_ONE, code(band, n));
        }
    }
    ta_list_end();
}

static void test_page(int page) {
    static uint32_t magenta[640];
    uint32_t notes[8] = { 0 };
    char name[32];
    int y;

    /* both buffers magenta: the first by a render without scaling, the second by hand */
    set_scaler(0x00400);
    ta_list_begin(ListType_Opaque);
    flat();
    quad0(0, 0, 640, 480, F32B_ONE, 0xffff00ff);
    ta_list_end();
    pvr_frame_render();
    for(y = 0; y < 640; y++)
        magenta[y] = 0x00ff00ff;
    for(y = 0; y < 480; y++)
        pvrio_w32(SOF2 + (uint32_t)y * 640 * 4, magenta, 640);

    set_scaler(settings[page % 4]);
    pvr_frame_begin();
    picture();
#ifdef _arch_dreamcast
    {
        volatile uint32_t *reg = (volatile uint32_t *)0xa05f8000;
        int tries = 0;
        do {
            vid_waitvbl();
            notes[0] = reg[0x10c / 4];
            tries++;
        } while((int)((notes[0] >> 10) & 1) != page / 4 && tries < 12);
        notes[1] = (uint32_t)tries;
        pvr_frame_render();
        notes[2] = reg[0x10c / 4];
        notes[3] = reg[0x0d0 / 4];
        notes[4] = reg[0x0d8 / 4];
        notes[5] = reg[0x060 / 4];
        notes[6] = reg[0x064 / 4];
        notes[7] = reg[0x0f4 / 4];
    }
#else
    pvr_frame_render();
#endif
    pvrio_w32(NOTES, notes, 8);
    snprintf(name, sizeof(name), "page%da", page);
    pvrio_dump32(SETUP_FB, 640 * 480, name, ".raw");
    snprintf(name, sizeof(name), "page%db", page);
    pvrio_dump32(SOF2, 640 * 480, name, ".raw");
    snprintf(name, sizeof(name), "page%ds", page);
    pvrio_dump32(NOTES, 8, name, ".raw");
    pvr_frame_begin();
    picture();
}
