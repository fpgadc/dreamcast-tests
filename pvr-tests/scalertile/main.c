/* The scaler at the borders of tiles, with the tiles in the region array in different orders, and the
   wrap of the output line.

   Every page first renders the whole screen magenta without scaling and with the usual region array, then
   the picture with the page's SCALER_CTL and tile order (pvr_region_write in ../../common/pvrsetup.h):

       order 0  column by column from the left (KallistiOS; the last entry is the bottom right tile)
             1  column by column from the right (the last entry is the bottom left tile)
             2  row by row from the top
             3  row by row from the bottom (the last entry is the top right tile)
             4  as 0, starting with the last column (the last entry is the bottom of column 18)
             5  as 0 without the last column

   The picture: one quad per line over the whole width, line y in the colour line_code(y); on top of that,
   in the tiles of coded[], one quad per pixel in the colour pixel_code(x in the tile, y in the tile).

   Result on a console (README, scalermeas / scalertile / sof2probe): the scaler is a stream through the
   tiles in the order of the region array; a tile with y = 0 starts it again. Only column by column gives the
   picture one expects. refsw2 writes the same pixels on every page. */
#define NUM_PAGES 34
#include "pvrtest2.h"

/* order2 >= 0: the picture is rendered twice without resetting the core in between, the second time with
   the tiles in order2 and other colours (state the scaler carries from one frame to the next).
   tall: FB_Y_CLIP lets through all 1024 lines; lines 480..959 are dumped as page<n>b. */
static const struct { uint32_t scaler; int order, order2, tall; } pages[NUM_PAGES] = {
    { 0x60800, 0, -1 }, { 0x00401, 0, -1 }, { 0x00555, 0, -1 }, { 0x00801, 0, -1 },
    { 0x00400, 1, -1 }, { 0x60800, 1, -1 }, { 0x00401, 1, -1 },
    { 0x00400, 2, -1 }, { 0x60800, 2, -1 }, { 0x00401, 2, -1 },
    { 0x00400, 3, -1 }, { 0x60800, 3, -1 }, { 0x00401, 3, -1 },
    { 0x00400, 4, -1 }, { 0x60800, 4, -1 }, { 0x00401, 4, -1 },
    { 0x00080, 0, -1 }, { 0x00040, 0, -1 },         /* 16: eight and sixteen lines out of one: where does the output line wrap? */
    { 0x00200, 2, -1 }, { 0x00300, 3, -1 },         /* 18: more lines, other orders */
    { 0x00200, 3, -1 }, { 0x00100, 3, -1 },         /* 20: the end of the frame without the filter */
    { 0x00200, 0, -1, 1 },                          /* 22: all the lines of a picture twice as high */
    { 0x60401, 2, -1 }, { 0x60555, 3, -1 },         /* 23: second field and fractions */
    { 0x10800, 2, -1 }, { 0x10400, 2, -1 }, { 0x60400, 3, -1 },     /* 25: halved; no vertical scaling */
    { 0x00800, 5, -1 },                             /* 28: without the last column */
    { 0x60800, 0, 5 }, { 0x00200, 0, 5 },           /* 29: two frames, the second without the last column */
    { 0x00800, 3, 3 }, { 0x00401, 3, 3 }, { 0x00300, 2, 2 },        /* 31: two frames in an order that does not start at the top */
};

static const int coded[][2] = { { 19, 14 }, { 0, 14 }, { 19, 0 }, { 10, 7 }, { 18, 14 }, { 19, 13 } };
#define NUM_CODED ((int)(sizeof(coded) / sizeof(coded[0])))

static void test_setup(void) {
}

static void test_describe(void) {
    int p;
    for(p = 0; p < NUM_PAGES; p++)
        label_span(p, 0, 20, 15, "SCALER_CTL %05x tile order %d then %d%s", (unsigned)pages[p].scaler, pages[p].order,
                   pages[p].order2, pages[p].tall ? " 1024 lines" : "");
}

/* read.py has the same */
static uint32_t line_code(int n) {
    uint32_t r = (uint32_t)(n * 97 + 13) & 255, g = (uint32_t)(n * 57 + 101) & 255, b = (uint32_t)(n >> 1) & 255;
    return 0xff000000 | r << 16 | g << 8 | b;
}

static uint32_t pixel_code(int x, int y) {
    uint32_t r = (uint32_t)(x * 8 + 3), g = (uint32_t)(y * 8 + 5), b = (uint32_t)(x * 5 + y * 11 + 1) & 255;
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

static void picture(int shift) {
    int n, x, y;

    ta_list_begin(ListType_Opaque);
    for(n = 0; n < 480; n++) {
        flat();
        quad0(0, n, 640, 1, F32B_ONE, line_code(n + shift));
    }
    for(n = 0; n < NUM_CODED; n++)
        for(y = 0; y < 32; y++)
            for(x = 0; x < 32; x++) {
                flat();
                quad0(coded[n][0] * 32 + x, coded[n][1] * 32 + y, 1, 1, F32B_ONE, pixel_code((x + shift) & 31, (y + 2 * shift) & 31));
            }
    ta_list_end();
}

static void test_page(int page) {
    static uint32_t magenta[640];
    char name[32];
    int y;

    /* a frame without scaling that fills the framebuffer */
    set_scaler(0x00400);
    pvr_region_write(0);
    pvr_frame.FB_Y_CLIP = 479u << 16;
    ta_list_begin(ListType_Opaque);
    flat();
    quad0(0, 0, 640, 480, F32B_ONE, 0xffff00ff);
    ta_list_end();
    pvr_frame_render();

    set_scaler(pages[page].scaler);
    pvr_region_write(pages[page].order);
    if(pages[page].tall) {              /* fill the lines below the screen, render, dump them; then the harness renders again */
        pvr_frame.FB_Y_CLIP = 1023u << 16;
        for(y = 0; y < 640; y++)
            magenta[y] = 0x00ff00ff;
        for(y = 480; y < 960; y++)
            pvrio_w32(SETUP_FB + (uint32_t)y * 640 * 4, magenta, 640);
        pvr_frame_begin();
        picture(0);
        pvr_frame_render();
        snprintf(name, sizeof(name), "page%db", page);
        pvrio_dump32(SETUP_FB + 480u * 640 * 4, 640 * 480, name, ".raw");
        set_scaler(pages[page].scaler);
    }
    pvr_frame_begin();
    picture(0);
    if(pages[page].order2 >= 0) {
        pvr_frame_render();
        pvr_region_write(pages[page].order2);
        pvr_frame_begin();
        picture(5);
    }
}
