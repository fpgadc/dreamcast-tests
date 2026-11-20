/* Edges of the overflow rules and of the stored depth (2026-11-19, hunt depthgaps (a), (b), (d); after
   depthgaps_ties, depthgaps_infstart, depthgaps_mm).

   Cells (one tile each; textured green, decal; a second polygon textured white):
     T: where a slope stops stepping as a number and the plane wraps (depthgaps_ties: 1.5 2^118 a pixel wraps,
        zword's 2^115 does not): a triangle with legs L, its top right corner at Z, the others at 0.5, so the slope
        is Z / L a pixel (a = L Z); its constant and start cancel (start 0). Against a background of 2^127, Greater:
        a wrapping plane stores m = i, above 2^127 from i = 2; one that steps from 0 stays far below.
     N: a start of -infinity (the constant past -2^128, a x0 + b y0 finite: top left corner at the left of the screen).
     M: zword's triangle (+inf top left, m = -(i + j): m = 16, the lowest of its words, at i + j = 16), then a flat
        quad at -2^-100 / -1.5 2^-126 with the cell's mode.
     D: a ramp from 2^-124 at the tile's first pixel, -1.75 2^-126 a pixel to the right, +2^-126 down: depths of
        multiples of 2^-128 around 0, some between 0 and 2^-126 (a float's denormals); alone against the background,
        and with a flat quad at 2^-126 (exactly: a power of two determinant) over it in the cell's mode.
   Pages: the background B[p] and mode M[p] for the cells drawn against the background. */
#define NUM_PAGES 7
#include "pvrtest2.h"
#include "texgen.h"

enum { TR, TL, RAMP, PAIR_C0, PAIR_RAMP };
typedef struct { int kind, legs; uint32_t z; int mode2; const char *name; } cell_t;
#define NC 20
static const cell_t C[NC] = {
    { TR, 16, 0x7e7fffffu, 0, "T: TR legs 16, Z = (2 - 2^-23) 2^121: slope just under 2^118" },
    { TR, 16, 0x7e800000u, 0, "T: TR legs 16, Z = 2^122: slope 2^118" },
    { TR, 16, 0x7e400000u, 0, "T: TR legs 16, Z = 1.5 2^121: slope 1.5 2^117" },
    { TR, 8, 0x7dffffffu, 0, "T: TR legs 8, Z = (2 - 2^-23) 2^120: slope just under 2^118" },
    { TR, 8, 0x7e000000u, 0, "T: TR legs 8, Z = 2^121: slope 2^118" },
    { TR, 32, 0x7effffffu, 0, "T: TR legs 32, Z = (2 - 2^-23) 2^122: slope just under 2^118" },
    { TR, 32, 0x7f000000u, 0, "T: TR legs 32, Z = 2^123: a = 2^128" },
    { TR, 16, 0x7e7ffff0u, 0, "T: TR legs 16, Z = 2^122 - 16 ulp" },
    { TR, 12, 0x7c400000u, 0, "T: TR legs 12, Z = 1.5 2^121: slope 2^118 / (1 / 144 in 24 bits)" },
    { TR, 12, 0x7c400001u, 0, "T: TR legs 12, Z = 1.5 2^121 + 1 ulp" },
    { TL, 32, 0xfa800000u, 0, "N: TL legs 32 at the left edge, Z = -2^118: start -infinity" },
    { TL, 32, 0xf9800000u, 0, "N: TL legs 32 at the left edge, Z = -2^116 (c = -2^128): start -infinity" },
    { TL, 32, 0xfa000000u, 0, "N: TL legs 32 at the left edge, Z = -2^117: c = -2^127, an ordinary plane" },
    { PAIR_C0, 0, 0x8d800000u, DepthMode_Greater, "M: zword's triangle, then flat -2^-100, Greater" },
    { PAIR_C0, 0, 0x8d800000u, DepthMode_Less, "M: zword's triangle, then flat -2^-100, Less" },
    { PAIR_C0, 0, 0x80c00000u, DepthMode_Greater, "M: zword's triangle, then flat -1.5 2^-126, Greater" },
    { PAIR_C0, 0, 0x80c00000u, DepthMode_Less, "M: zword's triangle, then flat -1.5 2^-126, Less" },
    { RAMP, 0, 0, 0, "D: ramp around 0 in steps of 2^-128" },
    { PAIR_RAMP, 0, 0x00800000u, DepthMode_Greater, "D: ramp around 0, then flat 2^-126, Greater" },
    { PAIR_RAMP, 0, 0x00800000u, DepthMode_Less, "D: ramp around 0, then flat 2^-126, Less" },
};
static const uint32_t B[NUM_PAGES] = { 0x7f000000u, 0, 0, 0xff000000u, 0xff000000u, 0xff000000u, 0x7f000000u };
static const int MD[NUM_PAGES] = { DepthMode_Greater, DepthMode_Greater, DepthMode_Equal, DepthMode_Equal, DepthMode_Greater,
                                   DepthMode_Less, DepthMode_Equal };
static const char *MN[8] = { "Never", "Less", "Equal", "LessEqual", "Greater", "NotEqual", "GreaterEqual", "Always" };
static uint32_t texG, texW;

static uint32_t green(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0x07e0; }
static uint32_t white(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0xffff; }

static void test_setup(void)
{
    const tex_layout_t l = { TEX_TWIDDLED, 16, 8, 8, 0 };
    texG = tex_build(&l, green, NULL);
    texW = tex_build(&l, white, NULL);
}

/* the cells: T at row 2 from x = 32 on (every other column), N at the left edge rows 0, 3, 5 (y 0, 96, 160),
   M at row 8, D at row 10 */
static void place(int k, int *x, int *y)
{
    if(k < 10) { *x = 32 + 64 * k; *y = 64; }
    else if(k < 13) { *x = 0; *y = k == 10 ? 0 : k == 11 ? 96 : 160; }
    else if(k < 17) { *x = 64 + 64 * (k - 13); *y = 256; }
    else { *x = 64 + 64 * (k - 17); *y = 320; }
}

static void test_describe(void)
{
    int p, k, x, y;
    for(p = 0; p < NUM_PAGES; p++)
        for(k = 0; k < NC; k++) {
            place(k, &x, &y);
            label(p, (y / 32) * 20 + x / 32, "%s; background %08x, %s", C[k].name, (unsigned)B[p], MN[MD[p]]);
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

static void quad(int x, int y, uint32_t z, int mode, uint32_t tex)
{
    int k;
    begin(mode, tex);
    for(k = 0; k < 4; k++)
        vtx(x + (k & 1) * 32, y + (k >> 1) * 32, z, k == 3);
}

/* the ramp: depth (16 - 7 (2 dx - 1) / 2 + 4 (2 dy - 1) / 2) 2^-128 at corner (dx, dy) of the tile, in units of 2^-129 */
static void ramp(int x, int y, int mode)
{
    int k;
    begin(mode, texG);
    for(k = 0; k < 4; k++) {
        const int dx = (k & 1) * 32, dy = (k >> 1) * 32;
        vtx(x + dx, y + dy, f32b_scaled(32 - 7 * (2 * dx - 1) + 4 * (2 * dy - 1), -129), k == 3);
    }
}

static void test_page(int page)
{
    const uint32_t half = 0x3f000000u;
    int k, x, y;
    pvr_frame.bg_color = 0x000040;
    pvr_frame.bg_depth = B[page];
    ta_list_begin(ListType_Opaque);
    for(k = 0; k < NC; k++) {
        const cell_t *c = &C[k];
        const int L = c->legs;
        place(k, &x, &y);
        switch(c->kind) {
        case TR: begin(MD[page], texG); vtx(x, y, half, 0); vtx(x + L, y, c->z, 0); vtx(x, y + L, half, 1); break;
        case TL: begin(MD[page], texG); vtx(x, y, c->z, 0); vtx(x + L, y, half, 0); vtx(x, y + L, half, 1); break;
        case RAMP: ramp(x, y, MD[page]); break;
        case PAIR_C0:
            begin(DepthMode_Always, texG); vtx(x, y, 0x7f800000u, 0); vtx(x + 32, y, half, 0); vtx(x, y + 32, half, 1);
            quad(x, y, c->z, c->mode2, texW);
            break;
        case PAIR_RAMP:
            ramp(x, y, DepthMode_Always);
            quad(x, y, c->z, c->mode2, texW);
            break;
        }
    }
    ta_list_end();
}
