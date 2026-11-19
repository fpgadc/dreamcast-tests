/* Where an overflowing depth plane starts (2026-11-19, after pvr-tests/zbad, zbadbg, zword: the item "a NaN at TR").

   A triangle with one corner at a NaN, an infinity or a depth whose slope overflows (32 z >= 2^128) is stored, in
   the depth unit's regime C (README "refsw3 without float", zspec), as the words of a register that steps by one
   unit a pixel and wraps every 32 (m = (s0 + sa i + sb j) mod 32, i, j from the tile's first pixel). zbad's
   triangle with the far corner TR at a NaN and +0.5 elsewhere started one step on (s0 = 1) where every other cell
   started at 0; that cell is also the only one at x = 0, where TR's term of the plane's constant is NaN * 0.
   Here: every cell one triangle with one bad corner (textured green, decal), and the cell's pixels sorted into
   the register's values by the background as the probe:
     page 0: DepthMode Always (which pixels the triangle covers)
     pages 1-8: DepthMode Greater against the background BG[page] (stored as itself: negative ones of 64 and
       more, positive ones, pvr-tests/bgneg, bgdepth): -64, -2^124, -2^125, -2^126, -1.99999 2^127, -2^127,
       2^100, 2^127. The number of pages a pixel is green on is its class: m 16 | 17 | 18-19 | 20-23 | 24-30 |
       31 | 0 | 1 | 2-15 (pvr_isp_depth.h kWrap), so a cell's row and column give s0, sa, sb.
   Cells (x, y the cell's top left; the three corners as sent, S the bad corner's depth, O the other two's):
     column 0 (x = 0), rows 0-14: zbad's TR triangle (x, y) O, (x + 32, y) S, (x, y + 32) O: TR's term of the
       constant is S * (x (y - (y + 32))) = S * 0
     column 1 (x = 32): the same, as controls
     row 0 (y = 0), columns 2-19: zbad's BL triangle (x, y) O, (x + 32, y) O, (x, y + 32) S: BL's term of the
       constant is S * 0 there
     row 1 (y = 32): the same, as controls
     rows 2-14, columns 2-19: right triangles inside the cell with the right angle at each of the tile's corners,
       each corner bad in turn, both orientations, three rotations of the vertex order; on the diagonal (x = y)
       the right angle at TR or BL with that corner bad: the edge facing it lies on the line through the origin.
   S: NaN 7fc00000, 7fa00000, 7f800001, 7fffffff, -NaN ffc00000, ffffffff, +-inf, +-the largest float, 2^127,
   2^123; O: +0.5, -0.5, 0, 2^100 (columns 0-1, rows 0-1). */
#define NUM_PAGES 9
#include "pvrtest2.h"
#include "texgen.h"

static const uint32_t S[12] = { 0x7fc00000u, 0x7fa00000u, 0x7f800001u, 0x7fffffffu, 0xffc00000u, 0xffffffffu,
                                0x7f800000u, 0xff800000u, 0x7f7fffffu, 0xff7fffffu, 0x7f000000u, 0x7d000000u };
static const char *SNAME[12] = { "NaN", "7fa00000", "7f800001", "7fffffff", "-NaN", "ffffffff", "+inf", "-inf",
                                 "max", "-max", "2^127", "2^123" };
static const uint32_t BG[NUM_PAGES] = { 0, 0xc2800000u, 0xfd800000u, 0xfe000000u, 0xfe800000u, 0xff7fffe0u,
                                        0xff000000u, 0x71800000u, 0x7f000000u };
#define O_HALF 0x3f000000u
#define O_MHALF 0xbf000000u
#define O_ZERO 0x00000000u
#define O_2E100 0x71800000u

/* tile-local corners of the shapes: right angle at TL, TR, BL, BR */
static const int SHAPE[4][3][2] = {
    { { 0, 0 }, { 32, 0 }, { 0, 32 } },
    { { 0, 0 }, { 32, 0 }, { 32, 32 } },
    { { 0, 0 }, { 0, 32 }, { 32, 32 } },
    { { 32, 0 }, { 32, 32 }, { 0, 32 } },
};
static const char *SHNAME[4] = { "right angle TL", "right angle TR", "right angle BL", "right angle BR" };

typedef struct { int shape, bad, rev, rot, s; uint32_t o; const char *group; } cellspec_t;

static uint32_t extra_o(int k) { return k == 0 ? O_MHALF : k == 1 ? O_ZERO : O_2E100; }

static cellspec_t spec(int c)
{
    const int cx = c % COLS, cy = c / COLS;
    cellspec_t r = { 0, 1, 0, 0, 0, O_HALF, "" };
    if(cx < 2) {                                 /* TR bad, x = 0 or 32 */
        r.shape = 0; r.bad = 1; r.group = cx ? "TR bad, x = 32" : "TR bad, x = 0";
        if(cy < 12) r.s = cy; else { r.s = 0; r.o = extra_o(cy - 12); }
    } else if(cy < 2) {                          /* BL bad, y = 0 or 32 */
        const int k = cx - 2;
        r.shape = 0; r.bad = 2; r.group = cy ? "BL bad, y = 32" : "BL bad, y = 0";
        if(k < 12) r.s = k;
        else { static const int ks[6] = { 0, 0, 0, 6, 4, 5 }; r.s = ks[k - 12]; r.o = k - 12 < 3 ? extra_o(k - 12) : O_MHALF; }
    } else if(cx == cy) {                        /* the diagonal */
        const int n = cx - 2;
        r.shape = (n & 1) ? 2 : 1; r.bad = 1; r.rev = (n >> 1) & 1; r.s = (n * 5) % 12;
        r.o = (n >> 2) & 1 ? O_MHALF : O_HALF; r.group = "diagonal";
    } else {
        const int n = (cy - 2) * 18 + (cx - 2), combo = n % 24;
        r.shape = combo / 6; r.bad = (combo / 2) % 3; r.rev = combo % 2; r.rot = (n / 24) % 3;
        r.s = (n * 7) % 12; r.o = (n / 12) % 2 ? O_MHALF : O_HALF; r.group = "block";
    }
    return r;
}

static const char *oname(uint32_t o) { return o == O_HALF ? "0.5" : o == O_MHALF ? "-0.5" : o == O_ZERO ? "0" : "2^100"; }

static uint32_t texG;
static uint32_t green(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0x07e0; }

static void test_setup(void)
{
    const tex_layout_t l = { TEX_TWIDDLED, 16, 8, 8, 0 };
    texG = tex_build(&l, green, NULL);
}

static void test_describe(void)
{
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 300; c++) {
            const cellspec_t r = spec(c);
            char bgs[40];
            if(p == 0) snprintf(bgs, sizeof(bgs), "Always");
            else snprintf(bgs, sizeof(bgs), "Greater, background %08x", (unsigned)BG[p]);
            label(p, c, "%s: %s, corner %d at %s, others %s, %s, rotation %d; %s", r.group, SHNAME[r.shape], r.bad,
                  SNAME[r.s], oname(r.o), r.rev ? "reversed" : "as listed", r.rot, bgs);
        }
}

static void vtx(int x, int y, uint32_t z, int end)
{
    ta_vertex3(f32b_int(x), f32b_int(y), z, 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, end);
}

static void test_page(int page)
{
    int c;
    pvr_frame.bg_color = 0x000040;
    pvr_frame.bg_depth = page ? BG[page] : 0xc2800000u;
    ta_list_begin(ListType_Opaque);
    for(c = 0; c < 300; c++) {
        PCW pcw = { .full = 0 };
        ISP_TSP isp = { .full = 0 };
        TSP tsp = { .full = 0 };
        TCW tcw = { .full = 0 };
        const cellspec_t r = spec(c);
        const int x = cell_px(c), y = cell_py(c);
        int order[3] = { 0, 1, 2 }, k;
        if(r.rev) { order[1] = 2; order[2] = 1; }
        pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
        pcw.ListType = ListType_Opaque;
        pcw.Group_En = 1;
        pcw.Strip_Len = 1;
        pcw.Texture = 1;
        isp.DepthMode = page ? DepthMode_Greater : DepthMode_Always;
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
        for(k = 0; k < 3; k++) {
            const int v = order[(k + r.rot) % 3];
            vtx(x + SHAPE[r.shape][v][0], y + SHAPE[r.shape][v][1], v == r.bad ? S[r.s] : r.o, k == 2);
        }
    }
    ta_list_end();
}
