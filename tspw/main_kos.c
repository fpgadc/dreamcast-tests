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
#include "pvrtest.h"

static pvr_poly_hdr_t hdr_fog;

static void test_setup(void) {
    pvr_poly_cxt_t cxt;

    cxt_plain(&cxt, PVR_LIST_TR_POLY);
    cxt.gen.fog_type = PVR_FOG_TABLE;
    cxt.gen.shading = PVR_SHADE_FLAT;
    pvr_poly_compile(&hdr_fog, &cxt);
}

static void test_describe(void) {
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 240; c++)
            label(p, c, "page %d, c = %d", p, c);
}

static void quad_z(float x, float y, float w, float h, float z00, float z10, float z01, float z11) {
    pvr_vertex_t v;
    float z[4] = { z00, z10, z01, z11 };
    int k;

    pvr_prim(&hdr_fog, sizeof(hdr_fog));
    v.argb = 0xff000000; v.oargb = 0; v.u = v.v = 0.0f;
    for(k = 0; k < 4; k++) {
        v.flags = (k == 3) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        v.x = x + (k & 1) * w;
        v.y = y + (k >> 1) * h;
        v.z = z[k];
        pvr_prim(&v, sizeof(v));
    }
}

static void test_page(int page) {
    static const double base[4] = { 1.03125, 1.5, 100.25, 1.03125 };
    const double unit = 1.0 / 1048576.0;      /* 2^-20 */
    int i, c;

    for(i = 0; i < 128; i++)
        PVR_SET(PVR_FOG_TABLE_BASE + 4 * i, 0x00ff);        /* every pair is (0, 255) */
    PVR_SET(PVR_FOG_DENSITY, 0x8000);                       /* density 1.0 */
    PVR_SET(PVR_FOG_TABLE_COLOR, 0x00ffffff);

    pvr_list_begin(PVR_LIST_TR_POLY);
    for(c = 0; c < 240; c++) {
        float x = cell_x(c), y = cell_y(c);
        if(page < 6) {
            double z0 = base[page < 4 ? page : 0] * (1.0 + (c - 120) * unit);
            float z = (float)z0, zr = (float)(z0 * (1.0 + 1.0 / 4096.0));
            if(page == 3)      quad_z(x + 12, y + 12, 8, 8, z, z, z, z);
            else if(page == 4) quad_z(x, y, 32, 32, z, zr, z, zr);
            else if(page == 5) quad_z(x, y, 32, 32, z, z, zr, zr);
            else               quad_z(x, y, 32, 32, z, z, z, z);
        }
        else if(page == 6)
            quad_z(x, y, 32, 32, 1.0f, 4.0f, 1.0f, 4.0f);
        else
            quad_z(x, y, 32, 32, 32.0f, 0.52f, 32.0f, 0.52f);
    }
    pvr_list_finish();
}
