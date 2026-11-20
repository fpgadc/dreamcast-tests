/* Where a pixel below zero of a sloped plane is its value and where the block's negative full scale (2026-11-19,
   after perpmean_negstore, perpmean_negedge): round 1 had refsw3 take the value where the pixel's exponent is the
   block's own (E, the larger of the start's exponent and the slopes' pair + 5) and the full scale elsewhere
   (withdrawn: see Scope). Here triangles of legs 3.5 (corners -D, -(D + d), -D; page 1 the slope along y), d 10, 30, 100, 300 (slopes of about 3, 9, 29, 86 a
   pixel), D on a geometric ladder of 16 from 150 to 4000: across the boundary 2^(pair + 4) where E changes from the
   start's exponent to pair + 5, and across powers of two of the start. Same method as perpmean_negstore (the
   background as the probe, a search per target, region array with tile (4, 4) only, targets textured green).
   Target k at (128 + 4 (k % 8), 128 + 4 (k / 8)), its pixel one right and down. Results: row 470 word k the number of
   probes the depth is above, row 471 word k 1 when it equals the next probe (perpmean_bg's probes).

   Scope (2026-11-19, round 2): a sloped plane's pixel below zero outside its block's top binade is the depthgaps hunt's
   item (StoredDepth::Neg / NegWord there: its words are ordered by a fixed point value between the float words, not
   the full-scale words this test was first read as). refsw3 with only the perpmean patch keeps such pixels cut to 19
   bits and misses both pages (0 of 2 regions exact) here; the depthgaps hunt's model (its worktree,
   2026-11-19 evening) has 2 of 2. */
#define NUM_PAGES 2
#define TEST_DRAW_PAGE
#include "pvrtest2.h"
#include "texgen.h"

#define TX 4
#define TY 4
#define NT 64
#define NTP(p) 64
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

static int sp(int p) { (void)p; return 4; }
static int np(int p) { (void)p; return 8; }

/* the bits of the float n / 64 (n > 0, at most 2^24) */
static uint32_t f64th(uint32_t n)
{
    int e = 0;
    while(n >= (1u << 24)) { n >>= 1; e++; }
    while(n < (1u << 23)) { n <<= 1; e--; }
    return (uint32_t)(127 + 23 + e - 6) << 23 | (n & 0x7fffffu);
}

static void test_setup(void)
{
    static const uint32_t dd[4] = { 10, 30, 100, 300 };
    const tex_layout_t l = { TEX_TWIDDLED, 16, 8, 8, 0 };
    int p, k;
    texG = tex_build(&l, green, NULL);
    region_write();
    for(p = 0; p < NUM_PAGES; p++)
        for(k = 0; k < 64; k++) {
            /* D = 150 * (4000 / 150)^(j / 15), j = k % 16, in 64ths: a table, no float arithmetic */
            static const uint32_t D64[16] = { 9600, 11949, 14873, 18512, 23042, 28681, 35699, 44435, 55308, 68842,
                                              85687, 106655, 132754, 165238, 205672, 256000 };
            const uint32_t d = 0x80000000u | f64th(D64[k % 16]);
            const uint32_t dk = 0x80000000u | f64th(D64[k % 16] + 64u * dd[k / 16]);
            tz[p][k][0] = d;
            tz[p][k][1] = p == 0 ? dk : d;
            tz[p][k][2] = p == 0 ? d : dk;
        }
}

static void test_describe(void)
{
    int p;
    for(p = 0; p < NUM_PAGES; p++)
        label_span(p, 14 * COLS, 2, 1, "slope along %s: the searches' results (rows 470, 471)", p ? "y" : "x");
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
        const int leg = 14;     /* quarter pixels */
        const uint32_t *z = tz[page][k];
        vtx(4 * X, 4 * Y, z[0], 0); vtx(4 * X + leg, 4 * Y, z[1], 0); vtx(4 * X, 4 * Y + leg, z[2], 1);
    }
}

static int render(int page, int k, uint32_t b, int mode)
{
    uint32_t w;
    const int X = TX * 32 + sp(page) * (k % np(page)) + 1, Y = TY * 32 + sp(page) * (k / np(page)) + 1;
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
