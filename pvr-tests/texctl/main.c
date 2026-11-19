/* TEXT_CONTROL's other bits (2026-11-19, TODO A3): code book endian (bit 17), index endian (bit 16) and the bank
   bit (bits 12..8). Every game and test writes 0 (or a stride in bits 4..0); refsw3 reads only the stride.

   Every page draws the same seven textures, 1:1 and point sampled, each texel's colour its address in the texture
   (decal, opaque), with TEXT_CONTROL set to the page's value:
     565 raster 256x256 at (0, 0)       each 16 bit word holds its own word address (0 .. 65535): the bank bit's
                                         remapping of address bits, if any
     565 twiddled 64x64 at (320, 0)     each word its twiddled word address: whether the bits touch 16 bit texels
     palette 8 bpp 64x64 at (384, 0)    each byte its byte address & 255 (palette entry i: red i): index endian
     palette 4 bpp 64x64 at (448, 0)    each nibble its nibble address & 15 (texel 0 the low nibble)
     VQ 565 64x64 at (320, 64)          code book byte at offset a: entry e = a / 8, o = a & 7; even bytes (e & 31) << 3 | o,
                                         odd bytes (e >> 5) << 3 | o (each byte names its place in the entry); index
                                         byte at offset o: o & 255
     VQ palette 8 bpp 64x64 at (384, 64)  code book byte a: a & 255; index byte o: o & 255
     VQ palette 4 bpp 64x64 at (448, 64)  code book nibble k of every entry: k; index byte o: o & 255
   Pages (TEXT_CONTROL):
     0  0x00000    1  0x10000 (index endian)    2  0x20000 (code book endian)    3  0x30000 (both)
     4  0x00000 (again, after the endian pages)
     5  0x00100    6  0x00400    7  0x00800    8  0x00200    9  0x00300    10  0x00700   (bank value 1, 4, 8, 2, 3, 7)
     11 0x30200 (both endians, bank value 2)
   (Bank values 16 and up read VQ index bytes past 8 MB, data that differs from run to run: pvr-tests/texctl_bank.)
   The bank pages come last: a setting that stops the core loses only them. */
#define NUM_PAGES 12
#include "pvrtest2.h"

static const uint32_t text_control[NUM_PAGES] = {
    0x00000, 0x10000, 0x20000, 0x30000, 0x00000, 0x00100, 0x00400, 0x00800, 0x00200, 0x00300, 0x00700, 0x30200,
};

enum { T_RASTER, T_TW565, T_PAL8, T_PAL4, T_VQ565, T_VQPAL8, T_VQPAL4, NUM_TEX };
static const struct { const char *name; int x, y, size, fmt, raster, vq; } tex_info[NUM_TEX] = {
    { "565 raster 256x256, word = its word address",         0,   0, 256, PixelFmt_565,  1, 0 },
    { "565 twiddled 64x64, word = its word address",       320,   0,  64, PixelFmt_565,  0, 0 },
    { "palette 8 bpp 64x64, byte = its byte address",      384,   0,  64, PixelFmt_Pal8, 0, 0 },
    { "palette 4 bpp 64x64, nibble = its nibble address",  448,   0,  64, PixelFmt_Pal4, 0, 0 },
    { "VQ 565 64x64, code book bytes name their place",    320,  64,  64, PixelFmt_565,  0, 1 },
    { "VQ palette 8 bpp 64x64, code book byte = a & 255",  384,  64,  64, PixelFmt_Pal8, 0, 1 },
    { "VQ palette 4 bpp 64x64, code book nibble k = k",    448,  64,  64, PixelFmt_Pal4, 0, 1 },
};
static uint32_t tex_addr[NUM_TEX];

static uint8_t buf[256 * 256 * 2];

static uint32_t upload(int bytes) {
    const uint32_t at = pvr_tex_alloc(bytes);
    pvr_tex_load(at, buf, bytes);
    return at;
}

static void test_setup(void) {
    int i, x, y;
    pvr_pal_format(PAL_RAM_CTRL_ARGB8888);
    for(i = 0; i < 256; i++)                    /* entry i: red i, green i ^ 0xa5 (a check), blue 0x3c */
        pvr_pal_entry(i, 0xff000000u | (uint32_t)i << 16 | (uint32_t)(i ^ 0xa5) << 8 | 0x3cu);
    for(i = 0; i < 65536; i++) {                /* 565 raster: word i = i */
        buf[2 * i] = (uint8_t)i;
        buf[2 * i + 1] = (uint8_t)(i >> 8);
    }
    tex_addr[T_RASTER] = upload(256 * 256 * 2);
    for(y = 0; y < 64; y++)                     /* 565 twiddled: the word at twiddled place w = w */
        for(x = 0; x < 64; x++) {
            const uint32_t w = tw_index((uint32_t)x, (uint32_t)y);
            buf[2 * w] = (uint8_t)w;
            buf[2 * w + 1] = (uint8_t)(w >> 8);
        }
    tex_addr[T_TW565] = upload(64 * 64 * 2);
    for(i = 0; i < 4096; i++)                   /* 8 bpp: byte a = a & 255 */
        buf[i] = (uint8_t)i;
    tex_addr[T_PAL8] = upload(4096);
    for(i = 0; i < 2048; i++)                   /* 4 bpp: nibble n = n & 15, texel 0 the low nibble */
        buf[i] = (uint8_t)(((2 * i) & 15) | (((2 * i + 1) & 15) << 4));
    tex_addr[T_PAL4] = upload(2048);
    for(i = 0; i < 2048; i++) {                 /* VQ 565: the code book bytes name their place */
        const int e = i >> 3, o = i & 7;
        buf[i] = (uint8_t)((o & 1) ? ((e >> 5) << 3 | o) : ((e & 31) << 3 | o));
    }
    for(i = 0; i < 1024; i++)                   /* 32 x 32 index bytes */
        buf[2048 + i] = (uint8_t)i;
    tex_addr[T_VQ565] = upload(2048 + 1024);
    for(i = 0; i < 2048; i++)                   /* VQ 8 bpp: code book byte a = a & 255; 512 index bytes (2x4 texels each) */
        buf[i] = (uint8_t)i;
    for(i = 0; i < 512; i++)
        buf[2048 + i] = (uint8_t)i;
    tex_addr[T_VQPAL8] = upload(2048 + 512);
    for(i = 0; i < 2048; i++)                   /* VQ 4 bpp: nibble k of every entry = k; 256 index bytes (4x4 texels each) */
        buf[i] = (uint8_t)(((2 * (i & 7)) & 15) | (((2 * (i & 7) + 1) & 15) << 4));
    for(i = 0; i < 256; i++)
        buf[2048 + i] = (uint8_t)i;
    tex_addr[T_VQPAL4] = upload(2048 + 256);
}

static void test_describe(void) {
    int p, t;
    for(p = 0; p < NUM_PAGES; p++)
        for(t = 0; t < NUM_TEX; t++) {
            const int s = tex_info[t].size / 32;
            label_span(p, (tex_info[t].y / 32) * COLS + tex_info[t].x / 32, s, s, "TEXT_CONTROL %05x: %s", (unsigned)text_control[p], tex_info[t].name);
        }
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
    tsp.TexU = tex_info[t].size == 256 ? 5 : 3;
    tsp.TexV = tsp.TexU;
    tcw.PixelFmt = tex_info[t].fmt;
    tcw.ScanOrder = tex_info[t].raster;
    tcw.VQ_Comp = tex_info[t].vq;
    tcw.TexAddr = tex_addr[t] >> 3;             /* palette textures: PalSelect 0 */
    ta_polygon(pcw, isp, tsp, tcw);
}

static void test_page(int page) {
    int t;
    pvrio_reg(TEXT_CONTROL_addr, text_control[page]);
    pvr_frame.bg_color = 0x000040;
    ta_list_begin(ListType_Opaque);
    for(t = 0; t < NUM_TEX; t++) {
        const int s = tex_info[t].size;
        textured(t);
        quad3(tex_info[t].x, tex_info[t].y, s, s, F32B_ONE, F32B_ZERO, F32B_ZERO, F32B_ONE, F32B_ONE, 0xffffffffu, 0);
    }
    ta_list_end();
}
