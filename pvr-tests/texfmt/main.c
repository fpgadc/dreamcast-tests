/* Texel formats: how each format's bits become 8 bits per channel, for every way a texture is stored and
   filtered.

   Every square (16x16 pixels, four per cell) is a quad textured with an 8x8 texture whose texels are all
   the same colour, so where the texture is sampled does not matter; what shows is how the texel's bits are
   widened, which is not the same for every kind of texture (a strided texture filtered bilinearly gets
   zeroes for low bits where the others repeat the top bits).

   Three pages for each of the ten kinds in stores[] (page = 3 * kind + n):
     n 0  RGB565 (each channel swept alone), ARGB1555, ARGB4444; then alpha of 1555 and 4444, shown as
          white * alpha through a (SRC_ALPHA, ZERO) blend
     n 1  YUV422: Y swept with U = V = 128, then a grid of U, V at Y = 0, 128, 255
     n 2  YUV422: U swept, then V swept, at Y = 128
   pages 30..45  palette textures (twiddled): 8 bpp then 4 bpp; point sampled then bilinear; for each the
          palette formats ARGB1555, RGB565, ARGB4444, ARGB8888. 256 squares: palette entry n. (4 bpp: the
          entry is 16 * the palette select + the texel.)

   A page's textures are built when the page is drawn and take the place of the page before's.
   (The earlier version of this test had pages 0 to 2 for RGB twiddled and YUV not twiddled, point sampled,
   and the four 8 bpp point sampled palette pages.) */
#define NUM_STORE 10
#define NUM_PAGES (3 * NUM_STORE + 16)
#include "pvrtest2.h"
#include "texgen.h"

enum { F565, F1555, F4444, FYUV };
typedef struct { uint8_t fmt, alpha_view; uint16_t raw, raw2; char tag[20]; } item_t;

#define MAX_ITEMS 520
static item_t items[3][MAX_ITEMS];
static int num_items[3];
static uint32_t pal_entries[4][256];
static uint32_t tex_base;           /* where a page's textures start */

typedef struct { const char *name; int layout, stride, ScanOrder, StrideSel, VQ, MipMapped, filter; } store_t;
static const store_t stores[NUM_STORE] = {
    { "twiddled, point",        TEX_TWIDDLED,  0, 0, 0, 0, 0, FilterMode_Point },
    { "twiddled, bilinear",     TEX_TWIDDLED,  0, 0, 0, 0, 0, FilterMode_Bilinear },
    { "not twiddled, point",    TEX_RASTER,    0, 1, 0, 0, 0, FilterMode_Point },
    { "not twiddled, bilinear", TEX_RASTER,    0, 1, 0, 0, 0, FilterMode_Bilinear },
    { "stride 32, point",       TEX_RASTER,   32, 1, 1, 0, 0, FilterMode_Point },
    { "stride 32, bilinear",    TEX_RASTER,   32, 1, 1, 0, 0, FilterMode_Bilinear },
    { "mipmapped, point",       TEX_MIP,       0, 0, 0, 0, 1, FilterMode_Point },
    { "mipmapped, bilinear",    TEX_MIP,       0, 0, 0, 0, 1, FilterMode_Bilinear },
    { "VQ, point",              TEX_VQ,        0, 0, 0, 1, 0, FilterMode_Point },
    { "VQ, bilinear",           TEX_VQ,        0, 0, 0, 1, 0, FilterMode_Bilinear },
};

static void add(int page, int fmt, int raw, int raw2, int alpha_view, const char *tag) {
    item_t *it = &items[page][num_items[page]++];
    it->fmt = (uint8_t)fmt;
    it->raw = (uint16_t)raw;
    it->raw2 = (uint16_t)raw2;
    it->alpha_view = (uint8_t)alpha_view;
    strncpy(it->tag, tag, sizeof(it->tag) - 1);
}

static void add_yuv(int page, int y, int u, int v, const char *tag) {
    add(page, FYUV, (y << 8) | u, (y << 8) | v, 0, tag);
}

/* every texel the item's colour. YUV422: the texel at an even x is Y and U, at an odd x Y and V */
static uint32_t uniform(const void *vi, int size, int x, int y) {
    const item_t *it = (const item_t *)vi;
    (void)size; (void)y;
    return (it->fmt == FYUV && (x & 1)) ? it->raw2 : it->raw;
}

static uint32_t uniform_index(const void *vi, int size, int x, int y) {
    (void)size; (void)x; (void)y;
    return (uint32_t)*(const int *)vi;
}

static void test_setup(void) {
    static const int uv[5] = { 0, 64, 128, 192, 255 };
    static const int ys[3] = { 0, 128, 255 };
    int i, j, k, f;

    for(i = 0; i < 32; i++) add(0, F565, i << 11, 0, 0, "565 R");
    for(i = 0; i < 64; i++) add(0, F565, i << 5, 0, 0, "565 G");
    for(i = 0; i < 32; i++) add(0, F565, i, 0, 0, "565 B");
    for(i = 0; i < 32; i++) add(0, F1555, 0x8000 | (i << 10), 0, 0, "1555 R");
    for(i = 0; i < 32; i++) add(0, F1555, 0x8000 | (i << 5), 0, 0, "1555 G");
    for(i = 0; i < 32; i++) add(0, F1555, 0x8000 | i, 0, 0, "1555 B");
    for(i = 0; i < 16; i++) add(0, F4444, 0xf000 | (i << 8), 0, 0, "4444 R");
    for(i = 0; i < 16; i++) add(0, F4444, 0xf000 | (i << 4), 0, 0, "4444 G");
    for(i = 0; i < 16; i++) add(0, F4444, 0xf000 | i, 0, 0, "4444 B");
    for(i = 0; i < 2; i++) add(0, F1555, (i << 15) | 0x7fff, 0, 1, "1555 A");
    for(i = 0; i < 2; i++) add(0, F1555, (i << 15) | 0x7fff, 0, 0, "1555 A, no blend");
    for(i = 0; i < 16; i++) add(0, F4444, (i << 12) | 0xfff, 0, 1, "4444 A");

    for(i = 0; i < 256; i++) add_yuv(1, i, 128, 128, "YUV Y");
    for(k = 0; k < 3; k++)
        for(i = 0; i < 5; i++)
            for(j = 0; j < 5; j++) {
                char tag[20];
                sprintf(tag, "YUV Y%d U%d V..", ys[k], uv[i]);
                add_yuv(1, ys[k], uv[i], uv[j], tag);
            }
    for(i = 0; i < 256; i++) add_yuv(2, 128, i, 128, "YUV U");
    for(i = 0; i < 256; i++) add_yuv(2, 128, 128, i, "YUV V");

    rng_state = 77;
    for(f = 0; f < 4; f++) {
        for(i = 0; i < 256; i++) {
            uint32_t r = rng(), e;
            if(f == 0)      e = 0x8000 | (i < 32 ? i << 10 : i < 64 ? (i - 32) << 5 : i < 96 ? i - 64 : (r & 0x7fff));
            else if(f == 1) e = i < 32 ? i << 11 : i < 96 ? (i - 32) << 5 : i < 128 ? i - 96 : (r & 0xffff);
            else if(f == 2) e = 0xf000 | (i < 16 ? i << 8 : i < 32 ? (i - 16) << 4 : i < 48 ? i - 32 : (r & 0xfff));
            else            e = 0xff000000 | (i << 16) | ((255 - i) << 8) | (i ^ 0x55);
            pal_entries[f][i] = e;
        }
    }
    tex_base = pvr_tex_next;
}

static void test_describe(void) {
    static const char *pal_name[4] = { "ARGB1555", "RGB565", "ARGB4444", "ARGB8888" };
    int s, b, n, p;

    for(s = 0; s < NUM_STORE; s++)
        for(b = 0; b < 3; b++)
            for(n = 0; n < num_items[b]; n += 4)
                label(3 * s + b, n / 4, "%s; %s, raw %04x.. (4 squares)", stores[s].name, items[b][n].tag, items[b][n].raw);
    for(p = 0; p < 16; p++)
        for(n = 0; n < 256; n += 4)
            label(3 * NUM_STORE + p, n / 4, "PAL%d %s %s entries %d..%d, first %08x", p < 8 ? 8 : 4, pal_name[p % 4], (p / 4) % 2 ? "bilinear" : "point",
                  n, n + 3, (unsigned)pal_entries[p % 4][n]);
}

/* square n of the page: an 8x8 texture of the store's kind (palette textures: twiddled, with their palette select) */
static void square(int n, int PixelFmt, const store_t *st, int filter, uint32_t txr, int alpha_view, int pal_select) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    int c = n / 4, q = n % 4;

    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Translucent;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Texture = 1;
    pcw.Gouraud = 1;
    isp.DepthMode = DepthMode_Always;
    isp.ZWriteDis = 1;
    isp.Texture = 1;
    tsp.SrcInstr = alpha_view ? BlendInstr_SrcAlpha : BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    tsp.ShadInstr = ShadInstr_Decal;
    tsp.FilterMode = filter;
    tsp.MipMapD = 4;
    tsp.TexU = 0;                               /* 8 texels */
    tsp.TexV = 0;
    if(PixelFmt == PixelFmt_Pal8 || PixelFmt == PixelFmt_Pal4)
        tcw.PalSelect = pal_select;
    else {
        tcw.ScanOrder = st->ScanOrder;
        tcw.StrideSel = st->StrideSel;
    }
    tcw.PixelFmt = PixelFmt;
    tcw.VQ_Comp = st->VQ;
    tcw.MipMapped = st->MipMapped;
    tcw.TexAddr = txr >> 3;
    ta_polygon(pcw, isp, tsp, tcw);
    quad3(cell_px(c) + (q & 1) * 16, cell_py(c) + (q >> 1) * 16, 16, 16, F32B_ONE, F32B_ZERO, F32B_ZERO, F32B_ONE, F32B_ONE, 0xffffffff, 0);
}

static void test_page(int page) {
    static const int fmt[4] = { PixelFmt_565, PixelFmt_1555, PixelFmt_4444, PixelFmt_YUV422 };
    static const int pal_fmt[4] = { PAL_RAM_CTRL_ARGB1555, PAL_RAM_CTRL_RGB565, PAL_RAM_CTRL_ARGB4444, PAL_RAM_CTRL_ARGB8888 };
    int n;

    pvr_tex_next = tex_base;                    /* this page's textures take the place of the page before's */
    pvrio_reg(TEXT_CONTROL_addr, 1);            /* the stride of strided textures: 32 texels */
    ta_list_begin(ListType_Translucent);
    if(page < 3 * NUM_STORE) {
        const store_t *st = &stores[page / 3];
        const tex_layout_t l = { st->layout, 16, 8, 8, st->stride };
        const int b = page % 3;
        for(n = 0; n < num_items[b]; n++)
            square(n, fmt[items[b][n].fmt], st, st->filter, tex_build(&l, uniform, &items[b][n]), items[b][n].alpha_view, 0);
    }
    else {
        const int p = page - 3 * NUM_STORE, four = p >= 8;
        const tex_layout_t l = { TEX_TWIDDLED, four ? 4 : 8, 8, 8, 0 };
        pvr_pal_format(pal_fmt[p % 4]);
        for(n = 0; n < 256; n++)
            pvr_pal_entry(n, pal_entries[p % 4][n]);
        for(n = 0; n < 256; n++) {
            int index = four ? n % 16 : n;
            square(n, four ? PixelFmt_Pal4 : PixelFmt_Pal8, &stores[0], (p / 4) % 2 ? FilterMode_Bilinear : FilterMode_Point,
                   tex_build(&l, uniform_index, &index), 0, four ? n / 16 : 0);
        }
    }
    ta_list_end();
}
