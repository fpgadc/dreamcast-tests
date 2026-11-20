/* hugepos_cov: which pixels the huge triangles of pvr-tests/hugepos COVER, whatever their colours (2026-11-19).
   hugepos draws them white (opaque) or in one channel each (translucent, added), so a pixel the edges take in but
   whose TSP colour comes out black (garbage 1/w at huge determinants) cannot be told from one outside. Here every
   triangle is textured with an 8x8 texture that is white everywhere, ShadInstr decal (the texel alone), point
   sampled, so every pixel it covers is white, whatever its planes are. One triangle per page, opaque, depth
   compare always, no culling, background black. With M = n 2^e:
     a  (-4M, -3M) (4M, 3M) (-3M, 4M)
     b  (-64, -64) (704, 0) (-3M, 4M)
     c  (-4M, 100) (4M, 380) (320, -1000)
   for M = 2^114, 2^116, .., 2^124, 2^125 and 1.25, 1.5, 1.75 times 2^125 (4M below 2^128: every corner finite;
   from 2^125 on 4M - (-4M), a coordinate difference, is 2^128 or more). */
#define NUM_PAGES 30
#include "pvrtest2.h"
#include "texgen.h"

static const int TRI[NUM_PAGES] = { 0,0,0,0,0,0,0,0,0,0, 1,1,1,1,1,1,1,1,1,1, 2,2,2,2,2,2,2,2,2,2 };
static const int MN[10] = { 1, 1, 1, 1, 1, 1, 1, 5, 3, 7 };
static const int ME[10] = { 114, 116, 118, 120, 122, 124, 125, 123, 124, 123 };
static uint32_t tex;

static uint32_t white(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0xffffu; }

static void test_setup(void) {
    tex_layout_t l = { TEX_RASTER, 16, 8, 8, 0 };
    tex = tex_build(&l, white, NULL);
}

static void test_describe(void) {
    int p;
    for(p = 0; p < NUM_PAGES; p++)
        label_span(p, 0, COLS, 15, "%c, M = %d 2^%d", "abc"[TRI[p]], MN[p % 10], ME[p % 10]);
}

static void tri(const f32b xy[3][2]) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    int k;

    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Opaque;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Texture = 1;
    isp.DepthMode = DepthMode_Always;
    isp.CullMode = CullMode_None;
    isp.ZWriteDis = 1;
    isp.Texture = 1;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    tsp.ShadInstr = ShadInstr_Decal;
    tsp.FilterMode = FilterMode_Point;
    tsp.MipMapD = 4;
    tsp.TexU = 0;
    tsp.TexV = 0;
    tcw.PixelFmt = PixelFmt_565;
    tcw.ScanOrder = 1;
    tcw.TexAddr = tex >> 3;
    ta_polygon(pcw, isp, tsp, tcw);
    for(k = 0; k < 3; k++)
        ta_vertex3(xy[k][0], xy[k][1], F32B_ONE, 0, 0, 0xffffffffu, 0, k == 2);
}

static void test_page(int page) {
    f32b m = f32b_scaled(MN[page % 10], ME[page % 10]);     /* M, exact */
    f32b m3 = sf_mul(f32b_int(3), m), m4 = sf_mul(f32b_int(4), m);      /* exact */
    f32b a[3][2] = { { sf_neg(m4), sf_neg(m3) }, { m4, m3 }, { sf_neg(m3), m4 } };
    f32b b[3][2] = { { f32b_int(-64), f32b_int(-64) }, { f32b_int(704), 0 }, { sf_neg(m3), m4 } };
    f32b c[3][2] = { { sf_neg(m4), f32b_int(100) }, { m4, f32b_int(380) }, { f32b_int(320), f32b_int(-1000) } };

    ta_list_begin(ListType_Opaque);
    tri(TRI[page] == 0 ? a : TRI[page] == 1 ? b : c);
    ta_list_end();
}
