/* Shading: Gouraud interpolation, and how texture, base color and offset color are combined.

   page 0  Gouraud quads (two triangles, split along the top right - bottom left diagonal):
           cells 0-15   one cell each, corner colors per the label
           rows 1, 2    a 640 pixel wide ramp, 0 -> 255 and 255 -> 0
           rows 3-10    256 pixel tall ramps, 32 wide
   page 1  texture environment: every combination of
               texture env (replace, modulate, decal, modulate alpha) x texture alpha on/off x
               vertex alpha on/off x offset color on/off
           cells 0-31 in the translucent list, cells 40-71 in the opaque list. Each cell is four squares
           with different texel (uniform ARGB4444 texture), base and offset colors; flat shaded.
   page 2  the same, showing the alpha that reaches the framebuffer (white * destination alpha) */
#define NUM_PAGES 3
#include "pvrtest2.h"

typedef struct { uint32_t c[4]; const char *name; } corners_t;
static const corners_t small[16] = {
    { { 0xff000000, 0xffffffff, 0xff000000, 0xffffffff }, "left 0, right 255" },
    { { 0xffffffff, 0xff000000, 0xffffffff, 0xff000000 }, "left 255, right 0" },
    { { 0xff000000, 0xff000000, 0xffffffff, 0xffffffff }, "top 0, bottom 255" },
    { { 0xffffffff, 0xffffffff, 0xff000000, 0xff000000 }, "top 255, bottom 0" },
    { { 0xffffffff, 0xff000000, 0xff000000, 0xff000000 }, "top left 255, rest 0" },
    { { 0xff000000, 0xffffffff, 0xff000000, 0xff000000 }, "top right 255, rest 0" },
    { { 0xff000000, 0xff000000, 0xffffffff, 0xff000000 }, "bottom left 255, rest 0" },
    { { 0xff000000, 0xff000000, 0xff000000, 0xffffffff }, "bottom right 255, rest 0" },
    { { 0xffff0000, 0xff00ff00, 0xff0000ff, 0xffffffff }, "red, green, blue, white" },
    { { 0xff000000, 0xff101010, 0xff000000, 0xff101010 }, "left 0, right 16" },
    { { 0xff000000, 0xff010101, 0xff000000, 0xff010101 }, "left 0, right 1" },
    { { 0xfffefefe, 0xffffffff, 0xfffefefe, 0xffffffff }, "left 254, right 255" },
    { { 0xff646464, 0xff656565, 0xff646464, 0xff656565 }, "left 100, right 101" },
    { { 0xff808080, 0xff808080, 0xff808080, 0xff808080 }, "all 128" },
    { { 0xff000000, 0xff0000ff, 0xff00ff00, 0xffff0000 }, "black, blue, green, red" },
    { { 0xff7f7f7f, 0xff808080, 0xff808080, 0xff7f7f7f }, "127, 128, 128, 127" },
};

/* texel, base and offset colors of the four squares of a cell */
static const uint16_t texel4444[4] = { 0xffff, 0x8c84, 0x4f0f, 0xc37b };
static const uint32_t base[4] = { 0x80c08040, 0xffffffff, 0xc0ff4080, 0x40a0e020 };
static const uint32_t offset[4] = { 0xff204060, 0x00404040, 0xffc0c0c0, 0x80ff00ff };
static uint32_t texel_txr[4];

static void test_setup(void) {
    uint16_t t[64];
    int q, i;

    for(q = 0; q < 4; q++) {
        for(i = 0; i < 64; i++)
            t[i] = texel4444[q];
        texel_txr[q] = pvr_tex_alloc(sizeof(t));
        pvr_tex_load(texel_txr[q], t, sizeof(t));
    }
}

static const char *env_name[4] = { "replace", "modulate", "decal", "modulate alpha" };

static void test_describe(void) {
    int c, page;

    for(c = 0; c < 16; c++)
        label(0, c, "gouraud: %s", small[c].name);
    label_span(0, 20, 20, 1, "gouraud: 640 px ramp 0 -> 255");
    label_span(0, 40, 20, 1, "gouraud: 640 px ramp 255 -> 0");
    label_span(0, 60, 1, 8, "gouraud: 256 px tall ramp 0 -> 255");
    label_span(0, 61, 1, 8, "gouraud: 256 px tall ramp 255 -> 0");
    label_span(0, 62, 1, 8, "gouraud: 256 px tall ramp, red 0 -> 255, blue 255 -> 0");
    for(page = 1; page < 3; page++)
        for(c = 0; c < 32; c++) {
            label(page, c, "TR %s%s%s%s%s", page == 2 ? "(alpha) " : "", env_name[c & 3], (c & 4) ? ", texture alpha" : "",
                  (c & 8) ? ", vertex alpha" : "", (c & 16) ? ", offset" : "");
            label(page, 40 + c, "OP %s%s%s%s%s", page == 2 ? "(alpha) " : "", env_name[c & 3], (c & 4) ? ", texture alpha" : "",
                  (c & 8) ? ", vertex alpha" : "", (c & 16) ? ", offset" : "");
        }
}

/* An untextured polygon in the translucent list, Gouraud shaded; blending: source as it is, or (alpha_view)
   the source scaled by the destination alpha. */
static void polygon_gouraud(int SrcInstr) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };

    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Translucent;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Gouraud = 1;
    isp.DepthMode = DepthMode_Always;
    isp.ZWriteDis = 1;
    isp.CullMode = CullMode_None;
    tsp.SrcInstr = SrcInstr;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    ta_polygon(pcw, isp, tsp, tcw);
}

/* corners: top left, top right, bottom left, bottom right */
static void quad_gouraud(int x, int y, int w, int h, const uint32_t argb[4]) {
    int k;

    polygon_gouraud(BlendInstr_One);
    for(k = 0; k < 4; k++)
        ta_vertex0(f32b_int(x + (k & 1) * w), f32b_int(y + (k >> 1) * h), F32B_ONE, argb[k], k == 3);
}

static void env_cells(int list, int first) {
    int c, q, k;

    for(c = 0; c < 32; c++) {
        for(q = 0; q < 4; q++) {
            PCW pcw = { .full = 0 };
            ISP_TSP isp = { .full = 0 };
            TSP tsp = { .full = 0 };
            TCW tcw = { .full = 0 };
            int x = cell_px(first + c) + (q & 1) * 16, y = cell_py(first + c) + (q >> 1) * 16;

            pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
            pcw.ListType = list;
            pcw.Group_En = 1;
            pcw.Strip_Len = 1;
            pcw.Gouraud = 0;                                    /* flat */
            pcw.Texture = 1;
            pcw.Offset = (c & 16) ? 1 : 0;
            isp.DepthMode = DepthMode_Always;
            isp.ZWriteDis = 1;
            isp.CullMode = CullMode_None;
            isp.Texture = 1;
            tsp.SrcInstr = BlendInstr_One;
            tsp.DstInstr = BlendInstr_Zero;
            tsp.FogCtrl = FogCtrl_None;
            tsp.UseAlpha = (c & 8) ? 1 : 0;
            tsp.IgnoreTexA = (c & 4) ? 0 : 1;
            tsp.ShadInstr = c & 3;
            tsp.FilterMode = FilterMode_Point;
            tsp.MipMapD = 4;
            tsp.TexU = tsp.TexV = 0;                            /* 8 x 8 */
            tcw.PixelFmt = PixelFmt_4444;
            tcw.TexAddr = texel_txr[q] >> 3;
            ta_polygon(pcw, isp, tsp, tcw);
            for(k = 0; k < 4; k++)
                ta_vertex3(f32b_int(x + (k & 1) * 16), f32b_int(y + (k >> 1) * 16), F32B_ONE,
                           (k & 1) ? F32B_ONE : F32B_ZERO, (k >> 1) ? F32B_ONE : F32B_ZERO, base[q], offset[q], k == 3);
        }
    }
}

static void test_page(int page) {
    int c;

    if(page == 0) {
        static const uint32_t up[4] = { 0xff000000, 0xffffffff, 0xff000000, 0xffffffff };
        static const uint32_t down[4] = { 0xffffffff, 0xff000000, 0xffffffff, 0xff000000 };
        static const uint32_t v_up[4] = { 0xff000000, 0xff000000, 0xffffffff, 0xffffffff };
        static const uint32_t v_down[4] = { 0xffffffff, 0xffffffff, 0xff000000, 0xff000000 };
        static const uint32_t v_mix[4] = { 0xff0000ff, 0xff0000ff, 0xffff0000, 0xffff0000 };
        ta_list_begin(ListType_Translucent);
        for(c = 0; c < 16; c++)
            quad_gouraud(cell_px(c), cell_py(c), 32, 32, small[c].c);
        quad_gouraud(0, 32, 640, 32, up);
        quad_gouraud(0, 64, 640, 32, down);
        quad_gouraud(0, 96, 32, 256, v_up);
        quad_gouraud(32, 96, 32, 256, v_down);
        quad_gouraud(64, 96, 32, 256, v_mix);
        ta_list_end();
        return;
    }
    ta_list_begin(ListType_Opaque);
    env_cells(ListType_Opaque, 40);
    ta_list_end();
    ta_list_begin(ListType_Translucent);
    env_cells(ListType_Translucent, 0);
    if(page == 2) {
        polygon_gouraud(BlendInstr_DstAlpha);           /* the alpha that reached the framebuffer */
        quad0(0, 0, 640, 480, F32B_ONE, 0xffffffff);
    }
    ta_list_end();
}
