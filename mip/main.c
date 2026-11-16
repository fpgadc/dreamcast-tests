/* Mipmaps, VQ and strided textures.

   The mipmapped texture is 16x16 RGB565 with one flat color per level, so only the level selection (and,
   with trilinear, the blend between levels) shows:
       16x16 red, 8x8 green, 4x4 blue, 2x2 white, 1x1 yellow
   pages 0, 1, 2: point sampling, trilinear pass 1, trilinear pass 2. Row b (0..14) uses mipmap bias b + 1
   (in quarters: 0.25 .. 3.75). Columns 0-9: a square quad of 32, 24, 16, 12, 8, 6, 4, 3, 2, 1 pixels showing the
   whole texture. Columns 10-17: quads of 32x4, 4x32, 16x2, 2x16, 32x16, 16x32, 8x1, 1x8 pixels.
   page 3: bilinear.

   page 4: cells 0-1 (2x2 span at cell 0): an 8x8 VQ texture, every codebook entry and index different, at 8x
           (point sampled, with POINT_FIX); cell 4 (2x2 span): the same texture with bilinear filtering;
           cell 8 (4x1 span): a strided texture, declared 64x8 with a stride of 32 texels, shown 2x.

   pages 5-16: the first four pages' quads for nine more textures (mvars[]: VQ, the other texel formats,
           palettes, a mipmapped texture with the scan order and stride bits set, and a texture without
           mipmaps), three to a page, with the biases 1/4, 1, 2, 3 and 15/4. Pages 5-7 point sampling, 8-10
           trilinear pass 1, 11-13 trilinear pass 2, 14-16 bilinear.

   page 17: which word of memory is read? The texture memory holds its own index in every word (and, behind
           2048 bytes of that, in every byte), so the picture shows what was fetched for each texel.
           row 0: a 16x16 RGB565 texture at 2x for the 16 settings of scan order (1), stride select (2),
           mipmapped (4) and VQ (8): cell = the sum. row 1: the same at half size (an 8x8 quad).
           row 2: palette textures, whose memory holds its index in every byte and whose palette entry e is
           the colour (0, e, 255 - e): cells 0-3 8 bpp at 2x for mipmapped (1) and VQ (2), 4-7 the same at
           half size, 8-15 the same with 4 bpp.

   pages 18, 19: textures that are not square (nsq[]: twiddled, not twiddled, VQ, and mipmapped ones whose
           polygon declares a V size other than the U size; a mipmapped texture is stored square). RGB565,
           every texel its own colour (red and one blue bit = x, green = y, another blue bit a checker), a
           texel per pixel, each in a block of 2x2 cells. Page 18 point sampled; page 19 bilinear, sampled a
           quarter of a texel off the texel centres along both axes. */
#define NUM_PAGES 20
#include "pvrtest2.h"
#include "texgen.h"

static uint32_t mip_txr, vq_txr, stride_txr;

typedef struct { const char *name; int PixelFmt, layout, bits, ScanOrder, StrideSel, VQ, MipMapped; } mvar_t;
#define NUM_MVARS 9
static const mvar_t mvars[NUM_MVARS] = {
    { "565 VQ mipmapped",                         PixelFmt_565,    TEX_VQ_MIP,   16, 0, 0, 1, 1 },
    { "1555 mipmapped",                           PixelFmt_1555,   TEX_MIP,      16, 0, 0, 0, 1 },
    { "4444 mipmapped",                           PixelFmt_4444,   TEX_MIP,      16, 0, 0, 0, 1 },
    { "YUV422 mipmapped",                         PixelFmt_YUV422, TEX_MIP,      16, 0, 0, 0, 1 },
    { "palette 8 bpp mipmapped",                  PixelFmt_Pal8,   TEX_MIP,       8, 0, 0, 0, 1 },
    { "palette 4 bpp mipmapped",                  PixelFmt_Pal4,   TEX_MIP,       4, 0, 0, 0, 1 },
    { "565 mipmapped, scan order bit set",        PixelFmt_565,    TEX_MIP,      16, 1, 0, 0, 1 },
    { "565 mipmapped, scan order and stride set", PixelFmt_565,    TEX_MIP,      16, 1, 1, 0, 1 },
    { "565 without mipmaps",                      PixelFmt_565,    TEX_TWIDDLED, 16, 0, 0, 0, 0 },
};
static uint32_t mvar_txr[NUM_MVARS], addr_txr, addr_pal_txr;

/* w, h: what the polygon declares; a mipmapped texture is built w x w */
typedef struct { const char *name; int layout, ScanOrder, VQ, MipMapped, w, h; } nsq_t;
#define NUM_NSQ 26
static const nsq_t nsq[NUM_NSQ] = {
    { "twiddled",     TEX_TWIDDLED, 0, 0, 0, 32,  8 }, { "twiddled",     TEX_TWIDDLED, 0, 0, 0,  8, 32 },
    { "twiddled",     TEX_TWIDDLED, 0, 0, 0, 64,  8 }, { "twiddled",     TEX_TWIDDLED, 0, 0, 0,  8, 64 },
    { "twiddled",     TEX_TWIDDLED, 0, 0, 0, 32, 16 }, { "twiddled",     TEX_TWIDDLED, 0, 0, 0, 16, 32 },
    { "not twiddled", TEX_RASTER,   1, 0, 0, 32,  8 }, { "not twiddled", TEX_RASTER,   1, 0, 0,  8, 32 },
    { "not twiddled", TEX_RASTER,   1, 0, 0, 64,  8 }, { "not twiddled", TEX_RASTER,   1, 0, 0,  8, 64 },
    { "VQ",           TEX_VQ,       0, 1, 0, 32,  8 }, { "VQ",           TEX_VQ,       0, 1, 0,  8, 32 },
    { "VQ",           TEX_VQ,       0, 1, 0, 64,  8 }, { "VQ",           TEX_VQ,       0, 1, 0,  8, 64 },
    { "VQ",           TEX_VQ,       0, 1, 0, 32, 16 }, { "VQ",           TEX_VQ,       0, 1, 0, 16, 32 },
    { "mipmapped",    TEX_MIP,      0, 0, 1, 32,  8 }, { "mipmapped",    TEX_MIP,      0, 0, 1,  8, 32 },
    { "mipmapped",    TEX_MIP,      0, 0, 1, 64,  8 }, { "mipmapped",    TEX_MIP,      0, 0, 1,  8, 64 },
    { "mipmapped",    TEX_MIP,      0, 0, 1, 32, 16 }, { "mipmapped",    TEX_MIP,      0, 0, 1, 16, 32 },
    { "VQ mipmapped", TEX_VQ_MIP,   0, 1, 1,  8, 32 }, { "VQ mipmapped", TEX_VQ_MIP,   0, 1, 1,  8, 64 },
    { "VQ mipmapped", TEX_VQ_MIP,   0, 1, 1, 16, 64 }, { "VQ mipmapped", TEX_VQ_MIP,   0, 1, 1, 32,  8 },
};
static uint32_t nsq_txr[NUM_NSQ];

/* every texel its own colour; the smaller levels of a mipmapped texture one colour each. A VQ mipmapped
   texture 32 wide repeats after 16 texels, to stay within 256 code book entries. */
static uint32_t nsq_texel(const void *vn, int size, int x, int y) {
    const nsq_t *n = (const nsq_t *)vn;
    if(size < n->w)
        return size * 2 == n->w ? 0xf81f : size * 4 == n->w ? 0x07ff : 0xffe0;
    if(n->layout == TEX_VQ_MIP && n->w == 32) {
        x &= 15;
        y &= 15;
    }
    return (uint32_t)(((x & 31) << 11) | ((y & 63) << 5) | ((x >> 5) & 1) | (((x ^ y) & 1) << 4));
}
static int log2i(int v) { int k = 0; while((1 << k) < v) k++; return k; }
static const int bias5[5] = { 1, 4, 8, 12, 15 };
static int pal_select;              /* for textured(): the palette select of palette textures */

/* one flat colour per level, in the variant's format: 16x16 red, 8x8 green, 4x4 blue, 2x2 white, 1x1 yellow */
static uint32_t level_texel(const void *vm, int size, int x, int y) {
    static const uint16_t c565[5] = { 0xf800, 0x07e0, 0x001f, 0xffff, 0xffe0 };
    static const uint16_t c1555[5] = { 0xfc00, 0x83e0, 0x801f, 0xffff, 0xffe0 };
    static const uint16_t c4444[5] = { 0xff00, 0xf0f0, 0xf00f, 0xffff, 0xfff0 };
    static const uint8_t cyuv[5][3] = { { 80, 90, 240 }, { 145, 54, 34 }, { 41, 240, 110 }, { 235, 128, 128 }, { 210, 16, 146 } };
    const mvar_t *m = (const mvar_t *)vm;
    const int lv = size == 16 ? 0 : size == 8 ? 1 : size == 4 ? 2 : size == 2 ? 3 : 4;
    (void)y;
    switch(m->PixelFmt) {
    case PixelFmt_565:    return c565[lv];
    case PixelFmt_1555:   return c1555[lv];
    case PixelFmt_4444:   return c4444[lv];
    case PixelFmt_YUV422: return (uint32_t)(cyuv[lv][0] << 8) | cyuv[lv][(x & 1) ? 2 : 1];
    default:              return (uint32_t)(lv + 1);        /* palette entries 1 .. 5 */
    }
}

static uint32_t upload_raw(const void *data, int bytes) {
    uint32_t at = pvr_tex_alloc(bytes);
    pvr_tex_load(at, data, bytes);
    return at;
}
static const int sq[10] = { 32, 24, 16, 12, 8, 6, 4, 3, 2, 1 };
static const int rect[8][2] = { { 32, 4 }, { 4, 32 }, { 16, 2 }, { 2, 16 }, { 32, 16 }, { 16, 32 }, { 8, 1 }, { 1, 8 } };

static void test_setup(void) {
    static uint16_t mip[348];
    static uint8_t vq[2048 + 16];
    static uint16_t st[32 * 8];
    static const uint16_t level_color[5] = { 0xffe0, 0xffff, 0x001f, 0x07e0, 0xf800 };   /* 1x1 .. 16x16 */
    int i, k, n = 3, size = 1;      /* the 1x1 level is the fourth word */

    for(k = 0; k < 5; k++, size *= 4)
        for(i = 0; i < size; i++)
            mip[n++] = level_color[k];
    mip_txr = upload_raw(mip, sizeof(mip));

    for(k = 0; k < 256; k++)
        for(i = 0; i < 4; i++) {
            uint16_t t = (uint16_t)(((k & 31) << 11) | (((k >> 5) * 8 + i * 2) << 5) | (i * 10));
            vq[k * 8 + i * 2] = t & 0xff;
            vq[k * 8 + i * 2 + 1] = t >> 8;
        }
    for(i = 0; i < 16; i++)
        vq[2048 + i] = (uint8_t)(i * 13 + 5);
    vq_txr = upload_raw(vq, sizeof(vq));

    for(i = 0; i < 32 * 8; i++)
        st[i] = (uint16_t)(((i % 32) << 11) | ((i / 32) * 8 << 5) | ((i & 1) ? 31 : 0));
    stride_txr = upload_raw(st, sizeof(st));

    for(k = 0; k < NUM_MVARS; k++) {
        const tex_layout_t l = { mvars[k].layout, mvars[k].bits, 16, 16, 0 };
        mvar_txr[k] = tex_build(&l, level_texel, &mvars[k]);
    }
    pvr_pal_format(PAL_RAM_CTRL_ARGB8888);
    {
        static const uint32_t level_rgb[5] = { 0xffff0000, 0xff00ff00, 0xff0000ff, 0xffffffff, 0xffffff00 };
        for(k = 0; k < 5; k++)
            pvr_pal_entry(1 + k, level_rgb[k]);
        for(k = 0; k < 256; k++)                    /* entry 256 + e: the colour (0, e, 255 - e) */
            pvr_pal_entry(256 + k, 0xff000000u | (uint32_t)(k << 8) | (uint32_t)(255 - k));
    }
    {
        static uint8_t a[4096];
        for(i = 0; i < 1024; i++) {                 /* 1024 words that are their own index, then bytes that are */
            a[2 * i] = (uint8_t)i;
            a[2 * i + 1] = (uint8_t)(i >> 8);
        }
        for(i = 2048; i < 4096; i++)
            a[i] = (uint8_t)i;
        addr_txr = upload_raw(a, sizeof(a));
        for(i = 0; i < 4096; i++)
            a[i] = (uint8_t)i;
        addr_pal_txr = upload_raw(a, sizeof(a));
    }
    {
        int k;
        for(k = 0; k < NUM_NSQ; k++) {
            const int mip = nsq[k].layout == TEX_MIP || nsq[k].layout == TEX_VQ_MIP;
            const tex_layout_t l = { nsq[k].layout, 16, nsq[k].w, mip ? nsq[k].w : nsq[k].h, 0 };
            nsq_txr[k] = tex_build(&l, nsq_texel, &nsq[k]);
        }
    }
}

static void test_describe(void) {
    static const char *f[4] = { "point", "trilinear 1", "trilinear 2", "bilinear" };
    int p, b, c, k;

    for(k = 0; k < NUM_NSQ; k++) {
        const int cell = (k / 10) * 2 * COLS + (k % 10) * 2;
        label_span(18, cell, 2, 2, "%s, declared %dx%d: a texel per pixel, point", nsq[k].name, nsq[k].w, nsq[k].h);
        label_span(19, cell, 2, 2, "%s, declared %dx%d: a texel per pixel, bilinear, a quarter texel off", nsq[k].name, nsq[k].w, nsq[k].h);
    }

    for(p = 0; p < 4; p++)
        for(b = 0; b < 15; b++) {
            for(c = 0; c < 10; c++)
                label(p, b * COLS + c, "mip %s, bias %d/4, %dx%d px", f[p], b + 1, sq[c], sq[c]);
            for(c = 0; c < 8; c++)
                label(p, b * COLS + 10 + c, "mip %s, bias %d/4, %dx%d px", f[p], b + 1, rect[c][0], rect[c][1]);
        }
    label_span(4, 0, 2, 2, "VQ 8x8 at 8x, point");
    label_span(4, 4, 2, 2, "VQ 8x8 at 8x, bilinear");
    label_span(4, 8, 4, 1, "stride 32, 64x8 at 2x, point");
    for(p = 0; p < 12; p++)
        for(k = 0; k < 3; k++)
            for(b = 0; b < 5; b++) {
                const char *n = mvars[(p % 3) * 3 + k].name;
                const int at = (k * 5 + b) * COLS;
                for(c = 0; c < 10; c++)
                    label(5 + p, at + c, "%s: %s, bias %d/4, %dx%d px", n, f[p / 3], bias5[b], sq[c], sq[c]);
                for(c = 0; c < 8; c++)
                    label(5 + p, at + 10 + c, "%s: %s, bias %d/4, %dx%d px", n, f[p / 3], bias5[b], rect[c][0], rect[c][1]);
            }
    for(c = 0; c < 16; c++) {
        label(17, c, "address 565 16x16 at 2x: scan order %d stride %d mipmapped %d VQ %d", c & 1, (c >> 1) & 1, (c >> 2) & 1, (c >> 3) & 1);
        label(17, COLS + c, "address 565 16x16 at half size: scan order %d stride %d mipmapped %d VQ %d", c & 1, (c >> 1) & 1, (c >> 2) & 1, (c >> 3) & 1);
        label(17, 2 * COLS + c, "address palette %d bpp %s: mipmapped %d VQ %d", c < 8 ? 8 : 4, (c & 4) ? "at half size" : "at 2x", c & 1, (c >> 1) & 1);
    }
}

/* every parameter word of a textured polygon (translucent list, replace, point/bilinear/trilinear) */
static void textured(uint32_t txr, int PixelFmt, int ScanOrder, int VQ, int MipMapped, int StrideSel, int TexU, int TexV, int filter, int bias) {
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
    isp.DepthMode = DepthMode_Always;
    isp.ZWriteDis = 1;
    isp.Texture = 1;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    tsp.ShadInstr = ShadInstr_Decal;
    tsp.FilterMode = filter;
    tsp.MipMapD = bias;
    tsp.TexU = TexU;
    tsp.TexV = TexV;
    tcw.PixelFmt = PixelFmt;
    tcw.ScanOrder = ScanOrder;
    tcw.VQ_Comp = VQ;
    tcw.MipMapped = MipMapped;
    tcw.StrideSel = StrideSel;
    if(PixelFmt == PixelFmt_Pal8 || PixelFmt == PixelFmt_Pal4)     /* these bits are the palette select there */
        tcw.PalSelect = pal_select;
    tcw.TexAddr = txr >> 3;
    ta_polygon(pcw, isp, tsp, tcw);
}

#define quad_uv(x, y, w, h, u0, v0, u1, v1) quad3(x, y, w, h, F32B_ONE, u0, v0, u1, v1, 0xffffffff, 0)

static void test_page(int page) {
    static const int filter[4] = { FilterMode_Point, FilterMode_TrilinearA, FilterMode_TrilinearB, FilterMode_Bilinear };
    int b, c, k;

    pvrio_reg(TEXT_CONTROL_addr, 1);            /* stride, in units of 32 texels */
    pal_select = 0;
    ta_list_begin(ListType_Translucent);
    if(page >= 5 && page < 17) {
        const int p = page - 5;
        for(k = 0; k < 3; k++) {
            const int v = (p % 3) * 3 + k;
            const mvar_t *m = &mvars[v];
            for(b = 0; b < 5; b++) {
                const int at = (k * 5 + b) * COLS;
                textured(mvar_txr[v], m->PixelFmt, m->ScanOrder, m->VQ, m->MipMapped, m->StrideSel, 1, 1, filter[p / 3], bias5[b]);
                for(c = 0; c < 10; c++)
                    quad_uv(cell_px(at + c), cell_py(at + c), sq[c], sq[c], F32B_ZERO, F32B_ZERO, F32B_ONE, F32B_ONE);
                for(c = 0; c < 8; c++)
                    quad_uv(cell_px(at + 10 + c), cell_py(at + 10 + c), rect[c][0], rect[c][1], F32B_ZERO, F32B_ZERO, F32B_ONE, F32B_ONE);
            }
        }
    }
    else if(page >= 18) {
        for(k = 0; k < NUM_NSQ; k++) {
            const nsq_t *n = &nsq[k];
            const int cell = (k / 10) * 2 * COLS + (k % 10) * 2;
            /* a quarter of a texel: 2^-2 / size, exact */
            const f32b du = page == 19 ? f32b_scaled(1, -2 - log2i(n->w)) : F32B_ZERO, dv = page == 19 ? f32b_scaled(1, -2 - log2i(n->h)) : F32B_ZERO;
            textured(nsq_txr[k], PixelFmt_565, n->ScanOrder, n->VQ, n->MipMapped, 0, log2i(n->w) - 3, log2i(n->h) - 3,
                     page == 19 ? FilterMode_Bilinear : FilterMode_Point, 4);
            quad_uv(cell_px(cell), cell_py(cell), n->w, n->h, du, dv, sf_add(F32B_ONE, du), sf_add(F32B_ONE, dv));
        }
    }
    else if(page == 17) {
        for(c = 0; c < 16; c++) {
            pal_select = 16;                    /* palette entries 256 .. */
            textured(addr_txr, PixelFmt_565, c & 1, (c >> 3) & 1, (c >> 2) & 1, (c >> 1) & 1, 1, 1, FilterMode_Point, 4);
            quad_uv(cell_px(c), cell_py(c), 32, 32, F32B_ZERO, F32B_ZERO, F32B_ONE, F32B_ONE);
            quad_uv(cell_px(COLS + c), cell_py(COLS + c), 8, 8, F32B_ZERO, F32B_ZERO, F32B_ONE, F32B_ONE);
            textured(addr_pal_txr, c < 8 ? PixelFmt_Pal8 : PixelFmt_Pal4, 0, (c >> 1) & 1, c & 1, 0, 1, 1, FilterMode_Point, 4);
            quad_uv(cell_px(2 * COLS + c), cell_py(2 * COLS + c), (c & 4) ? 8 : 32, (c & 4) ? 8 : 32, F32B_ZERO, F32B_ZERO, F32B_ONE, F32B_ONE);
        }
    }
    else if(page < 4) {
        for(b = 0; b < 15; b++) {
            textured(mip_txr, PixelFmt_565, 0, 0, 1, 0, 1, 1, filter[page], b + 1);     /* 16x16, mipmapped, bias b + 1 */
            for(c = 0; c < 10; c++)
                quad_uv(cell_px(b * COLS + c), cell_py(b * COLS + c), sq[c], sq[c], F32B_ZERO, F32B_ZERO, F32B_ONE, F32B_ONE);
            for(c = 0; c < 8; c++)
                quad_uv(cell_px(b * COLS + 10 + c), cell_py(b * COLS + 10 + c), rect[c][0], rect[c][1], F32B_ZERO, F32B_ZERO, F32B_ONE, F32B_ONE);
        }
    }
    else {
        /* POINT_FIX was 0 in the runs that matter: no shift */
        textured(vq_txr, PixelFmt_565, 0, 1, 0, 0, 0, 0, FilterMode_Point, 4);
        quad_uv(cell_px(0), cell_py(0), 64, 64, F32B_ZERO, F32B_ZERO, F32B_ONE, F32B_ONE);
        textured(vq_txr, PixelFmt_565, 0, 1, 0, 0, 0, 0, FilterMode_Bilinear, 4);
        quad_uv(cell_px(4), cell_py(4), 64, 64, F32B_ZERO, F32B_ZERO, F32B_ONE, F32B_ONE);

        textured(stride_txr, PixelFmt_565, 1, 0, 0, 1, 3, 0, FilterMode_Point, 4);     /* 64x8 */
        quad_uv(cell_px(8), cell_py(8), 128, 16, F32B_ZERO, F32B_ZERO, F32B_ONE, F32B_ONE);
    }
    ta_list_end();
}
