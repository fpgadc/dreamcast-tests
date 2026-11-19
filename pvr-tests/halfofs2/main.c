/* HALF_OFFSET (0x80) and where the TSP samples, to a 256th of a pixel (TODO C9; 2026-11-19). pvr-tests/halfofs
   (the old harness) has the register's eight values against coverage, depth compares, Gouraud ramps and textures
   shifted in eighths of a pixel; refsw3 takes bit 1 (TSP pixel half offset) as a sample point at the pixel's
   corner instead of its centre and bit 2 (texel half offset) as half a texel off the coordinate in the filtered
   modes. Not measured: the sample point with bit 1 off to better than an eighth, and SupSample's four points
   with it off. Page p has the register at p (KallistiOS and the boot ROM leave 7).

   The readout: an 8 bpp palette texture, 256 texels along the coordinate read, twiddled; palette entry i is
   ARGB8888 (0xff, i, 255 - i, i ^ 0x5a), so red is the texel's index. The coordinate is the pixel's screen
   position less the cell's corner: u = X - x0 (whole texture widths per pixel), so red = 256 frac(u) at the sample
   point, a 256th of a pixel (1/w = 1 everywhere: the planes are exact to 2^-11 there).
     row 0, cells  0- 3   u along x, point sampled, u started k / 4096 of a texture in (k = 0..3)
     row 0, cells  4- 7   the same, bilinear (where the texel half offset shows)
     row 0, cells  8-11   point sampled, SupSample (four points: ((a + b) >> 1 + (c + d) >> 1) >> 1 of their reds)
     row 0, cells 12-15   bilinear, SupSample
     row 1, cells  0-15   the same along y (v = Y - y0, the texture 8 x 256)
   Gouraud: a 4 pixel ramp, red 0 at its first edge to 255 at the last (64 a pixel), untextured, opaque:
     row 2, cells  0- 3   along x, the quad at x0 + 8 + k / 8 (k = 0..3)
     row 2, cells  4- 7   the same, SupSample (untextured polygons take it too: README supsample2)
     row 2, cells  8-15   along y, plain and SupSample */
#define NUM_PAGES 8
#include "pvrtest2.h"
#include "texgen.h"

static uint32_t tex_u, tex_v;

static uint32_t idx_u(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)y; return (uint32_t)x; }
static uint32_t idx_v(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; return (uint32_t)y; }

static void test_setup(void)
{
    const tex_layout_t lu = { TEX_TWIDDLED, 8, 256, 8, 0 }, lv = { TEX_TWIDDLED, 8, 8, 256, 0 };
    int i;
    pvr_pal_format(PAL_RAM_CTRL_ARGB8888);
    for(i = 0; i < 256; i++)
        pvr_pal_entry(i, 0xff000000u | (uint32_t)i << 16 | (uint32_t)(255 - i) << 8 | (uint32_t)(i ^ 0x5a));
    tex_u = tex_build(&lu, idx_u, NULL);
    tex_v = tex_build(&lv, idx_v, NULL);
}

static void test_describe(void)
{
    static const char *kind[4] = { "point", "bilinear", "point SupSample", "bilinear SupSample" };
    int p, c;
    for(p = 0; p < NUM_PAGES; p++) {
        for(c = 0; c < 16; c++) {
            label(p, c, "HALF_OFFSET %d: u along x, %s, start +%d/4096", p, kind[c / 4], c % 4);
            label(p, COLS + c, "HALF_OFFSET %d: v along y, %s, start +%d/4096", p, kind[c / 4], c % 4);
        }
        for(c = 0; c < 16; c++)
            label(p, 2 * COLS + c, "HALF_OFFSET %d: Gouraud 4 pixel ramp along %s%s, at +%d/8", p, c < 8 ? "x" : "y",
                  (c / 4) % 2 ? ", SupSample" : "", c % 4);
    }
}

static void header(int textured, uint32_t tex, int along_v, int filter, int super)
{
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Opaque;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Texture = textured;
    pcw.Gouraud = 1;
    isp.DepthMode = DepthMode_Always;
    isp.CullMode = CullMode_None;
    isp.Texture = textured;
    isp.Gouraud = 1;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    tsp.ShadInstr = ShadInstr_Decal;
    tsp.FilterMode = (uint32_t)filter;
    tsp.SupSample = (uint32_t)super;
    tsp.MipMapD = 4;
    tsp.TexU = along_v ? 0 : 5;         /* 8 or 256 texels */
    tsp.TexV = along_v ? 5 : 0;
    tcw.PixelFmt = PixelFmt_Pal8;
    tcw.TexAddr = tex >> 3;
    ta_polygon(pcw, isp, tsp, tcw);
}

static void test_page(int page)
{
    int c, k;
    pvrio_reg(HALF_OFFSET_addr, (uint32_t)page);
    pvr_frame.bg_color = 0x000040;
    ta_list_begin(ListType_Opaque);
    for(c = 0; c < 32; c++) {
        const int along_v = c >= 16, i = c % 16, cell = along_v ? COLS + i : i;
        const int x = cell_px(cell), y = cell_py(cell), filter = (i / 4) % 2 ? FilterMode_Bilinear : FilterMode_Point;
        header(1, along_v ? tex_v : tex_u, along_v, filter, i >= 8);
        for(k = 0; k < 4; k++) {
            const int X = (k & 1) * 32, Y = (k >> 1) * 32;
            /* the coordinate read: (X or Y) + (i % 4) / 4096, in whole textures; the other one mid texture */
            const f32b t = f32b_scaled((along_v ? Y : X) * 4096 + i % 4, -12), mid = F(0.5f);
            ta_vertex3(f32b_int(x + X), f32b_int(y + Y), F32B_ONE, along_v ? mid : t, along_v ? t : mid,
                       0xffffffffu, 0, k == 3);
        }
    }
    for(c = 0; c < 16; c++) {
        const int cell = 2 * COLS + c, along_y = c >= 8, super = (c / 4) % 2, off = c % 4;
        const int x = cell_px(cell), y = cell_py(cell);
        PCW pcw = { .full = 0 };
        ISP_TSP isp = { .full = 0 };
        TSP tsp = { .full = 0 };
        TCW tcw = { .full = 0 };
        pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
        pcw.ListType = ListType_Opaque;
        pcw.Group_En = 1;
        pcw.Strip_Len = 1;
        pcw.Gouraud = 1;
        isp.DepthMode = DepthMode_Always;
        isp.CullMode = CullMode_None;
        isp.Gouraud = 1;
        tsp.SrcInstr = BlendInstr_One;
        tsp.DstInstr = BlendInstr_Zero;
        tsp.FogCtrl = FogCtrl_None;
        tsp.UseAlpha = 1;
        tsp.SupSample = (uint32_t)super;
        ta_polygon(pcw, isp, tsp, tcw);
        for(k = 0; k < 4; k++) {
            /* a 4 x 32 (or 32 x 4) quad at 8 + off / 8 along the ramp; red 0 at the first edge, 255 at the last */
            const int last = along_y ? (k >> 1) : (k & 1);
            const f32b px = along_y ? f32b_int(x + (k & 1) * 32) : f32b_scaled((x + 8) * 8 + off + 32 * (k & 1), -3);
            const f32b py = along_y ? f32b_scaled((y + 8) * 8 + off + 32 * (k >> 1), -3) : f32b_int(y + (k >> 1) * 32);
            ta_vertex0(px, py, F32B_ONE, last ? 0xffff0000u : 0xff000000u, k == 3);
        }
    }
    ta_list_end();
}
