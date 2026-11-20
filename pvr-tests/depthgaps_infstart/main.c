/* A plane whose constant overflows, at a tile where the start does not cancel (2026-11-19, hunt depthgaps (b);
   after zovf, depthgaps_regb).

   zword: only the constant c overflows: the tile's first pixel 0, every other one the word UW of the block (a start
   of 0, stepped: depthgaps_negconv). There a x0 + b y0 overflows as well, with the other sign, and the start's sum
   cancels to 0. zovf's six cells where c is exactly 2^128 at the left edge of the screen (a x0 + b y0 finite) and
   depthgaps_regb's big triangles near the origin are above 0 everywhere instead. Here such triangles against
   backgrounds near +-2^127: textured green, decal, the page's mode; white background... no: drawn (green) where the
   triangle passes against the background.
   Cells (one tile each): G[k]. Pages: background B[p], mode M[p]. */
#define NUM_PAGES 10
#include "pvrtest2.h"
#include "texgen.h"

typedef struct { int x, y, legs, corner; uint32_t z, o; const char *name; } g_t;       /* corner 0 TL, 1 TR, 2 BL */
#define NG 12
static const g_t G[NG] = {
    { 0, 96, 32, 0, 0x79800000u, 0x3f000000u, "TL 2^116 at (0, 96) (c = 2^128)" },
    { 0, 0, 32, 0, 0x7a800000u, 0x3f000000u, "TL 2^118 at (0, 0)" },
    { 32, 0, 32, 0, 0xfa800000u, 0x3f000000u, "TL -2^118 at (32, 0)" },
    { 64, 0, 32, 0, 0x7b800000u, 0x3f000000u, "TL 2^120 at (64, 0)" },
    { 64, 448, 32, 0, 0x7b800000u, 0x3f000000u, "TL 2^120 at (64, 448) (zword: start 0)" },
    { 96, 0, 32, 0, 0x7b000000u, 0x3f000000u, "TL 2^119 at (96, 0)" },
    { 128, 0, 16, 0, 0x7a800000u, 0x3f000000u, "TL 2^118 legs 16 at (128, 0)" },
    { 0, 160, 32, 1, 0x7b000000u, 0x3f000000u, "TR 2^119 at (0, 160)" },
    { 160, 0, 32, 2, 0x7b000000u, 0x3f000000u, "BL 2^119 at (160, 0)" },
    { 192, 0, 32, 0, 0xfb000000u, 0x3f000000u, "TL -2^119 at (192, 0)" },
    { 224, 0, 32, 0, 0x7a800000u, 0xbf000000u, "TL 2^118 at (224, 0), others -0.5" },
    { 256, 0, 32, 0, 0x7a800000u, 0x42c80000u, "TL 2^118 at (256, 0), others 100" },
};
static const uint32_t B[NUM_PAGES] = { 0x7f000000u, 0x7f000000u, 0x7f000000u, 0x7effffe0u, 0x7f7fffe0u, 0xff000000u, 0xff000000u,
                                       0xff000000u, 0, 0x7e800000u };
static const int M[NUM_PAGES] = { DepthMode_Equal, DepthMode_Greater, DepthMode_Less, DepthMode_Greater, DepthMode_Greater,
                                  DepthMode_Equal, DepthMode_Greater, DepthMode_Less, DepthMode_Greater, DepthMode_Greater };
static const char *MN[8] = { "Never", "Less", "Equal", "LessEqual", "Greater", "NotEqual", "GreaterEqual", "Always" };
static uint32_t texG;

static uint32_t green(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0x07e0; }

static void test_setup(void)
{
    const tex_layout_t l = { TEX_TWIDDLED, 16, 8, 8, 0 };
    texG = tex_build(&l, green, NULL);
}

static void test_describe(void)
{
    int p, k;
    for(p = 0; p < NUM_PAGES; p++)
        for(k = 0; k < NG; k++)
            label(p, (G[k].y / 32) * 20 + G[k].x / 32, "%s; background %08x, %s", G[k].name, (unsigned)B[p], MN[M[p]]);
}

static void vtx(int x, int y, uint32_t z, int end)
{
    ta_vertex3(f32b_int(x), f32b_int(y), z, 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, end);
}

static void test_page(int page)
{
    int k;
    pvr_frame.bg_color = 0x000040;
    pvr_frame.bg_depth = B[page];
    ta_list_begin(ListType_Opaque);
    for(k = 0; k < NG; k++) {
        const g_t *g = &G[k];
        const int x = g->x, y = g->y, L = g->legs;
        PCW pcw = { .full = 0 };
        ISP_TSP isp = { .full = 0 };
        TSP tsp = { .full = 0 };
        TCW tcw = { .full = 0 };
        pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
        pcw.ListType = ListType_Opaque;
        pcw.Group_En = 1;
        pcw.Strip_Len = 1;
        pcw.Texture = 1;
        isp.DepthMode = M[page];
        isp.CullMode = CullMode_None;
        isp.Texture = 1;
        tsp.SrcInstr = BlendInstr_One;
        tsp.DstInstr = BlendInstr_Zero;
        tsp.FogCtrl = FogCtrl_None;
        tsp.UseAlpha = 1;
        tsp.ShadInstr = ShadInstr_Decal;
        tsp.FilterMode = FilterMode_Point;
        tsp.MipMapD = 4;
        tcw.PixelFmt = PixelFmt_565;
        tcw.TexAddr = texG >> 3;
        ta_polygon(pcw, isp, tsp, tcw);
        if(g->corner == 1) { vtx(x, y, g->o, 0); vtx(x + L, y, g->z, 0); vtx(x, y + L, g->o, 1); }
        else if(g->corner == 2) { vtx(x, y, g->o, 0); vtx(x + L, y, g->o, 0); vtx(x, y + L, g->z, 1); }
        else { vtx(x, y, g->z, 0); vtx(x + L, y, g->o, 0); vtx(x, y + L, g->o, 1); }
    }
    ta_list_end();
}
