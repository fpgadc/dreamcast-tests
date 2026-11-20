/* A finite start of 2^127 and more in an overflowing depth plane: is it cut to 20 bits before it goes into the
   register (2026-11-20, hunt depthgaps round 3; after depthgaps_rstart).

   depthgaps_rstart: in the depth unit's regime C (a slope overflows, the register steps by 2^127 a pixel, its sign
   at 2^131) a finite start goes onto the register's grid 2^102 toward zero. Whether it is cut to a start's 20 bits
   first shows only with a start of 2^127 and more (20 bits from 2^127 reach 2^108) one step toward zero, where the
   bits under 2^108 are all that is left; the constant must stay below 2^128 for that (nantr_fin2's triangle has c =
   -256 Z). Here V0 = (2, 6 + 1/8) at S (+-inf), V1 = (0, 0) and V2 = (2, 6 - 1/8) at Z: V0's and V2's terms of c are
   S * 0 and Z * 0, c = Z / 2, the determinant 1/2, a x0 and b y0 infinities of opposite sign, the start Z exactly;
   the triangle covers the pixel centers (0.5, 1.5) and (1.5, 4.5) on the line y = 3 x, where the register is Z +
   sign(S) 2^127 and Z + 3 sign(S) 2^127. With Z = +-(2^127 + d) and S of the other sign the first is d, or 0 when
   the start was cut. Both read as depthgaps_bigslope reads a pixel (a binary search with the background as the
   probe; row 470 word k: the number of probes below the depth, row 471: 1 where it equals the next one). Page p:
   Z[p / 2], S -inf (p even) or +inf (p odd). */
#define NUM_PAGES 16
#define TEST_DRAW_PAGE
#include "pvrtest2.h"
#include "texgen.h"

typedef struct { uint32_t z; const char *name; } start_t;
static const start_t Z[NUM_PAGES / 2] = {
    { 0x7f000001u, "Z 2^127 + 2^104" },
    { 0x7f000008u, "Z 2^127 + 2^107" },
    { 0x7f000010u, "Z 2^127 + 2^108 (20 bits)" },
    { 0x7f000018u, "Z 2^127 + 2^108 + 2^107" },
    { 0xff000001u, "Z -(2^127 + 2^104)" },
    { 0xff000008u, "Z -(2^127 + 2^107)" },
    { 0x7f400001u, "Z 1.5 2^127 + 2^104" },
    { 0x7f7fffffu, "Z max" },
};
#define NNEG (122u * 0x8001u)           /* negative probes: fields 133 .. 254, 0x7fffe0 then the 0x100 grid */
#define NPROBES (NNEG + 1u + (130u << 18))      /* 0, positive ones: fields 125 .. 254 */

static uint32_t texG;
static uint32_t green(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0x07e0; }

/* probe i in the ISP's order */
static uint32_t probe(uint32_t i)
{
    if(i < NNEG) {
        const uint32_t f = 133u + i / 0x8001u, g = i % 0x8001u;
        return 0x80000000u | f << 23 | (g ? 0x800000u - 0x100u * g : 0x7fffe0u);
    }
    i -= NNEG;
    if(!i)
        return 0;
    i--;
    return (125u + (i >> 18)) << 23 | ((i & 262143u) << 5);
}

static void test_setup(void)
{
    const tex_layout_t l = { TEX_TWIDDLED, 16, 8, 8, 0 };
    texG = tex_build(&l, green, NULL);
}

static void test_describe(void)
{
    int p;
    for(p = 0; p < NUM_PAGES; p++) {
        label(p, 280, "%s, S %s: the depths of pixels (0, 1), (1, 4) (rows 470, 471)", Z[p / 2].name, p & 1 ? "+inf" : "-inf");
        label(p, 0, "%s, S %s: the last render", Z[p / 2].name, p & 1 ? "+inf" : "-inf");
    }
}

static void vtx(int x8, int y8, uint32_t z, int end)
{
    ta_vertex3(f32b_scaled(x8, -3), f32b_scaled(y8, -3), z, 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, end);
}
static const int PX[2] = { 0, 1 }, PY[2] = { 1, 4 };

/* one render with the background at b and the triangle compared by mode; pixel (PX[x], PY[x]) green? */
static int render(int page, uint32_t b, int mode, int x)
{
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    uint32_t w;
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
    vtx(16, 49, page & 1 ? 0x7f800000u : 0xff800000u, 0); vtx(0, 0, Z[page / 2].z, 0); vtx(16, 47, Z[page / 2].z, 1);
    ta_list_end();
    pvr_frame_render();
    pvrio_read32(SETUP_FB + ((uint32_t)PY[x] * 640 + (uint32_t)PX[x]) * 4, 1, &w);
    return (w & 0xffffff) == 0x00ff00;
}

static void test_page(int page) { (void)page; }

static void test_draw_page(int page)
{
    static uint32_t res[2][32];
    int k;
    pvrio_unhashed = 1;
    for(k = 0; k < 2; k++) {
        uint32_t lo = 0, hi = NPROBES;
        while(lo < hi) {
            const uint32_t mid = lo + (hi - lo + 1) / 2;       /* is it above probe mid - 1? */
            if(render(page, probe(mid - 1), DepthMode_Greater, k)) lo = mid; else hi = mid - 1;
        }
        res[0][k] = lo;
        res[1][k] = lo < NPROBES ? (uint32_t)render(page, probe(lo), DepthMode_Equal, k) : 0;
    }
    pvrio_w32(SETUP_FB + 470u * 640 * 4, res[0], 32);
    pvrio_w32(SETUP_FB + 471u * 640 * 4, res[1], 32);
    pvrio_unhashed = 0;
}
