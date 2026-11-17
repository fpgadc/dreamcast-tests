/* From the TSP's texture coordinate to a texel and a bilinear weight, at every place: both signs, every fraction,
   large coordinates, clamp and flip (2026-11-18).

   refsw2 takes C = floor(u * size * 256) as a two's complement integer, subtracts the half texel (128) for
   bilinear filtering, and reads the texel as C >> 8 and the weight as (C & 255) >> 3: a guess at first, then
   checked only on coordinates above zero (pvr-tests/sample, mipbil, alphafilt). Here the coordinate sweeps C (in
   1/256 texel) across every weight step, below zero too, so that truncating a negative C toward zero, or keeping
   its magnitude and sign, or another number of fraction bits, would show.

   Every cell is a quad (depth 1) whose coordinate is C = c0 + x / 4 + y / 128 at the pixel (x, y) of the cell
   (pixel centers: x + 1/2, y + 1/2): 32 columns cover 8 units, the rows a quarter unit in steps of 1/128. The
   texture's texels alternate black and white along the coordinate (rows of the texture alike), so a bilinear pixel
   shows its weight and which texel it starts at; point sampled, the index texture shows the texel. Decal, opaque,
   0888 framebuffer: exact values.
     page 0  U, 8 texels, bilinear: cell c (0 .. 255) has c0 = -1024 + 8 c (texels -4 .. 4)
     page 1  the same with V
     page 2  U, 8 texels, point sampled, texel i of the index texture: grey 32 i + 16
     page 3  U, 1024 texels, bilinear, large coordinates: c0 = +-(2^k + 8 j) units, k = 8 .. 25, j = 0 .. 7
     page 4  page 0 with U clamped
     page 5  page 0 with U flipped (mirrored repeat) */
#define NUM_PAGES 6
#include "pvrtest2.h"
#include "texgen.h"

static uint32_t tex_altU8, tex_altV8, tex_idx8, tex_alt1024;

static uint32_t altU(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)y; return (x & 1) ? 0xffffu : 0; }
static uint32_t altV(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; return (y & 1) ? 0xffffu : 0; }
static uint32_t idx(const void *ctx, int size, int x, int y)
{
    const uint32_t g = (uint32_t)(32 * x + 16);         /* grey 32 i + 16, as 565 */
    (void)ctx; (void)size; (void)y;
    return (g >> 3) << 11 | (g >> 2) << 5 | (g >> 3);
}

static void test_setup(void)
{
    tex_layout_t l = { TEX_RASTER, 16, 8, 8, 0 };
    tex_altU8 = tex_build(&l, altU, NULL);
    tex_altV8 = tex_build(&l, altV, NULL);
    tex_idx8 = tex_build(&l, idx, NULL);
    l.w = 1024;
    tex_alt1024 = tex_build(&l, altU, NULL);
}

static const char *what[NUM_PAGES] = { "U, 8 texels, bilinear", "V, 8 texels, bilinear", "U, 8 texels, point", "U, 1024 texels, bilinear, large",
                                       "U clamped, 8 texels, bilinear", "U flipped, 8 texels, bilinear" };

/* c0 of a cell (units of 1/256 texel), or 0 with *use = 0 for an unused cell */
static int64_t cell_c0(int page, int c, int *use)
{
    *use = 1;
    if(page != 3) {
        if(c >= 256) { *use = 0; return 0; }
        return -1024 + 8 * (int64_t)c;
    }
    if(c >= 2 * 18 * 8) { *use = 0; return 0; }
    {
        const int k = 8 + (c / 16) % 18, j = c % 8, s = (c / 8) % 2;
        const int64_t v = ((int64_t)1 << k) + 8 * j;
        return s ? -v : v;
    }
}

static void test_describe(void)
{
    int p, c, use;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 300; c++) {
            const int64_t c0 = cell_c0(p, c, &use);
            if(use)
                label(p, c, "%s: C = %lld + x/4 + y/128", what[p], (long long)c0);
        }
}

/* the coordinate of C units (1/256 texel) of a texture `size` texels wide: C / (256 size), exact */
static f32b coord(int64_t c4, int size)                /* c4: C times 128 (the corners are on a grid of 1/128 unit) */
{
    int sh = 0;
    while(c4 > (1 << 23) || c4 < -(1 << 23)) {        /* keep 24 bits (the large ones are multiples of 2^k anyway) */
        c4 /= 2;
        sh++;
    }
    return f32b_scaled((int32_t)c4, sh - 7 - 8 - (size == 8 ? 3 : 10));
}

static void test_page(int page)
{
    const int size = page == 3 ? 1024 : 8, alongV = page == 1;
    int c, k, use;
    pvr_frame.bg_color = 0x000040;
    ta_list_begin(ListType_Opaque);
    for(c = 0; c < 300; c++) {
        PCW pcw = { .full = 0 };
        ISP_TSP isp = { .full = 0 };
        TSP tsp = { .full = 0 };
        TCW tcw = { .full = 0 };
        const int64_t c0 = cell_c0(page, c, &use);
        const int x = cell_px(c), y = cell_py(c);
        if(!use)
            continue;
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
        tsp.FilterMode = page == 2 ? FilterMode_Point : FilterMode_Bilinear;
        tsp.MipMapD = 4;
        tsp.TexU = size == 8 ? 0 : 7;
        tsp.TexV = 0;
        tsp.ClampU = page == 4;
        tsp.FlipU = page == 5;
        tcw.PixelFmt = PixelFmt_565;
        tcw.ScanOrder = 1;
        tcw.TexAddr = (page == 1 ? tex_altV8 : page == 2 ? tex_idx8 : page == 3 ? tex_alt1024 : tex_altU8) >> 3;
        ta_polygon(pcw, isp, tsp, tcw);
        for(k = 0; k < 4; k++) {
            /* corners: C = c0 + X / 4 + Y / 128 at the corner (X, Y) of the cell, in 1/128 units: 128 c0 + 32 X + Y */
            const int X = (k & 1) * 32, Y = (k >> 1) * 32;
            const f32b a = coord(128 * c0 + 32 * X + Y, size), mid = sf_ratio(1, 2 * 8 * 2);   /* the other: texel 0 + 1/4 */
            ta_vertex3(f32b_int(x + X), f32b_int(y + Y), F32B_ONE, alongV ? mid : a, alongV ? a : mid, 0xffffffff, 0, k == 3);
        }
    }
    ta_list_end();
}
