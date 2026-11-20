/* The overflowing depth register (regime C) with HALF_OFFSET bit 0 off (2026-11-20, hunt depth2).

   pvr_isp_depth.h GAP: when a slope overflows, every pixel stores a word of the register r = r0 + (sa i + sb j)
   2^127 + i fa + j fb (zword, nantr_*, zovf_creg); r0 is the plane's start at the tile's first pixel CENTER and the
   steps count whole pixels from it. refsw3 does the same when the ISP samples at pixel corners (HALF_OFFSET bit 0
   clear), not measured: the register could start at the corner (in tile (0, 0) an infinite slope times x0 = 0 is 0
   there, not an infinity: zword's TL +inf start would be +1 step, zovf_creg's -1 step) or step from half a pixel
   back (half steps of 2^126). Here such triangles with the register on both settings, against background probes:
     tile (0, 0), the page's variant: zword's TL +inf (legs 32), zovf_creg's C0 (B (0, 0) at -2^118, R (32, 0) at
       2^123: a = +inf, D (0, 32): B = 2^116 a row), nantr_fin2's (V0 (16, 0) at +inf, V1 (0, 0), V2 (16, 16) at 2^119)
     tile (2, 0) zword TL +inf, (4, 0) TR +inf, (6, 0) TL -inf, (8, 0) zovf_creg C0 there (legs 32 each)
     controls (ordinary planes): tile (10, 0) a ramp along x crossing 1 at x = 16.25 from the tile's left, (12, 0)
       along y, both over a flat quad at 1 (Always), Greater
   Pages: variant v (3) x HALF_OFFSET 7, 6 x probe (7): page = 14 v + 7 h + p; probes: 0 Greater, 0 Less, 2^127
   Equal, -2^127 Equal, 2^120 Greater, the largest float Greater, -2^126 Greater. */
#define NUM_PAGES 42
#include "pvrtest2.h"
#include "texgen.h"

static const uint32_t PROBE[7] = { 0, 0, 0x7f000000u, 0xff000000u, 0x7b800000u, 0x7f7fffffu, 0xfe800000u };
static const int PMODE[7] = { DepthMode_Greater, DepthMode_Less, DepthMode_Equal, DepthMode_Equal, DepthMode_Greater, DepthMode_Greater, DepthMode_Greater };
static const char *PNAME[7] = { "0 Greater", "0 Less", "2^127 Equal", "-2^127 Equal", "2^120 Greater", "max Greater", "-2^126 Greater" };
static const char *VNAME[3] = { "zword TL +inf", "zovf_creg C0, B 2^116", "nantr_fin2 Z 2^119" };
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
    int p;
    for(p = 0; p < NUM_PAGES; p++) {
        const int v = p / 14, h = (p / 7) % 2, q = p % 7;
        label(p, 0, "HALF_OFFSET %d, %s at tile (0, 0), probe %s", 7 - h, VNAME[v], PNAME[q]);
        label(p, 2, "HALF_OFFSET %d, zword TL +inf at (64, 0), probe %s", 7 - h, PNAME[q]);
        label(p, 4, "HALF_OFFSET %d, zword TR +inf at (128, 0), probe %s", 7 - h, PNAME[q]);
        label(p, 6, "HALF_OFFSET %d, zword TL -inf at (192, 0), probe %s", 7 - h, PNAME[q]);
        label(p, 8, "HALF_OFFSET %d, zovf_creg C0 at (256, 0), probe %s", 7 - h, PNAME[q]);
        label(p, 10, "HALF_OFFSET %d, ramp along x over a flat 1, Greater", 7 - h);
        label(p, 12, "HALF_OFFSET %d, ramp along y over a flat 1, Greater", 7 - h);
    }
}

static void begin(int mode, uint32_t tex, int list)
{
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = list;
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

/* zword's triangle: the right-angle corner at (x, y), legs 32; tr: the far corner (x + 32, y) at s instead */
static void zword(int x, int y, int tr, uint32_t s, int mode)
{
    const uint32_t half = 0x3f000000u;
    begin(mode, texW, ListType_Opaque);
    if(tr) { vtx(x, y, half, 0); vtx(x + 32, y, s, 0); vtx(x, y + 32, half, 1); }
    else { vtx(x, y, s, 0); vtx(x + 32, y, half, 0); vtx(x, y + 32, half, 1); }
}

/* zovf_creg's C0: B (x, y) at -2^118, R (x + 32, y) at 2^123 (a = 2^128: +inf), D (x, y + 32) at -2^118 + 32 2^116 */
static void creg(int x, int y, int mode)
{
    begin(mode, texW, ListType_Opaque);
    vtx(x, y, 0xfa800000u, 0); vtx(x + 32, y, 0x7d000000u, 0); vtx(x, y + 32, 0x7be00000u, 1);
}

static void test_page(int page)
{
    const int v = page / 14, h = (page / 7) % 2, q = page % 7, mode = PMODE[q];
    int k;
    pvrio_reg(HALF_OFFSET_addr, h ? 6 : 7);
    pvr_frame.bg_color = 0x000040;
    pvr_frame.bg_depth = PROBE[q];
    ta_list_begin(ListType_Opaque);
    /* the controls: flat quads at 1 first */
    for(k = 0; k < 2; k++) {
        begin(DepthMode_Always, texG, ListType_Opaque);
        quad3(320 + 64 * k, 0, 32, 32, 0x3f800000u, 0x3e800000u, 0x3e800000u, 0x3e800000u, 0x3e800000u, 0xffffffffu, 0);
    }
    if(v == 0)
        zword(0, 0, 0, 0x7f800000u, mode);
    else if(v == 1)
        creg(0, 0, mode);
    else {
        begin(mode, texW, ListType_Opaque);
        vtx(16, 0, 0x7f800000u, 0); vtx(0, 0, 0x7b000000u, 0); vtx(16, 16, 0x7b000000u, 1);
    }
    zword(64, 0, 0, 0x7f800000u, mode);
    zword(128, 0, 1, 0x7f800000u, mode);
    zword(192, 0, 0, 0xff800000u, mode);
    creg(256, 0, mode);
    /* ramps: 1 - 65 2^-12 at the tile's left (top), 1 + 63 2^-12 at its right (bottom): 1 at 16.25 */
    begin(DepthMode_Greater, texW, ListType_Opaque);
    for(k = 0; k < 4; k++)
        ta_vertex3(f32b_int(320 + 32 * (k & 1)), f32b_int(32 * (k >> 1)), (k & 1) ? 0x3f81f800u : 0x3f7bf000u, 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, k == 3);
    begin(DepthMode_Greater, texW, ListType_Opaque);
    for(k = 0; k < 4; k++)
        ta_vertex3(f32b_int(384 + 32 * (k & 1)), f32b_int(32 * (k >> 1)), (k >> 1) ? 0x3f81f800u : 0x3f7bf000u, 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, k == 3);
    ta_list_end();
}
