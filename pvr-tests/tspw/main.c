/* The TSP's own 1/w, read through the fog table.

   Table fog looks the table up with 1/w: the exponent and the top 4 mantissa bits pick an entry pair and the
   next 8 mantissa bits blend between the pair. Here every entry pair is (0, 255), the polygons are black and
   the fog color white, so a pixel's gray level is (almost) those 8 mantissa bits, bits 11..18 of the 23:
   12 bits of the TSP's 1/w are visible directly, and where the level steps as the depth is swept in fine
   increments shows the bits below.

   Cell c of a page has depth z0 * (1 + c * 2^-20) (c = 0..239), with z0 chosen so that a step of the 8 bit
   value falls near c = 120.
   page 0  flat 32x32 quads, z0 just under 1 + 2^-5            page 1  flat, z0 just under 1.5
   page 2  flat, z0 just under 100.25 * ... (another exponent)  page 3  flat 8x8 quads in the middle of the cell, as page 0
   page 4  as page 0 with the depth rising by z * 2^-12 along x (one step of the 8 bit value across the cell)
   page 5  the same rising along y
   page 6  depth rising 1 -> 4 along x (the same in every cell)   page 7  depth falling 32 -> 0.52 along x */
#define NUM_PAGES 8
#include "pvrtest2.h"

static void test_setup(void) {
    int i;

    for(i = 0; i < 128; i++)
        pvrio_reg(FOG_TABLE_START_addr + 4 * i, 0x00ff);    /* every pair is (0, 255) */
    pvrio_reg(FOG_DENSITY_addr, 0x8000);                    /* density 1.0 */
    pvrio_reg(FOG_COL_RAM_addr, 0x00ffffff);
}

static void test_describe(void) {
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 240; c++)
            label(p, c, "page %d, c = %d", p, c);
}

/* A flat black quad with table fog; the depth at the four corners (top left, top right, bottom left, bottom right). */
static void quad_z(int x, int y, int w, int h, f32b z00, f32b z10, f32b z01, f32b z11) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    f32b z[4];
    int k;

    z[0] = z00; z[1] = z10; z[2] = z01; z[3] = z11;
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Translucent;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    isp.DepthMode = DepthMode_Always;
    isp.ZWriteDis = 1;
    isp.CullMode = CullMode_None;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_Table;
    tsp.UseAlpha = 1;
    ta_polygon(pcw, isp, tsp, tcw);
    for(k = 0; k < 4; k++)
        ta_vertex0(f32b_int(x + (k & 1) * w), f32b_int(y + (k >> 1) * h), z[k], 0xff000000, k == 3);
}

static void test_page(int page) {
    /* the original computed in double: base * (1 + (c - 120) * 2^-20) is exact there (a product of integers of
       at most 30 bits); it was rounded to float once, and z0 * (1 + 2^-12) (also exact) once more.
       base = num / 2^den_shift: 1.03125 = 33/32, 1.5 = 3/2, 100.25 = 401/4 */
    static const int num[4] = { 33, 3, 401, 33 }, den_shift[4] = { 5, 1, 2, 5 };
    int c;

    ta_list_begin(ListType_Translucent);
    for(c = 0; c < 240; c++) {
        int x = cell_px(c), y = cell_py(c);
        if(page < 6) {
            int b = page < 4 ? page : 0;
            uint64_t n = (uint64_t)num[b] * (uint64_t)((1 << 20) + (c - 120));
            f32b z = sf_round(0, n, -(den_shift[b] + 20), 0);
            f32b zr = sf_round(0, n * 4097, -(den_shift[b] + 20 + 12), 0);
            if(page == 3)      quad_z(x + 12, y + 12, 8, 8, z, z, z, z);
            else if(page == 4) quad_z(x, y, 32, 32, z, zr, z, zr);
            else if(page == 5) quad_z(x, y, 32, 32, z, z, zr, zr);
            else               quad_z(x, y, 32, 32, z, z, z, z);
        }
        else if(page == 6)
            quad_z(x, y, 32, 32, F(1.0f), F(4.0f), F(1.0f), F(4.0f));
        else
            quad_z(x, y, 32, 32, F(32.0f), F(0.52f), F(32.0f), F(0.52f));
    }
    ta_list_end();
}
