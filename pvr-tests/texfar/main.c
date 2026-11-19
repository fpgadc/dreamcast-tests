/* Texture coordinates far outside the texture, up to the end of the float range (2026-11-19).

   refsw3 makes the texel coordinate C = floor(u size 256) in two's complement (pvr-tests/coordfix), keeping the
   sign and the low 26 bits beyond 2^30 (all that wrap, flip and clamp look at); at 2^128 and beyond, or for a
   quotient that is an infinity or a NaN, it takes texel 0, also when clamped (a float's overflow, as the old
   float code had it: a positive coordinate clamped would otherwise give the last texel). Not measured.
   Every cell a quad, 1/w = 2^-64 at every corner (so that u / w stays inside the setup's range however large u
   is), u the same at every corner (U[col]) and v = 1/4 texel; point sampled, the texture's texel (i, j) red 32 i + 16,
   green 32 j + 16 (8 x 8), or grey 4 i + 2 mod 256 (rows 6, 7: 64 wide). By row:
     0 ClampU, 1 FlipU, 2 wrap, 3 ClampU with bilinear filtering, 4 ClampV (u and v exchanged), 5 ClampU and
     ClampV with v = u, 6 ClampU 64 texels wide, 7 wrap 64 texels wide
   U: 3.5, 2^19, 2^60, 2^100, 2^116, 1.5 2^116, 2^117, 1.5 2^117, 2^118, 2^120, 2^126, the largest float, +inf,
   NaN, -2^116, -2^117, -2^120, the largest negative, -inf, -3.5 (u size 256 reaches 2^128 at u = 2^117 with 8
   texels, 2^114 with 64). */
#define NUM_PAGES 1
#include "pvrtest2.h"
#include "texgen.h"

static const uint32_t U[20] = {
    0x40600000u, 0x49000000u, 0x5d800000u, 0x71800000u, 0x79800000u, 0x79c00000u, 0x7a000000u, 0x7a400000u,
    0x7a800000u, 0x7b800000u, 0x7e800000u, 0x7f7fffffu, 0x7f800000u, 0x7fc00000u,
    0xf9800000u, 0xfa000000u, 0xfb800000u, 0xff7fffffu, 0xff800000u, 0xc0600000u,
};
static uint32_t tex8, tex64;

static uint32_t rgb(int r, int g, int b) { return (uint32_t)(r >> 3) << 11 | (uint32_t)(g >> 2) << 5 | (uint32_t)(b >> 3); }
static uint32_t grey(int g) { return rgb(g, g, g); }
static uint32_t idx8(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; return rgb(32 * x + 16, 32 * y + 16, 128); }
static uint32_t idx64(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)y; return grey((4 * x + 2) & 255); }

static void test_setup(void)
{
    tex_layout_t l = { TEX_RASTER, 16, 8, 8, 0 };
    tex8 = tex_build(&l, idx8, NULL);
    l.w = 64;
    tex64 = tex_build(&l, idx64, NULL);
}

static void test_describe(void)
{
    static const char *row[8] = { "ClampU", "FlipU", "wrap", "ClampU bilinear", "ClampV", "ClampU ClampV, v = u",
                                  "ClampU, 64 wide", "wrap, 64 wide" };
    int c;
    for(c = 0; c < 160; c++)
        label(0, c, "%s, u %08x", row[c / 20], (unsigned)U[c % 20]);
}

static void test_page(int page)
{
    int c, k;
    (void)page;
    pvr_frame.bg_color = 0x000040;
    ta_list_begin(ListType_Opaque);
    for(c = 0; c < 160; c++) {
        PCW pcw = { .full = 0 };
        ISP_TSP isp = { .full = 0 };
        TSP tsp = { .full = 0 };
        TCW tcw = { .full = 0 };
        const int row = c / 20, wide = row >= 6;
        const uint32_t u = U[c % 20];
        f32b uu = u, vv = 0x3d000000u;                  /* v = 1/32: a quarter texel of 8 */
        if(row == 4) { uu = 0x3d000000u; vv = u; }
        if(row == 5) vv = u;
        pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
        pcw.ListType = ListType_Opaque;
        pcw.Group_En = 1;
        pcw.Strip_Len = 1;
        pcw.Texture = 1;
        isp.DepthMode = DepthMode_Always;
        isp.CullMode = CullMode_None;
        isp.Texture = 1;
        tsp.SrcInstr = BlendInstr_One;
        tsp.DstInstr = BlendInstr_Zero;
        tsp.FogCtrl = FogCtrl_None;
        tsp.UseAlpha = 1;
        tsp.ShadInstr = ShadInstr_Decal;
        tsp.FilterMode = row == 3 ? FilterMode_Bilinear : FilterMode_Point;
        tsp.MipMapD = 4;
        tsp.TexU = wide ? 3 : 0;
        tsp.TexV = 0;
        tsp.ClampU = row == 0 || row == 3 || row == 5 || row == 6;
        tsp.FlipU = row == 1;
        tsp.ClampV = row == 4 || row == 5;
        tcw.PixelFmt = PixelFmt_565;
        tcw.ScanOrder = 1;
        tcw.TexAddr = (wide ? tex64 : tex8) >> 3;
        ta_polygon(pcw, isp, tsp, tcw);
        for(k = 0; k < 4; k++)
            ta_vertex3(f32b_int(cell_px(c) + (k & 1) * 32), f32b_int(cell_py(c) + (k >> 1) * 32), 0x1f800000u /* 2^-64 */,
                       uu, vv, 0xffffffffu, 0, k == 3);
    }
    ta_list_end();
}
