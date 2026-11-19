/* slivers_perp: FPU_PERP_VAL's triangles get the 1/w of a determinant of zero too (2026-11-19).

   pvr-tests/slivers_detzero: the TSP's 1/w of a triangle whose determinant is zero is the mean of its depths, ((z1 +
   z2) + z3) times 0x3eaaaaab in the setup's arithmetic, and U and colour are the third corner's over it. FPU_PERP_VAL
   (pvr-tests/perpval) makes a triangle with |det| <= its value one of determinant zero; whether the TSP's 1/w then
   takes the same path was not measured (perpval's colours do not show 1/w). Here slivers_detzero's depths and
   readouts on real triangles: per cell 16, one on each even row, corners A (4, y), B (28, y), C (4, y + 1/2) with y
   = row + 1/4 (the ISP draws pixels 4 .. 15 of the row), det 12 (B A C on odd rows: det -12, the third corner the
   same). Pages 4 s + m, m as in slivers_detzero (U with the third corner's U 64, 1/w by fog, U, colour), set 0 of
   its triples:
     s = 0  FPU_PERP_VAL 16: every triangle is taken as determinant zero
     s = 1  FPU_PERP_VAL 0: the same triangles as they are (control)

   Results (one console, 2026-11-19): 2400 of 2400 cells with the mean of slivers_detzero for 1/w; the third
   corner's depth gets 1500 (pages 0-2 wrong). */
#define NUM_PAGES 8
#include "pvr_params.h"
#include "../slivers_detzero/cases.h"
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
            label(p, c, "page %d, FPU_PERP_VAL %s, %s, cell %d (rows 0, 2, .. 30: triples %d ..)", p, p < 4 ? "16" : "0", mode[p % 4], c, 16 * c);
}

static f32b u_random(void) { return 0x42800000u | (rng() & 0x7fffffu); }
static uint32_t col_random(void) { return 0xff000000u | (rng() & 0xffffffu); }

static void test_page(int page) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    const int mode = page % 4;
    int c, r, k;

    rng_state = 1 + mode;
    pvr_frame.bg_color = 0x00ff0000;
    pvrio_reg(FPU_PERP_VAL_addr, page < 4 ? 0x41800000u : 0);
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
            const uint32_t *z = tz[0][16 * c + r];
            const f32b xa = f32b_int(cell_px(c) + 4), xb = f32b_int(cell_px(c) + 28);
            const f32b y = f32b_scaled(4 * (cell_py(c) + 2 * r) + 1, -2), yc = f32b_scaled(4 * (cell_py(c) + 2 * r) + 3, -2);
            const f32b xs[3] = { (r & 1) ? xb : xa, (r & 1) ? xa : xb, xa }, ys[3] = { y, y, yc };
            ta_polygon(pcw, isp, tsp, tcw);
            for(k = 0; k < 3; k++) {
                if(mode == 0 || mode == 2) {
                    const f32b u = (k == 2 && mode == 0) ? 0x42800000u : u_random();
                    ta_vertex3(xs[k], ys[k], z[k], u, 0x3f000000u, 0xffffffffu, 0, k == 2);
                } else
                    ta_vertex0(xs[k], ys[k], z[k], mode == 1 ? 0xff000000u : col_random(), k == 2);
            }
        }
    ta_list_end();
}
