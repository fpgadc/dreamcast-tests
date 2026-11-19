/* TEXT_CONTROL's endian bits (17, 16) and bank bits (12..8) against every address bit (2026-11-19, TODO A3, after
   pvr-tests/texctl and texctl_bank).

   Video memory that nothing else uses is filled with words that name their own place (as texctl_bank): the 32 bit
   word at byte offset W of chip c (the 64 bit area's even words are chip 0, odd words chip 1) holds bytes
   b0 = N & 255, b1 = N >> 8 & 255, b2 = N >> 16 & 15 | c << 4 | 0x80, b3 = (b0 + 3 b1 + 5 b2 + 0x5b) & 255,
   N = W / 4. Textures are read straight from it (no data of their own), so every texel shows the address it was
   read from, all address bits; VQ textures have a code book of their own whose entries show the index byte read:
   565 entry e: four texels e | (e ^ 0xff) << 8; palette 8 bpp entry e: eight bytes e. Palette entry i: red i,
   green i ^ 0xa5, blue 0x3c (ARGB8888).
     565 twiddled 256x256 at 0x6d0000             (0, 0)
     565 strided 256x256, stride 384, at 0x5f0000  (256, 0)   (TEXT_CONTROL bits 4..0 = 12 on every page)
     VQ 565 128x128, code book 0x57f000          (512, 0)   index 0x57f800 .. 0x5807ff: across 0x580000
     VQ 565 128x128 mipmapped, code book 0x7f0000 (512, 128) level 0
     palette 8 bpp twiddled 64x64 at 0x4c3000     (0, 256)
     palette 4 bpp twiddled 64x64 at 0x4b2000     (64, 256)
     VQ palette 8 bpp 64x64, code book 0x6aa000   (128, 256)
     YUV422 twiddled 64x64 at 0x5c4000            (192, 256)
     YUV422 strided 64x64 at 0x5d0000             (256, 256)
     bump map twiddled 64x64 at 0x5e2000          (320, 256)  translucent, (SRC_ALPHA, ZERO), light 0x40605020 in the
                                                             offset colour: the intensity of each (S, R) read shows
                                                             (pvr-tests/bumpfn; opaque and decal it is plain white)
     4444 twiddled 64x64 at 0x4d0000              (384, 256)
     VQ palette 4 bpp 64x64, code book 0x4e6000   (448, 256)  entry e: nibbles e & 15, e >> 4, e & 15, ...
   Pages (TEXT_CONTROL, | 12): 0 0x00000, 1 0x10000, 2 0x20000, 3 0x30000, 4 0x00100, 5 0x00700, 6 0x10300, 7 0x30600,
   then every other bank value, 8 .. 18: 0x00200, 0x00400, 0x00500, 0x00800, 0x00900, 0x00a00, 0x00b00, 0x00c00,
   0x00d00, 0x00e00, 0x00f00, and with the endian bits 19 0x10b00, 20 0x20d00, 21 0x30e00 (every bank value with the
   textures that are not VQ, which read their own places whatever it is). A page draws a VQ texture only where all
   its index reads land in the filled memory (vq_drawn[], bits in the order of the four VQ textures above; not into
   the framebuffer, written while the texture is read, nor past 8 MB, where a console reads data that changes from
   run to run, texctl_bank).
   Filled as texctl_bank: chip 0 0x12c000-0x1fffff, chip 1 0x002000-0x0fefff and 0x173000-0x1fffff, both chips
   0x200000-0x3fffff. */
#define NUM_PAGES 22
#include "pvrtest2.h"

static const uint32_t text_control[NUM_PAGES] = {
    0x00000, 0x10000, 0x20000, 0x30000, 0x00100, 0x00700, 0x10300, 0x30600, 0x00200, 0x00400, 0x00500, 0x00800,
    0x00900, 0x00a00, 0x00b00, 0x00c00, 0x00d00, 0x00e00, 0x00f00, 0x10b00, 0x20d00, 0x30e00,
};
/* VQ 565, VQ 565 mipmapped, VQ palette 8 bpp, VQ palette 4 bpp (bits 0 .. 3): drawn on the page
   (agents/nulldc3/hunt/texctl/plan.py) */
static const uint8_t vq_drawn[NUM_PAGES] = { 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xe, 0x2, 0x2, 0x6, 0x4, 0x9, 0x1, 0x8, 0x9, 0x4, 0x1, 0x8 };
#define STRIDE 12

enum { T_TW, T_STRIDE, T_VQ, T_VQMIP, T_PAL8, T_PAL4, T_VQPAL8, T_YUV, T_YUVS, T_BUMP, T_4444, T_VQPAL4, NUM_TEX };
static const struct { const char *name; int x, y, size, fmt, raster, vq, mip; uint32_t base; } tex_info[NUM_TEX] = {
    { "565 twiddled 256x256",            0,   0, 256, PixelFmt_565,     0, 0, 0, 0x6d0000u },
    { "565 strided 256x256, stride 384", 256, 0, 256, PixelFmt_565,     1, 0, 0, 0x5f0000u },
    { "VQ 565 128x128, index across 0x580000", 512, 0, 128, PixelFmt_565, 0, 1, 0, 0x57f000u },
    { "VQ 565 128x128 mipmapped, level 0", 512, 128, 128, PixelFmt_565, 0, 1, 1, 0x7f0000u },
    { "palette 8 bpp twiddled 64x64",    0, 256,  64, PixelFmt_Pal8,    0, 0, 0, 0x4c3000u },
    { "palette 4 bpp twiddled 64x64",   64, 256,  64, PixelFmt_Pal4,    0, 0, 0, 0x4b2000u },
    { "VQ palette 8 bpp 64x64",        128, 256,  64, PixelFmt_Pal8,    0, 1, 0, 0x6aa000u },
    { "YUV422 twiddled 64x64",         192, 256,  64, PixelFmt_YUV422,  0, 0, 0, 0x5c4000u },
    { "YUV422 strided 64x64, stride 384", 256, 256, 64, PixelFmt_YUV422, 1, 0, 0, 0x5d0000u },
    { "bump map twiddled 64x64",       320, 256,  64, PixelFmt_BumpMap, 0, 0, 0, 0x5e2000u },
    { "4444 twiddled 64x64",           384, 256,  64, PixelFmt_4444,    0, 0, 0, 0x4d0000u },
    { "VQ palette 4 bpp 64x64",        448, 256,  64, PixelFmt_Pal4,    0, 1, 0, 0x4e6000u },
};

static int drawn(int page, int t) {
    const int k = t == T_VQ ? 0 : t == T_VQMIP ? 1 : t == T_VQPAL8 ? 2 : t == T_VQPAL4 ? 3 : -1;
    return k < 0 || (vq_drawn[page] >> k & 1);
}

static uint32_t pat(int c, uint32_t W) {
    const uint32_t N = W >> 2;
    const uint32_t b0 = N & 255, b1 = (N >> 8) & 255, b2 = ((N >> 16) & 15) | (uint32_t)c << 4 | 0x80;
    const uint32_t b3 = (b0 + 3 * b1 + 5 * b2 + 0x5b) & 255;
    return b0 | b1 << 8 | b2 << 16 | b3 << 24;
}

static uint32_t words[2048];

static void fill_chip(int c, uint32_t from, uint32_t to) {
    uint32_t W, n;
    for(W = from; W < to; W += n * 4) {
        uint32_t i;
        n = (to - W) / 4;
        if(n > 2048)
            n = 2048;
        for(i = 0; i < n; i++)
            words[i] = pat(c, W + 4 * i);
        pvrio_w32((c ? 0x400000u : 0) + W, words, (int)n);
    }
}

static void fill64(uint32_t from, uint32_t to) {
    uint32_t A, n;
    for(A = from; A < to; A += n * 4) {
        uint32_t i;
        n = (to - A) / 4;
        if(n > 2048)
            n = 2048;
        for(i = 0; i < n; i++) {
            const uint32_t a = A + 4 * i;
            words[i] = pat((int)(a >> 2 & 1), (a >> 3) << 2);
        }
        pvrio_w64(A, words, (int)n);
    }
}

static void test_setup(void) {
    int t, e;
    pvr_pal_format(PAL_RAM_CTRL_ARGB8888);
    for(e = 0; e < 256; e++)
        pvr_pal_entry(e, 0xff000000u | (uint32_t)e << 16 | (uint32_t)(e ^ 0xa5) << 8 | 0x3cu);
    fill_chip(0, 0x12c000u, 0x200000u);
    fill_chip(1, 0x002000u, 0x0ff000u);
    fill_chip(1, 0x173000u, 0x200000u);
    fill64(0x400000u, 0x800000u);
    for(t = 0; t < NUM_TEX; t++) {
        if(!tex_info[t].vq)
            continue;
        for(e = 0; e < 256; e++) {
            const uint32_t w = tex_info[t].fmt == PixelFmt_565 ? (uint32_t)(e | (e ^ 0xff) << 8) * 0x10001u
                             : tex_info[t].fmt == PixelFmt_Pal8 ? (uint32_t)e * 0x01010101u
                             : (uint32_t)((e & 15) | (e >> 4) << 4) * 0x01010101u;   /* 4 bpp: texel j even e & 15, odd e >> 4 */
            words[2 * e] = w;
            words[2 * e + 1] = w;
        }
        pvrio_w64(tex_info[t].base, words, 512);
    }
}

static void test_describe(void) {
    int p, t;
    for(p = 0; p < NUM_PAGES; p++)
        for(t = 0; t < NUM_TEX; t++) {
            const int s = tex_info[t].size / 32 ? tex_info[t].size / 32 : 1;
            if(!drawn(p, t))
                continue;
            label_span(p, (tex_info[t].y / 32) * COLS + tex_info[t].x / 32, s, s, "TEXT_CONTROL %05x: %s at %06x",
                       (unsigned)(text_control[p] | STRIDE), tex_info[t].name, (unsigned)tex_info[t].base);
        }
}

static int log2i(int s) { int k = 0; while((8 << k) < s) k++; return k; }

#define BUMP_LIGHT 0x40605020u

static void textured(int t) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    const int bump = t == T_BUMP;
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = bump ? ListType_Translucent : ListType_Opaque;
    pcw.Offset = (uint32_t)bump;
    isp.Offset = (uint32_t)bump;
    isp.ZWriteDis = (uint32_t)bump;
    tsp.UseAlpha = (uint32_t)bump;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Texture = 1;
    pcw.Gouraud = 1;
    isp.DepthMode = DepthMode_Always;
    isp.Texture = 1;
    tsp.SrcInstr = bump ? BlendInstr_SrcAlpha : BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.ShadInstr = ShadInstr_Decal;
    tsp.FilterMode = FilterMode_Point;
    tsp.MipMapD = 4;
    tsp.TexU = (uint32_t)log2i(tex_info[t].size);
    tsp.TexV = tsp.TexU;
    tcw.PixelFmt = (uint32_t)tex_info[t].fmt;
    tcw.ScanOrder = (uint32_t)tex_info[t].raster;
    tcw.StrideSel = (uint32_t)tex_info[t].raster;
    tcw.VQ_Comp = (uint32_t)tex_info[t].vq;
    tcw.MipMapped = (uint32_t)tex_info[t].mip;
    tcw.TexAddr = tex_info[t].base >> 3;
    ta_polygon(pcw, isp, tsp, tcw);
}

static void test_page(int page) {
    int t;
    pvrio_reg(TEXT_CONTROL_addr, text_control[page] | STRIDE);
    pvr_frame.bg_color = 0x000040;
    ta_list_begin(ListType_Opaque);
    for(t = 0; t < NUM_TEX; t++) {
        const int s = tex_info[t].size;
        if(!drawn(page, t) || t == T_BUMP)
            continue;
        textured(t);
        quad3(tex_info[t].x, tex_info[t].y, s, s, F32B_ONE, F32B_ZERO, F32B_ZERO, F32B_ONE, F32B_ONE, 0xffffffffu, 0);
    }
    ta_list_end();
    ta_list_begin(ListType_Translucent);
    textured(T_BUMP);
    quad3(tex_info[T_BUMP].x, tex_info[T_BUMP].y, 64, 64, F32B_ONE, F32B_ZERO, F32B_ZERO, F32B_ONE, F32B_ONE, 0xffffffffu, BUMP_LIGHT);
    ta_list_end();
}
