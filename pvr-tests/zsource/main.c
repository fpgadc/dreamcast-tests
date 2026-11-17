/* Which depth a punch through polygon leaves in the depth buffer: the ISP's (its depth plane, as the depth test
   used it) or the TSP's 1/w (the plane the shading interpolates, which the alpha test goes with). The two are
   computed in different arithmetic and differ in the last bits (2026-11-17).

   Every cell: a quad with random corners and depths (P), then a translucent copy of it (T: the same corners in
   the same order, so the same ISP depth, bit for bit), compare GREATER, green, alpha 0x80. A tie fails GREATER
   on a console and in refsw2, so T shows only where the depth P left is below P's own ISP depth.
     rows 0-1  P punch through, alpha 0xff (passes), red: T nothing if the ISP's depth is stored; where the TSP's
               1/w is below it if that is
     rows 2-3  P opaque, red (reference: the ISP's depth, T nothing)
   rows 0 and 2: depths 0.2 .. 0.9; rows 1 and 3: the same / 256. */
#define NUM_PAGES 1
#include "pvrtest2.h"

#define N 20

typedef struct { f32b x[4], y[4], z[4]; } geom_t;
static geom_t geom[2][N];

static uint32_t rnd(uint32_t n) { return rng() % n; }

static void test_setup(void) {
    int r, c, k;
    for(r = 0; r < 2; r++)
        for(c = 0; c < N; c++) {
            const int x = cell_px(c), y = cell_py(c);
            geom_t *g = &geom[r][c];
            for(k = 0; k < 4; k++) {
                /* corners in 1/16 pixels, within 2 pixels of the cell's corners */
                g->x[k] = f32b_scaled(16 * (x + (k & 1) * 30) + (int32_t)rnd(32), -4);
                g->y[k] = f32b_scaled(16 * (y + (k >> 1) * 30) + (int32_t)rnd(32), -4);
                g->z[k] = f32b_scaled((int32_t)(3355443u + rnd(11744051u)), r ? -32 : -24);    /* 0.2 .. 0.9 (/ 256) */
            }
        }
}

static void test_describe(void) {
    int r, c;
    for(r = 0; r < 4; r++)
        for(c = 0; c < N; c++)
            label(0, r * COLS + c, "%s, depths %s, then a translucent copy (greater): nothing?", r < 2 ? "punch through" : "opaque",
                  r & 1 ? "0.2..0.9 / 256" : "0.2..0.9");
}

static void poly(int list, int depth, const geom_t *g, int dy, uint32_t colour) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    int k;
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = list;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    isp.DepthMode = depth;
    isp.CullMode = CullMode_None;
    tsp.SrcInstr = list == ListType_Opaque ? BlendInstr_One : BlendInstr_SrcAlpha;
    tsp.DstInstr = list == ListType_Opaque ? BlendInstr_Zero : BlendInstr_InvSrcAlpha;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    ta_polygon(pcw, isp, tsp, tcw);
    for(k = 0; k < 4; k++)
        ta_vertex0(g->x[k], sf_add(g->y[k], f32b_int(dy)), g->z[k], colour, k == 3);
}

static void test_page(int page) {
    int r, c;
    (void)page;
    pvrio_reg(PT_ALPHA_REF_addr, 0x80);
    ta_list_begin(ListType_Opaque);
    for(r = 0; r < 2; r++)
        for(c = 0; c < N; c++)
            poly(ListType_Opaque, DepthMode_Greater, &geom[r][c], 64 + 32 * r, 0xffff0000);
    ta_list_end();
    ta_list_begin(ListType_Punch_Through);
    for(r = 0; r < 2; r++)
        for(c = 0; c < N; c++)
            poly(ListType_Punch_Through, DepthMode_Greater, &geom[r][c], 32 * r, 0xffff0000);
    ta_list_end();
    ta_list_begin(ListType_Translucent);
    for(r = 0; r < 4; r++)
        for(c = 0; c < N; c++)
            poly(ListType_Translucent, DepthMode_Greater, &geom[r & 1][c], 32 * r, 0x8000ff00);
    ta_list_end();
}
