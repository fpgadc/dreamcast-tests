/* Edges of the rules perpmean_negstore found for depths below zero (2026-11-19): a flat plane's value is stored
   with 16 bits, rounded up in size without a carry into the exponent; a pixel below zero of a plane with steps is the
   block's negative full scale. Same method as perpmean_negstore (the background as the probe, one search per target,
   a region array with tile (4, 4) only, targets textured green, decal).
     page 0   16 triangles of legs 7.5 (spaced 8), corners -D, -(D + k ulp), -D (and the same along y), k = 1, 2, 3,
              4, 5, 6, 8, 16: with k = 1 the slope is under the tile's grid (it steps by 0), from 2 on it steps
     page 1   64 triangles of legs 3.5 whose plane crosses zero (corners of both signs, sizes 64 .. 2^12): the target
              pixel above or below zero
     page 2   64 flat 2x2 quads below zero at the top of binades 2^6 .. 2^21 and other fractions
   Target k at (128 + s (k % n), 128 + s (k / n)) (s 8, n 4 on page 0; s 4, n 8 otherwise), its pixel one right and
   down of that corner (the corner itself on page 2). Results: row 470 word k the number of probes the depth is above,
   row 471 word k 1 when it equals the next probe (perpmean_bg's probes).

   Scope (2026-11-19, round 2): a sloped plane's pixel below zero outside its block's top binade is the depthgaps hunt's
   item (StoredDepth::Neg / NegWord there: its words are ordered by a fixed point value between the float words, not
   the full-scale words this test was first read as). refsw3 with only the perpmean patch keeps such pixels cut to 19
   bits and misses page 1 (2 of 3 regions exact) here; the depthgaps hunt's model (its worktree,
   2026-11-19 evening) has 3 of 3. */
#define NUM_PAGES 3
#define TEST_DRAW_PAGE
#include "pvrtest2.h"
#include "texgen.h"

#define TX 4
#define TY 4
#define NT 64
#define NTP(p) ((p) == 0 ? 16 : 64)
#define REGION_AT 0x5c0000u             /* 32 bit area, free (pvr-tests/zexfil) */
#define NNEG (122u << 18)
#define NPROBES (NNEG + (130u << 18))

static uint32_t texG, region;
static uint32_t tz[NUM_PAGES][NT][3];

static uint32_t green(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0x07e0; }

static uint32_t probe(uint32_t i)
{
    if(i < NNEG)
        return 0x80000000u | (133u + (i >> 18)) << 23 | ((262143u - (i & 262143u)) << 5);
    i -= NNEG;
    return (125u + (i >> 18)) << 23 | ((i & 262143u) << 5);
}

static void region_write(void)
{
    static uint32_t w[18 + 12];
    RegionArrayEntry e;
    ListPointer *p[5] = { &e.opaque, &e.opaque_mod, &e.trans, &e.trans_mod, &e.puncht };
    uint32_t bytes;
    int i;
    memset(w, 0, sizeof(w));
    memset(&e, 0, sizeof(e));
    e.control.no_writeout = 1;
    e.opaque.empty = e.opaque_mod.empty = e.trans.empty = e.trans_mod.empty = e.puncht.empty = 1;
    memcpy(&w[18], &e, sizeof(e));
    memset(&e, 0, sizeof(e));
    e.control.tilex = TX;
    e.control.tiley = TY;
    e.control.pre_sort = pvr_cfg.pre_sort;
    e.control.last_region = 1;
    for(i = 0; i < 5; i++) {
        bytes = (uint32_t)pvr_cfg.opb_words[i] * 4;
        if(bytes)
            p[i]->ptr_in_words = (pvr_list_base[i] + bytes * (uint32_t)(SETUP_TILES_X * TY + TX)) >> 2;
        else
            p[i]->empty = 1;
    }
    memcpy(&w[24], &e, sizeof(e));
    pvrio_w32(REGION_AT, w, 30);
    region = REGION_AT + 18 * 4;
}

static int sp(int p) { return p == 0 ? 8 : 4; }
static int np(int p) { return p == 0 ? 4 : 8; }

static void test_setup(void)
{
    static const uint32_t frac[8] = { 0x7fff01u, 0x7fff20u, 0x7fffe0u, 0x7ffff0u, 0x7fffffu, 0x7ffeffu, 0x7fff00u, 0x00000fu };
    static const int kk[8] = { 1, 2, 3, 4, 5, 6, 8, 16 };
    const tex_layout_t l = { TEX_TWIDDLED, 16, 8, 8, 0 };
    int k, i;
    texG = tex_build(&l, green, NULL);
    region_write();
    for(k = 0; k < 16; k++) {                   /* page 0: D of 256 .. 4096, 24 bit */
        const uint32_t d = 0x80000000u | (127u + 8u + rng() % 4u) << 23 | (rng() & 0x7fffffu);
        const uint32_t dk = d + (uint32_t)kk[k % 8];       /* k ulps more in size (no binade crossed: fraction < 2^23 - 16) */
        tz[0][k][0] = d;
        tz[0][k][1] = k < 8 ? dk : d;
        tz[0][k][2] = k < 8 ? d : dk;
    }
    for(k = 0; k < 64; k++)                      /* page 1: corners of both signs */
        for(i = 0; i < 3; i++)
            tz[1][k][i] = (rng() & 1u) << 31 | (127u + 6u + rng() % 7u) << 23 | (rng() & 0x7fffffu);
    for(k = 0; k < 64; k++) {                    /* page 2: binades 2^6 .. 2^21 */
        const uint32_t e = 127u + 6u + (uint32_t)(k / 4);
        tz[2][k][0] = tz[2][k][1] = tz[2][k][2] = 0x80000000u | e << 23 | frac[(k + k / 8) % 8];
    }
}

static void test_describe(void)
{
    static const char *what[NUM_PAGES] = { "slopes under and over the tile's grid", "planes crossing zero",
                                           "flat quads at the top of binades" };
    int p;
    for(p = 0; p < NUM_PAGES; p++)
        label_span(p, 14 * COLS, 2, 1, "%s: the searches' results (rows 470, 471)", what[p]);
}

static void vtx(int x4, int y4, uint32_t z, int end)
{
    ta_vertex3(f32b_scaled(x4, -2), f32b_scaled(y4, -2), z, 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, end);
}

static void scene(int page, int mode)
{
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    int k;
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
    for(k = 0; k < NTP(page); k++) {
        const int X = TX * 32 + sp(page) * (k % np(page)), Y = TY * 32 + sp(page) * (k / np(page));
        const int leg = page == 0 ? 30 : 14;     /* quarter pixels */
        const uint32_t *z = tz[page][k];
        if(page == 2) {                          /* a 2x2 quad: strip TL, TR, BL, BR */
            vtx(4 * X, 4 * Y, z[0], 0); vtx(4 * X + 8, 4 * Y, z[1], 0);
            vtx(4 * X, 4 * Y + 8, z[2], 0); vtx(4 * X + 8, 4 * Y + 8, z[2], 1);
        } else {
            vtx(4 * X, 4 * Y, z[0], 0); vtx(4 * X + leg, 4 * Y, z[1], 0); vtx(4 * X, 4 * Y + leg, z[2], 1);
        }
    }
}

static int render(int page, int k, uint32_t b, int mode)
{
    uint32_t w;
    const int X = TX * 32 + sp(page) * (k % np(page)) + (page != 2 ? 1 : 0), Y = TY * 32 + sp(page) * (k / np(page)) + (page != 2 ? 1 : 0);
    pvr_frame.bg_color = 0x000040;
    pvr_frame.bg_depth = b;
    pvr_frame_begin();
    scene(page, mode);
    pvr_frame_lists();
    pvr_frame_start(region);
    pvrio_read32(SETUP_FB + ((uint32_t)Y * 640 + (uint32_t)X) * 4, 1, &w);
    return (w & 0xffffff) == 0x00ff00;
}

static void test_page(int page) { (void)page; }

static void test_draw_page(int page)
{
    static uint32_t res[2][NT];
    int k;
    pvrio_unhashed = 1;
    memset(res, 0, sizeof(res));
    for(k = 0; k < NTP(page); k++) {
        uint32_t lo = 0, hi = NPROBES;
        while(lo < hi) {
            const uint32_t mid = lo + (hi - lo + 1) / 2;
            if(render(page, k, probe(mid - 1), DepthMode_Greater)) lo = mid; else hi = mid - 1;
        }
        res[0][k] = lo;
        res[1][k] = lo < NPROBES ? (uint32_t)render(page, k, probe(lo), DepthMode_Equal) : 0;
    }
    pvrio_fill32(SETUP_FB, 640 * 480, 0);
    pvrio_w32(SETUP_FB + 470u * 640 * 4, res[0], NT);
    pvrio_w32(SETUP_FB + 471u * 640 * 4, res[1], NT);
    pvrio_unhashed = 0;
}
