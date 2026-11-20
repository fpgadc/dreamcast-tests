/* How the depth buffer keeps a depth below zero, as words (2026-11-19, after pvr-tests/perpmean_bg).

   perpmean_bg found every mean below zero, and flat 2x2 quads below zero, stored with 16 significant bits, the size
   rounded up (toward minus infinity); perpmean_neg that a size rounded up past the binade's largest 16 bit value
   stays in its binade. Here the edges of that and pixels of sloped planes, with perpmean_bg's method: the background
   as the probe (zword), a binary search per target, one render a step (a region array with the targets' tile (4, 4)
   only), the target's pixel read back (targets textured green, decal: below zero they show), then one render with
   DepthMode Equal. FPU_PERP_VAL 0: no triangle here is perpendicular.
     page 0   flat 2x2 quads (determinant 4) at sizes 2^6, 2^10, 2^14, 2^18, every one below zero: fractions under,
              at and over the 16 bit grid, at the top of a binade (0x7fff00 .. 0x7fffff), the binade's bottom
     pages 1 .. 3  triangles of legs 3.5 pixels, three corners below zero of size 64 .. 2^16 (any fraction), the
              target the pixel one right and one down of the right-angle corner: a sloped plane's stepped value
   Target k at (128 + 4 (k % 8), 128 + 4 (k / 8)) (the quad's or triangle's first corner); its pixel is that one on
   page 0, one right and down on pages 1 .. 3. Results in the framebuffer: row 470 word k: the number of probes the
   depth is above (0: none, NPROBES: all), row 471 word k: 1 when it equals the next probe. The probes: perpmean_bg's
   (negative words of exponent fields 133 .. 254, positive ones of 125 .. 254, low 5 bits clear, in the ISP's order).

   Scope (2026-11-19, round 2): a sloped plane's pixel below zero outside its block's top binade is the depthgaps hunt's
   item (StoredDepth::Neg / NegWord there: its words are ordered by a fixed point value between the float words, not
   the full-scale words this test was first read as). refsw3 with only the perpmean patch keeps such pixels cut to 19
   bits and misses pages 1 .. 3 (1 of 4 regions exact) here; the depthgaps hunt's model (its worktree,
   2026-11-19 evening) has 4 of 4. */
#define NUM_PAGES 4
#define TEST_DRAW_PAGE
#include "pvrtest2.h"
#include "texgen.h"

#define TX 4
#define TY 4
#define NT 64
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

static void test_setup(void)
{
    static const uint32_t frac[16] = {
        0x7fff01u, 0x7fff80u, 0x7fffffu, 0x7fff00u, 0x7ffeffu, 0x7ffe01u, 0x000001u, 0x000010u,
        0x0000ffu, 0x000100u, 0x000000u, 0x123400u, 0x123401u, 0x1234f0u, 0x12340fu, 0x555555u,
    };
    const tex_layout_t l = { TEX_TWIDDLED, 16, 8, 8, 0 };
    int p, k, i;
    texG = tex_build(&l, green, NULL);
    region_write();
    for(k = 0; k < NT; k++) {                   /* page 0: sizes 2^(6 + 4 (k / 16)) */
        const uint32_t e = 127u + 6u + (uint32_t)(k / 16) * 4u;
        tz[0][k][0] = tz[0][k][1] = tz[0][k][2] = 0x80000000u | e << 23 | frac[k % 16];
    }
    for(p = 1; p < NUM_PAGES; p++)
        for(k = 0; k < NT; k++)
            for(i = 0; i < 3; i++)
                tz[p][k][i] = 0x80000000u | (127u + 6u + rng() % 11u) << 23 | (rng() & 0x7fffffu);
}

static void test_describe(void)
{
    int p;
    for(p = 0; p < NUM_PAGES; p++)
        label_span(p, 14 * COLS, 2, 1, "%s: the searches' results (rows 470, 471)",
                   p == 0 ? "flat quads below zero, chosen fractions" : "sloped triangles below zero");
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
    for(k = 0; k < NT; k++) {
        const int X = TX * 32 + 4 * (k % 8), Y = TY * 32 + 4 * (k / 8);
        const uint32_t *z = tz[page][k];
        if(page == 0) {                          /* a 2x2 quad: strip TL, TR, BL, BR */
            vtx(4 * X, 4 * Y, z[0], 0); vtx(4 * X + 8, 4 * Y, z[1], 0);
            vtx(4 * X, 4 * Y + 8, z[2], 0); vtx(4 * X + 8, 4 * Y + 8, z[2], 1);
        } else {                                 /* legs 3.5 */
            vtx(4 * X, 4 * Y, z[0], 0); vtx(4 * X + 14, 4 * Y, z[1], 0); vtx(4 * X, 4 * Y + 14, z[2], 1);
        }
    }
}

static int render(int page, int k, uint32_t b, int mode)
{
    uint32_t w;
    const int X = TX * 32 + 4 * (k % 8) + (page ? 1 : 0), Y = TY * 32 + 4 * (k / 8) + (page ? 1 : 0);
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
    for(k = 0; k < NT; k++) {
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
