/* The stored depth of single pixels of a triangle whose depth plane overflows, as words (2026-11-19, after
   pvr-tests/zbad, zbadbg, zbadk, zovf).

   The probe is the background: one render per step of a binary search, the triangle (textured green, decal) drawn
   DepthMode Greater against a background of the probe depth, the pixel read back: green where the triangle's depth
   is above the probe's in the ISP's order (pvr-tests/zsign). A background is stored as itself when it is positive
   (cut to 19 significant bits; from 0.25 on, pvr-tests/bgdepth) or negative of 64 and more (pvr-tests/bgneg), so
   the probes are those words with their low 5 bits clear, in the ISP's order: the negative ones of exponent fields
   133 .. 254 (by field up, within one by magnitude down), then the positive ones of fields 125 .. 254. After the
   search one render with DepthMode Equal at the probe found says whether the depth is that word.
   The renders depend on what is read back, so they are left out of the PVRIO checksum (pvrio_unhashed); the
   results are written into the framebuffer (rows 470, 471, one word per target), so that the page dumps carry
   them: row 470 word k: the index of the last probe the depth is above, + 1 (0: none, NPROBES: all); row 471 word
   k: 1 when the depth equals the next probe.
   Pages: the triangle (corners (x, y), (x + 32, y), (x, y + 32), the right-angle corner at S, the other two at
   0.5), and the targets: pixels (x + k, y), k = 0 .. 30. TR: the far corner (x + 32, y) at S. */
#define NUM_PAGES 8
#define TEST_DRAW_PAGE
#include "pvrtest2.h"
#include "texgen.h"

typedef struct { int x, y, tr; uint32_t s; const char *name; } variant_t;
static const variant_t V[NUM_PAGES] = {
    { 0, 0, 0, 0x7f800000u, "TL +inf at (0, 0)" },
    { 64, 0, 0, 0x7f800000u, "TL +inf at (64, 0)" },
    { 64, 64, 0, 0x7f800000u, "TL +inf at (64, 64)" },
    { 64, 0, 1, 0x7f800000u, "TR +inf at (64, 0)" },
    { 64, 0, 0, 0xff800000u, "TL -inf at (64, 0)" },
    { 0, 448, 0, 0x7f000000u, "TL 2^127 at (0, 448)" },
    { 64, 448, 0, 0x7b800000u, "TL 2^120 at (64, 448)" },
    { 64, 0, 0, 0x7fc00000u, "TL NaN at (64, 0)" },
};
#define NNEG (122u << 18)               /* negative probes: fields 133 .. 254, 2^18 each */
#define NPROBES (NNEG + (130u << 18))   /* and positive ones: fields 125 .. 254 */

static uint32_t texG;
static uint32_t green(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0x07e0; }

/* probe i in the ISP's order */
static uint32_t probe(uint32_t i)
{
    if(i < NNEG)
        return 0x80000000u | (133u + (i >> 18)) << 23 | ((262143u - (i & 262143u)) << 5);
    i -= NNEG;
    return (125u + (i >> 18)) << 23 | ((i & 262143u) << 5);
}

static void test_setup(void)
{
    const tex_layout_t l = { TEX_TWIDDLED, 16, 8, 8, 0 };
    texG = tex_build(&l, green, NULL);
}

static void test_describe(void)
{
    int p, k;
    for(p = 0; p < NUM_PAGES; p++)
        for(k = 0; k < 31; k++)
            label(p, k, "%s, pixel %d of its top row", V[p].name, k);
}

static void vtx(int x, int y, uint32_t z, int end)
{
    ta_vertex3(f32b_int(x), f32b_int(y), z, 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, end);
}

/* one render with the background at b and the triangle compared by mode; the target pixel green? */
static int render(const variant_t *v, uint32_t b, int mode, int k)
{
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    uint32_t w;
    const uint32_t half = 0x3f000000u;
    pvr_frame.bg_color = 0x000040;
    pvr_frame.bg_depth = b;
    pvr_frame_begin();
    ta_list_begin(ListType_Opaque);
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
    tcw.TexAddr = texG >> 3;
    ta_polygon(pcw, isp, tsp, tcw);
    if(v->tr) { vtx(v->x, v->y, half, 0); vtx(v->x + 32, v->y, v->s, 0); vtx(v->x, v->y + 32, half, 1); }
    else { vtx(v->x, v->y, v->s, 0); vtx(v->x + 32, v->y, half, 0); vtx(v->x, v->y + 32, half, 1); }
    ta_list_end();
    pvr_frame_render();
    pvrio_read32(SETUP_FB + ((uint32_t)v->y * 640 + (uint32_t)(v->x + k)) * 4, 1, &w);
    return (w & 0xffffff) == 0x00ff00;
}

static void test_page(int page) { (void)page; }

static void test_draw_page(int page)
{
    const variant_t *v = &V[page];
    static uint32_t res[2][32];
    int k;
    pvrio_unhashed = 1;
    for(k = 0; k < 31; k++) {
        /* the number of probes the depth is above: lo .. hi */
        uint32_t lo = 0, hi = NPROBES;
        while(lo < hi) {
            const uint32_t mid = lo + (hi - lo + 1) / 2;       /* is it above probe mid - 1? */
            if(render(v, probe(mid - 1), DepthMode_Greater, k)) lo = mid; else hi = mid - 1;
        }
        res[0][k] = lo;
        res[1][k] = lo < NPROBES ? (uint32_t)render(v, probe(lo), DepthMode_Equal, k) : 0;
    }
    res[0][31] = res[1][31] = 0;
    pvrio_w32(SETUP_FB + 470u * 640 * 4, res[0], 32);
    pvrio_w32(SETUP_FB + 471u * 640 * 4, res[1], 32);
    pvrio_unhashed = 0;
}
