/* TEXT_CONTROL's bank bits (12..8): where a VQ texture's index bytes are read from (2026-11-19, TODO A3, after
   pvr-tests/texctl: only VQ reads move, to far places).

   Video memory that nothing else uses is filled with words that name their own place: the 32 bit word at byte
   offset W of chip c (the 64 bit area's even words are chip 0, odd words chip 1) holds bytes
   b0 = N & 255, b1 = N >> 8 & 255, b2 = N >> 16 & 15 | c << 4 | 0x80, b3 = (b0 + 3 b1 + 5 b2 + 0x5b) & 255 (a check),
   N = W / 4. Filled: chip 0 0x12c000-0x1fffff (after the framebuffer), chip 1 0x002000-0x0fefff (the TA's
   parameter area past what it writes here) and 0x173000-0x1fffff (past the object lists and the region array),
   both chips 0x200000-0x3fffff (the 64 bit area 0x400000-0x7fffff). Four VQ 565 textures over it (a code book,
   then the index bytes, the pattern itself); code book entry e: all four texels e | (e ^ 0xff) << 8, so every
   texel shows the index byte read; with the bank bits 0 the index words name their own places, otherwise the
   places they were read from (agents/nulldc3/hunt/texctl/vqdecode.py and bankfit.py decode them):
     A 256x256, code book 0x465800 (index from 0x466000)   (0, 0)
     B 256x256, code book 0x5e5800 (index from 0x5e6000)   (320, 0)
     C 128x128, code book 0x665800 (index from 0x666000)   (0, 288)
     D 128x128, code book 0x7e5800 (index from 0x7e6000)   (320, 288)
   Page p: TEXT_CONTROL = p << 8 (bank value p, 0 .. 15). A page draws only the textures whose index reads all
   land in the filled memory (the bank bits move them by multiples of 512 KB; into the framebuffer, which is being
   written while the texture is read, or past 8 MB, where a console reads data that changes from run to run, they
   would show nothing that can be compared): all four on pages 0-7, then 8: D, 9-10: C, D, 11: C, 12: B,
   13-14: A, B, 15: A. Bank values 16-31 (bit 12) read past 8 MB from every texture (an earlier version of this
   test: those pages differed between two console runs). */
#define NUM_PAGES 16
#include "pvrtest2.h"

enum { NUM_TEX = 4 };
static const struct { int x, y, size; uint32_t base; } tex_info[NUM_TEX] = {
    { 0, 0, 256, 0x465800u }, { 320, 0, 256, 0x5e5800u }, { 0, 288, 128, 0x665800u }, { 320, 288, 128, 0x7e5800u },
};
static const uint8_t drawn[NUM_PAGES] = { 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0x8, 0xc, 0xc, 0x4, 0x2, 0x3, 0x3, 0x1 };

static uint32_t pat(int c, uint32_t W) {
    const uint32_t N = W >> 2;
    const uint32_t b0 = N & 255, b1 = (N >> 8) & 255, b2 = ((N >> 16) & 15) | (uint32_t)c << 4 | 0x80;
    const uint32_t b3 = (b0 + 3 * b1 + 5 * b2 + 0x5b) & 255;
    return b0 | b1 << 8 | b2 << 16 | b3 << 24;
}

static uint32_t words[2048];

/* chip c, chip byte offsets [from, to): through the 32 bit area (chip 1 is 0x400000 up) */
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

/* the 64 bit area [from, to) */
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
        pvrio_w64(tex_info[t].base, words, 512);
    }
}

static void test_describe(void) {
    int p, t;
    for(p = 0; p < NUM_PAGES; p++)
        for(t = 0; t < NUM_TEX; t++)
            if(drawn[p] >> t & 1)
                label_span(p, (tex_info[t].y / 32) * COLS + tex_info[t].x / 32, tex_info[t].size / 32, tex_info[t].size / 32,
                           "TEXT_CONTROL %05x: VQ 565 %dx%d, code book %06x, the index bytes read", (unsigned)(p << 8),
                           tex_info[t].size, tex_info[t].size, (unsigned)tex_info[t].base);
}

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
    tsp.TexU = tex_info[t].size == 256 ? 5 : 4;
    tsp.TexV = tsp.TexU;
    tcw.PixelFmt = PixelFmt_565;
    tcw.VQ_Comp = 1;
    tcw.TexAddr = tex_info[t].base >> 3;
    ta_polygon(pcw, isp, tsp, tcw);
}

static void test_page(int page) {
    int t;
    pvrio_reg(TEXT_CONTROL_addr, (uint32_t)page << 8);
    pvr_frame.bg_color = 0x000040;
    ta_list_begin(ListType_Opaque);
    for(t = 0; t < NUM_TEX; t++)
        if(drawn[page] >> t & 1) {
            const int s = tex_info[t].size;
            textured(t);
            quad3(tex_info[t].x, tex_info[t].y, s, s, F32B_ONE, F32B_ZERO, F32B_ZERO, F32B_ONE, F32B_ONE, 0xffffffffu, 0);
        }
    ta_list_end();
}
