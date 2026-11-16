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
#include "pvrtest2.h"

static const uint32_t mant[8] = { 0x000000, 0x7fffff, 0x400000, 0x000001, 0x2aaaaa, 0x555555, 0x00ffff, 0x7f0000 };
static uint32_t bases[14];
/* 1.0f, 1.5f, 1000.0f, 0.001f, 1.9999999f; the names are what the labels printed with %g */
static const f32b z0s[5] = { 0x3f800000, 0x3fc00000, 0x447a0000, 0x3a83126f, 0x3fffffff };
static const char *const z0names[5] = { "1", "1.5", "1000", "0.001", "2" };

/* Translucent, untextured polygon header with depth compare `mode`; depth is written only if `zwrite`. */
static void polygon(int mode, int zwrite) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };

    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Translucent;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Gouraud = 1;
    isp.DepthMode = mode;
    isp.ZWriteDis = !zwrite;
    isp.CullMode = CullMode_None;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    ta_polygon(pcw, isp, tsp, tcw);
}

static void test_setup(void) {
    int i;

    for(i = 0; i < 8; i++)
        bases[i] = (127u << 23) | mant[i];
    for(i = 0; i < 3; i++) {
        bases[8 + i] = (117u << 23) | mant[i == 2 ? 5 : i];
        bases[11 + i] = (137u << 23) | mant[i == 2 ? 5 : i];
    }
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
            label(3, r * COLS + c, "slope %s, z0 %s, k %d", r < 5 ? "x" : "y", z0names[r % 5], c + 1);
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

/* a red quad over a cell, depth za at the left (top, if vertical) edge and zb at the right (bottom) edge */
static void sloped(int cell, f32b za, f32b zb, int vertical) {
    int k;

    polygon(DepthMode_Always, 1);
    for(k = 0; k < 4; k++)
        ta_vertex0(f32b_int(cell_px(cell) + (k & 1) * 32), f32b_int(cell_py(cell) + (k >> 1) * 32),
                   (vertical ? (k >> 1) : (k & 1)) ? zb : za, 0xffff0000, k == 3);
}

/* z0 * (1 + 2^-q) as the double expression evaluated it: exact in a double (24 + 21 bits), rounded once to float */
static f32b z0_scaled(f32b z0, int q) {
    int e;
    uint32_t m = sf_unpack(z0, &e);
    return sf_round(0, (uint64_t)m * (((uint64_t)1 << q) + 1), e - q, 0);
}

static void slope_cell(int cell, f32b z0, int k, int vertical) {
    sloped(cell, z0, z0_scaled(z0, k), vertical);
    polygon(DepthMode_Greater, 0);
    quad0(cell_px(cell), cell_py(cell), 32, 32, z0_scaled(z0, k + 1), 0xff00ff00);     /* z0 * (1 + step * 0.5) */
}

/* a quad given by two corners (floats), wound one way or the other */
static void rect(f32b x0, f32b y0, f32b x1, f32b y1, int flip) {
    int k;

    polygon(DepthMode_Always, 0);
    for(k = 0; k < 4; k++) {
        int i = flip ? (k ^ 1) : k;         /* swapping each pair reverses the strip's winding */
        ta_vertex0((i & 1) ? x1 : x0, (i >> 1) ? y1 : y0, F32B_ONE, 0xffffffff, k == 3);
    }
}

static void test_page(int page) {
    int r, c, i;

    ta_list_begin(ListType_Translucent);
    if(page < 3) {
        for(r = 0; r < 14; r++)
            for(c = 0; c < 20; c++) {
                int cell = r * COLS + c;
                int x = cell_px(cell), y = cell_py(cell);
                uint32_t b;
                int cmp;
                if(page == 0) {
                    b = c < 10 ? bases[r] + (1u << c) : bases[r] - (1u << (c - 10));
                    cmp = c < 10 ? DepthMode_Greater : DepthMode_Less;
                }
                else {
                    b = bases[r] + (uint32_t)(c - 10);
                    cmp = DepthMode_Equal;
                }
                polygon(DepthMode_Always, 1);
                quad0(x, y, 32, 32, bases[r], 0xffff0000);
                polygon(cmp, 0);
                if(page == 2)
                    quad0(x, y, 32, 32, b, 0xff00ff00);
                else
                    quad0(x + 4, y + 4, 24, 24, b, 0xff00ff00);
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
                slope_cell(r * COLS + c, F32B_ONE, r + 6, 0);
    }
    else if(page >= 6) {
        int less = page >= 14;
        uint32_t d = 1u << (less ? page - 14 : page - 6), base = 127u << 23;
        f32b zb = less ? base - d : base + d;
        for(r = 0; r < 15; r++)
            for(c = 0; c < 20; c++) {
                int s = (c & 1) ? 8 : 32, x = cell_px(r * COLS + c) + (32 - s) / 2, y = cell_py(r * COLS + c) + (32 - s) / 2;
                int k;
                polygon(DepthMode_Always, 1);
                quad0(x, y, s, s, F32B_ONE, 0xffff0000);
                /* the same square, starting the strip at the top right: split along the other diagonal */
                polygon(less ? DepthMode_Less : DepthMode_Greater, 0);
                for(k = 0; k < 4; k++) {
                    static const int order[4] = { 1, 3, 0, 2 };
                    ta_vertex0(f32b_int(x + (order[k] & 1) * s), f32b_int(y + (order[k] >> 1) * s), zb, 0xff00ff00, k == 3);
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
                int cx = cell_px(cell), cy = cell_py(cell);
                for(i = 0; i < 32; i++) {
                    f32b d = sf_scale(F32B_ONE, -(i + 1));      /* 1.0f / 2^(i+1), all exact */
                    if(kind % 2)
                        d = sf_neg(d);
                    if(kind < 2)        /* rect(cx + 16.5f + d, cy + i, cx + 32.0f, cy + i + 1.0f) */
                        rect(sf_add(sf_add(f32b_int(cx), F(16.5f)), d), f32b_int(cy + i), f32b_int(cx + 32), f32b_int(cy + i + 1), flip);
                    else                /* rect(cx + i, cy + 16.5f + d, cx + i + 1.0f, cy + 32.0f) */
                        rect(f32b_int(cx + i), sf_add(sf_add(f32b_int(cy), F(16.5f)), d), f32b_int(cx + i + 1), f32b_int(cy + 32), flip);
                }
            }
        }
    }
    ta_list_end();
}
