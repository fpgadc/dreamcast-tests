/* The depth of a perpendicular triangle (FPU_PERP_VAL) where perpmean_word cannot read it: means whose sum
   overflows, and means below zero, as words (2026-11-19, after pvr-tests/perpmean_word and perpmean_neg).

   The probe is the background, as in pvr-tests/zword: one render per step of a binary search, every target drawn
   DepthMode Greater (textured green, decal: a polygon below zero shows, pvr-tests/zsign) against a background of the
   probe depth, the target's pixel read back: green where its depth is above the probe's in the ISP's order. A
   background is stored as itself when it is positive (19 significant bits, from 0.25 on: bgdepth) or negative of 64
   and more (bgneg), so the probes are those words with their low 5 bits clear, in the ISP's order: the negative ones
   of exponent fields 133 .. 254, then the positive ones of fields 125 .. 254. After the search one render with
   DepthMode Equal at the probe found says whether the depth is that word. The render has a region array of its own
   with tile (4, 4) only (the targets' tile).
   Targets (gen.cpp, cases.h): 64 a page in tile (4, 4), pixel (128 + 4 i, 128 + 4 j), each a triangle of corners
   (x + 1/4, y + 1/4), (x + 1, y + 1/4), (x + 1/4, y + 1) (determinant 9/16, perpendicular at FPU_PERP_VAL 0.75) or,
   kind 1, a flat 2x2 quad from (x, y) (determinant 4: an ordinary plane). Page 0: sums that overflow (2^128) or
   nearly do, both signs; pages 1, 2: means below zero, 64 .. 2^20 in size; page 3: means above zero, flat quads.
   Results in the framebuffer (rows 470, 471, one word per target), so that the page dumps carry them: row 470 word k:
   the number of probes target k's depth is above (0: none, NPROBES: all); row 471 word k: 1 when it equals the next
   probe. The renders depend on what is read back, so they are left out of the PVRIO checksum (pvrio_unhashed). */
#define NUM_PAGES 4
#define TEST_DRAW_PAGE
#include "pvrtest2.h"
#include "texgen.h"
#include "cases.h"

#define PERP_VAL 0x3f400000u            /* 0.75 */
#define TX 4
#define TY 4
#define REGION_AT 0x5c0000u             /* 32 bit area, free (pvr-tests/zexfil) */
#define NNEG (122u << 18)               /* negative probes: fields 133 .. 254, 2^18 each */
#define NPROBES (NNEG + (130u << 18))   /* and positive ones: fields 125 .. 254 */

static uint32_t texG, region;

static uint32_t green(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0x07e0; }

/* probe i in the ISP's order */
static uint32_t probe(uint32_t i)
{
    if(i < NNEG)
        return 0x80000000u | (133u + (i >> 18)) << 23 | ((262143u - (i & 262143u)) << 5);
    i -= NNEG;
    return (125u + (i >> 18)) << 23 | ((i & 262143u) << 5);
}

/* the region array: 18 zero words, the entry that draws nothing, tile (TX, TY) */
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
    const tex_layout_t l = { TEX_TWIDDLED, 16, 8, 8, 0 };
    texG = tex_build(&l, green, NULL);
    pvrio_reg(FPU_PERP_VAL_addr, PERP_VAL);
    region_write();
}

static void test_describe(void)
{
    int p;
    static const char *what[NUM_PAGES] = { "sums that overflow, or nearly", "means below zero", "means below zero",
                                           "means above zero, flat 2x2 quads" };
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
    for(k = 0; k < NTARGET; k++) {
        const int X = TX * 32 + 4 * (k % 8), Y = TY * 32 + 4 * (k / 8);
        const uint32_t *z = targets[page][k].z;
        if(targets[page][k].kind == 1) {        /* a 2x2 quad: strip TL, TR, BL, BR */
            vtx(4 * X, 4 * Y, z[0], 0); vtx(4 * X + 8, 4 * Y, z[1], 0);
            vtx(4 * X, 4 * Y + 8, z[2], 0); vtx(4 * X + 8, 4 * Y + 8, z[2], 1);
        } else {
            vtx(4 * X + 1, 4 * Y + 1, z[0], 0); vtx(4 * X + 4, 4 * Y + 1, z[1], 0); vtx(4 * X + 1, 4 * Y + 4, z[2], 1);
        }
    }
}

/* one render with the background at b and the targets compared by mode; target k's pixel green? */
static int render(int page, int k, uint32_t b, int mode)
{
    uint32_t w;
    const int X = TX * 32 + 4 * (k % 8), Y = TY * 32 + 4 * (k / 8);
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
    static uint32_t res[2][NTARGET];
    int k;
    pvrio_unhashed = 1;
    for(k = 0; k < NTARGET; k++) {
        uint32_t lo = 0, hi = NPROBES;
        while(lo < hi) {
            const uint32_t mid = lo + (hi - lo + 1) / 2;       /* is it above probe mid - 1? */
            if(render(page, k, probe(mid - 1), DepthMode_Greater)) lo = mid; else hi = mid - 1;
        }
        res[0][k] = lo;
        res[1][k] = lo < NPROBES ? (uint32_t)render(page, k, probe(lo), DepthMode_Equal) : 0;
    }
    pvrio_fill32(SETUP_FB, 640 * 480, 0);
    pvrio_w32(SETUP_FB + 470u * 640 * 4, res[0], NTARGET);
    pvrio_w32(SETUP_FB + 471u * 640 * 4, res[1], NTARGET);
    pvrio_unhashed = 0;
}
