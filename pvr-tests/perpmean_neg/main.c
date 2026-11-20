/* The depth of a "perpendicular" triangle whose mean is below zero, as words (2026-11-19, after pvr-tests/perpmean_word).

   perpmean_word reads the means above zero (common/zexfil.h searches the positive depths only). Here the same
   triangles with every corner's depth negated (the bit patterns of perpmean_word's cases.h, sign bit flipped: no
   float arithmetic), so that the means are below zero, and a search over the negative depths: zexfil.h's probes and
   renders, but the probe depths are the negative words of 19 significant bits in the ISP's order (pvr-tests/zsign:
   among negative depths the larger exponent is the larger depth, within one exponent the float's order), exponent
   fields 27 .. 227 (2^-100 .. 2^100 in size, the range where the probes' depths are exact; zexfil.h), textured
   (a polygon below zero is shaded black: pvr-tests/zneg; green decal shows: zword, zsign), one render a
   step, 26 steps. A pixel's result is the largest probe that does not pass (compare greater), the depth itself: a
   negative word; 0x7fffffff when no probe passes (a depth above every probe, e.g. one above zero), 0xffffffff when
   every probe passes (a depth below the range).
   Tile 0 is a calibration: 64 quads of 4x4 pixels (determinant 16, so not perpendicular) at negative depths of at
   most 15 significant bits, which the setup computes exactly: they have to read as themselves (the probes' depths are
   exact below zero too).
   Pages: 0 tiles 0 .. 3, 1 tiles 4 .. 7, 2 tiles 8 .. 11; tile t > 0 is perpmean_word's tile SRC[t] negated. */
#define NUM_PAGES 3
#define TEST_DRAW_PAGE
#include "pvrtest2.h"
#include "zexfil.h"
#include "texgen.h"
#include "../perpmean_word/cases.h"

#define ZEXFIL_AT 0x580000u
#define NEG_PARAMS 0x5a0000u            /* the textured probes' parameters (86 kB: more than zexfil.h's area holds); free */
#define PERP_VAL 0x3f400000u            /* 0.75, as perpmean_word */
#define NTILES 12
#define FMIN 27u
#define NWORDS ((227u - FMIN + 1u) << 18)

static const int SRC[NTILES] = { -1, 0, 1, 2, 5, 6, 9, 10, 11, 12, 13, 15 };
static const struct { unsigned char tx, ty; } AT[NTILES] = {
    { 3, 10 }, { 5, 10 }, { 7, 10 }, { 9, 10 }, { 3, 12 }, { 5, 12 }, { 7, 12 }, { 9, 12 },
    { 11, 10 }, { 13, 10 }, { 11, 12 }, { 13, 12 },
};
static uint32_t cal_z[64];
static int cur_tile;
static uint32_t texG;
static uint32_t green(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0x07e0; }

static void scene(void);

/* the negative word of 19 significant bits number i in the ISP's order (rising) */
static uint32_t neg_word(uint32_t i) {
    return 0x80000000u | (FMIN + (i >> 18)) << 23 | ((262143u - (i & 262143u)) << 5);
}

static void test_setup(void) {
    int i;
    for(i = 0; i < 64; i++)                     /* negative, 15 significant bits, exponents -12 .. 12 */
        cal_z[i] = 0x80000000u | (uint32_t)(127 - 12 + (int)(rng() % 25)) << 23 | ((rng() & 0x3fffu) << 9);
    const tex_layout_t l = { TEX_TWIDDLED, 16, 8, 8, 0 };
    texG = tex_build(&l, green, NULL);
    pvrio_reg(FPU_PERP_VAL_addr, PERP_VAL);
    zexfil_use_pvrsetup(scene, ZEXFIL_AT);
}

static void test_describe(void) {
    int t;
    for(t = 0; t < NTILES; t++) {
        if(SRC[t] < 0)
            label(t / 4, AT[t].ty * COLS + AT[t].tx, "tile %d: calibration, 4x4 quads at negative 15 bit depths", t);
        else
            label(t / 4, AT[t].ty * COLS + AT[t].tx, "tile %d: perpmean_word tile %d negated: %s", t, SRC[t], tiles[SRC[t]].name);
    }
}

static void poly(void) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Opaque;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    isp.DepthMode = DepthMode_Always;
    isp.CullMode = CullMode_None;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    ta_polygon(pcw, isp, tsp, tcw);
}

static void scene(void) {
    const int t = cur_tile, s = SRC[t];
    int k;

    ta_list_begin(ListType_Opaque);
    poly();
    if(s < 0) {
        for(k = 0; k < 64; k++)                 /* an 8x8 grid of 4x4 quads over the tile */
            quad0(AT[t].tx * 32 + (k % 8) * 4, AT[t].ty * 32 + (k / 8) * 4, 4, 4, cal_z[k], 0xff808080u);
        return;
    }
    for(k = 0; k < 1024; k++) {
        const int X = AT[t].tx * 32 + k % 32, Y = AT[t].ty * 32 + k / 32;
        const f32b x0 = f32b_scaled(4 * X + 1, -2), y0 = f32b_scaled(4 * Y + 1, -2);
        const f32b x1 = f32b_int(X + 1), y1 = f32b_int(Y + 1);
        ta_vertex0(x0, y0, tz[s][k][0] ^ 0x80000000u, 0xff808080u, 0);
        if(tiles[s].mirror) {
            ta_vertex0(x0, y1, tz[s][k][1] ^ 0x80000000u, 0xff808080u, 0);
            ta_vertex0(x1, y0, tz[s][k][2] ^ 0x80000000u, 0xff808080u, 1);
        } else {
            ta_vertex0(x1, y0, tz[s][k][1] ^ 0x80000000u, 0xff808080u, 0);
            ta_vertex0(x0, y1, tz[s][k][2] ^ 0x80000000u, 0xff808080u, 1);
        }
    }
}

/* zexfil.h's probes with negative depths. A polygon whose depth (1/w) is below zero is shaded black (pvr-tests/zneg),
   so these are textured: an 8x8 texture all green, point sampled, decal (pvr-tests/zword, zsign): green where drawn.
   Vertices x, y, z, u, v, colour: skip 3 in the object list entry. */
static void neg_probes_write(void) {
    static uint32_t p[19 + 21 * 1024], ol[2 + 1024];
    int n = 0, m = 0, k, i, q;

    ol[m++] = zexfil_entry(NEG_PARAMS, 2);
    p[n++] = zexfil_isp(DepthMode_Always); p[n++] = zexfil_tsp(); p[n++] = 0;
    for(k = 0; k < 4; k++) {
        p[n++] = f32b_int((k & 1) * 32); p[n++] = f32b_int((k >> 1) * 32); p[n++] = F32B_ONE; p[n++] = 0xff000000u;
    }
    for(i = 0; i < 1024; i++) {
        q = zexfil_probe[i].pixel;
        if(zexfil_lo[q] >= zexfil_hi[q])
            continue;
        ol[m++] = (zexfil_entry(NEG_PARAMS + 4u * (uint32_t)n, 1) & ~(7u << 21)) | 3u << 21;
        {
            ISP_TSP isp = { .full = zexfil_isp(DepthMode_Greater) };
            TSP tsp = { .full = zexfil_tsp() };
            TCW tcw = { .full = 0 };
            isp.Texture = 1;
            tsp.ShadInstr = ShadInstr_Decal;
            tsp.FilterMode = FilterMode_Point;
            tsp.MipMapD = 4;
            tcw.PixelFmt = PixelFmt_565;
            tcw.TexAddr = texG >> 3;
            p[n++] = isp.full; p[n++] = tsp.full; p[n++] = tcw.full;
        }
        for(k = 0; k < 3; k++) {
            p[n++] = zexfil_probe[i].x[k]; p[n++] = zexfil_probe[i].y[k]; p[n++] = neg_word(zexfil_mid(q));
            p[n++] = 0x3e800000u; p[n++] = 0x3e800000u; p[n++] = zexfil_colour(q);
        }
    }
    ol[m++] = 0xf0000000u;
    pvrio_unhashed = 1;
    pvrio_w32(NEG_PARAMS, p, n);
    pvrio_w32(zexfil_ol(), ol, m);
    pvrio_unhashed = 0;
}

static void neg_tile(int x, int y, uint32_t *z) {
    int i, step;
    for(i = 0; i < 1024; i++) {
        zexfil_lo[i] = 0;
        zexfil_hi[i] = NWORDS;
    }
    zexfil_region_write(x, y);
    for(step = 0; step < 26; step++) {
        neg_probes_write();
        zexfil_cfg.render(zexfil_region());
        zexfil_update();
    }
    for(i = 0; i < 1024; i++)
        z[i] = zexfil_hi[i] == NWORDS ? 0x7fffffffu : zexfil_hi[i] == 0 ? 0xffffffffu : neg_word(zexfil_hi[i] - 1);
}

static void test_page(int page) {
    (void)page;
}

static void test_draw_page(int page) {
    static uint32_t z[4][1024];
    char line[80];
    int k;

    zexfil_foreign = 0;
    for(k = 0; k < 4; k++) {
        cur_tile = page * 4 + k;
        neg_tile(AT[cur_tile].tx, AT[cur_tile].ty, z[k]);
    }
    pvrio_fill32(SETUP_FB, 640 * 480, 0);
    for(k = 0; k < 4; k++)
        zexfil_show(AT[page * 4 + k].tx, AT[page * 4 + k].ty, z[k]);
    if(zexfil_foreign) {
        snprintf(line, sizeof(line), "PVRIO zexfil page %d: %d readings of another colour than the probe's", page, zexfil_foreign);
        pvrio_hw_say(line);
    }
}
