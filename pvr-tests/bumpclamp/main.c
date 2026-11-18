/* A clamped texture coordinate at either end of the texture, filtered bilinearly (2026-11-18).

   pvr-tests/bumpiso (rayman_ingame's bump maps, V clamped, v just below 0 after the half texel): the console's
   bump angle R there is the circular filter from row 0 to row 1 with weight 0, where refsw2 read row 0 twice.
   The circular step is not exact at weight 0 when it starts from the second texel (31 - weight), so this
   shows on R while a colour (or S) would not change. As if the coordinate itself were clamped to 0 before
   it is cut into a texel and a weight. Here the coordinate sweeps both ends of an 8 texel texture, clamped,
   to see what the top end does as well: the texel and the weight it is clamped to, and whether the second
   texel there is clamped too or wraps round to texel 0.

   Every cell is a quad (depth 1) whose coordinate is C = c0 + x / 4 + y / 128 (in 1/256 texel, before the half
   texel is taken off) at the pixel (x, y) of the cell, as in pvr-tests/coordfix: cell c has c0 = -1024 + 8 c (texels -4 .. 4,
   the bottom end) or c0 = 1024 + 8 c (texels 4 .. 12, the top end).
   Bump pages: an 8x8 bump map (twiddled), S 0, R by row (V pages) or by column (U pages): 20, 230, 5, 10, 15, 25,
   50, 60; light K3 = 255 (I follows cos[4 R]), drawn (SRC_ALPHA, ZERO) on a white base so that the intensity
   shows.
     page 0  bump, V clamped, bottom
     page 1  bump, V clamped, top
     page 2  bump, U clamped, bottom
     page 3  bump, U clamped, top
     page 4  bump, V not clamped (wraps), bottom
   Colour pages: 8 texels alternating black and white (pvr-tests/coordfix), decal, 0888: exact values.
     page 5  V clamped, top
     page 6  U clamped, top */
#define NUM_PAGES 7
#include "pvrtest2.h"
#include "texgen.h"

static uint32_t tex_bV, tex_bU, tex_altV8, tex_altU8;
static const uint8_t rowR[8] = { 20, 230, 5, 10, 15, 25, 50, 60 };

static uint32_t bV(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; return rowR[y & 7]; }
static uint32_t bU(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)y; return rowR[x & 7]; }
static uint32_t altU(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)y; return (x & 1) ? 0xffffu : 0; }
static uint32_t altV(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; return (y & 1) ? 0xffffu : 0; }

static void test_setup(void)
{
    const tex_layout_t lb = { TEX_TWIDDLED, 16, 8, 8, 0 }, l = { TEX_RASTER, 16, 8, 8, 0 };
    tex_bV = tex_build(&lb, bV, NULL);
    tex_bU = tex_build(&lb, bU, NULL);
    tex_altV8 = tex_build(&l, altV, NULL);
    tex_altU8 = tex_build(&l, altU, NULL);
}

static const char *what[NUM_PAGES] = { "bump, V clamped, bottom", "bump, V clamped, top", "bump, U clamped, bottom",
                                       "bump, U clamped, top", "bump, V wraps, bottom", "565, V clamped, top",
                                       "565, U clamped, top" };
static const int top[NUM_PAGES] = { 0, 1, 0, 1, 0, 1, 1 }, alongV[NUM_PAGES] = { 1, 1, 0, 0, 1, 1, 0 };

static int64_t cell_c0(int page, int c) { return (top[page] ? 1024 : -1024) + 8 * (int64_t)c; }

static void test_describe(void)
{
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 256; c++)
            label(p, c, "%s: C = %lld + x/4 + y/128", what[p], (long long)cell_c0(p, c));
}

/* the coordinate of C units (1/256 texel) of an 8 texel texture: C / (256 * 8), exact */
static f32b coord(int64_t c4)                           /* c4: C times 128 (the corners are on a grid of 1/128 unit) */
{
    return f32b_scaled((int32_t)c4, -7 - 8 - 3);
}

static void test_page(int page)
{
    const int bump = page <= 4;
    int c, k;
    pvr_frame.bg_color = 0x000040;
    ta_list_begin(ListType_Opaque);
    for(c = 0; c < 256; c++) {
        PCW pcw = { .full = 0 };
        ISP_TSP isp = { .full = 0 };
        TSP tsp = { .full = 0 };
        TCW tcw = { .full = 0 };
        const int64_t c0 = cell_c0(page, c);
        const int x = cell_px(c), y = cell_py(c);
        pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
        pcw.ListType = ListType_Opaque;
        pcw.Group_En = 1;
        pcw.Strip_Len = 1;
        pcw.Texture = 1;
        pcw.Offset = bump;
        pcw.Gouraud = bump;
        isp.Offset = bump;
        isp.Gouraud = bump;
        isp.DepthMode = DepthMode_Always;
        isp.CullMode = CullMode_None;
        isp.Texture = 1;
        tsp.SrcInstr = bump ? BlendInstr_SrcAlpha : BlendInstr_One;
        tsp.DstInstr = BlendInstr_Zero;
        tsp.FogCtrl = FogCtrl_None;
        tsp.UseAlpha = 1;
        tsp.ShadInstr = ShadInstr_Decal;
        tsp.FilterMode = FilterMode_Bilinear;
        tsp.MipMapD = 4;
        tsp.TexU = 0;
        tsp.TexV = 0;
        tsp.ClampU = page == 2 || page == 3 || page == 6;
        tsp.ClampV = page == 0 || page == 1 || page == 5;
        tcw.PixelFmt = bump ? PixelFmt_BumpMap : PixelFmt_565;
        tcw.ScanOrder = !bump;
        tcw.TexAddr = (bump ? (alongV[page] ? tex_bV : tex_bU) : (alongV[page] ? tex_altV8 : tex_altU8)) >> 3;
        ta_polygon(pcw, isp, tsp, tcw);
        for(k = 0; k < 4; k++) {
            /* corners: C = c0 + X / 4 + Y / 128 at the corner (X, Y) of the cell, in 1/128 units: 128 c0 + 32 X + Y;
               the other coordinate: texel 0 + 1/4 */
            const int X = (k & 1) * 32, Y = (k >> 1) * 32;
            const f32b a = coord(128 * c0 + 32 * X + Y), mid = sf_ratio(1, 2 * 8 * 2);
            ta_vertex3(f32b_int(x + X), f32b_int(y + Y), F32B_ONE, alongV[page] ? mid : a, alongV[page] ? a : mid,
                       0xffffffff, bump ? 0x0000ff00u : 0, k == 3);
        }
    }
    ta_list_end();
}
