/* The TSP's 1/w of a perpendicular triangle (FPU_PERP_VAL), read through the fog table (2026-11-19, after
   pvr-tests/perpmean_word; README perpval).

   A perpendicular triangle's colours are its third corner's (covmeas, perpval), whatever 1/w the TSP gives it: so its
   planes are attribute x X over X, and only the fog sees X. refsw3 took X = the third corner's 1/w (a guess). And
   detinf run with FPU_PERP_VAL 2^20 or +inf shows its perpendicular black probe quads at depths 2^127 and the largest
   float white: the TSP divided by an X that is not the third corner's 2^127.
   Here every pixel of four tiles has a perpendicular triangle of its own (corners (x + 1/4, y + 1/4), (x + 1, y + 1/4),
   (x + 1/4, y + 1), determinant 9/16, FPU_PERP_VAL 0.75), black, flat shaded, opaque, with table fog (FogCtrl 0:
   the colour goes to FOG_COL_RAM, white, by the table's alpha), FOG_DENSITY 1.0, so that the fog's w is X itself
   (1 .. 256 here). The table's index is the w's exponent (its low 3 bits) and the top 4 bits of its fraction, the
   blend between two entries the next 8 bits (refsw3's LookupFogTable): page 0 has a table of entries (255, 0), so
   that a pixel shows the blend (8 more bits of X); page 1 entries (2k, 2k) for index k, so that it shows the index.
   Pages 2, 3: the same with the triangles mirrored (the other winding).
   Corner depths (1/w), all of 1 .. 128 (gen: rng() below): tile 0 three independent ones; tile 1 the third corner
   fixed at 1.5 and the others anything; tile 2 a triple in its six orders; tile 3 three equal ones (controls). */
#define NUM_PAGES 4
#include "pvrtest2.h"

#define PERP_VAL 0x3f400000u            /* 0.75 */
static const struct { int tx, ty; } AT[4] = { { 4, 3 }, { 6, 3 }, { 8, 3 }, { 10, 3 } };
static uint32_t tz[4][1024][3];

static uint32_t rnd_z(void)              /* 1 .. 128, any 23 bit fraction */
{
    return (127u + rng() % 7u) << 23 | (rng() & 0x7fffffu);
}

static void test_setup(void)
{
    int t, k, i;
    static const int perm[6][3] = { {0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0} };
    uint32_t base[3] = { 0, 0, 0 };
    for(t = 0; t < 4; t++)
        for(k = 0; k < 1024; k++) {
            switch(t) {
            case 0: for(i = 0; i < 3; i++) tz[t][k][i] = rnd_z(); break;
            case 1: tz[t][k][0] = rnd_z(); tz[t][k][1] = rnd_z(); tz[t][k][2] = 0x3fc00000u; break;
            case 2:
                if(k % 6 == 0)
                    for(i = 0; i < 3; i++) base[i] = rnd_z();
                for(i = 0; i < 3; i++) tz[t][k][i] = base[perm[k % 6][i]];
                break;
            default: tz[t][k][0] = tz[t][k][1] = tz[t][k][2] = rnd_z(); break;
            }
        }
    pvrio_reg(FPU_PERP_VAL_addr, PERP_VAL);
    pvrio_reg(FOG_DENSITY_addr, 0x00008000u);           /* mantissa 0x80 (1.0), exponent 0 */
    pvrio_reg(FOG_COL_RAM_addr, 0x00ffffffu);
}

static void test_describe(void)
{
    static const char *what[4] = { "three corners anything", "the third corner at 1.5", "a triple in its six orders",
                                    "three equal corners" };
    int p, t;
    for(p = 0; p < NUM_PAGES; p++)
        for(t = 0; t < 4; t++)
            label(p, AT[t].ty * COLS + AT[t].tx, "%s, %s%s", what[t], p & 1 ? "fog table: the index" : "fog table: the blend",
                  p >= 2 ? ", mirrored" : "");
}

static void test_page(int page)
{
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    int t, k;
    for(k = 0; k < 128; k++)
        pvrio_reg(FOG_TABLE_START_addr + 4 * (uint32_t)k, page & 1 ? (uint32_t)(2 * k) * 0x101u : 0x00ffu);
    ta_list_begin(ListType_Opaque);
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Opaque;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    isp.DepthMode = DepthMode_Always;
    isp.CullMode = CullMode_None;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_Table;
    ta_polygon(pcw, isp, tsp, tcw);
    for(t = 0; t < 4; t++)
        for(k = 0; k < 1024; k++) {
            const int X = AT[t].tx * 32 + k % 32, Y = AT[t].ty * 32 + k / 32;
            const f32b x0 = f32b_scaled(4 * X + 1, -2), y0 = f32b_scaled(4 * Y + 1, -2);
            const f32b x1 = f32b_int(X + 1), y1 = f32b_int(Y + 1);
            ta_vertex0(x0, y0, tz[t][k][0], 0xff000000u, 0);
            if(page >= 2) {
                ta_vertex0(x0, y1, tz[t][k][1], 0xff000000u, 0);
                ta_vertex0(x1, y0, tz[t][k][2], 0xff000000u, 1);
            } else {
                ta_vertex0(x1, y0, tz[t][k][1], 0xff000000u, 0);
                ta_vertex0(x0, y1, tz[t][k][2], 0xff000000u, 1);
            }
        }
}
