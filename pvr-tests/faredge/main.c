/* What does the ISP's edge unit do when a tile's start value overflows only through the edge's constant?

   The modvol test's two-volume polygons, read with the single-volume layout in intensity mode, give a triangle
   with a corner at (0.5, -2.56e38) whose edges the console draws as exact geometry in the screen's first tile
   column, where refsw2 (start saturated to 2^127, README "hugepos") draws a one-pixel sliver. There the tile's
   first sample point is x0 = 0.5, so dx y0 - dy x0 stays below 2^128 while the constant xb ya - xa yb does not.

   Every page has strips: triangles with two corners P1, P2 in a cell and the third, F, far along the column or
   row (+-m 2^(K-23), m a 24 bit mantissa), so that the triangle is a strip between the lines P1 F and P2 F:
     V0  four vertical strips in the screen's first tile column (x0 = 0.5), K 118 .. 127
     H0  four horizontal strips in the first tile row (y0 = 0.5), K 118 .. 127
     V1  four vertical strips in the second column (x0 = 32.5: dx y0 - dy x0 overflows first), K 116 .. 124
     H1  four horizontal strips in the second row, K 116 .. 124
   Each strip has a cross interval of its own (8 pixels apart) and a colour of its own; strips of a V and an H
   group cross in tiles (0, 0) .. (1, 1), the later one on top. Opaque list, depth 1, compare always.
   Pages 8 to 11: modvol's own corners at cell 20, as the single-volume reading of its two-volume quad gives
   them (v0' .. v3', see modvol): 8 the second triangle alone (v2', v1', v3'), depth 1; 9 the same with modvol's
   depths (NaN, -2.56e38, 0.5); 10 the whole strip v0' .. v3', depth 1; 11 the strip with modvol's depths.
   Pages 12 to 35: where does such an edge cross a tile? Per page two triangles, both shaded black: V in the
   first tile column, A = (xa, -4), B = (xb, +m 2^(K-23)) far down, C = (-4096, -44), so that tiles (0, 1 .. 14)
   are covered left of the (all but vertical) edge A B; and H, the same turned, in the first tile row. Exact
   geometry: xa (ya) pixels from the border. On the console, lines "FE page g a16 b16 K m" give them (g 0 V, 1 H,
   a and b in sixteenths).
   The background is 0x204060: triangles shaded black (determinant past 2^128) show against it.
   Scored with refsw2-offline like every pvrtest2.h test (regress.sh); nulldc3's EDGE_STARTINF switches the
   rule tried. */
#define NUM_PAGES 36
#include "pvrtest2.h"

static void test_setup(void) {
}

static int labelled[NUM_PAGES][300];
static int describing_now;

static void mark_cell(int page, int cell, const char *what) {
    if(labelled[page][cell])
        return;
    labelled[page][cell] = 1;
    label(page, cell, "%s, cell %d", what, cell);
}

static void test_describe(void) {
    int p, i;
    describing_now = 1;
    for(p = 12; p < NUM_PAGES; p++)
        for(i = 1; i < 20; i++) {
            if(i < 15) mark_cell(p, 20 * i, "V crossing");
            mark_cell(p, i, "H crossing");
        }
    for(p = 8; p < 12; p++) {
        mark_cell(p, 0, "modvol corners"); mark_cell(p, 1, "modvol corners"); mark_cell(p, 20, "modvol corners");
        mark_cell(p, 21, "modvol corners"); mark_cell(p, 40, "modvol corners");
    }
    for(p = 0; p < 8; p++) {
        for(i = 0; i < 15; i++) { mark_cell(p, 20 * i, "column 0"); mark_cell(p, 20 * i + 1, "column 1"); }
        for(i = 0; i < 20; i++) { mark_cell(p, i, "row 0"); mark_cell(p, 20 + i, "row 1"); }
    }
}

static void poly(int n, const f32b *x, const f32b *y, const f32b *z, uint32_t argb) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    int k;

    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Opaque;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    isp.DepthMode = DepthMode_Always;
    isp.ZWriteDis = 1;
    isp.CullMode = CullMode_None;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    ta_polygon(pcw, isp, tsp, tcw);
    for(k = 0; k < n; k++)
        ta_vertex0(x[k], y[k], z[k], argb, k == n - 1);
}

static void tri(const f32b x[3], const f32b y[3], uint32_t argb) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    int k;

    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Opaque;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    isp.DepthMode = DepthMode_Always;
    isp.ZWriteDis = 1;
    isp.CullMode = CullMode_None;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    ta_polygon(pcw, isp, tsp, tcw);
    for(k = 0; k < 3; k++)
        ta_vertex0(x[k], y[k], F32B_ONE, argb, k == 2);
}

static const uint32_t colours[16] = {
    0xffff0000, 0xff00ff00, 0xff0000ff, 0xffffff00, 0xffff00ff, 0xff00ffff, 0xffff8000, 0xff8000ff,
    0xff00ff80, 0xffffffff, 0xff808080, 0xff804000, 0xff408080, 0xff800040, 0xff80ff80, 0xffc0c0ff };

/* one strip: g 0 V0, 1 H0, 2 V1, 3 H1; j the cross interval */
static void strip(int g, int j) {
    const int vertical = g == 0 || g == 2, second = g >= 2;
    const int K = second ? 116 + (int)(rng() % 9) : 118 + (int)(rng() % 10);
    const int32_t m = (int32_t)(0x800000u | (rng() & 0x7fffffu));
    const int neg = (int)(rng() & 1);
    const int base = 16 * ((second ? 32 : 0) + 8 * j + 1);              /* in sixteenths of a pixel */
    const int a = base + (int)(rng() % 16), b = a + 16 * 5 + (int)(rng() % 16);
    const int f = a + (b - a) / 5 + (int)(rng() % (uint32_t)((b - a) * 3 / 5));
    const int along = vertical ? 1 + (int)(rng() % 13) : 1 + (int)(rng() % 18);   /* the cell the segment is in */
    const int pa = 16 * (32 * along + 4) + (int)(rng() % (24 * 16)), pb = 16 * (32 * along + 4) + (int)(rng() % (24 * 16));
    f32b cross[3] = { f32b_scaled(a, -4), f32b_scaled(b, -4), f32b_scaled(f, -4) };
    f32b alongv[3] = { f32b_scaled(pa, -4), f32b_scaled(pb, -4), f32b_scaled(neg ? -m : m, K - 23) };
    f32b x[3], y[3];
    static const int orders[6][3] = { { 0, 1, 2 }, { 1, 0, 2 }, { 0, 2, 1 }, { 2, 0, 1 }, { 1, 2, 0 }, { 2, 1, 0 } };
    const int *o = orders[rng() % 6];
    int k;
    for(k = 0; k < 3; k++) {
        x[k] = vertical ? cross[o[k]] : alongv[o[k]];
        y[k] = vertical ? alongv[o[k]] : cross[o[k]];
    }
    tri(x, y, colours[4 * g + j]);
}

static void test_page(int page) {
    int g, j;
    describing_now = 0;
    /* a background that is not black: a triangle whose setup determinant passes 2^128 is shaded black (the TSP's
       reciprocal of it is zero, README "hugepos"), and only shows against this */
    pvr_frame.bg_color = 0x204060;
    if(page >= 12) {
        /* V: A = (xa, -4), B = (xb, +m 2^(K-23)) far down, C = (-4096, -44): column 0, x left of the edge A B.
           H: the same turned: A = (-4, ya), B = (+m 2^(K-23), yb) far right, C = (-44, -4096): row 0, y above A B.
           Both shaded black (their determinants pass 2^128); they meet only in tile (0, 0). */
        int g;
        ta_list_begin(ListType_Opaque);
        for(g = 0; g < 2; g++) {
            const int K = 124 + (int)(rng() % 4);
            const int32_t m = (int32_t)(0x800000u | (rng() & 0x7fffffu));
            const int na = 32 + (int)(rng() % (29 * 16)), nb = (int)(rng() % 32);      /* sixteenths: 2 .. 31, 0 .. 2 */
            f32b u[3] = { f32b_scaled(na, -4), f32b_scaled(nb, -4), f32b_int(-4096) };   /* across */
            f32b v[3] = { f32b_int(-4), f32b_scaled(m, K - 23), f32b_int(-44) };          /* along */
#ifdef _arch_dreamcast
            printf("FE %d %d %d %d %d %ld\n", page, g, na, nb, K, (long)m);
#endif
            if(g == 0)
                tri(u, v, 0xffffffff);
            else
                tri(v, u, 0xffffffff);
        }
        ta_list_end();
        return;
    }
    if(page >= 8) {
        /* modvol cell 20 (x 0, y 32): the words of its block after a 3 word header, 4 words a vertex */
        TSP t = { .full = 0 };
        f32b vx[4], vy[4], vz[4], one[4] = { F32B_ONE, F32B_ONE, F32B_ONE, F32B_ONE };
        t.SrcInstr = BlendInstr_One; t.DstInstr = BlendInstr_Zero; t.FogCtrl = FogCtrl_None; t.UseAlpha = 1;
        vx[0] = t.full;          vy[0] = 0;               vz[0] = 0;                       /* TSP1, TCW1, X0 */
        vx[1] = F32B_HALF;       vy[1] = 0xff4080c0u;     vz[1] = 0xffc04020u;             /* Z0, B0, B1 */
        vx[2] = f32b_int(32);    vy[2] = F32B_HALF;       vz[2] = 0xff4080c0u;             /* Y1, Z1, B0 */
        vx[3] = F32B_ZERO;       vy[3] = f32b_int(64);    vz[3] = F32B_HALF;               /* X2, Y2, Z2 */
        ta_list_begin(ListType_Opaque);
        if(page <= 9) {
            f32b x[3] = { vx[2], vx[1], vx[3] }, y[3] = { vy[2], vy[1], vy[3] }, z[3] = { vz[2], vz[1], vz[3] };
            poly(3, x, y, page == 8 ? one : z, 0xffffffff);
        } else
            poly(4, vx, vy, page == 10 ? one : vz, 0xffffffff);
        ta_list_end();
        return;
    }
    ta_list_begin(ListType_Opaque);
    for(g = 0; g < 4; g++)
        for(j = 0; j < 4; j++)
            strip(g, j);
    ta_list_end();
}
