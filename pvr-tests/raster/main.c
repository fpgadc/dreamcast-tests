/* Rasterization: which pixels a polygon covers.

   page 0  an 8x8 axis-aligned white quad at (8 + i/16, 8 + j/16) in column i, row j   (i = 0..15, j = 0..14)
   page 1  a right triangle (0,0) (12,0) (0,12) at the same offsets
   page 2  two triangles sharing an edge, with random corners on a 1/16 pixel grid, drawn additively at
           brightness 100: a pixel on the shared edge that both cover shows 200, one that neither covers 0
   page 3  the same shapes as page 2, as one four-vertex strip
   page 4  culling: none / small / counter-clockwise / clockwise (rows of 20), for a clockwise and a
           counter-clockwise triangle of decreasing size (legs 16, 8, 4, 2, 1, 1/2, 1/4, 1/8, 1/16, 1/32 pixel,
           the small ones placed over a pixel center)

   The PVR is driven directly (../../common/pvrtest2.h); main_kos.c is the earlier version on KallistiOS's driver.
   Coordinates are bit patterns computed with softfp in the order the earlier version's float expressions had. */
#define NUM_PAGES 5
#include "pvrtest2.h"

static f32b shapes[256][4][2];

static void test_setup(void) {
    int c, k;

    rng_state = 4242;
    for(c = 0; c < 256; c++)
        for(k = 0; k < 4; k++) {
            /* corner k in its own quadrant of the cell, so the quad is convex-ish and the diagonal is shared */
            f32b x = sf_add(sf_add(sf_add(f32b_int(cell_px(c)), f32b_int((k & 1) * 14)), F(2.0f)), f32b_scaled(rng() % 193, -4));
            f32b y = sf_add(sf_add(sf_add(f32b_int(cell_py(c)), f32b_int((k >> 1) * 14)), F(2.0f)), f32b_scaled(rng() % 193, -4));
            shapes[c][k][0] = x;
            shapes[c][k][1] = y;
        }
}

/* leg i % 10 is 2^(4 - i) pixels: 16, 8, 4, 2, 1, 1/2, ... 1/32 */
static const char *legs_str[10] = { "16", "8", "4", "2", "1", "0.5", "0.25", "0.125", "0.0625", "0.03125" };

static void test_describe(void) {
    static const char *cull[4] = { "none", "small", "ccw", "cw" };
    int c, k;

    for(c = 0; c < 256; c++) {
        if(c / 16 < 15) {
            label(0, (c / 16) * COLS + c % 16, "quad 8x8 at +%d/16, +%d/16", c % 16, c / 16);
            label(1, (c / 16) * COLS + c % 16, "triangle at +%d/16, +%d/16", c % 16, c / 16);
        }
        if(c < 240) {
            label(2, c, "two triangles, shape %d", c);
            label(3, c, "strip, shape %d", c);
        }
    }
    for(k = 0; k < 4; k++)
        for(c = 0; c < 20; c++)
            label(4, k * 20 + c, "cull %s, %s, leg %s px", cull[k], c < 10 ? "cw" : "ccw", legs_str[c % 10]);
}

/* A translucent polygon, destination factor ZERO (white replaces) or ONE (additive), and a culling mode. */
static void polygon(int dst, int cull) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };

    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Translucent;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Gouraud = 1;
    isp.DepthMode = DepthMode_Always;
    isp.ZWriteDis = 1;
    isp.CullMode = cull;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = dst;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    ta_polygon(pcw, isp, tsp, tcw);
}

static void tri(int dst, int cull, const f32b p[3][2], uint32_t argb) {
    int k;

    polygon(dst, cull);
    for(k = 0; k < 3; k++)
        ta_vertex0(p[k][0], p[k][1], F32B_ONE, argb, k == 2);
}

static void test_page(int page) {
    int c, k;

    ta_list_begin(ListType_Translucent);
    for(c = 0; c < 256 && page < 4; c++) {
        /* 16 columns of cells on pages 0 and 1 */
        int cell = (page < 2) ? (c / 16) * COLS + c % 16 : c;
        f32b x = sf_add(sf_add(f32b_int(cell_px(cell)), F(8.0f)), f32b_scaled(c % 16, -4));
        f32b y = sf_add(sf_add(f32b_int(cell_py(cell)), F(8.0f)), f32b_scaled(c / 16, -4));
        if(page == 0 && c / 16 < 15) {
            polygon(BlendInstr_Zero, CullMode_None);
            for(k = 0; k < 4; k++)
                ta_vertex0(sf_add(x, f32b_int((k & 1) * 8)), sf_add(y, f32b_int((k >> 1) * 8)), F32B_ONE, 0xffffffff, k == 3);
        }
        else if(page == 1 && c / 16 < 15) {
            f32b t[3][2] = { { x, y }, { sf_add(x, F(12.0f)), y }, { x, sf_add(y, F(12.0f)) } };
            tri(BlendInstr_Zero, CullMode_None, t, 0xffffffff);
        }
        else if(page == 2 && c < 240) {
            f32b t1[3][2] = { { shapes[c][0][0], shapes[c][0][1] }, { shapes[c][1][0], shapes[c][1][1] }, { shapes[c][2][0], shapes[c][2][1] } };
            f32b t2[3][2] = { { shapes[c][2][0], shapes[c][2][1] }, { shapes[c][1][0], shapes[c][1][1] }, { shapes[c][3][0], shapes[c][3][1] } };
            tri(BlendInstr_One, CullMode_None, t1, 0xff646464);
            tri(BlendInstr_One, CullMode_None, t2, 0xff646464);
        }
        else if(page == 3 && c < 240) {
            polygon(BlendInstr_One, CullMode_None);
            for(k = 0; k < 4; k++)
                ta_vertex0(shapes[c][k][0], shapes[c][k][1], F32B_ONE, 0xff646464, k == 3);
        }
    }
    if(page == 4) {
        for(k = 0; k < 4; k++)
            for(c = 0; c < 20; c++) {
                f32b s = f32b_scaled(1, 4 - c % 10);
                /* the right angle just up and left of the pixel center at (16.5, 16.5) of the cell */
                f32b off = (c % 10 >= 4) ? sf_mul(s, F(0.25f)) : F32B_ZERO;       /* s < 2 */
                f32b x = sf_sub(sf_add(f32b_int(cell_px(k * 20 + c)), F(16.5f)), off);
                f32b y = sf_sub(sf_add(f32b_int(cell_py(k * 20 + c)), F(16.5f)), off);
                f32b xs = sf_add(x, s), ys = sf_add(y, s);
                f32b cw[3][2] = { { x, y }, { xs, y }, { x, ys } }, ccw[3][2] = { { x, y }, { x, ys }, { xs, y } };
                tri(BlendInstr_Zero, k, c < 10 ? cw : ccw, 0xffffffff);
            }
    }
    ta_list_end();
}
