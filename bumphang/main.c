/* A bump map that is not twiddled, filtered bilinearly, stops a console's render.

   Found by pvr_tests/sample: pages with such a texture never signalled the end of their render, and the
   tiles after some tile kept the picture of the page before. The harness waits 2 s, resets the core
   (SOFTRESET 2, then 0) and goes on; the log says "the render did not end".

   Every page draws one kind of bump quad in the tiles (kinds[]), shown through (SRC_ALPHA, ZERO), and in
   every tile a 4x4 marker in the page's colour. A tile whose marker has another colour was not written; the
   tiles are rendered column by column, so the first such tile is where the render stopped (analyze.py).

   What a console does (2026-11, the same in every run):
   - Only this combination stops it: bump map, scan order bit set (not twiddled), bilinear. Twiddled and VQ
     bump maps filtered bilinearly, and the same texture point sampled, render all 300 tiles; so do other
     texel formats that are not twiddled (pvr_tests/sample).
   - What is counted is the quads (two triangles each) of that kind that tiles have shaded: the render stops
     in the tile that holds the 16th. One 32x32, 32x8 or 8x32 quad per tile: tile 15. Four 16x16 quads per
     tile: tile 3. Quads in every second or third tile: the 16th of those tiles. Starting at tile 100: tile
     115. A 64x64 quad counts in each tile it covers (sample's wrap page: 20 tiles).
   - In the opaque and the punch-through list it is the 8th quad: tile 7.
   - The size on the screen, the magnification (8x to half size), the texel values and the light make no
     difference.
   - The count carries over from one render to the next (ten quads, then ten again: the second render stops
     at its sixth) and starts again after the core reset.
   Why is not known. refsw2 draws these pages to the end; nothing here is compared with it.

   shift: tile (x, y) samples u moved by (x - 8) / 8 pixel and v by (y - 7) / 8 pixel. */
#define KINDS 24
#define NUM_PAGES KINDS
#include "pvrtest2.h"
#include "texgen.h"

enum { T_A, T_FLAT };
enum { G_FULL, G_32x8, G_8x32, G_4x16, G_FIRST10, G_FROM100, G_EVERY2, G_EVERY3 };   /* which tiles, and what a tile holds */
/* mag: the texture is 2^mag times its size on the screen (2: the whole 8x8 texture over a tile) */
typedef struct { const char *name; int layout, ScanOrder, VQ, tex, filter, geom, mag, shift, list; uint32_t light; } kind_t;
#define TR ListType_Translucent
#define BIL FilterMode_Bilinear
static const kind_t kinds[KINDS] = {
    { "not twiddled, bilinear",                              TEX_RASTER,   1, 0, T_A,    BIL, G_FULL,     2, 0, TR, 0x00807f00u },
    { "not twiddled, bilinear, shifted",                     TEX_RASTER,   1, 0, T_A,    BIL, G_FULL,     2, 1, TR, 0x00807f00u },
    { "twiddled, bilinear, shifted",                         TEX_TWIDDLED, 0, 0, T_A,    BIL, G_FULL,     2, 1, TR, 0x00807f00u },
    { "VQ, bilinear, shifted",                               TEX_VQ,       0, 1, T_A,    BIL, G_FULL,     2, 1, TR, 0x00807f00u },
    { "not twiddled, bilinear, one texel value",             TEX_RASTER,   1, 0, T_FLAT, BIL, G_FULL,     2, 0, TR, 0x00807f00u },
    { "not twiddled, bilinear, light 0",                     TEX_RASTER,   1, 0, T_A,    BIL, G_FULL,     2, 0, TR, 0x00000000u },
    { "not twiddled, point",                                 TEX_RASTER,   1, 0, T_A,    FilterMode_Point, G_FULL, 2, 1, TR, 0x00807f00u },
    { "not twiddled, bilinear, a 32x8 quad per tile",        TEX_RASTER,   1, 0, T_A,    BIL, G_32x8,     2, 0, TR, 0x00807f00u },
    { "not twiddled, bilinear, an 8x32 quad per tile",       TEX_RASTER,   1, 0, T_A,    BIL, G_8x32,     2, 0, TR, 0x00807f00u },
    { "not twiddled, bilinear, four 16x16 quads per tile",   TEX_RASTER,   1, 0, T_A,    BIL, G_4x16,     2, 0, TR, 0x00807f00u },
    { "not twiddled, bilinear, the first 10 tiles only",     TEX_RASTER,   1, 0, T_A,    BIL, G_FIRST10,  2, 0, TR, 0x00807f00u },
    { "not twiddled, bilinear, the first 10 tiles only, again", TEX_RASTER, 1, 0, T_A,   BIL, G_FIRST10,  2, 0, TR, 0x00807f00u },
    { "not twiddled, bilinear, opaque list",                 TEX_RASTER,   1, 0, T_A,    BIL, G_FULL,     2, 0, ListType_Opaque, 0x00807f00u },
    { "not twiddled, bilinear, from tile 100 on",            TEX_RASTER,   1, 0, T_A,    BIL, G_FROM100,  2, 0, TR, 0x00807f00u },
    { "not twiddled, bilinear, every second tile",           TEX_RASTER,   1, 0, T_A,    BIL, G_EVERY2,   2, 0, TR, 0x00807f00u },
    { "not twiddled, bilinear, every third tile",            TEX_RASTER,   1, 0, T_A,    BIL, G_EVERY3,   2, 0, TR, 0x00807f00u },
    { "not twiddled, bilinear, texture at 8x",               TEX_RASTER,   1, 0, T_A,    BIL, G_FULL,     3, 0, TR, 0x00807f00u },
    { "not twiddled, bilinear, texture at 2x",               TEX_RASTER,   1, 0, T_A,    BIL, G_FULL,     1, 0, TR, 0x00807f00u },
    { "not twiddled, bilinear, a texel per pixel",           TEX_RASTER,   1, 0, T_A,    BIL, G_FULL,     0, 0, TR, 0x00807f00u },
    { "not twiddled, bilinear, texture at half size",        TEX_RASTER,   1, 0, T_A,    BIL, G_FULL,    -1, 0, TR, 0x00807f00u },
    { "twiddled, bilinear, light 80 40 80 40",               TEX_TWIDDLED, 0, 0, T_A,    BIL, G_FULL,     2, 1, TR, 0x80408040u },
    { "VQ, bilinear, light 80 40 80 40, a texel per pixel",  TEX_VQ,       0, 1, T_A,    BIL, G_FULL,     0, 1, TR, 0x80408040u },
    { "twiddled, bilinear, opaque list",                     TEX_TWIDDLED, 0, 0, T_A,    BIL, G_FULL,     2, 0, ListType_Opaque, 0x00807f00u },
    { "not twiddled, bilinear, punch-through list",          TEX_RASTER,   1, 0, T_A,    BIL, G_FULL,     2, 0, ListType_Punch_Through, 0x00807f00u },
};
static uint32_t txr[KINDS];

static uint32_t texel(const void *c, int size, int x, int y) {
    (void)size;
    return *(const int *)c == T_A ? (uint32_t)(((20 + 30 * x) << 8) | (30 * y + 5 * x)) : 0x8040u;   /* S, R: as pvr_tests/sample */
}

static uint32_t marker(int page) { return 0xff000000u | (uint32_t)(0x203040 + page * 0x0b0705); }

static void test_setup(void) {
    int k;
    for(k = 0; k < KINDS; k++) {
        const tex_layout_t l = { kinds[k].layout, 16, 8, 8, 0 };
        txr[k] = tex_build(&l, texel, &kinds[k].tex);
    }
}

static void test_describe(void) {
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < COLS * 15; c++)
            label(p, c, "%s", kinds[p].name);
}

/* the bump quad of a tile, or a part of it: w x h pixels at (x, y), texture coordinates from (u0, v0) */
static void bump_quad(const kind_t *k, int page, int x, int y, int w, int h, f32b u0, f32b v0) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };

    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = k->list;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Texture = 1;
    pcw.Gouraud = 1;
    pcw.Offset = 1;
    isp.DepthMode = DepthMode_Always;
    isp.ZWriteDis = 1;
    isp.Texture = 1;
    isp.Offset = 1;
    tsp.SrcInstr = BlendInstr_SrcAlpha;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    tsp.ShadInstr = ShadInstr_Decal;
    tsp.FilterMode = k->filter;
    tsp.MipMapD = 4;
    tcw.PixelFmt = PixelFmt_BumpMap;
    tcw.ScanOrder = k->ScanOrder;
    tcw.VQ_Comp = k->VQ;
    tcw.TexAddr = txr[page] >> 3;
    ta_polygon(pcw, isp, tsp, tcw);
    /* a pixel is 2^-mag texels of 8: 2^(-mag - 3) of the texture */
    quad3(x, y, w, h, F32B_ONE, u0, v0, sf_add(u0, f32b_scaled(w, -k->mag - 3)), sf_add(v0, f32b_scaled(h, -k->mag - 3)), 0xffffffff, k->light);
}

/* the marker of a tile: a 4x4 quad in the page's colour, translucent list */
static void marker_quad(int page, int x, int y) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };

    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Translucent;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    isp.DepthMode = DepthMode_Always;
    isp.ZWriteDis = 1;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    ta_polygon(pcw, isp, tsp, tcw);
    quad0(x, y, 4, 4, F32B_ONE, marker(page));
}

static void test_page(int page) {
    const kind_t *k = &kinds[page];
    int c;

    /* a list is sent once: the markers follow each tile's bump quad when that is in the translucent list too */
    ta_list_begin(k->list);
    for(c = 0; c < COLS * 15; c++) {
        const int tx = c % COLS, ty = c / COLS, x = tx * 32, y = ty * 32, n = tx * 15 + ty;    /* n: the tile's place in the render */
        const f32b su = k->shift ? f32b_scaled(tx - 8, -8) : F32B_ZERO, sv = k->shift ? f32b_scaled(ty - 7, -8) : F32B_ZERO;
        switch(k->geom) {
        case G_FULL:    bump_quad(k, page, x, y, 32, 32, su, sv); break;
        case G_32x8:    bump_quad(k, page, x, y + 12, 32, 8, su, sv); break;
        case G_8x32:    bump_quad(k, page, x + 12, y, 8, 32, su, sv); break;
        case G_4x16:    bump_quad(k, page, x, y, 16, 16, su, sv); bump_quad(k, page, x + 16, y, 16, 16, su, sv);
                        bump_quad(k, page, x, y + 16, 16, 16, su, sv); bump_quad(k, page, x + 16, y + 16, 16, 16, su, sv); break;
        case G_FIRST10: if(n < 10) bump_quad(k, page, x, y, 32, 32, su, sv); break;
        case G_FROM100: if(n >= 100) bump_quad(k, page, x, y, 32, 32, su, sv); break;
        case G_EVERY2:  if(n % 2 == 0) bump_quad(k, page, x, y, 32, 32, su, sv); break;
        default:        if(n % 3 == 0) bump_quad(k, page, x, y, 32, 32, su, sv); break;
        }
        if(k->list == ListType_Translucent)
            marker_quad(page, x, y);
    }
    ta_list_end();
    if(k->list != ListType_Translucent) {
        ta_list_begin(ListType_Translucent);
        for(c = 0; c < COLS * 15; c++)
            marker_quad(page, (c % COLS) * 32, (c / COLS) * 32);
        ta_list_end();
    }
}
