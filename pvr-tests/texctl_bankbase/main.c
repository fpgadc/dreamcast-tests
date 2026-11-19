/* TEXT_CONTROL's bank bits (12..8): which part of a VQ texture's index address they change (2026-11-19, TODO A3,
   after pvr-tests/texctl_bank: index address bits 22..19 XOR the bank value). Found: the XOR is on the texture's
   start plus the index byte's place (mipmap level, twiddled place), before the code book's 2 KB is added:
   ((start + offset) ^ (bank << 19)) + 2048 (agents/nulldc3/hunt/texctl/bankfit.py: every decoded half; XORing
   after the 2 KB, or the start alone, is wrong on the first 2 KB past a 512 KB line).

   The video memory fill of texctl_bank (words that name their own place) and VQ textures whose code book entries
   show the index byte read (565 entry e: four texels e | (e ^ 0xff) << 8; palette 8 bpp entry e: eight bytes e,
   palette entry i: red i, green i ^ 0xa5, blue 0x3c), placed so that one of the additions crosses a 512 KB line:
     VQ 565 64x64, code book 0x57f800            (0, 0)     the code book's 2 KB: index from 0x580000
     VQ 565 256x256 mipmapped, code book 0x67e800 (64, 0)   level 0 (offset 0x1556 bytes): from 0x680556
     VQ palette 8 bpp 64x64, code book 0x4ff800  (320, 0)   the code book's 2 KB: index from 0x500000
     VQ 565 128x128 mipmapped, code book 0x77f000 (384, 0)  level 0 (offset 0x556): from 0x77fd56 across 0x780000
   Pages: TEXT_CONTROL = p << 8 on page p (0 .. 15), then 0x10200, 0x20400, 0x30700 (the endian bits too). A page
   draws only the textures whose index reads all land in the filled memory (not the framebuffer, written while
   the texture is read; not past 8 MB, where a console reads data that changes from run to run): the bits of
   drawn[] are the four textures in the order above.
   Filled as texctl_bank: chip 0 0x12c000-0x1fffff, chip 1 0x002000-0x0fefff and 0x173000-0x1fffff, both chips
   0x200000-0x3fffff. */
#define NUM_PAGES 19
#include "pvrtest2.h"

static const uint32_t text_control[NUM_PAGES] = {
    0x00000, 0x00100, 0x00200, 0x00300, 0x00400, 0x00500, 0x00600, 0x00700, 0x00800, 0x00900, 0x00a00, 0x00b00,
    0x00c00, 0x00d00, 0x00e00, 0x00f00, 0x10200, 0x20400, 0x30700,
};
static const uint8_t drawn[NUM_PAGES] = { 0xf, 0x7, 0xf, 0xd, 0xf, 0xe, 0xb, 0xf, 0x8, 0x8, 0x2, 0x2, 0x5, 0x1, 0x4, 0x5, 0xf, 0xf, 0xf };

enum { NUM_TEX = 4 };
static const struct { const char *name; int x, y, size, fmt, mip; uint32_t base; } tex_info[NUM_TEX] = {
    { "VQ 565 64x64, index from 0x580000",                0,  0,  64, PixelFmt_565,  0, 0x57f800u },
    { "VQ 565 256x256 mipmapped, level 0 from 0x680556",  64, 0, 256, PixelFmt_565,  1, 0x67e800u },
    { "VQ palette 8 bpp 64x64, index from 0x500000",     320, 0,  64, PixelFmt_Pal8, 0, 0x4ff800u },
    { "VQ 565 128x128 mipmapped, level 0 from 0x77fd56", 384, 0, 128, PixelFmt_565,  1, 0x77f000u },
};

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
        for(e = 0; e < 256; e++) {
            const uint32_t w = tex_info[t].fmt == PixelFmt_565 ? (uint32_t)(e | (e ^ 0xff) << 8) * 0x10001u : (uint32_t)e * 0x01010101u;
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
            const int s = tex_info[t].size / 32;
            if(!(drawn[p] >> t & 1))
                continue;
            label_span(p, (tex_info[t].y / 32) * COLS + tex_info[t].x / 32, s, s, "TEXT_CONTROL %05x: %s (code book %06x)",
                       (unsigned)text_control[p], tex_info[t].name, (unsigned)tex_info[t].base);
        }
}

static int log2i(int s) { int k = 0; while((8 << k) < s) k++; return k; }

static void textured(int t) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Opaque;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Texture = 1;
    pcw.Gouraud = 1;
    isp.DepthMode = DepthMode_Always;
    isp.Texture = 1;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.ShadInstr = ShadInstr_Decal;
    tsp.FilterMode = FilterMode_Point;
    tsp.MipMapD = 4;
    tsp.TexU = (uint32_t)log2i(tex_info[t].size);
    tsp.TexV = tsp.TexU;
    tcw.PixelFmt = (uint32_t)tex_info[t].fmt;
    tcw.VQ_Comp = 1;
    tcw.MipMapped = (uint32_t)tex_info[t].mip;
    tcw.TexAddr = tex_info[t].base >> 3;
    ta_polygon(pcw, isp, tsp, tcw);
}

static void test_page(int page) {
    int t;
    pvrio_reg(TEXT_CONTROL_addr, text_control[page]);
    pvr_frame.bg_color = 0x000040;
    ta_list_begin(ListType_Opaque);
    for(t = 0; t < NUM_TEX; t++) {
        const int s = tex_info[t].size;
        if(!(drawn[page] >> t & 1))
            continue;
        textured(t);
        quad3(tex_info[t].x, tex_info[t].y, s, s, F32B_ONE, F32B_ZERO, F32B_ZERO, F32B_ONE, F32B_ONE, 0xffffffffu, 0);
    }
    ta_list_end();
}
