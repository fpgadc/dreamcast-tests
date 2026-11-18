/* The depth buffer read out (2026-11-18): common/zexfil.h on scenes of its own. Each page shows the depths that the
   core keeps, one word per pixel (the float's bits, 19 significant) in the window's place, black elsewhere;
   compare2.py hw off compares them word for word. Every tile is 27 renders.

   page 0  row 1: calibration, a full-tile quad at a depth of at most 15 significant bits in each of tiles 1..4,
           which the setup computes exactly (zmeas): the depths read back have to be these, on a console too (the
           keep Z entry at tile (0, 0) sees the depths of the tile before it).
           row 2: flat quads, 8x8 pixels, compare always with depth write, at depths given as bit patterns:
           denormals, the smallest normals, around the background depth's steps (bgdepth: 2^-20, 0.25), around
           1.0, the largest float, then random positive floats. The first column of quads is the background
           (0.0001). A window of 4x2 tiles at tile (1, 1).
   page 1  32 triangles at random sub-pixel corners (1/16), a random depth at each corner in [0.5, 2), compare
           greater with depth write, over a quad at 0.25: interpolated depths. Window 4x2 at tile (1, 5).
   page 2  the same window: a quad at 0.25, then 8 opaque, 8 punch-through and 8 translucent triangles (in the
           order sent), compare greater with depth write: the depth after the punch-through and translucent
           passes.
   page 3  the background alone at eight depths, one tile each (tiles 1, 3, .., 15 of row 9). */
#define NUM_PAGES 4
#define TEST_DRAW_PAGE
#include "pvrtest2.h"
#include "zexfil.h"

/* the probes' video memory: free (the object lists and the region array end below 573000, textures start at
   600000), above PARAM_BASE (400000). Below it, with offsets that reach past the 8 MB of video memory, a console
   draws no probe (2026-11-18: everything read as 0x7fffffe0); refsw3-core wraps them. */
#define ZEXFIL_AT 0x580000u

static void scene(void);

#define N_FLAT 64
#define N_TRIS 32
#define N_MIX  24

static f32b flat_z[N_FLAT];
static struct { f32b x[3], y[3], z[3]; } tris[N_TRIS], mix[N_MIX];
static const f32b bg_depths[8] = {
    0x38d1b717,             /* 0.0001, the harness's */
    0x3e800000,             /* 0.25 */
    0x3e7fffff,             /* below 0.25 */
    0x3f800001,             /* 1 + 2^-23 */
    0x3f80001f,             /* 1 + 31 * 2^-23 */
    0x3f800020,             /* 1 + 2^-18 */
    0x35800000,             /* 2^-20 */
    0x357fffff,             /* below 2^-20 */
};
static const f32b cal_z[4] = { 0x3f8a5e00, 0x3e123400, 0x40490e00, 0x3c75c200 };     /* 15, 14, 15, 15 bits */
typedef struct { int tx, ty, tw, th; } window_t;
static const window_t win0 = { 1, 1, 4, 2 }, win1 = { 1, 5, 4, 2 };
static int cur_page;

static f32b rnd_float(void) {                   /* a positive normal float, any exponent */
    uint32_t e = rng() % 254 + 1;
    return e << 23 | (rng() & 0x7fffffu);
}

static void test_setup(void) {
    static const f32b given[] = {
        0x00000001, 0x00000002, 0x007fffff, 0x00800000, 0x00800001, 0x34000000, 0x357fffff, 0x35800000,
        0x35800001, 0x3e7fffff, 0x3e800000, 0x3e800001, 0x3f7fffff, 0x3f800000, 0x3f800001, 0x3f80001f,
        0x3f800020, 0x3f800021, 0x3fffffff, 0x4b7fffff, 0x7f7ffffe, 0x7f7fffff,
    };
    int i, k, n = 0;

    for(i = 0; i < N_FLAT; i++)
        flat_z[i] = i % 16 == 0 ? 0 : n < (int)(sizeof(given) / sizeof(given[0])) ? given[n++] : rnd_float();
    for(i = 0; i < N_TRIS + N_MIX; i++)
        for(k = 0; k < 3; k++) {                /* corners up to 16 pixels outside the window */
            f32b *x = i < N_TRIS ? tris[i].x : mix[i - N_TRIS].x;
            f32b *y = i < N_TRIS ? tris[i].y : mix[i - N_TRIS].y;
            f32b *z = i < N_TRIS ? tris[i].z : mix[i - N_TRIS].z;
            x[k] = f32b_scaled((int32_t)(rng() % (160 * 16)) - 16 * 16 + win1.tx * 32 * 16, -4);
            y[k] = f32b_scaled((int32_t)(rng() % (96 * 16)) - 16 * 16 + win1.ty * 32 * 16, -4);
            z[k] = 0x3f000000u + rng() % 0x1000000u;
        }
    pvrio_reg(PT_ALPHA_REF_addr, 0x80);
    zexfil_use_pvrsetup(scene, ZEXFIL_AT);
}

static void test_describe(void) {
    int k;
    for(k = 0; k < 4; k++)
        label(0, win0.ty * COLS + win0.tx + k, "calibration: a full-tile quad at %08x, exact", (unsigned)cal_z[k]);
    label_span(0, (win0.ty + 1) * COLS + win0.tx, win0.tw, 1, "flat quads at given depths (first column: the background)");
    label_span(1, win1.ty * COLS + win1.tx, win1.tw, win1.th, "32 triangles, a depth per corner, compare greater");
    label_span(2, win1.ty * COLS + win1.tx, win1.tw, win1.th, "opaque, punch-through, translucent triangles, compare greater");
    for(k = 0; k < 8; k++)
        label(3, 9 * COLS + 1 + 2 * k, "the background at depth %08x", (unsigned)bg_depths[k]);
}

static void poly(int list, int depth_mode) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = list;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Gouraud = 1;
    isp.DepthMode = depth_mode;
    isp.CullMode = CullMode_None;
    tsp.SrcInstr = list == ListType_Opaque ? BlendInstr_One : BlendInstr_SrcAlpha;
    tsp.DstInstr = list == ListType_Opaque ? BlendInstr_Zero : BlendInstr_InvSrcAlpha;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    ta_polygon(pcw, isp, tsp, tcw);
}

static void tri(const f32b *x, const f32b *y, const f32b *z, uint32_t colour) {
    int k;
    for(k = 0; k < 3; k++)
        ta_vertex0(x[k], y[k], z[k], colour, k == 2);
}

static void base_quad(void) {
    poly(ListType_Opaque, DepthMode_Always);
    quad0(win1.tx * 32, win1.ty * 32, win1.tw * 32, win1.th * 32, 0x3e800000, 0xff404040u);
}

static void scene(void) {
    int i;
    switch(cur_page) {
    case 0:
        ta_list_begin(ListType_Opaque);
        poly(ListType_Opaque, DepthMode_Always);
        for(i = 0; i < 4; i++)
            quad0((win0.tx + i) * 32, win0.ty * 32, 32, 32, cal_z[i], 0xff000000u | (cal_z[i] >> 8));
        for(i = 0; i < N_FLAT; i++)
            if(flat_z[i])
                quad0(win0.tx * 32 + (i % 16) * 8, (win0.ty + 1) * 32 + (i / 16) * 8, 8, 8, flat_z[i], 0xff000000u | (flat_z[i] >> 8));
        break;
    case 1:
        ta_list_begin(ListType_Opaque);
        base_quad();
        for(i = 0; i < N_TRIS; i++) {
            poly(ListType_Opaque, DepthMode_Greater);
            tri(tris[i].x, tris[i].y, tris[i].z, 0xff000000u | (uint32_t)i * 0x070503u);
        }
        break;
    case 2:
        ta_list_begin(ListType_Opaque);
        base_quad();
        for(i = 0; i < 8; i++) {
            poly(ListType_Opaque, DepthMode_Greater);
            tri(mix[i].x, mix[i].y, mix[i].z, 0xff800000u | (uint32_t)i * 0x001020u);
        }
        ta_list_begin(ListType_Punch_Through);
        for(i = 8; i < 16; i++) {
            poly(ListType_Punch_Through, DepthMode_Greater);
            tri(mix[i].x, mix[i].y, mix[i].z, 0xff008000u | (uint32_t)i * 0x100010u);
        }
        ta_list_begin(ListType_Translucent);
        for(i = 16; i < 24; i++) {
            poly(ListType_Translucent, DepthMode_Greater);
            tri(mix[i].x, mix[i].y, mix[i].z, 0x80000080u | (uint32_t)i * 0x102000u);
        }
        break;
    case 3:
        break;
    }
}

static void test_page(int page) {
    (void)page;
}

static void test_draw_page(int page) {
    static uint32_t z[8][1024];
    char line[80];
    int k, n = 0;

    cur_page = page;
    zexfil_foreign = 0;
    if(page < 3) {
        const window_t *w = page == 0 ? &win0 : &win1;
        for(k = 0; k < w->tw * w->th; k++)
            zexfil_tile(w->tx + k % w->tw, w->ty + k / w->tw, z[n++]);
        pvrio_fill32(SETUP_FB, 640 * 480, 0);
        for(k = 0; k < n; k++)
            zexfil_show(w->tx + k % w->tw, w->ty + k / w->tw, z[k]);
    } else {
        for(k = 0; k < 8; k++) {
            pvr_frame.bg_depth = bg_depths[k];
            zexfil_tile(1 + 2 * k, 9, z[k]);
        }
        pvr_frame.bg_depth = bg_depths[0];
        pvrio_fill32(SETUP_FB, 640 * 480, 0);
        for(k = 0; k < 8; k++)
            zexfil_show(1 + 2 * k, 9, z[k]);
    }
    if(zexfil_foreign) {
        snprintf(line, sizeof(line), "PVRIO zexfil page %d: %d readings of another colour than the probe's", page, zexfil_foreign);
        pvrio_hw_say(line);
    }
}
