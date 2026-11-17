/* Texture environment, wrap modes and color clamp, without the texel expansion and sampling-position
   differences getting in the way: the textures are 8 bpp paletted with an ARGB8888 palette (exact on both
   sides) and the point-sampled pattern uses POINT_FIX.

   page 0  texture environment: env (replace, modulate, decal, modulate alpha) x texture alpha on/off x
           vertex alpha on/off x offset color on/off; cells 0-31 translucent list, 40-71 opaque list.
           Four squares per cell with different texel, base and offset colors (flat shaded).
   page 1  the same, showing the alpha that reaches the framebuffer (white * destination alpha)
   page 2  wrap modes: 64x64 quads with coordinates -1 .. 3 (2 pixels per texel) for the 16 combinations
           of flip and clamp. The texture is 8x8: red = column, green = row, blue = checkerboard.
   page 3  color clamp (max 0xc0a080, min 0x203040): 64 gray levels, 4 squares per cell.
           cells 0-15 untextured, clamp on; 20-35 untextured, clamp off;
           40-55 textured (modulate) + offset 0x404040, clamp on; 60-75 the same, clamp off */
#define NUM_PAGES 4
#include "pvrtest2.h"

static const uint32_t texel[4] = { 0xffffffff, 0x80c08040, 0x40ff00ff, 0xc03070b0 };
static const uint32_t base[4] = { 0x80c08040, 0xffffffff, 0xc0ff4080, 0x40a0e020 };
static const uint32_t offset[4] = { 0xff204060, 0x00404040, 0xffc0c0c0, 0x80ff00ff };
static uint32_t texel_txr[4], pattern;      /* 64 bit area addresses */

static void test_setup(void) {
    uint8_t t[64];
    int q, i;

    pvr_pal_format(PAL_RAM_CTRL_ARGB8888);
    for(i = 0; i < 64; i++)
        pvr_pal_entry(i, 0xff000000 | ((i % 8) * 36 << 16) | ((i / 8) * 36 << 8) | ((((i % 8) ^ (i / 8)) & 1) ? 255 : 0));
    for(q = 0; q < 4; q++) {
        pvr_pal_entry(64 + q, texel[q]);
        memset(t, 64 + q, sizeof(t));
        texel_txr[q] = pvr_tex_alloc(sizeof(t));
        pvr_tex_load(texel_txr[q], t, sizeof(t));
    }
    for(i = 0; i < 64; i++)
        t[i] = i;
    pattern = pvr_tex_alloc(64);
    pvr_tex_load_twiddled8(pattern, t, 8, 8);
}

static const char *env_name[4] = { "replace", "modulate", "decal", "modulate alpha" };
static const char *uvmode[4] = { "none", "V", "U", "UV" };

static void test_describe(void) {
    int c, page;

    for(page = 0; page < 2; page++)
        for(c = 0; c < 32; c++) {
            label(page, c, "TR %s%s%s%s%s", page ? "(alpha) " : "", env_name[c & 3], (c & 4) ? ", texture alpha" : "",
                  (c & 8) ? ", vertex alpha" : "", (c & 16) ? ", offset" : "");
            label(page, 40 + c, "OP %s%s%s%s%s", page ? "(alpha) " : "", env_name[c & 3], (c & 4) ? ", texture alpha" : "",
                  (c & 8) ? ", vertex alpha" : "", (c & 16) ? ", offset" : "");
        }
    for(c = 0; c < 16; c++)
        label_span(2, (c / 8) * 80 + (c % 8) * 2, 2, 2, "wrap: flip %s clamp %s", uvmode[c / 4], uvmode[c % 4]);
    for(c = 0; c < 16; c++) {
        label(3, c, "clamp on, untextured, gray %d..", c * 16);
        label(3, 20 + c, "clamp off, untextured, gray %d..", c * 16);
        label(3, 40 + c, "clamp on, modulate + offset, gray %d..", c * 16);
        label(3, 60 + c, "clamp off, modulate + offset, gray %d..", c * 16);
    }
}

/* the header words of a polygon: textured (at != 0) with an 8x8 8 bpp texture, point sampled, or plain */
static void poly(int list, uint32_t at, int ShadInstr, int IgnoreTexA, int UseAlpha, int Offset, int Gouraud,
                 int ColorClamp, int FlipU, int FlipV, int ClampU, int ClampV, int SrcInstr) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };

    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = list;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Gouraud = Gouraud;
    pcw.Texture = at != 0;
    pcw.Offset = Offset;
    isp.DepthMode = DepthMode_Always;
    isp.ZWriteDis = 1;
    isp.CullMode = CullMode_None;
    isp.Texture = at != 0;
    isp.Gouraud = Gouraud;
    isp.Offset = Offset;
    tsp.SrcInstr = SrcInstr;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = UseAlpha;
    tsp.ColorClamp = ColorClamp;
    if(at) {
        tsp.ShadInstr = ShadInstr;
        tsp.IgnoreTexA = IgnoreTexA;
        tsp.FilterMode = FilterMode_Point;
        tsp.MipMapD = 4;
        tsp.FlipU = FlipU;
        tsp.FlipV = FlipV;
        tsp.ClampU = ClampU;
        tsp.ClampV = ClampV;
        tsp.TexU = 0;                           /* 8 texels */
        tsp.TexV = 0;
        tcw.PixelFmt = PixelFmt_Pal8;
        tcw.PalSelect = 0;
        tcw.TexAddr = at >> 3;
    }
    ta_polygon(pcw, isp, tsp, tcw);
}

static void env_cells(int list, int first) {
    int c, q;

    for(c = 0; c < 32; c++) {
        for(q = 0; q < 4; q++) {
            /* env c & 3 is ShadInstr (replace = decal-type 0, modulate 1, decal 2, modulate alpha 3) */
            poly(list, texel_txr[q], c & 3, !(c & 4), !!(c & 8), !!(c & 16), 0, 0, 0, 0, 0, 0, BlendInstr_One);
            quad3(cell_px(first + c) + (q & 1) * 16, cell_py(first + c) + (q >> 1) * 16, 16, 16, F32B_ONE,
                  F32B_ZERO, F32B_ZERO, F32B_ONE, F32B_ONE, base[q], offset[q]);
        }
    }
}

static void clamp_cells(int first, int textured, int clamp) {
    int n;

    poly(ListType_Translucent, textured ? texel_txr[0] : 0, ShadInstr_Modulate, 0, 1, textured, 0, clamp, 0, 0, 0, 0, BlendInstr_One);
    for(n = 0; n < 64; n++) {
        uint32_t g = n * 4 + (n == 63 ? 3 : 0);
        int x = cell_px(first + n / 4) + (n & 1) * 16, y = cell_py(first + n / 4) + ((n >> 1) & 1) * 16;
        uint32_t col = 0xff000000 | (g << 16) | (g << 8) | g;
        if(textured)
            quad3(x, y, 16, 16, F32B_ONE, F32B_ZERO, F32B_ZERO, F32B_ONE, F32B_ONE, col, 0x00404040);
        else
            quad0(x, y, 16, 16, F32B_ONE, col);
    }
}

static void test_page(int page) {
    int c;

    pvrio_reg(FOG_CLAMP_MAX_addr, 0xffc0a080);          /* color clamp max / min */
    pvrio_reg(FOG_CLAMP_MIN_addr, 0x00203040);
    if(page < 2) {
        ta_list_begin(ListType_Opaque);
        env_cells(ListType_Opaque, 40);
        ta_list_end();
    }
    ta_list_begin(ListType_Translucent);
    if(page < 2) {
        env_cells(ListType_Translucent, 0);
        if(page == 1) {                                 /* white * destination alpha */
            poly(ListType_Translucent, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, BlendInstr_DstAlpha);
            quad0(0, 0, 640, 480, F32B_ONE, 0xffffffff);
        }
    }
    else if(page == 2) {
        for(c = 0; c < 16; c++) {
            int cell = (c / 8) * 80 + (c % 8) * 2;
            int fl = c / 4, cl = c % 4;                 /* none, V, U, UV: bit 0 = V, bit 1 = U */
            poly(ListType_Translucent, pattern, ShadInstr_Decal, 0, 1, 0, 1, 0, fl >> 1, fl & 1, cl >> 1, cl & 1, BlendInstr_One);
            quad3(cell_px(cell), cell_py(cell), 64, 64, F32B_ONE, F(-1.0f), F(-1.0f), F(3.0f), F(3.0f), 0xffffffff, 0);
        }
    }
    else {
        clamp_cells(0, 0, 1);
        clamp_cells(20, 0, 0);
        clamp_cells(40, 1, 1);
        clamp_cells(60, 1, 0);
    }
    ta_list_end();
}
