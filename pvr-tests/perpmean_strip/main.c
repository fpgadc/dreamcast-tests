/* The mean depth of a perpendicular triangle (FPU_PERP_VAL) inside strips and in sprites, as words (2026-11-19,
   after pvr-tests/perpmean_word).

   perpmean_word: a perpendicular triangle (one strip of one triangle) stores (z1 + z2) + z3 / 3, z1 .. z3 its
   corners in the order sent, both windings. Here the triangle is the last of a longer strip (the ISP takes an odd
   triangle of a strip with its first two corners swapped, to keep the winding), or the polygon is a sprite (whose
   plane is set up from A, B, C). Every pixel of 8 tiles has a target of its own: a strip whose last triangle has the
   corners (x + 1/4, y + 1/4), (x + 1, y + 1/4), (x + 1/4, y + 1) in one of six orders (determinant 9/16) and whose
   earlier triangles lie beyond that triangle's first edge, inside the pixel (they cover no pixel center); or a sprite
   on the square (x + 1/4 .. x + 1, y + 1/4 .. y + 1) (determinant 9/16) with A at any of the four corners, B and C
   the next ones either way round. FPU_PERP_VAL 0.75: every target and decoy is perpendicular, zexfil.h's probes
   (determinants 1 and 2) are not. Opaque, DepthMode Always. Corner depths from gen.cpp (cases.h): three in four
   triples are ones whose stored mean depends on which corner is added last.
     tiles 0 .. 3   strips sent with Strip_Len 6, the target triangle 1, 2, 3, 5 of the strip
     tile 4         Strip_Len 1, triangle 1          tile 5   Strip_Len 2, triangle 3
     tiles 6, 7     sprites
   zexfil.h reads every tile's depths out (27 renders a tile); the pages show them as words, one per pixel, in the
   tile's place, black elsewhere; compare2.py hw off compares them word for word. Pages 0, 1: four tiles each. */
#define NUM_PAGES 2
#define TEST_DRAW_PAGE
#include "pvrtest2.h"
#include "zexfil.h"
#ifndef CASES_H
#define CASES_H "cases.h"
#endif
#include CASES_H

#define ZEXFIL_AT 0x580000u             /* the probes' video memory: free (pvr-tests/zexfil) */
#define PERP_VAL 0x3f400000u            /* 0.75 */

static int cur_tile;

static void scene(void);

static void test_setup(void) {
    pvrio_reg(FPU_PERP_VAL_addr, PERP_VAL);
    zexfil_use_pvrsetup(scene, ZEXFIL_AT);
}

static void test_describe(void) {
    int t;
    for(t = 0; t < NT; t++)
        label(t / 4, tiles[t].ty * COLS + tiles[t].tx, "tile %d: %s", t, tiles[t].name);
}

static f32b at(int pix, int sixteenths) { return f32b_scaled(16 * pix + sixteenths, -4); }

static void scene(void) {
    const int t = cur_tile;
    int k, i;

    ta_list_begin(ListType_Opaque);
    if(tiles[t].kind == 0) {
        PCW pcw = { .full = 0 };
        ISP_TSP isp = { .full = 0 };
        TSP tsp = { .full = 0 };
        TCW tcw = { .full = 0 };
        pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
        pcw.ListType = ListType_Opaque;
        pcw.Group_En = 1;
        pcw.Strip_Len = tiles[t].len == 1 ? 0 : tiles[t].len == 2 ? 1 : tiles[t].len == 4 ? 2 : 3;
        isp.DepthMode = DepthMode_Always;
        isp.CullMode = CullMode_None;
        tsp.SrcInstr = BlendInstr_One;
        tsp.DstInstr = BlendInstr_Zero;
        tsp.FogCtrl = FogCtrl_None;
        ta_polygon(pcw, isp, tsp, tcw);
        for(k = 0; k < 1024; k++) {
            const int X = tiles[t].tx * 32 + k % 32, Y = tiles[t].ty * 32 + k / 32;
            const cell_t *c = &cells[t][k];
            for(i = 0; i < c->nv; i++) {
                const int j = i - (c->nv - 3);              /* the target's corner, or a decoy (j < 0) */
                ta_vertex0(at(X, c->xy[i][0]), at(Y, c->xy[i][1]), j >= 0 ? c->z[j] : 0x3f800000u, 0xff808080u, i == c->nv - 1);
            }
        }
    } else {
        TA_SpriteParam s;
        TA_Sprite0 v;
        memset(&s, 0, sizeof(s));
        s.pcw.ParaType = ParamType_Sprite;
        s.pcw.ListType = ListType_Opaque;
        s.pcw.UV_16bit = 1;
        s.isp.DepthMode = DepthMode_Always;
        s.isp.CullMode = CullMode_None;
        s.tsp.FogCtrl = FogCtrl_None;
        s.tsp.SrcInstr = BlendInstr_One;
        s.tsp.DstInstr = BlendInstr_Zero;
        s.BaseCol = 0xff808080u;
        ta_send(&s, sizeof(s));
        for(k = 0; k < 1024; k++) {
            const int X = tiles[t].tx * 32 + k % 32, Y = tiles[t].ty * 32 + k / 32;
            const cell_t *c = &cells[t][k];
            memset(&v, 0, sizeof(v));
            v.pcw = pcw_vertex(1);
            v.x0 = at(X, c->xy[0][0]); v.y0 = at(Y, c->xy[0][1]); v.z0 = c->z[0];
            v.x1 = at(X, c->xy[1][0]); v.y1 = at(Y, c->xy[1][1]); v.z1 = c->z[1];
            v.x2 = at(X, c->xy[2][0]); v.y2 = at(Y, c->xy[2][1]); v.z2 = c->z[2];
            v.x3 = at(X, c->xy[3][0]); v.y3 = at(Y, c->xy[3][1]);
            ta_send(&v, sizeof(v));
        }
    }
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
        zexfil_tile(tiles[cur_tile].tx, tiles[cur_tile].ty, z[k]);
    }
    pvrio_fill32(SETUP_FB, 640 * 480, 0);
    for(k = 0; k < 4; k++)
        zexfil_show(tiles[page * 4 + k].tx, tiles[page * 4 + k].ty, z[k]);
    if(zexfil_foreign) {
        snprintf(line, sizeof(line), "PVRIO zexfil page %d: %d readings of another colour than the probe's", page, zexfil_foreign);
        pvrio_hw_say(line);
    }
}
