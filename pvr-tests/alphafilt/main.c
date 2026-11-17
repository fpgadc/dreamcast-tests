/* The alpha of a bilinear sample between a texel of alpha 1 and one of alpha 0 (2026-11-18).

   polly2-data shenmue_intro3 and shenmue_intro4 (PT_ALPHA_REF 0xff): punch-through pixels whose sample takes
   8/256 of a transparent 1555 texel (VQ) pass the alpha test on a console; refsw2 filters the alpha to 0xf7 and
   fails them.

   8x8 white textures, texel column 3 opaque and column 4 transparent (alpha 1 / 0, or 0xf / 0); every pixel
   samples between them at a known fraction: in cell c, pixel x is f = 32 c + x 256ths of a texel past the
   center of the last opaque texel (row 3: pixel y, along v). Cells 0-7 of a row: f = 0 .. 255.
     row 0  1555, translucent SRC_ALPHA, INV_SRC_ALPHA over black: the filtered alpha read out (about a - 1)
     row 1  4444 (alpha 0xf / 0), the same
     row 2  1555 VQ, the same
     row 3  1555, along v (texel row 3 opaque, row 4 transparent), the same
     row 4  1555, punch through, white over red
     row 5  1555 VQ, punch through
     row 6  4444, punch through
   page 0: PT_ALPHA_REF 0xff; page 1: 0x80 */
#define NUM_PAGES 2
#include "pvrtest2.h"
#include "texgen.h"

enum { T1555, T4444, TVQ, T1555V, NTEX };
static uint32_t tex[NTEX];

/* opaque white left of column 4 (or above row 4) */
static uint32_t texel(const void *ctx, int size, int x, int y) {
    const int k = *(const int *)ctx;
    const int opaque = k == T1555V ? y < 4 : x < 4;
    (void)size;
    if(k == T4444)
        return (opaque ? 0xf000u : 0) | 0x0fffu;
    return (opaque ? 0x8000u : 0) | 0x7fffu;
}

static void test_setup(void) {
    static const int ids[NTEX] = { T1555, T4444, TVQ, T1555V };
    int k;
    for(k = 0; k < NTEX; k++) {
        const tex_layout_t l = { k == TVQ ? TEX_VQ : TEX_TWIDDLED, 16, 8, 8, 0 };
        tex[k] = tex_build(&l, texel, &ids[k]);
    }
}

static const char *what[7] = { "1555 translucent", "4444 translucent", "1555 VQ translucent", "1555 along v translucent",
                               "1555 punch through", "1555 VQ punch through", "4444 punch through" };

static void test_describe(void) {
    int p, r, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(r = 0; r < 7; r++)
            for(c = 0; c < 8; c++)
                label(p, r * COLS + c, "PT_ALPHA_REF %s, %s, f = %d .. %d / 256", p ? "80" : "ff", what[r], 32 * c, 32 * c + 31);
}

/* a 32x32 quad whose samples are at texel 3 + f / 256 along u (or v), f = 32 c + x (or + y); the other
   coordinate at texel 2 exactly */
static void cell(int row, int c, int list, int k) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    const int along_v = k == T1555V;
    /* sample position s = coord * 8 - 1/2 = 3 + f / 256 at the pixel center p + 1/2: coord * 2048 = 896 + 32 c + p
       - 1/2 at the edge p = 0, + 32 at the other edge */
    const f32b a0 = sf_ratio(1791 + 64 * c, 4096), a1 = sf_ratio(1855 + 64 * c, 4096), mid = sf_ratio(5, 16);
    const int x = cell_px(row * COLS + c), y = cell_py(row * COLS + c);
    int i;

    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = list;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Texture = 1;
    isp.DepthMode = DepthMode_Always;
    isp.CullMode = CullMode_None;
    isp.Texture = 1;
    tsp.SrcInstr = BlendInstr_SrcAlpha;
    tsp.DstInstr = BlendInstr_InvSrcAlpha;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    tsp.ShadInstr = ShadInstr_Modulate;
    tsp.FilterMode = FilterMode_Bilinear;
    tsp.MipMapD = 4;
    tsp.TexU = tsp.TexV = 0;
    tcw.PixelFmt = k == T4444 ? PixelFmt_4444 : PixelFmt_1555;
    tcw.VQ_Comp = k == TVQ;
    tcw.TexAddr = tex[k] >> 3;
    ta_polygon(pcw, isp, tsp, tcw);
    for(i = 0; i < 4; i++) {
        const f32b along = (i & 1) ? a1 : a0, across = mid;
        const f32b alongv = (i >> 1) ? a1 : a0;
        ta_vertex3(f32b_int(x + (i & 1) * 32), f32b_int(y + (i >> 1) * 32), F32B_ONE,
                   along_v ? across : along, along_v ? alongv : across, 0xffffffff, 0, i == 3);
    }
}

static void test_page(int page) {
    static const int trows[4] = { T1555, T4444, TVQ, T1555V }, prows[3] = { T1555, TVQ, T4444 };
    int r, c;
    pvrio_reg(PT_ALPHA_REF_addr, page ? 0x80 : 0xff);
    pvr_frame.bg_color = 0x000000;
    ta_list_begin(ListType_Opaque);
    for(r = 4; r < 7; r++)
        for(c = 0; c < 8; c++) {
            PCW pcw = { .full = 0 };
            ISP_TSP isp = { .full = 0 };
            TSP tsp = { .full = 0 };
            TCW tcw = { .full = 0 };
            pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
            pcw.ListType = ListType_Opaque;
            pcw.Group_En = 1;
            pcw.Strip_Len = 1;
            isp.DepthMode = DepthMode_Always;
            isp.CullMode = CullMode_None;
            tsp.SrcInstr = BlendInstr_One;
            tsp.DstInstr = BlendInstr_Zero;
            tsp.FogCtrl = FogCtrl_None;
            ta_polygon(pcw, isp, tsp, tcw);
            quad0(cell_px(r * COLS + c), cell_py(r * COLS + c), 32, 32, F32B_HALF, 0xffff0000);
        }
    ta_list_end();
    ta_list_begin(ListType_Translucent);
    for(r = 0; r < 4; r++)
        for(c = 0; c < 8; c++)
            cell(r, c, ListType_Translucent, trows[r]);
    ta_list_end();
    ta_list_begin(ListType_Punch_Through);
    for(r = 0; r < 3; r++)
        for(c = 0; c < 8; c++)
            cell(4 + r, c, ListType_Punch_Through, prows[r]);
    ta_list_end();
}
