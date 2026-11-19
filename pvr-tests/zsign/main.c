/* The depth compare below zero, and what a negative background is stored as (2026-11-19).

   pvr-tests/bgspec: the console's relation of probe depths to a background below zero is one cut in the order of
   the raw float bits read as SIGNED 32 bit integers (not in float order), and a finite negative background
   (-1, -0.5, -2.4e-6, -1.0000037 alike) sits between the bits of -2 and of -1e30 in that order, not at its own
   bits. pvr-tests/zneg could not tell how two negative depths compare (both polygons are shaded black: the TSP's
   quotient has the sign of 1/w).
   Pages 0-3, polygon against polygon: every cell a quad at depth A (DepthMode Always) textured green, then a quad
   at depth B textured white, DepthMode Greater (pages 0, 1) or Less (2, 3), both decal (the texel shows whatever
   the sign of 1/w). Rows A = V[row] (pages 0, 2) or V[row + 5] (1, 3), columns B = V[col].
   V: -2^100, -1e30, -2^60, -2^20, -2, -1.5, -1, -0.75, -0.5, -2^-20, -2^-21, -2^-100, -0, +0, 2^-100, 0.5, 1, 2,
   2^60, 2^100.
   Pages 4-15, a background below zero, BG[page - 4]: cells 0-147 a flat opaque quad at -2^k, k = c - 20
   (-2^-20 .. -2^127), DepthMode Greater; cells 150-297 the same, Less. Drawn: the probe is above (below) the
   background as the ISP compares them. */
#define NUM_PAGES 16
#include "pvrtest2.h"
#include "texgen.h"

static const uint32_t V[20] = {
    0xf1800000u, 0xf149f2cau, 0xdd800000u, 0xc9800000u, 0xc0000000u, 0xbfc00000u, 0xbf800000u, 0xbf400000u,
    0xbf000000u, 0xb5800000u, 0xb5000000u, 0x8d800000u, 0x80000000u, 0x00000000u, 0x0d800000u, 0x3f000000u,
    0x3f800000u, 0x40000000u, 0x5d800000u, 0x71800000u,
};
static const uint32_t BG[12] = {
    0xbf800000u, 0xbf000000u, 0xc0000000u, 0xbfc00000u,     /* -1, -0.5, -2, -1.5 */
    0xb6000000u, 0xba800000u, 0xc4800000u, 0xc9800000u,     /* -2^-19, -2^-10, -2^10, -2^20 */
    0xdd800000u, 0xf1800000u, 0xb5c00000u, 0xff000000u,     /* -2^60, -2^100, -1.5 2^-20, -2^127 */
};
static uint32_t texG, texW;

static uint32_t green(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0x07e0; }
static uint32_t white(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0xffff; }

static void test_setup(void)
{
    const tex_layout_t l = { TEX_TWIDDLED, 16, 8, 8, 0 };
    texG = tex_build(&l, green, NULL);
    texW = tex_build(&l, white, NULL);
}

static void test_describe(void)
{
    int p, c;
    for(p = 0; p < 4; p++)
        for(c = 0; c < 300; c++)
            label(p, c, "A %08x, B %08x, %s", (unsigned)V[c / 20 + (p & 1) * 5], (unsigned)V[c % 20], p < 2 ? "Greater" : "Less");
    for(p = 4; p < NUM_PAGES; p++)
        for(c = 0; c < 300; c++)
            if(c % 150 < 148)
                label(p, c, "background %08x, probe -2^%d, %s", (unsigned)BG[p - 4], c % 150 - 20, c < 150 ? "Greater" : "Less");
}

static void quad(int c, int mode, uint32_t z, uint32_t tex)
{
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    int k;
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Opaque;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Texture = tex != 0;
    isp.DepthMode = mode;
    isp.CullMode = CullMode_None;
    isp.Texture = tex != 0;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    tsp.ShadInstr = ShadInstr_Decal;
    tsp.FilterMode = FilterMode_Point;
    tsp.MipMapD = 4;
    tcw.PixelFmt = PixelFmt_565;
    tcw.TexAddr = tex >> 3;
    ta_polygon(pcw, isp, tsp, tcw);
    for(k = 0; k < 4; k++)
        if(tex)
            ta_vertex3(f32b_int(cell_px(c) + (k & 1) * 32), f32b_int(cell_py(c) + (k >> 1) * 32), z,
                       0x3e800000u /* 1/4 */, 0x3e800000u, 0xffffffffu, 0, k == 3);
        else
            ta_vertex0(f32b_int(cell_px(c) + (k & 1) * 32), f32b_int(cell_py(c) + (k >> 1) * 32), z, 0xffffffffu, k == 3);
}

static void test_page(int page)
{
    int c;
    pvr_frame.bg_color = 0x000040;
    if(page >= 4)
        pvr_frame.bg_depth = BG[page - 4];
    ta_list_begin(ListType_Opaque);
    for(c = 0; c < 300; c++) {
        if(page < 4) {
            quad(c, DepthMode_Always, V[c / 20 + (page & 1) * 5], texG);
            quad(c, page < 2 ? DepthMode_Greater : DepthMode_Less, V[c % 20], texW);
        } else if(c % 150 < 148) {
            const uint32_t z = 0x80000000u | (uint32_t)(c % 150 - 20 + 127) << 23;     /* -2^k */
            quad(c, c < 150 ? DepthMode_Greater : DepthMode_Less, z, 0);
        }
    }
    ta_list_end();
}
