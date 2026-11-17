/* The scaler (SCALER_CTL): what reaches the framebuffer when the core scales the picture vertically
   (bits 15..0: 0x400 is 1:1, 0x800 two lines into one, 0x200 one into two), halves it horizontally (bit 16),
   or writes one field of an interlaced frame (bits 17, 18). Analyze with read.py.

   Every page first renders the whole screen in one colour (magenta) without scaling, then the picture with
   the page's SCALER_CTL: what the scaled render did not write stays magenta. With the interlace bit the
   second framebuffer (FB_W_SOF2, at SOF2 below) is filled with magenta too and dumped as page<n>b. Some pages also
   put the display into interlaced mode (SPG_CONTROL) or set the pixel clip (FB_X_CLIP, FB_Y_CLIP).

       left half     one quad per line, in two bands of 160 pixels: line y has the colour code(band, y)
       right half    one quad per column, in two bands of 240 lines: column x has code(band, x - 320)

   A console whose SCALER_CTL is changed after it has rendered a frame never finishes the next render (it
   writes the first tile and stops; every value works when it is set before the first render). Resetting the
   core (SOFTRESET bit 1) before the change avoids that. */
#define NUM_PAGES 43
#include "pvrtest2.h"

#define SOF2 0x200000u          /* 32 bit area; no textures in this test */

/* SCALER_CTL; SPG_CONTROL (0x100: as with a VGA cable; 0x150: interlaced); FB_X_CLIP and FB_Y_CLIP (0: the whole screen) */
static const struct { uint32_t scaler, spg, xclip, yclip; } pages[NUM_PAGES] = {
    { 0x00400 }, { 0x00401 }, { 0x00420 }, { 0x00555 }, { 0x00600 }, { 0x00700 }, { 0x00800 }, { 0x00801 },       /* down */
    { 0x00c00 }, { 0x01000 }, { 0x02000 },
    { 0x003ff }, { 0x003c0 }, { 0x00300 }, { 0x00280 }, { 0x00200 }, { 0x00100 },                                 /* up */
    { 0x10400 }, { 0x10800 }, { 0x10300 },                                                                        /* halved horizontally */
    { 0x20400 }, { 0x60400 }, { 0x20800 }, { 0x60800 }, { 0x70800 },                                              /* interlace: field 0, field 1 */
    { 0x60600 }, { 0x60500 }, { 0x60c00 }, { 0x61000 }, { 0x60401 }, { 0x60300 }, { 0x20600 },                    /* 25: other factors */
    { 0x40800 }, { 0x40400 },                                                                                     /* 32: the field bit alone */
    { 0x20400, 0x150 }, { 0x60400, 0x150 }, { 0x20800, 0x150 }, { 0x60800, 0x150 },                               /* 34: display interlaced */
    { 0x00200, 0, 0, 239u << 16 }, { 0x00800, 0, 0, 199u << 16 | 100 },                                          /* 38: FB_Y_CLIP */
    { 0x00400, 0, 399u << 16 | 100, 299u << 16 | 50 }, { 0x00800, 0, 399u << 16 | 100, 299u << 16 | 50 },         /* 40: both clips */
    { 0x10400, 0, 399u << 16 | 100, 0 },
};

static void test_setup(void) {
    pvrio_reg(FB_W_SOF2_addr, SOF2);
}

static void test_describe(void) {
    int p;
    for(p = 0; p < NUM_PAGES; p++)
        label_span(p, 0, 20, 15, "SCALER_CTL %05x SPG_CONTROL %03x clip x %08x y %08x", (unsigned)pages[p].scaler,
                   (unsigned)(pages[p].spg ? pages[p].spg : 0x100), (unsigned)pages[p].xclip, (unsigned)pages[p].yclip);
}

/* the colour of line (or column) n in a band: read.py has the same */
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
    char name[32];
    int second = (pages[page].scaler >> 17) != 0, y;       /* interlace or field bit: look at FB_W_SOF2 too */

    /* a frame without scaling or clipping that fills the framebuffer */
    set_scaler(0x00400);
    pvrio_reg(SPG_CONTROL_addr, 0x100);
    pvr_frame.FB_X_CLIP = 639u << 16;
    pvr_frame.FB_Y_CLIP = 479u << 16;
    ta_list_begin(ListType_Opaque);
    flat();
    quad0(0, 0, 640, 480, F32B_ONE, 0xffff00ff);
    ta_list_end();
    pvr_frame_render();

    set_scaler(pages[page].scaler);
    if(pages[page].spg)
        pvrio_reg(SPG_CONTROL_addr, pages[page].spg);
    if(pages[page].xclip)
        pvr_frame.FB_X_CLIP = pages[page].xclip;
    if(pages[page].yclip)
        pvr_frame.FB_Y_CLIP = pages[page].yclip;
    pvr_frame_begin();
    if(second) {                     /* fill the second buffer, render, dump it; then the harness renders again */
        for(y = 0; y < 640; y++)
            magenta[y] = 0x00ff00ff;
        for(y = 0; y < 480; y++)
            pvrio_w32(SOF2 + (uint32_t)y * 640 * 4, magenta, 640);
        picture();
        pvr_frame_render();
        snprintf(name, sizeof(name), "page%db", page);
        pvrio_dump32(SOF2, 640 * 480, name, ".raw");
        pvr_frame_begin();
    }
    picture();
}
