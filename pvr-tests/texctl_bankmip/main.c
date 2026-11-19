/* TEXT_CONTROL's bank bits (12..8) on the smaller mipmap levels of VQ textures, point sampled, bilinear and
   trilinear (2026-11-19, TODO A3, after pvr-tests/texctl_bankbase: the index address read is
   ((start + offset) ^ (bank << 19)) + 2048, offset the index byte's place, mipmap offset and twiddled place; there
   only level 0 of the mipmapped textures was read).

   The video memory fill of texctl_bank (words that name their own place) and ten VQ 565 64x64 mipmapped textures
   over it whose code book entries show the index byte read (entry e: four texels e | (e ^ 0xff) << 8). Their index
   bytes are offsets 0 (1x1), 1 (2x2), 2-5 (4x4), 0x6-0x15 (8x8), 0x16-0x55 (16x16), 0x56-0x155 (32x32) and
   0x156-0x555 (64x64) from the start; each code book starts d bytes below a 512 KB line, so that start + offset
   crosses the line inside one level (through the twiddled place) and the levels above it lie wholly past the line
   (through the mipmap offset):
     T0 0x3ffff8 (0x400000 - 0x8, inside 8x8)     T1 0x4ffff0 (0x500000 - 0x10, inside 8x8)
     T2 0x57ffd0 (0x580000 - 0x30, inside 16x16)  T3 0x5fff00 (0x600000 - 0x100, inside 32x32)
     T4 0x67fc00 (0x680000 - 0x400, inside 64x64) T5 0x6fffb0 (0x700000 - 0x50, inside 16x16)
     T6 0x77fea8 (0x780000 - 0x158, inside 64x64) T7 0x37fab0 (0x380000 - 0x550, inside 64x64)
     T8 0x47ffa8 (0x480000 - 0x58, inside 32x32)  T9 0x2fffe0 (0x300000 - 0x20, inside 16x16)
   Each texture is drawn in 28 cells (32x32 quads, translucent list, ONE / ZERO, decal): point sampling, bilinear,
   trilinear pass A, trilinear pass B, each at 1.375 x 2^L texels of the full size a pixel (level L, as pvr-tests/mipbil),
   L = 0 (64x64) .. 6 (1x1); cell (t * 4 + filter) * 7 + L.
   Pages: TEXT_CONTROL = p << 8 on page p (bank value 0 .. 15), then 16 0x10900, 17 0x20b00, 18 0x30d00, 19 0x30100
   (the endian bits too). A page draws only the textures whose index reads all land in the filled memory (not the
   framebuffer, written while the texture is read; not past 8 MB, where a console reads data that changes from run to
   run): the bits of drawn[] (agents/nulldc3/hunt/texctl/plan_mip.py).
   Filled as texctl_bank: chip 0 0x12c000-0x1fffff, chip 1 0x002000-0x0fefff and 0x173000-0x1fffff, both chips
   0x200000-0x3fffff. */
#define NUM_PAGES 20
#include "pvrtest2.h"

static const uint32_t text_control[NUM_PAGES] = {
    0x00000, 0x00100, 0x00200, 0x00300, 0x00400, 0x00500, 0x00600, 0x00700, 0x00800, 0x00900, 0x00a00, 0x00b00,
    0x00c00, 0x00d00, 0x00e00, 0x00f00, 0x10900, 0x20b00, 0x30d00, 0x30100,
};
static const uint16_t drawn[NUM_PAGES] = {
    0x3ff, 0x1bf, 0x15f, 0x16e, 0x176, 0x17a, 0x17c, 0x07e, 0x2e0, 0x240, 0x090, 0x290, 0x286, 0x284, 0x381, 0x381,
    0x240, 0x290, 0x284, 0x1bf,
};

enum { NUM_TEX = 10, NUM_FILTERS = 4, NUM_LEVELS = 7 };
static const uint32_t tex_base[NUM_TEX] = {
    0x3ffff8u, 0x4ffff0u, 0x57ffd0u, 0x5fff00u, 0x67fc00u, 0x6fffb0u, 0x77fea8u, 0x37fab0u, 0x47ffa8u, 0x2fffe0u,
};
static const char *filter_name[NUM_FILTERS] = { "point", "bilinear", "trilinear A", "trilinear B" };
static const int filter_mode[NUM_FILTERS] = { FilterMode_Point, FilterMode_Bilinear, FilterMode_TrilinearA, FilterMode_TrilinearB };

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
    fill_chip(0, 0x12c000u, 0x200000u);
    fill_chip(1, 0x002000u, 0x0ff000u);
    fill_chip(1, 0x173000u, 0x200000u);
    fill64(0x400000u, 0x800000u);
    for(t = 0; t < NUM_TEX; t++) {              /* the code books: entry e, four texels e | ~e << 8 */
        for(e = 0; e < 256; e++)
            words[2 * e] = words[2 * e + 1] = (uint32_t)(e | (e ^ 0xff) << 8) * 0x10001u;
        pvrio_w64(tex_base[t], words, 512);
    }
}

static void test_describe(void) {
    int p, t, f, l;
    for(p = 0; p < NUM_PAGES; p++)
        for(t = 0; t < NUM_TEX; t++)
            if(drawn[p] >> t & 1)
                for(f = 0; f < NUM_FILTERS; f++)
                    for(l = 0; l < NUM_LEVELS; l++)
                        label(p, (t * NUM_FILTERS + f) * NUM_LEVELS + l, "TEXT_CONTROL %05x: VQ 565 64x64 mipmapped, code book %06x, %s, level %dx%d",
                              (unsigned)text_control[p], (unsigned)tex_base[t], filter_name[f], 64 >> l, 64 >> l);
}

static void textured(int t, int filter) {
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
    tsp.MipMapD = 4;
    tsp.TexU = 3;
    tsp.TexV = 3;
    tcw.PixelFmt = PixelFmt_565;
    tcw.VQ_Comp = 1;
    tcw.MipMapped = 1;
    tcw.TexAddr = tex_base[t] >> 3;
    ta_polygon(pcw, isp, tsp, tcw);
}

/* p / q texels of the full size (64) as a coordinate: exact */
static f32b coord(int32_t p, int32_t q) { return sf_ratio(p, q * 64); }

static void test_page(int page) {
    int t, f, l;
    pvrio_reg(TEXT_CONTROL_addr, text_control[page]);
    pvr_frame.bg_color = 0x000040;
    ta_list_begin(ListType_Translucent);
    for(t = 0; t < NUM_TEX; t++)
        if(drawn[page] >> t & 1)
            for(f = 0; f < NUM_FILTERS; f++) {
                textured(t, filter_mode[f]);
                for(l = 0; l < NUM_LEVELS; l++) {
                    /* 32 pixels of 1.375 x 2^L texels: 44 x 2^L texels, from 5/32, 7/32 of a texel */
                    const int cell = (t * NUM_FILTERS + f) * NUM_LEVELS + l;
                    const f32b u0 = coord(5, 32), v0 = coord(7, 32);
                    const f32b u1 = sf_add(u0, coord(44 << l, 1)), v1 = sf_add(v0, coord(44 << l, 1));
                    quad3(cell_px(cell), cell_py(cell), 32, 32, F32B_ONE, u0, v0, u1, v1, 0xffffffffu, 0);
                }
            }
    ta_list_end();
}
