/* slivers_detzero: the TSP's 1/w, U and colour of a triangle whose determinant is zero, as numbers (2026-11-19).
   See gen.cpp.

   pvr-tests/slivers_tsp: on degenerate triangles of covmeas (determinant exactly zero) the console's 1/w is the mean
   of the three corners' depths, not the third corner's depth, and U and the colour are the third corner's times
   that over it. Here the arithmetic of the mean. Every triangle is three equal corners (a point at y = row + 1/4:
   the ISP draws the whole width of the tile on that row), 16 per cell (rows 0, 2, .. 30), each with three depths
   of its own (gen.cpp). Pages 4 s + m, set s (0, 1) of 4800 triples, read as
     m = 0  U: textured, the 1024 x 8 texture whose texel i holds the number i (RGB565: green i >> 5, blue i & 31),
            point sampled, repeating, decal (uvmeas); the third corner's U is exactly 64, so that the reading is
            the divider's product of the 16 bit 1/w and its reciprocal (uvmeas recipscan): D * r(D)
     m = 1  1/w: black, table fog, every fog table pair (0, 255), fog white (wmeas): bits 11..18 of the mantissa
     m = 2  U as m = 0 with the third corner's U anywhere in 64 .. 128 (rng)
     m = 3  colour: Gouraud, a colour per corner (rng)
   The first two corners' U are anywhere in 64 .. 128 (rng). The background is red. Translucent list, in the order
   sent, depth compare ALWAYS, no depth write.

   Results (one console, 2026-11-19): the 1/w is ((z1 + z2) + z3) * 0x3eaaaaab (the single nearest 1/3, rounded up),
   sums and product cut to 24 bits as the setup's, then to 16: all 9,600 triples on all four readings. 1/3 cut down
   (0xaaaaaa, refsw3's ISP mean) 8,143; 1/3 rounded up at 26 bits or more 9,594; sums 25 bits wide 9,474; the other orders
   9,413 and 9,379; the exact mean 9,373 (agents' tools/dzfit.cpp). The third corner's depth (refsw3's guess before):
   600 of 2400 cells. Colours (page 3, 7) are the third corner's under every rule. */
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
    static const char *mode[4] = { "U, third corner's U 64", "1/w (fog)", "U", "colour" };
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 300; c++)
            label(p, c, "page %d, set %d, %s, cell %d (rows 0, 2, .. 30: triples %d ..)", p, p / 4, mode[p % 4], c, 16 * c);
}

/* U anywhere in 64 .. 128: the bits of 64 with a random mantissa (no float arithmetic at run time) */
static f32b u_random(void) { return 0x42800000u | (rng() & 0x7fffffu); }
static uint32_t col_random(void) { return 0xff000000u | (rng() & 0xffffffu); }

static void test_page(int page) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    const int set = page / 4, mode = page % 4;
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
            const uint32_t *z = tz[set][16 * c + r];
            const f32b x = f32b_scaled(4 * (cell_px(c) + 16) + 1, -2), y = f32b_scaled(4 * (cell_py(c) + 2 * r) + 1, -2);
            ta_polygon(pcw, isp, tsp, tcw);
            for(k = 0; k < 3; k++) {
                if(mode == 0 || mode == 2) {
                    const f32b u = (k == 2 && mode == 0) ? 0x42800000u : u_random();
                    ta_vertex3(x, y, z[k], u, 0x3f000000u, 0xffffffffu, 0, k == 2);
                } else
                    ta_vertex0(x, y, z[k], mode == 1 ? 0xff000000u : col_random(), k == 2);
            }
        }
    ta_list_end();
}
