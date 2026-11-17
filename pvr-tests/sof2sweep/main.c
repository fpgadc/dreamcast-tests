/* Is there any setting with which the core writes to FB_W_SOF2? KallistiOS's header calls the register
   "output for strip-buffering", so: the strip buffer bits of FB_R_CTRL (bit 22 enable, bits 21-16 strip
   size), every reserved bit of SCALER_CTL and FB_W_CTRL, and other values of SPG_CONTROL.

   Every page, on a console: both buffers (at FB_W_SOF1 and at FB_W_SOF2) are filled with magenta by the
   CPU, the page's registers are set after a core reset, one quad over the whole screen is rendered, and
   the CPU counts the words of each buffer that are no longer magenta. Dump page<n>s, 12 words:
       0 words changed in the buffer at FB_W_SOF1    1 the first of them (index)     2 the last of them
       3 words changed in the buffer at FB_W_SOF2    4 the first of them             5 the last of them
       6 FB_R_CTRL  7 SCALER_CTL  8 FB_W_CTRL  9 SPG_CONTROL as read back    10 SPG_STATUS before the render
       11 the value of the first changed word of the buffer at FB_W_SOF2
   (all zero on a PC: this test is about the console only). */
#define NUM_PAGES 52
#include "pvrtest2.h"

#define SOF2  0x200000u
#define NOTES 0x3f0000u
#define WORDS (640 * 480)

/* what: 0 FB_R_CTRL bits to add, 1 SCALER_CTL, 2 FB_W_CTRL bits to add, 3 SPG_CONTROL.
   The strip buffer pages come last: a render with the strip buffer on does not finish here, and the core
   stays stuck for the pages after it. They also dump both buffers (page<n>a, page<n>b); order: the tiles
   of the region array column by column (0) or row by row (2). */
static struct { int what; uint32_t value, scaler, spg; int order; } pages[NUM_PAGES];

static void add(int *n, int what, uint32_t value, uint32_t scaler, uint32_t spg, int order) {
    pages[*n].what = what; pages[*n].value = value; pages[*n].scaler = scaler; pages[*n].spg = spg; pages[*n].order = order;
    (*n)++;
}

static void make_pages(void) {
    static const uint32_t strips[6] = { 0, 1, 2, 4, 8, 15 };
    static const uint32_t spg[7] = { 0x190, 0x130, 0x170, 0x1d0, 0x110, 0x350, 0x050 };
    int n = 0, k;
    add(&n, 1, 0x60800, 0x60800, 0x150, 0);                                                     /* 0, 1: nothing special */
    add(&n, 1, 0x00400, 0x00400, 0x150, 0);
    for(k = 19; k < 32; k++) add(&n, 1, 0x60800 | 1u << k, 0x60800 | 1u << k, 0x150, 0);        /* 2 */
    for(k = 4; k < 8; k++) add(&n, 2, 1u << k, 0x60800, 0x150, 0);                              /* 15 */
    for(k = 24; k < 32; k++) add(&n, 2, 1u << k, 0x60800, 0x150, 0);                            /* 19 */
    for(k = 0; k < 7; k++) add(&n, 3, spg[k], 0x60800, spg[k], 0);                              /* 27 */
    for(k = 0; k < 6; k++) add(&n, 0, 1u << 22 | strips[k] << 16, 0x00400, 0x100, 0);           /* 34: not interlaced */
    for(k = 1; k < 5; k++) add(&n, 0, 1u << 22 | strips[k] << 16, 0x00400, 0x100, 2);           /* 40: row by row */
    for(k = 1; k < 5; k++) add(&n, 0, 1u << 22 | strips[k] << 16, 0x00400, 0x150, 2);           /* 44: interlaced */
    add(&n, 0, 1u << 22 | 2u << 16, 0x60800, 0x150, 2);                                         /* 48 */
    add(&n, 0, 1u << 22 | 2u << 16, 0x20800, 0x150, 2);
    add(&n, 0, 1u << 22 | 2u << 16, 0x00800, 0x100, 2);
    add(&n, 0, 1u << 22 | 2u << 16, 0x00400, 0x100, 0);                                         /* 51: as 36 once more */
}

static void test_setup(void) {
    make_pages();
    pvrio_reg(FB_W_SOF2_addr, SOF2);
    pvrio_reg(SPG_CONTROL_addr, 0x150);
}

static void test_describe(void) {
    static const char *names[4] = { "FB_R_CTRL |=", "SCALER_CTL =", "FB_W_CTRL |=", "SPG_CONTROL =" };
    int p;
    make_pages();
    for(p = 0; p < NUM_PAGES; p++)
        label_span(p, 0, 20, 15, "%s %08x with SCALER_CTL %05x SPG_CONTROL %03x tile order %d", names[pages[p].what], (unsigned)pages[p].value,
                   (unsigned)pages[p].scaler, (unsigned)pages[p].spg, pages[p].order);
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

static void screen(uint32_t argb) {
    ta_list_begin(ListType_Opaque);
    flat();
    quad0(0, 0, 640, 480, F32B_ONE, argb);
    ta_list_end();
}

static void core_reset(void) {
    pvrio_reg(SOFTRESET_addr, 2);
    pvrio_reg(SOFTRESET_addr, 0);
}

/* a picture that says where a pixel comes from: tile (tx, ty) has the colour (8 tx + 4, 16 ty + 8, page) */
static void tiles_picture(int page) {
    int tx, ty;
    ta_list_begin(ListType_Opaque);
    for(ty = 0; ty < 15; ty++)
        for(tx = 0; tx < 20; tx++) {
            flat();
            quad0(tx * 32, ty * 32, 32, 32, F32B_ONE, 0xff000000u | (uint32_t)(8 * tx + 4) << 16 | (uint32_t)(16 * ty + 8) << 8 | (uint32_t)page);
        }
    ta_list_end();
}

#ifdef _arch_dreamcast
static void count(uint32_t at, uint32_t *out, uint32_t *value) {
    volatile uint32_t *v = (volatile uint32_t *)(0xa5000000 + at);
    uint32_t i, n = 0, first = 0xffffffff, last = 0;
    for(i = 0; i < WORDS; i++)
        if((v[i] & 0x00ffffff) != 0x00ff00ff) {
            if(!n) { first = i; if(value) *value = v[i]; }
            last = i;
            n++;
        }
    out[0] = n; out[1] = first; out[2] = last;
}
#endif

static void test_page(int page) {
    uint32_t notes[12] = { 0 };
    char name[32];
    int strip = pages[page].what == 0;
#ifdef _arch_dreamcast
    volatile uint32_t *reg = (volatile uint32_t *)0xa05f8000;
    volatile uint32_t *v1 = (volatile uint32_t *)(0xa5000000 + SETUP_FB), *v2 = (volatile uint32_t *)(0xa5000000 + SOF2);
    uint32_t fb_r = reg[0x44 / 4] & ~0x007f0000u, fb_w = reg[0x48 / 4], i;

    for(i = 0; i < WORDS; i++) { v1[i] = 0x00ff00ff; v2[i] = 0x00ff00ff; }
#endif
    if(strip) {             /* everything from the start: the page before may have left the core stuck */
        pvrio_reg(SOFTRESET_addr, 0xffffffff);
        pvrio_reg(SOFTRESET_addr, 0);
        pvrio_reg(SCALER_CTL_addr, pages[page].scaler);
        pvrio_reg(SPG_CONTROL_addr, pages[page].spg);
        pvr_region_write(pages[page].order);
        pvr_frame_begin();
    } else {
        core_reset();
        pvrio_reg(SCALER_CTL_addr, pages[page].scaler);
        pvrio_reg(SPG_CONTROL_addr, pages[page].spg);
    }
#ifdef _arch_dreamcast
    if(pages[page].what == 0) reg[0x44 / 4] = fb_r | pages[page].value;
    if(pages[page].what == 2) reg[0x48 / 4] = fb_w | pages[page].value;
    tiles_picture(page);
    vid_waitvbl();
    notes[10] = reg[0x10c / 4];
    pvr_frame_render();
    notes[6] = reg[0x44 / 4]; notes[7] = reg[0xf4 / 4]; notes[8] = reg[0x48 / 4]; notes[9] = reg[0xd0 / 4];
    count(SETUP_FB, &notes[0], 0);
    count(SOF2, &notes[3], &notes[11]);
    reg[0x44 / 4] = fb_r;
    reg[0x48 / 4] = fb_w;
#else
    tiles_picture(page);
    pvr_frame_render();
#endif
    pvrio_w32(NOTES, notes, 12);
    snprintf(name, sizeof(name), "page%ds", page);
    pvrio_dump32(NOTES, 12, name, ".raw");
    if(strip) {
        snprintf(name, sizeof(name), "page%da", page);
        pvrio_dump32(SETUP_FB, WORDS, name, ".raw");
        snprintf(name, sizeof(name), "page%db", page);
        pvrio_dump32(SOF2, WORDS, name, ".raw");
    }
    if(strip) {
        pvrio_reg(SOFTRESET_addr, 0xffffffff);
        pvrio_reg(SOFTRESET_addr, 0);
        pvrio_reg(SCALER_CTL_addr, 0x00400);
        pvrio_reg(SPG_CONTROL_addr, 0x150);
        pvr_region_write(0);
    } else {
        core_reset();
        pvrio_reg(SCALER_CTL_addr, 0x00400);
        pvrio_reg(SPG_CONTROL_addr, 0x150);
    }
    pvr_frame_begin();
    screen(0xff004000u | (uint32_t)page);
}
