/* slivers_dzspec: the mean of a determinant of zero at the limits of the setup's numbers, and table fog over a 1/w
   below zero (2026-11-19). See gen.cpp.

   pvr-tests/slivers_detzero: a triangle of determinant zero has the 1/w ((z1 + z2) + z3) * 0x3eaaaaab in the setup's
   arithmetic, U and colour the third corner's over it. Here with depths below zero, zero, denormal, huge, tiny,
   infinite and NaN (tz: per triple one kind, gen.cpp), on slivers_detzero's points (16 per cell, one on each even
   row; the ISP draws the tile's width there), pages 0-3 read as slivers_detzero's modes: U with the third corner's
   U 64, 1/w by fog (fog table pairs (0, 255), fog white: bits 11..18 of the mantissa), U, Gouraud colour.
   Pages 4-7: ordinary triangles of determinant 12 (slivers_perp's: 12 pixels of an even row) with one flat depth
   (zflat: below zero, +-0, +-inf, NaN, above zero), read as U = 64 at every corner, 1/w by fog (what does table
   fog make of a 1/w below zero?), U (random corners), colour (Gouraud). The background is red. Translucent list,
   in the order sent, depth compare ALWAYS, no depth write.

   Results (one console, 2026-11-19): 2400 of 2400 cells as refsw3 has it now. Before, 4 rows of tiny depths: a zero
   depth (exponent field 0) against one of 2^-126 .. 2^-125 of the other sign in the mean's first addition drops it
   in some cases (pvr-tests/slivers_dzzero). Table fog over a 1/w below zero: no fog (as below 1), 1,501 rows of
   page 5 and 575 of page 1; its magnitude would make 36,412 pixels grey. */
#define NUM_PAGES 8
#include "pvr_params.h"
#ifndef CASES_H
#define CASES_H "cases.h"
#endif
#include CASES_H
#include "pvrtest2.h"

static uint32_t texture;        /* 64 bit area address */

static void test_setup(void) {
    static uint32_t words[1024 * 8 / 2];        /* two 16 bit texels per word, the first in the low half */
    int i, j;

    for(j = 0; j < 8; j++)
        for(i = 0; i < 1024; i += 2)
            words[(j * 1024 + i) / 2] = (uint32_t)i | (uint32_t)(i + 1) << 16;
    texture = pvr_tex_alloc(sizeof(words));
    pvrio_w64(texture, words, sizeof(words) / 4);
    for(i = 0; i < 128; i++)
        pvrio_reg(FOG_TABLE_START_addr + 4 * i, 0x00ff);    /* every pair is (0, 255) */
    pvrio_reg(FOG_DENSITY_addr, 0x8000);                    /* density 1.0 */
    pvrio_reg(FOG_COL_RAM_addr, 0x00ffffff);
}

static void test_describe(void) {
    static const char *mode[8] = { "U, third corner's U 64", "1/w (fog)", "U", "colour", "U 64 at every corner", "1/w (fog)", "U", "colour" };
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 300; c++)
            label(p, c, "page %d, %s, %s, cell %d (rows 0, 2, .. 30: %d ..)", p, p < 4 ? "determinant zero" : "determinant 12, flat depth",
                  mode[p], c, 16 * c);
}

static f32b u_random(void) { return 0x42800000u | (rng() & 0x7fffffu); }
static uint32_t col_random(void) { return 0xff000000u | (rng() & 0xffffffu); }

static void test_page(int page) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    const int mode = page % 4;      /* 0 U (64), 1 fog, 2 U, 3 colour */
    int c, r, k;

    rng_state = 1 + page;
    pvr_frame.bg_color = 0x00ff0000;
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Translucent;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    isp.DepthMode = DepthMode_Always;
    isp.CullMode = CullMode_None;
    isp.ZWriteDis = 1;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = mode == 1 ? FogCtrl_Table : FogCtrl_None;
    tsp.UseAlpha = 1;
    if(mode == 0 || mode == 2) {
        pcw.Texture = 1;
        isp.Texture = 1;
        tsp.ShadInstr = ShadInstr_Decal;
        tsp.FilterMode = FilterMode_Point;
        tsp.MipMapD = 4;
        tsp.TexU = 7;                           /* 1024 */
        tsp.TexV = 0;                           /* 8 */
        tcw.PixelFmt = PixelFmt_565;
        tcw.ScanOrder = 1;
        tcw.TexAddr = texture >> 3;
    }
    if(mode == 3) {
        pcw.Gouraud = 1;
        isp.Gouraud = 1;
    }

    ta_list_begin(ListType_Translucent);
    for(c = 0; c < 300; c++)
        for(r = 0; r < 16; r++) {
            const int i = 16 * c + r;
            const f32b y = f32b_scaled(4 * (cell_py(c) + 2 * r) + 1, -2), yc = f32b_scaled(4 * (cell_py(c) + 2 * r) + 3, -2);
            f32b xs[3], ys[3], z[3];
            if(page < 4) {
                xs[0] = xs[1] = xs[2] = f32b_scaled(4 * (cell_px(c) + 16) + 1, -2);
                ys[0] = ys[1] = ys[2] = y;
                z[0] = tz[i][0]; z[1] = tz[i][1]; z[2] = tz[i][2];
            } else {
                const f32b xa = f32b_int(cell_px(c) + 4), xb = f32b_int(cell_px(c) + 28);
                xs[0] = (r & 1) ? xb : xa; xs[1] = (r & 1) ? xa : xb; xs[2] = xa;
                ys[0] = ys[1] = y; ys[2] = yc;
                z[0] = z[1] = z[2] = zflat[i];
            }
            ta_polygon(pcw, isp, tsp, tcw);
            for(k = 0; k < 3; k++) {
                if(mode == 0 || mode == 2) {
                    const f32b u = (mode == 0 && (k == 2 || page >= 4)) ? 0x42800000u : u_random();
                    ta_vertex3(xs[k], ys[k], z[k], u, 0x3f000000u, 0xffffffffu, 0, k == 2);
                } else
                    ta_vertex0(xs[k], ys[k], z[k], mode == 1 ? 0xff000000u : col_random(), k == 2);
            }
        }
    ta_list_end();
}
