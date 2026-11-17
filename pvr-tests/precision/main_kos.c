/* Numeric precision of the depth buffer and of triangle setup. Analyze with precision.py.

   Depths are given as float bit patterns. bases[] has 14 depths: mantissas 000000, 7fffff, 400000, 000001,
   2aaaaa, 555555, 00ffff, 7f0000 at exponent 0 (1.0 .. 2.0), and 000000, 7fffff, 555555 at exponents -10
   and +10. "A" is a red quad over the whole cell that always passes and writes depth; "B" is green, on the
   inner 24x24 pixels (pages 0-1) or the whole cell (page 2).

   page 0  row = base. columns 0-9: B at base + 2^c units in the last place, compare GREATER;
           columns 10-19: B at base - 2^(c-10) units, compare LESS. Green = the difference was resolved.
   page 1  row = base, column c: B at base + (c - 10) units, compare EQUAL. Green = counted as equal.
   page 2  as page 1, B with exactly A's vertices.
   page 3  interpolated depth. A slopes from z0 at the cell's left edge to z0 * (1 + 2^-k) at its right edge;
           B is flat at z0 * (1 + 2^-(k+1)), the value A has at the middle, compare GREATER. B should win
           on the left half. Column = k - 1 (k = 1..20); rows 0-4: z0 = 1.0, 1.5, 1000, 0.001, 1.9999999;
           rows 5-9: the same sloping top to bottom.
   page 4  as page 3 with z0 = 1.0, row r = k = r + 6 (6..20), at each of the 20 columns: does it depend on
           where on the screen the polygon is?
   page 5  edge position. Each pixel row y of a cell is its own 1 pixel tall quad from x0 to the cell's
           right edge, x0 = the center of pixel 16, moved by 2^-(y+1) pixels: to the right (rows 0, 2 of cells)
           or to the left (rows 1, 3). Pixel 16 is lit if x0 is at or left of its center.
           rows 0-1 at columns 0, 5, 10, 15, 19. rows 2-3: the same for a top edge (1 pixel wide quads,
           one per pixel column). rows 5-8: the same four with the quad's vertices in the opposite winding.
   pages 6-21  the same flat depth through two different triangulations, all over the screen. In every cell
           A is a flat quad at z = 1.0 split along one diagonal, B the same quad at z = 1.0 +- 2^p units in
           the last place, split along the other diagonal. Even columns: 32x32 quads; odd columns: 8x8 quads
           in the middle of the cell. Pages 6-13: B above A by 2^(page - 6), compare GREATER.
           Pages 14-21: B below A by 2^(page - 14), compare LESS. Green = resolved. */
#define NUM_PAGES 22
#include "pvrtest.h"

static const uint32_t mant[8] = { 0x000000, 0x7fffff, 0x400000, 0x000001, 0x2aaaaa, 0x555555, 0x00ffff, 0x7f0000 };
static uint32_t bases[14];
static const float z0s[5] = { 1.0f, 1.5f, 1000.0f, 0.001f, 1.9999999f };
static pvr_poly_hdr_t hdr_a, hdr_cmp[8], hdr_flat;

static float from_bits(uint32_t b) {
    union { uint32_t u; float f; } v;
    v.u = b;
    return v.f;
}

static void test_setup(void) {
    pvr_poly_cxt_t cxt;
    int i;

    for(i = 0; i < 8; i++)
        bases[i] = (127u << 23) | mant[i];
    for(i = 0; i < 3; i++) {
        bases[8 + i] = (117u << 23) | mant[i == 2 ? 5 : i];
        bases[11 + i] = (137u << 23) | mant[i == 2 ? 5 : i];
    }
    cxt_plain(&cxt, PVR_LIST_TR_POLY);
    cxt.depth.write = PVR_DEPTHWRITE_ENABLE;
    pvr_poly_compile(&hdr_a, &cxt);
    for(i = 0; i < 8; i++) {
        cxt_plain(&cxt, PVR_LIST_TR_POLY);
        cxt.depth.comparison = i;
        pvr_poly_compile(&hdr_cmp[i], &cxt);
    }
    cxt_plain(&cxt, PVR_LIST_TR_POLY);
    pvr_poly_compile(&hdr_flat, &cxt);
}

static void test_describe(void) {
    int r, c, i;

    for(r = 0; r < 14; r++)
        for(c = 0; c < 20; c++) {
            label(0, r * COLS + c, "base %08x, %s 2^%d ulp", (unsigned)bases[r], c < 10 ? "greater, +" : "less, -", c % 10);
            label(1, r * COLS + c, "base %08x, equal, %+d ulp", (unsigned)bases[r], c - 10);
            label(2, r * COLS + c, "base %08x, equal, %+d ulp, same vertices", (unsigned)bases[r], c - 10);
        }
    for(r = 0; r < 10; r++)
        for(c = 0; c < 20; c++)
            label(3, r * COLS + c, "slope %s, z0 %g, k %d", r < 5 ? "x" : "y", (double)z0s[r % 5], c + 1);
    for(r = 0; r < 15; r++)
        for(c = 0; c < 20; c++)
            label(4, r * COLS + c, "slope x, z0 1, k %d, column %d", r + 6, c);
    for(i = 6; i < 22; i++)
        for(r = 0; r < 15; r++)
            for(c = 0; c < 20; c++)
                label(i, r * COLS + c, "two triangulations, %s 2^%d ulp, %s", i < 14 ? "+" : "-", (i - 6) % 8, (c & 1) ? "8x8" : "32x32");
    for(r = 0; r < 9; r++)
        if(r != 4)
            for(c = 0; c < 5; c++)
                label(5, r * COLS + (c == 4 ? 19 : c * 5), "%s edge moved %s%s", (r % 5) < 2 ? "left" : "top",
                      (r % 5) % 2 ? "before the center" : "past the center", r > 4 ? ", other winding" : "");
}

static void sloped(int cell, float za, float zb, int vertical) {
    pvr_vertex_t v;
    int k;

    pvr_prim(&hdr_a, sizeof(hdr_a));
    v.argb = 0xffff0000; v.oargb = 0; v.u = v.v = 0.0f;
    for(k = 0; k < 4; k++) {
        v.flags = (k == 3) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        v.x = cell_x(cell) + (k & 1) * 32.0f;
        v.y = cell_y(cell) + (k >> 1) * 32.0f;
        v.z = ((vertical ? (k >> 1) : (k & 1))) ? zb : za;
        pvr_prim(&v, sizeof(v));
    }
}

static void slope_cell(int cell, float z0, int k, int vertical) {
    double step = 1.0 / (double)(1u << k);
    sloped(cell, z0, (float)(z0 * (1.0 + step)), vertical);
    quad(&hdr_cmp[PVR_DEPTHCMP_GREATER], cell_x(cell), cell_y(cell), 32.0f, 32.0f, (float)(z0 * (1.0 + step * 0.5)), 0xff00ff00);
}

/* a quad given by two corners, wound one way or the other */
static void rect(float x0, float y0, float x1, float y1, int flip) {
    pvr_vertex_t v;
    int k;

    pvr_prim(&hdr_flat, sizeof(hdr_flat));
    v.argb = 0xffffffff; v.oargb = 0; v.u = v.v = 0.0f; v.z = 1.0f;
    for(k = 0; k < 4; k++) {
        int i = flip ? (k ^ 1) : k;         /* swapping each pair reverses the strip's winding */
        v.flags = (k == 3) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        v.x = (i & 1) ? x1 : x0;
        v.y = (i >> 1) ? y1 : y0;
        pvr_prim(&v, sizeof(v));
    }
}

static void test_page(int page) {
    int r, c, i;

    pvr_list_begin(PVR_LIST_TR_POLY);
    if(page < 3) {
        for(r = 0; r < 14; r++)
            for(c = 0; c < 20; c++) {
                int cell = r * COLS + c;
                float x = cell_x(cell), y = cell_y(cell);
                uint32_t b;
                int cmp;
                if(page == 0) {
                    b = c < 10 ? bases[r] + (1u << c) : bases[r] - (1u << (c - 10));
                    cmp = c < 10 ? PVR_DEPTHCMP_GREATER : PVR_DEPTHCMP_LESS;
                }
                else {
                    b = bases[r] + (uint32_t)(c - 10);
                    cmp = PVR_DEPTHCMP_EQUAL;
                }
                quad(&hdr_a, x, y, 32.0f, 32.0f, from_bits(bases[r]), 0xffff0000);
                if(page == 2)
                    quad(&hdr_cmp[cmp], x, y, 32.0f, 32.0f, from_bits(b), 0xff00ff00);
                else
                    quad(&hdr_cmp[cmp], x + 4.0f, y + 4.0f, 24.0f, 24.0f, from_bits(b), 0xff00ff00);
            }
    }
    else if(page == 3) {
        for(r = 0; r < 10; r++)
            for(c = 0; c < 20; c++)
                slope_cell(r * COLS + c, z0s[r % 5], c + 1, r >= 5);
    }
    else if(page == 4) {
        for(r = 0; r < 15; r++)
            for(c = 0; c < 20; c++)
                slope_cell(r * COLS + c, 1.0f, r + 6, 0);
    }
    else if(page >= 6) {
        int less = page >= 14;
        uint32_t d = 1u << (less ? page - 14 : page - 6), base = 127u << 23;
        float zb = from_bits(less ? base - d : base + d);
        for(r = 0; r < 15; r++)
            for(c = 0; c < 20; c++) {
                float s = (c & 1) ? 8.0f : 32.0f, x = cell_x(r * COLS + c) + (32.0f - s) / 2, y = cell_y(r * COLS + c) + (32.0f - s) / 2;
                pvr_vertex_t v;
                int k;
                quad(&hdr_a, x, y, s, s, 1.0f, 0xffff0000);
                /* the same square, starting the strip at the top right: split along the other diagonal */
                pvr_prim(&hdr_cmp[less ? PVR_DEPTHCMP_LESS : PVR_DEPTHCMP_GREATER], sizeof(pvr_poly_hdr_t));
                v.argb = 0xff00ff00; v.oargb = 0; v.u = v.v = 0.0f; v.z = zb;
                for(k = 0; k < 4; k++) {
                    static const int order[4] = { 1, 3, 0, 2 };
                    v.flags = (k == 3) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
                    v.x = x + (order[k] & 1) * s;
                    v.y = y + (order[k] >> 1) * s;
                    pvr_prim(&v, sizeof(v));
                }
            }
    }
    else {
        for(r = 0; r < 9; r++) {
            int kind = r % 5, flip = r > 4;
            if(r == 4)
                continue;
            for(c = 0; c < 5; c++) {
                int cell = r * COLS + (c == 4 ? 19 : c * 5);
                float cx = cell_x(cell), cy = cell_y(cell);
                for(i = 0; i < 32; i++) {
                    float d = 1.0f / (float)(1u << (i + 1 > 30 ? 30 : i + 1)) * (i + 1 > 30 ? 1.0f / (float)(1u << (i + 1 - 30)) : 1.0f);
                    if(kind % 2)
                        d = -d;
                    if(kind < 2)
                        rect(cx + 16.5f + d, cy + i, cx + 32.0f, cy + i + 1.0f, flip);
                    else
                        rect(cx + i, cy + 16.5f + d, cx + i + 1.0f, cy + 32.0f, flip);
                }
            }
        }
    }
    pvr_list_finish();
}
