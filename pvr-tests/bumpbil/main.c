/* How a bump map is filtered bilinearly: the two bytes of its texels, S and the angle R, are filtered and
   the intensity is computed from the result (pvr-tests/bumpfn has the function). The angle goes round a circle:
   which way does the filter go from one texel's R to the next one's?

   Every cell has an 8x8 bump map of two halves, texel value "lo" and texel value "hi", and the 32 pixels of
   the cell cross from the middle of a lo texel to the middle of a hi texel: the weight of hi goes from 0 to
   1. Cell c of a pair starts c/256 texel later (c = 0..7). Rows 0-6 cross along u, rows 7-13 along v.
   Shown through (SRC_ALPHA, ZERO) with a white base colour: the picture is the intensity.

   pages 0-3: the pairs of set A (S = 0, R differs) under the lights lights_a[]
   pages 4-6: the pairs of set B (S differs, or both) under the lights lights_b[] */
#define NUM_PAGES 7
#include "pvrtest2.h"
#include "texgen.h"

typedef struct { uint8_t S0, R0, S1, R1; } pair_t;
static const pair_t pairs[2][16] = {
    { { 0, 0, 0, 64 }, { 0, 64, 0, 0 }, { 0, 0, 0, 120 }, { 0, 120, 0, 0 }, { 0, 0, 0, 127 }, { 0, 0, 0, 128 }, { 0, 0, 0, 129 }, { 0, 0, 0, 136 },
      { 0, 136, 0, 0 }, { 0, 200, 0, 10 }, { 0, 10, 0, 200 }, { 0, 250, 0, 5 }, { 0, 5, 0, 250 }, { 0, 0, 0, 255 }, { 0, 100, 0, 228 }, { 0, 228, 0, 100 } },
    { { 230, 35, 20, 0 }, { 20, 0, 230, 35 }, { 0, 0, 255, 0 }, { 255, 0, 0, 0 }, { 100, 0, 101, 0 }, { 0, 100, 255, 100 }, { 128, 10, 128, 250 }, { 128, 250, 128, 10 },
      { 230, 245, 20, 210 }, { 20, 210, 230, 245 }, { 60, 60, 200, 200 }, { 200, 200, 60, 60 }, { 0, 0, 0, 127 }, { 0, 127, 0, 0 }, { 0, 129, 0, 0 }, { 0, 255, 0, 0 } },
};
static const uint32_t lights_a[4] = { 0x80007f00, 0x80007f40, 0x0000ff00, 0x0000ff80 };
static const uint32_t lights_b[3] = { 0x00ff0000, 0x80007f00, 0x80007f40 };
static uint32_t txr[2][16][2];

typedef struct { pair_t p; int dir; } ctx_t;
static uint32_t texel(const void *vc, int size, int x, int y) {
    const ctx_t *c = (const ctx_t *)vc;
    const int hi = ((c->dir ? y : x) * 8 / size) >= 4;
    return hi ? (uint32_t)((c->p.S1 << 8) | c->p.R1) : (uint32_t)((c->p.S0 << 8) | c->p.R0);
}

static void test_setup(void) {
    static const tex_layout_t l = { TEX_TWIDDLED, 16, 8, 8, 0 };
    int s, k, d;
    for(s = 0; s < 2; s++)
        for(k = 0; k < 16; k++)
            for(d = 0; d < 2; d++) {
                ctx_t c = { pairs[s][k], d };
                txr[s][k][d] = tex_build(&l, texel, &c);
            }
}

static uint32_t light_of(int page) { return page < 4 ? lights_a[page] : lights_b[page - 4]; }

static void test_describe(void) {
    int p, k, c, d;
    for(p = 0; p < NUM_PAGES; p++)
        for(d = 0; d < 2; d++)
            for(k = 0; k < 16; k++)
                for(c = 0; c < 8; c++) {
                    const pair_t *q = &pairs[p >= 4][k];
                    label(p, d * 140 + k * 8 + c, "light %08x: S %d R %d -> S %d R %d along %s, start +%d/256 texel", (unsigned)light_of(p),
                          q->S0, q->R0, q->S1, q->R1, d ? "v" : "u", c);
                }
}

static void test_page(int page) {
    const int s = page >= 4;
    int k, c, d;

    ta_list_begin(ListType_Translucent);
    for(d = 0; d < 2; d++)
        for(k = 0; k < 16; k++)
            for(c = 0; c < 8; c++) {
                const int cell = d * 140 + k * 8 + c;
                /* texel centers 3 and 4 are at 3.5/8 and 4.5/8: a = (3.5 + c/256) / 8, b = a + 1/8 (exact) */
                f32b a = f32b_scaled(896 + c, -11), b = sf_add(a, F(0.125f)), q = F(0.25f);
                PCW pcw = { .full = 0 };
                ISP_TSP isp = { .full = 0 };
                TSP tsp = { .full = 0 };
                TCW tcw = { .full = 0 };

                pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
                pcw.ListType = ListType_Translucent;
                pcw.Group_En = 1;
                pcw.Strip_Len = 1;
                pcw.Texture = 1;
                pcw.Gouraud = 1;
                pcw.Offset = 1;
                isp.DepthMode = DepthMode_Always;
                isp.ZWriteDis = 1;
                isp.Texture = 1;
                isp.Offset = 1;
                tsp.SrcInstr = BlendInstr_SrcAlpha;
                tsp.DstInstr = BlendInstr_Zero;
                tsp.FogCtrl = FogCtrl_None;
                tsp.UseAlpha = 1;
                tsp.ShadInstr = ShadInstr_Decal;
                tsp.FilterMode = FilterMode_Bilinear;
                tsp.MipMapD = 4;
                tcw.PixelFmt = PixelFmt_BumpMap;
                tcw.TexAddr = txr[s][k][d] >> 3;
                ta_polygon(pcw, isp, tsp, tcw);
                if(d)
                    quad3(cell_px(cell), cell_py(cell), 32, 32, F32B_ONE, q, a, q, b, 0xffffffff, light_of(page));
                else
                    quad3(cell_px(cell), cell_py(cell), 32, 32, F32B_ONE, a, q, b, q, 0xffffffff, light_of(page));
            }
    ta_list_end();
}
