/* Texture sampling: where a pixel samples, bilinear weights, and the wrap modes, for every way a texture is
   stored and every texel format.

   Texture A is 8x8: red = column, green = row (both in steps that survive any 4, 5 or 6 bit expansion),
   blue = checkerboard; as YUV422 its Y and chroma bytes change with column and row; as a palette texture its
   indices pick those colours from the palette.

   A page shows three variants (five rows of cells each); there are ten pages of each kind:
   pages 0-9    point sampling, texture magnified 4x (a 32x32 quad). The texture coordinates are shifted by
                d = -1 .. +1 pixel in steps of 1/8: row 0 along u, row 1 along v, row 2 along both.
                row 3: the texture 1:1 (an 8x8 quad), shifted along u by d = -1 .. +1 texel in steps of 1/8
                row 4: the texture at half size (a 4x4 quad), same shifts
   pages 10-19  the same with bilinear filtering
   pages 20-29  bilinear weights. One texel step stretched over the 32 pixels of a cell, starting k/256 of a
                texel further along in each of 8 cells, so all 256 fractional positions are covered. Four
                steps (0->255, 255->0, 0->8, 64->192), along u (cells 0-31) and along v (cells 40-71)
   pages 30-39  wrap modes: 64x64 quads with coordinates -1 .. 3, point sampled, for the 16 combinations of
                flip (none, V, U, UV) and clamp (none, V, U, UV)
   pages 40-49  the same with bilinear filtering (what is blended across the texture's edges)

   The variants, three to a page: see variants[]. Bump maps are drawn with the light K1 K2 K3 Q = 00 80 7f 00
   in the offset colour; textures that are not 8x8 have texture A stretched to their size.

   Not drawn: the bump map that is not twiddled with bilinear filtering (its cells on pages 19, 29 and 49
   are empty). A console's render stops in the middle of such a page and never signals its end
   (pvr_tests/bumphang). */
#define NUM_VARIANTS 30
#define GROUPS 10                    /* pages of each kind: three variants to a page */
#define NUM_PAGES (5 * GROUPS)
#include "pvrtest2.h"
#include "texgen.h"

/* w, h: the size the polygon declares; bh: the height the texture is built with, when that is another;
   alpha: drawn with (SRC_ALPHA, ZERO), which shows colour * alpha (a bump map's result is its alpha) */
typedef struct { const char *name; int PixelFmt, layout, bits, stride, ScanOrder, StrideSel, VQ, MipMapped, PalSelect, w, h, bh, alpha; } variant_t;
static const variant_t variants[NUM_VARIANTS] = {
    { "565 twiddled",                 PixelFmt_565,    TEX_TWIDDLED, 16,  0, 0, 0, 0, 0,  0,  8,  8, 0, 0 },
    { "565 not twiddled",             PixelFmt_565,    TEX_RASTER,   16,  0, 1, 0, 0, 0,  0,  8,  8, 0, 0 },
    { "565 not twiddled, stride 32",  PixelFmt_565,    TEX_RASTER,   16, 32, 1, 1, 0, 0,  0,  8,  8, 0, 0 },
    { "565 mipmapped",                PixelFmt_565,    TEX_MIP,      16,  0, 0, 0, 0, 1,  0,  8,  8, 0, 0 },
    { "565 VQ",                       PixelFmt_565,    TEX_VQ,       16,  0, 0, 0, 1, 0,  0,  8,  8, 0, 0 },
    { "565 VQ mipmapped",             PixelFmt_565,    TEX_VQ_MIP,   16,  0, 0, 0, 1, 1,  0,  8,  8, 0, 0 },
    { "1555 twiddled",                PixelFmt_1555,   TEX_TWIDDLED, 16,  0, 0, 0, 0, 0,  0,  8,  8, 0, 0 },
    { "4444 twiddled",                PixelFmt_4444,   TEX_TWIDDLED, 16,  0, 0, 0, 0, 0,  0,  8,  8, 0, 0 },
    { "YUV422 twiddled",              PixelFmt_YUV422, TEX_TWIDDLED, 16,  0, 0, 0, 0, 0,  0,  8,  8, 0, 0 },
    { "YUV422 not twiddled",          PixelFmt_YUV422, TEX_RASTER,   16,  0, 1, 0, 0, 0,  0,  8,  8, 0, 0 },
    { "palette 8 bpp twiddled",       PixelFmt_Pal8,   TEX_TWIDDLED,  8,  0, 0, 0, 0, 0,  0,  8,  8, 0, 0 },   /* palette entries 0.., greys 256.. */
    { "palette 4 bpp twiddled",       PixelFmt_Pal4,   TEX_TWIDDLED,  4,  0, 0, 0, 0, 0, 32,  8,  8, 0, 0 },   /* entries 512.., greys 528.. */
    { "1555 not twiddled, stride 32", PixelFmt_1555,   TEX_RASTER,   16, 32, 1, 1, 0, 0,  0,  8,  8, 0, 0 },
    { "4444 not twiddled, stride 32", PixelFmt_4444,   TEX_RASTER,   16, 32, 1, 1, 0, 0,  0,  8,  8, 0, 0 },
    { "YUV422 not twiddled, stride 32", PixelFmt_YUV422, TEX_RASTER, 16, 32, 1, 1, 0, 0,  0,  8,  8, 0, 0 },
    { "YUV422 VQ",                    PixelFmt_YUV422, TEX_VQ,       16,  0, 0, 0, 1, 0,  0,  8,  8, 0, 0 },
    { "palette 8 bpp VQ",             PixelFmt_Pal8,   TEX_VQ,        8,  0, 0, 0, 1, 0,  0,  8,  8, 0, 0 },
    { "palette 4 bpp VQ",             PixelFmt_Pal4,   TEX_VQ,        4,  0, 0, 0, 1, 0, 32,  8,  8, 0, 0 },
    { "bump map twiddled",            PixelFmt_BumpMap, TEX_TWIDDLED, 16, 0, 0, 0, 0, 0,  0,  8,  8, 0, 0 },
    { "bump map twiddled, alpha shown", PixelFmt_BumpMap, TEX_TWIDDLED, 16, 0, 0, 0, 0, 0, 0,  8,  8, 0, 1 },
    { "bump map VQ, alpha shown",     PixelFmt_BumpMap, TEX_VQ,      16,  0, 0, 0, 1, 0,  0,  8,  8, 0, 1 },
    { "565 twiddled 16x8",            PixelFmt_565,    TEX_TWIDDLED, 16,  0, 0, 0, 0, 0,  0, 16,  8, 0, 0 },
    { "565 twiddled 8x16",            PixelFmt_565,    TEX_TWIDDLED, 16,  0, 0, 0, 0, 0,  0,  8, 16, 0, 0 },
    { "565 not twiddled 16x8",        PixelFmt_565,    TEX_RASTER,   16,  0, 1, 0, 0, 0,  0, 16,  8, 0, 0 },
    { "565 VQ 16x8",                  PixelFmt_565,    TEX_VQ,       16,  0, 0, 0, 1, 0,  0, 16,  8, 0, 0 },
    { "565 VQ 8x16",                  PixelFmt_565,    TEX_VQ,       16,  0, 0, 0, 1, 0,  0,  8, 16, 0, 0 },
    { "565 mipmapped, declared 16x8", PixelFmt_565,    TEX_MIP,      16,  0, 0, 0, 0, 1,  0, 16,  8, 16, 0 },
    { "565 mipmapped, declared 8x16", PixelFmt_565,    TEX_MIP,      16,  0, 0, 0, 0, 1,  0,  8, 16,  8, 0 },
    { "565 VQ mipmapped, declared 16x8", PixelFmt_565, TEX_VQ_MIP,   16,  0, 0, 0, 1, 1,  0, 16,  8, 16, 0 },
    { "bump map not twiddled, alpha shown", PixelFmt_BumpMap, TEX_RASTER, 16, 0, 1, 0, 0, 0, 0, 8,  8, 0, 1 },
};

static uint32_t tex_a[NUM_VARIANTS], tex_h[NUM_VARIANTS][4], tex_v[NUM_VARIANTS][4];
static const uint8_t steps[4][2] = { { 0, 255 }, { 255, 0 }, { 0, 8 }, { 64, 192 } };

/* a grey of 0 .. 255 as a texel of the format (palette formats: the index of that grey) */
static uint32_t grey(int fmt, int v) {
    switch(fmt) {
    case PixelFmt_565:    return (uint32_t)(((v >> 3) << 11) | ((v >> 2) << 5) | (v >> 3));
    case PixelFmt_1555:   return 0x8000u | (uint32_t)(((v >> 3) << 10) | ((v >> 3) << 5) | (v >> 3));
    case PixelFmt_4444:   return 0xf000u | (uint32_t)((v >> 4) * 0x111);
    case PixelFmt_YUV422: return (uint32_t)(v << 8) | 128;
    case PixelFmt_BumpMap: return (uint32_t)(v << 8) | 0x40;      /* S = v, R = 64 */
    case PixelFmt_Pal8:   return (uint32_t)v;
    default:              return (uint32_t)(v >> 4);
    }
}

typedef struct { int fmt, kind, lo, hi, w, h; } texctx_t;      /* kind 0: texture A; 1: lo left, hi right; 2: lo above, hi below */

static uint32_t texel(const void *vc, int size, int x, int y) {
    const texctx_t *c = (const texctx_t *)vc;
    const int w = size, h = c->h * size / c->w;         /* the level's size */
    if(c->kind)
        return grey(c->fmt, ((c->kind == 1 ? x * 8 / w : y * 8 / h) >= 4) ? c->hi : c->lo);
    if(size < c->w)                                     /* the smaller levels of a mipmapped texture: one colour each */
        return size * 2 == c->w ? 0xf81f : size * 4 == c->w ? 0x07ff : 0xffe0;
    switch(c->fmt) {
    case PixelFmt_565:    return (uint32_t)((((x * 32 + 16) / w) << 11) | (((y * 64 + 32) / h) << 5) | (((x ^ y) & 1) ? 31 : 0));
    case PixelFmt_1555:   return 0x8000u | (uint32_t)((((x * 32 + 16) / w) << 10) | (((y * 32 + 16) / h) << 5) | (((x ^ y) & 1) ? 31 : 0));
    case PixelFmt_4444:   return 0xf000u | (uint32_t)((((x * 16 + 8) / w) << 8) | (((y * 16 + 8) / h) << 4) | (((x ^ y) & 1) ? 15 : 0));
    case PixelFmt_YUV422: return (uint32_t)(((40 + 20 * x + 4 * y) << 8) | (60 + 18 * x + 3 * y));
    case PixelFmt_BumpMap: return (uint32_t)(((20 + 30 * x) << 8) | (30 * y + 5 * x));   /* S, R */
    case PixelFmt_Pal8:   return (uint32_t)(y * 8 + x);
    default:              return (uint32_t)((x & 3) | ((y & 3) << 2));
    }
}

static void test_setup(void) {
    int v, k, i;

    pvr_pal_format(PAL_RAM_CTRL_ARGB8888);
    for(i = 0; i < 64; i++)                             /* texture A's colours for 8 bit indices */
        pvr_pal_entry(i, 0xff000000u | (uint32_t)(((i & 7) * 32 + 16) << 16) | (uint32_t)(((i >> 3) * 32 + 16) << 8) | (((i ^ (i >> 3)) & 1) ? 255u : 0u));
    for(i = 0; i < 256; i++)
        pvr_pal_entry(256 + i, 0xff000000u | (uint32_t)i * 0x010101u);
    for(i = 0; i < 16; i++) {                           /* and for 4 bit indices */
        pvr_pal_entry(512 + i, 0xff000000u | (uint32_t)(((i & 3) * 64 + 32) << 16) | (uint32_t)(((i >> 2) * 64 + 32) << 8) | (((i ^ (i >> 2)) & 1) ? 255u : 0u));
        pvr_pal_entry(528 + i, 0xff000000u | (uint32_t)(i * 17) * 0x010101u);
    }
    for(v = 0; v < NUM_VARIANTS; v++) {
        const int bh = variants[v].bh ? variants[v].bh : variants[v].h;
        const tex_layout_t l = { variants[v].layout, variants[v].bits, variants[v].w, bh, variants[v].stride };
        texctx_t c = { variants[v].PixelFmt, 0, 0, 0, variants[v].w, bh };
        tex_a[v] = tex_build(&l, texel, &c);
        for(k = 0; k < 4; k++) {
            c.lo = steps[k][0]; c.hi = steps[k][1];
            c.kind = 1; tex_h[v][k] = tex_build(&l, texel, &c);
            c.kind = 2; tex_v[v][k] = tex_build(&l, texel, &c);
        }
    }
}

/* a bump map that is not twiddled, filtered bilinearly, stops a console's render: not drawn */
static int left_out(int v, int kind) {
    return variants[v].PixelFmt == PixelFmt_BumpMap && variants[v].ScanOrder && (kind == 1 || kind == 2 || kind == 4);
}

static void test_describe(void) {
    int page, k, c, s;

    for(page = 0; page < NUM_PAGES; page++)
        for(k = 0; k < 3; k++) {
            const char *n = variants[(page % GROUPS) * 3 + k].name;
            const int at = k * 5 * COLS, kind = page / GROUPS;
            if(left_out((page % GROUPS) * 3 + k, kind))
                continue;
            if(kind < 2) {
                const char *f = kind ? "bilinear" : "point";
                for(c = 0; c <= 16; c++) {
                    label(page, at + c, "%s: %s 4x, u shifted %+d/8 px", n, f, c - 8);
                    label(page, at + 20 + c, "%s: %s 4x, v shifted %+d/8 px", n, f, c - 8);
                    label(page, at + 40 + c, "%s: %s 4x, u and v shifted %+d/8 px", n, f, c - 8);
                    label(page, at + 60 + c, "%s: %s 1:1, u shifted %+d/8 texel", n, f, c - 8);
                    label(page, at + 80 + c, "%s: %s half size, u shifted %+d/8 texel", n, f, c - 8);
                }
            } else if(kind == 2) {
                for(s = 0; s < 4; s++)
                    for(c = 0; c < 8; c++) {
                        label(page, at + s * 8 + c, "%s: bilinear %d->%d along u, start +%d/256 texel", n, steps[s][0], steps[s][1], c);
                        label(page, at + 40 + s * 8 + c, "%s: bilinear %d->%d along v, start +%d/256 texel", n, steps[s][0], steps[s][1], c);
                    }
            } else {
                for(c = 0; c < 16; c++)
                    label_span(page, at + (c / 8) * 40 + (c % 8) * 2, 2, 2, "%s: wrap %s, flip %s clamp %s", n, kind == 4 ? "bilinear" : "point",
                               (const char *[]){ "none", "V", "U", "UV" }[c / 4], (const char *[]){ "none", "V", "U", "UV" }[c % 4]);
            }
        }
}

static uint32_t offs_col;           /* the offset colour of the quads that follow */

/* every parameter word of a textured polygon of variant v: UV 32 bit, replace. greys: the palette of greys */
static void textured(int v, uint32_t txr, int filter, int flip, int clamp, int greys) {
    const variant_t *va = &variants[v];
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };

    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Translucent;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Texture = 1;
    pcw.Gouraud = 1;
    pcw.Offset = va->PixelFmt == PixelFmt_BumpMap;      /* the offset colour is the light */
    isp.DepthMode = DepthMode_Always;
    isp.ZWriteDis = 1;
    isp.Texture = 1;
    isp.Offset = pcw.Offset;
    offs_col = pcw.Offset ? 0x00807f00u : 0;     /* K1 0, K2 80, K3 7f, Q 0: nothing saturates */
    tsp.SrcInstr = va->alpha ? BlendInstr_SrcAlpha : BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    tsp.ShadInstr = ShadInstr_Decal;
    tsp.FilterMode = filter;
    tsp.FlipU = (flip >> 1) & 1;
    tsp.FlipV = flip & 1;
    tsp.ClampU = (clamp >> 1) & 1;
    tsp.ClampV = clamp & 1;
    tsp.MipMapD = 4;
    tsp.TexU = va->w == 16 ? 1 : 0;             /* 8 or 16 texels */
    tsp.TexV = va->h == 16 ? 1 : 0;
    tcw.StrideSel = va->StrideSel;
    tcw.ScanOrder = va->ScanOrder;
    if(va->bits != 16)                          /* palette: these six bits are the palette select instead */
        tcw.PalSelect = va->PalSelect + (greys ? (va->bits == 8 ? 16 : 1) : 0);
    tcw.PixelFmt = va->PixelFmt;
    tcw.VQ_Comp = va->VQ;
    tcw.MipMapped = va->MipMapped;
    tcw.TexAddr = txr >> 3;
    ta_polygon(pcw, isp, tsp, tcw);
}

#define quad_uv(x, y, w, h, u0, v0, u1, v1) quad3(x, y, w, h, F32B_ONE, u0, v0, u1, v1, 0xffffffff, offs_col)

static void test_page(int page) {
    const int kind = page / GROUPS;
    int k, c, s;

    pvrio_reg(TEXT_CONTROL_addr, 1);            /* the stride of strided textures: 32 texels */
    ta_list_begin(ListType_Translucent);
    for(k = 0; k < 3; k++) {
        const int v = (page % GROUPS) * 3 + k, at = k * 5 * COLS;
        if(left_out(v, kind))
            continue;
        if(kind < 2) {
            textured(v, tex_a[v], kind ? FilterMode_Bilinear : FilterMode_Point, 0, 0, 0);
            for(c = 0; c <= 16; c++) {
                /* d = (c - 8) / 8, s = d / 32, t = d / 8: all exact */
                f32b sh = f32b_scaled(c - 8, -8), t = f32b_scaled(c - 8, -6), z = F32B_ZERO;
                f32b one_s = sf_add(F32B_ONE, sh), one_t = sf_add(F32B_ONE, t);
                quad_uv(cell_px(at + c), cell_py(at + c), 32, 32, sh, z, one_s, F32B_ONE);
                quad_uv(cell_px(at + 20 + c), cell_py(at + 20 + c), 32, 32, z, sh, F32B_ONE, one_s);
                quad_uv(cell_px(at + 40 + c), cell_py(at + 40 + c), 32, 32, sh, sh, one_s, one_s);
                quad_uv(cell_px(at + 60 + c) + 8, cell_py(at + 60 + c) + 8, 8, 8, t, z, one_t, F32B_ONE);
                quad_uv(cell_px(at + 80 + c) + 8, cell_py(at + 80 + c) + 8, 4, 4, t, z, one_t, F32B_ONE);
            }
        } else if(kind == 2) {
            for(s = 0; s < 4; s++)
                for(c = 0; c < 8; c++) {
                    /* texel centers 3 and 4 are at 3.5/8 and 4.5/8: a = (3.5 + c/256) / 8, b = a + 1/8 (exact) */
                    f32b a = f32b_scaled(896 + c, -11), b = sf_add(a, F(0.125f));
                    textured(v, tex_h[v][s], FilterMode_Bilinear, 0, 0, 1);
                    quad_uv(cell_px(at + s * 8 + c), cell_py(at + s * 8 + c), 32, 32, a, F(0.25f), b, F(0.25f));
                    textured(v, tex_v[v][s], FilterMode_Bilinear, 0, 0, 1);
                    quad_uv(cell_px(at + 40 + s * 8 + c), cell_py(at + 40 + s * 8 + c), 32, 32, F(0.25f), a, F(0.25f), b);
                }
        } else {
            for(c = 0; c < 16; c++) {
                const int cell = at + (c / 8) * 40 + (c % 8) * 2;
                /* flip and clamp, bit 1 = U, bit 0 = V; the order none, V, U, UV */
                textured(v, tex_a[v], kind == 4 ? FilterMode_Bilinear : FilterMode_Point, c / 4, c % 4, 0);
                quad_uv(cell_px(cell), cell_py(cell), 64, 64, F(-1.0f), F(-1.0f), F(3.0f), F(3.0f));
            }
        }
    }
    ta_list_end();
}
