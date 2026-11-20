/* Two leftovers of the stored depth (2026-11-19, hunt depthgaps (b), (c); after depthgaps_uwmat, depthgaps_infstart).

   G cells: the grid of a depth below zero outside its block's window (depthgaps_uwmat: ordered to steps of
   2^(E - 6)). A ramp (DepthMode Always, textured green), then the same ramp a little higher (white) over it,
   Greater on even pages... no: the cell's mode: white where the second passes.
     ramp(S, A): a quad over the cell, depth S at the tile's first pixel center, A a pixel to the right
     G0, G1: ramp(-1, -1) then ramp(-1 + 2^-10, -1) (above the window of E 6: 2^(E - 16) = 2^-10), Greater / Equal
     G2, G3: ramp(-1, -1) then ramp(-1 + 2^-11, -1), Greater / Equal
     G4, G5: ramp(-127.75, -1) then ramp(-127.75 + 2^-9, -1) (below the window of E 7 from the second pixel)
     G6, G7: ramp(-127.75, -1) then ramp(-127.75 + 2^-10, -1)
   X cells: a triangle whose a x0 + b y0 overflows alone (its constant finite): corners (0, 0) 0.5, (640, 0) Z,
   (0, 640) 0.5, so a = 640 Z and the bad corner's cross product is 0: in the tiles from x = 128 on a x0 passes 2^128
   (Z = +-2^112). Rows 5 and 6 of the screen, Z = 2^112 and -2^112 (two triangles, one per row band: drawn clipped
   to their rows by a second... no: each triangle covers the rows below it; row 5 is drawn with Z = 2^112 alone on
   even pages, -2^112 on odd ones.) Pages: background B[p], mode M[p], X with Z = (p & 1 ? -1 : 1) 2^112. */
#define NUM_PAGES 6
#include "pvrtest2.h"
#include "texgen.h"

static const uint32_t B[NUM_PAGES] = { 0x7f000000u, 0xff000000u, 0x7f000000u, 0xff000000u, 0x7f000000u, 0xff000000u };
static const int MD[NUM_PAGES] = { DepthMode_Equal, DepthMode_Equal, DepthMode_Greater, DepthMode_Greater, DepthMode_Less,
                                   DepthMode_Less };
static const char *MN[8] = { "Never", "Less", "Equal", "LessEqual", "Greater", "NotEqual", "GreaterEqual", "Always" };
typedef struct { int32_t s1, s2, a; int e; int mode; const char *name; } g_t;     /* S = s 2^e, A = a 2^e */
#define NG 8
static const g_t G[NG] = {
    { -1024, -1023, -1024, -10, DepthMode_Greater, "ramp(-1, -1), then from -1 + 2^-10, Greater" },
    { -1024, -1023, -1024, -10, DepthMode_Equal, "ramp(-1, -1), then from -1 + 2^-10, Equal" },
    { -2048, -2047, -2048, -11, DepthMode_Greater, "ramp(-1, -1), then from -1 + 2^-11, Greater" },
    { -2048, -2047, -2048, -11, DepthMode_Equal, "ramp(-1, -1), then from -1 + 2^-11, Equal" },
    { -65408, -65407, -512, -9, DepthMode_Greater, "ramp(-127.75, -1), then from -127.75 + 2^-9, Greater" },
    { -65408, -65407, -512, -9, DepthMode_Equal, "ramp(-127.75, -1), then from -127.75 + 2^-9, Equal" },
    { -130816, -130815, -1024, -10, DepthMode_Greater, "ramp(-127.75, -1), then from -127.75 + 2^-10, Greater" },
    { -130816, -130815, -1024, -10, DepthMode_Equal, "ramp(-127.75, -1), then from -127.75 + 2^-10, Equal" },
};
static uint32_t texG, texW;

static uint32_t green(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0x07e0; }
static uint32_t white(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0xffff; }

static void test_setup(void)
{
    const tex_layout_t l = { TEX_TWIDDLED, 16, 8, 8, 0 };
    texG = tex_build(&l, green, NULL);
    texW = tex_build(&l, white, NULL);
}

static void test_describe(void)
{
    int p, k;
    for(p = 0; p < NUM_PAGES; p++) {
        for(k = 0; k < NG; k++)
            label(p, 21 + 2 * k, "%s", G[k].name);
        for(k = 0; k < 20; k++)
            label(p, 100 + k, "X: corners (0, 0) 0.5, (640, 0) %s2^112, (0, 640) 0.5, tile (%d, 5); background %08x, %s",
                  p & 1 ? "-" : "", k, (unsigned)B[p], MN[MD[p]]);
    }
}

static void begin(int mode, uint32_t tex)
{
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Opaque;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Texture = 1;
    isp.DepthMode = mode;
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
    tcw.TexAddr = tex >> 3;
    ta_polygon(pcw, isp, tsp, tcw);
}

static void vtx(int x, int y, uint32_t z, int end)
{
    ta_vertex3(f32b_int(x), f32b_int(y), z, 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, end);
}

/* corner (dx, dy) of the cell: S + A (dx - 0.5) = (2 s + a (2 dx - 1)) 2^(e - 1) */
static void ramp(int x, int y, int32_t s, int32_t a, int e, int mode, uint32_t tex)
{
    int k;
    begin(mode, tex);
    for(k = 0; k < 4; k++) {
        const int dx = (k & 1) * 32, dy = (k >> 1) * 32;
        vtx(x + dx, y + dy, f32b_scaled(2 * s + a * (2 * dx - 1), e - 1), k == 3);
    }
}

static void test_page(int page)
{
    int k;
    pvr_frame.bg_color = 0x000040;
    pvr_frame.bg_depth = B[page];
    ta_list_begin(ListType_Opaque);
    for(k = 0; k < NG; k++) {
        const int c = 21 + 2 * k;
        ramp(cell_px(c), cell_py(c), G[k].s1, G[k].a, G[k].e, DepthMode_Always, texG);
        ramp(cell_px(c), cell_py(c), G[k].s2, G[k].a, G[k].e, G[k].mode, texW);
    }
    /* X: the triangle covers rows 5 .. 14 from the left; only row 5 is labelled */
    begin(MD[page], texG);
    vtx(0, 160, 0x3f000000u, 0); vtx(640, 160, (page & 1) ? 0xf7800000u : 0x77800000u, 0); vtx(0, 800, 0x3f000000u, 1);
    ta_list_end();
}
