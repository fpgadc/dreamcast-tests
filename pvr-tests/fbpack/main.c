/* What does the core write into the framebuffer in each pixel format (FB_W_CTRL.fb_packmode), and where do
   the bits that are not colour come from: fb_kval, fb_alpha_threshold or the alpha of the pixel?

   Every page draws the same picture with another FB_W_CTRL (the last four with dithering). Column c of the screen has its own colour and
   alpha (ALPHAS); the rows:
       0  opaque list, UseAlpha            1  opaque list, alpha ignored
       2  translucent, source only         3  translucent, source only, alpha ignored
       4  translucent, blended with the background by source alpha
       5  punch through, PT_ALPHA_REF 0x80
       6  translucent, source only, over an opaque polygon
       7..14  background
   The dump is always 640x480 words; a 16 bit format fills the first half of it, the 24 bit format three
   quarters (read.py takes the page's format into account). */
#define NUM_PAGES 17
#include "pvrtest2.h"

static const struct { uint8_t packmode, kval, threshold, dither; } formats[NUM_PAGES] = {
    { fb_packmode_0888, 0x00, 0x00 }, { fb_packmode_0888, 0xa5, 0x00 }, { fb_packmode_8888, 0xa5, 0x00 },
    { fb_packmode_8888, 0x00, 0x80 }, { fb_packmode_0555, 0x00, 0x00 }, { fb_packmode_0555, 0x80, 0x00 },
    { fb_packmode_0555, 0x7f, 0x00 }, { fb_packmode_1555, 0x00, 0x80 }, { fb_packmode_1555, 0xff, 0x00 },
    { fb_packmode_1555, 0x00, 0xff }, { fb_packmode_4444, 0x00, 0x00 }, { fb_packmode_888, 0xa5, 0x00 },
    { fb_packmode_565, 0x80, 0x00 },
    { fb_packmode_4444, 0x00, 0x00, 1 }, { fb_packmode_1555, 0x00, 0x80, 1 }, { fb_packmode_8888, 0x00, 0x00, 1 },
    { fb_packmode_0555, 0x80, 0x00, 1 },
};
static const uint8_t ALPHAS[COLS] = { 0x00, 0x01, 0x0f, 0x10, 0x1f, 0x20, 0x3f, 0x40, 0x7e, 0x7f,
                                      0x80, 0x81, 0xbf, 0xc0, 0xdf, 0xe0, 0xef, 0xf0, 0xfe, 0xff };

static uint32_t colour(int c) {
    return (uint32_t)ALPHAS[c] << 24 | (uint32_t)(255 - 12 * c) << 16 | (uint32_t)(13 * c) << 8 | (uint32_t)(0x55 ^ (c * 9));
}

static void polygon(int list, int UseAlpha, int SrcInstr, int DstInstr) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };

    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = list;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    isp.DepthMode = DepthMode_Always;
    isp.ZWriteDis = 1;
    tsp.SrcInstr = SrcInstr;
    tsp.DstInstr = DstInstr;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = UseAlpha;
    ta_polygon(pcw, isp, tsp, tcw);
}

static void row(int r, int list, int UseAlpha, int SrcInstr, int DstInstr) {
    int c;
    for(c = 0; c < COLS; c++) {
        polygon(list, UseAlpha, SrcInstr, DstInstr);
        quad0(c * 32, r * 32, 32, 32, F32B_ONE, colour(c));
    }
}

static void test_setup(void) {
    pvrio_reg(PT_ALPHA_REF_addr, 0x80);
}

static void test_describe(void) {
    static const char *rows[7] = { "opaque", "opaque, alpha ignored", "translucent", "translucent, alpha ignored",
                                   "translucent, blended", "punch through", "translucent over opaque" };
    int p, r;
    for(p = 0; p < NUM_PAGES; p++) {
        for(r = 0; r < 7; r++)
            label_span(p, r * COLS, COLS, 1, "packmode %d kval %02x threshold %02x dither %d: %s", formats[p].packmode, formats[p].kval, formats[p].threshold, formats[p].dither, rows[r]);
        label_span(p, 7 * COLS, COLS, 8, "packmode %d kval %02x threshold %02x dither %d: background", formats[p].packmode, formats[p].kval, formats[p].threshold, formats[p].dither);
    }
}

static void test_page(int page) {
    static const uint8_t bytes[8] = { 2, 2, 2, 2, 3, 4, 4, 4 };
    FB_W_CTRL_type fbw = { .full = 0 };

    fbw.fb_packmode = formats[page].packmode;
    fbw.fb_kval = formats[page].kval;
    fbw.fb_alpha_threshold = formats[page].threshold;
    fbw.fb_dither = formats[page].dither;
    pvrio_reg(FB_W_CTRL_addr, fbw.full);
    pvrio_reg(FB_W_LINESTRIDE_addr, 640 * bytes[formats[page].packmode] / 8);

    ta_list_begin(ListType_Opaque);
    row(0, ListType_Opaque, 1, BlendInstr_One, BlendInstr_Zero);
    row(1, ListType_Opaque, 0, BlendInstr_One, BlendInstr_Zero);
    polygon(ListType_Opaque, 0, BlendInstr_One, BlendInstr_Zero);
    quad0(0, 6 * 32, 640, 32, F32B_ONE, 0x40ffffff);
    ta_list_end();
    ta_list_begin(ListType_Translucent);
    row(2, ListType_Translucent, 1, BlendInstr_One, BlendInstr_Zero);
    row(3, ListType_Translucent, 0, BlendInstr_One, BlendInstr_Zero);
    row(4, ListType_Translucent, 1, BlendInstr_SrcAlpha, BlendInstr_InvSrcAlpha);
    row(6, ListType_Translucent, 1, BlendInstr_One, BlendInstr_Zero);
    ta_list_end();
    ta_list_begin(ListType_Punch_Through);
    row(5, ListType_Punch_Through, 1, BlendInstr_One, BlendInstr_Zero);
    ta_list_end();
}
