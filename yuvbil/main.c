/* YUV422 with bilinear filtering: is the blend made of the YUV values (and converted afterwards) or of the
   texels' RGB values (each converted, and clamped, first)?

   The conversion is linear except where it clamps, so the two orders give the same colours unless a texel's
   red, green or blue clamps. Every cell is a 32x32 quad showing two texels' width of an 8x8 texture whose
   one half is colour A and the other colour B (so that the two texels of every YUV pair are the same
   colour): 8 pixels of A, 16 pixels in which the weight goes from A to B, 8 pixels of B.

   Rows of five cells, one row per pair of colours:
     cell 0  twiddled, bilinear, A left and B right        1  twiddled, bilinear, A above and B below
          2  not twiddled, bilinear, left / right          3  not twiddled, bilinear, above / below
          4  twiddled, point sampled, left / right (the two colours as they are)
   page 0: YUV422. page 1: the same layouts with RGB565 textures of plain colours (the filter by itself).

   Which two texels share their U and V? A texel's word is Y and one chroma byte. Not twiddled, the words of a
   row alternate U, V. A console shows that a twiddled texture is read the same way: the texel at an even x
   has U, its neighbour at x + 1 has V, in the same row, although the two are not next to each other in
   memory. (Pages 0 and 1 are built that way; the first version of this test gave the U and V words by
   memory order, and the console showed rows of two different colours.)
   page 2: 8x8 textures whose texels all differ, each shown whole on 64x64 pixels: not twiddled and twiddled,
   point sampled and bilinear (columns), six textures (rows). */
#define NUM_PAGES 3
#include "pvrtest2.h"

#define NUM_PAIRS 14
static const uint8_t pairs[NUM_PAIRS][6] = {        /* Y, U, V of A and of B */
    { 128, 128, 255,  128, 128, 128 },      /* A's red clamps at 255 */
    { 128, 128,   0,  128, 128, 128 },      /* A's red clamps at 0 */
    { 128, 255, 128,  128, 128, 128 },      /* A's blue clamps at 255 */
    { 128,   0, 128,  128, 128, 128 },      /* A's blue clamps at 0 */
    { 255, 128, 255,    0, 128, 128 },      /* white-ish with red far over, against black */
    { 255, 255, 255,    0,   0,   0 },      /* everything clamps, both ways */
    {  16,   0,   0,  240, 255, 255 },
    { 128, 128, 160,  128, 128, 128 },      /* nothing clamps: both orders agree */
    { 128, 100, 150,  140, 150, 110 },      /* nothing clamps */
    {  64, 200,  60,  200,  60, 200 },
    { 128, 128, 255,  128, 128,   0 },      /* red clamps high on one side, low on the other */
    { 200, 128, 220,  200, 128, 128 },      /* A's red clamps, same Y */
    {   0, 128, 128,  255, 128, 128 },      /* greys: Y only */
    {  30, 128,  60,  128, 128, 128 },      /* A's red below 0, green above its Y */
};
static uint32_t yuv_txr[NUM_PAIRS][4];      /* twiddled H, twiddled V, not twiddled H, not twiddled V */
static uint32_t rgb_txr[NUM_PAIRS][4];

static void make(int p) {
    uint16_t t[64];
    int k, i;
    for(k = 0; k < 8; k++) {                /* bit 0: split above / below; bit 1: not twiddled; bit 2: RGB565 */
        for(i = 0; i < 64; i++) {
            int x, y, b;
            if(k & 2) { x = i & 7; y = i >> 3; }
            else {                          /* twiddled: y in the even bits of the index, x in the odd ones */
                y = (i & 1) | ((i >> 1) & 2) | ((i >> 2) & 4);
                x = ((i >> 1) & 1) | ((i >> 2) & 2) | ((i >> 3) & 4);
            }
            b = ((k & 1) ? y : x) >= 4;
            if(k & 4) {
                static const uint16_t c565[4] = { 0xf800, 0x07e0, 0x001f, 0xffff };
                t[i] = b ? c565[(p + 1) & 3] : c565[p & 3];
                if(p >= 8) t[i] = b ? (uint16_t)(p * 0x1234 + 0x0421) : (uint16_t)(p * 0x4321 + 0x8410);
            } else {
                const uint8_t *c = &pairs[p][b ? 3 : 0];
                t[i] = (uint16_t)((c[0] << 8) | ((x & 1) ? c[2] : c[1]));   /* even x: Y and U, odd x: Y and V */
            }
        }
        {
            uint32_t at = pvr_tex_alloc(sizeof(t));
            pvr_tex_load(at, t, sizeof(t));
            if(k & 4) rgb_txr[p][k & 3] = at; else yuv_txr[p][k] = at;
        }
    }
}

#define NUM_TEX 6
static uint32_t all_txr[NUM_TEX][2];        /* not twiddled, twiddled */

/* texel (x, y) of texture n: Y << 8 | chroma byte */
static uint16_t texel(int n, int x, int y) {
    int Y, C;
    switch(n) {
    case 0: Y = 40 + 20 * x + 4 * y; C = 60 + 18 * x + 3 * y; break;            /* smooth, hardly any clamping */
    case 1: Y = 128; C = (x & 1) ? 60 : 200; break;                             /* one colour: U 200, V 60 */
    case 2: Y = 255 - 30 * x; C = (x * 37 + y * 91 + 13) & 255; break;
    case 3: rng_state = (uint32_t)(x * 8 + y) * 2654435761u + 99; Y = rng() & 255; C = rng() & 255; break;
    case 4: Y = 16 + 30 * y; C = (x & 1) ? 220 - 25 * y : 30 + 25 * y; break;
    default: Y = C = ((x + y) & 1) ? 255 : 0; break;
    }
    return (uint16_t)((Y & 255) << 8 | (C & 255));
}

static void test_setup(void) {
    uint16_t t[64];
    int p, n, k, i;
    for(p = 0; p < NUM_PAIRS; p++)
        make(p);
    for(n = 0; n < NUM_TEX; n++)
        for(k = 0; k < 2; k++) {
            for(i = 0; i < 64; i++) {
                int x = i & 7, y = i >> 3;
                if(k) {
                    y = (i & 1) | ((i >> 1) & 2) | ((i >> 2) & 4);
                    x = ((i >> 1) & 1) | ((i >> 2) & 2) | ((i >> 3) & 4);
                }
                t[i] = texel(n, x, y);
            }
            all_txr[n][k] = pvr_tex_alloc(sizeof(t));
            pvr_tex_load(all_txr[n][k], t, sizeof(t));
        }
}

static void test_describe(void) {
    static const char *kind[5] = { "twiddled bilinear, left/right", "twiddled bilinear, above/below", "not twiddled bilinear, left/right",
                                   "not twiddled bilinear, above/below", "twiddled point, left/right" };
    int page, p, k;
    static const char *kind2[4] = { "not twiddled, point", "twiddled, point", "not twiddled, bilinear", "twiddled, bilinear" };
    for(page = 0; page < 2; page++)
        for(p = 0; p < NUM_PAIRS; p++)
            for(k = 0; k < 5; k++)
                label(page, p * COLS + k, "%s A %d,%d,%d B %d,%d,%d: %s", page ? "565" : "YUV", pairs[p][0], pairs[p][1], pairs[p][2],
                      pairs[p][3], pairs[p][4], pairs[p][5], kind[k]);
    for(p = 0; p < NUM_TEX; p++)
        for(k = 0; k < 4; k++)
            label_span(2, 2 * p * COLS + 2 * k, 2, 2, "YUV texture %d, %s", p, kind2[k]);
}

static void test_page(int page) {
    int p, k;
    ta_list_begin(ListType_Translucent);
    for(p = 0; p < (page == 2 ? NUM_TEX : NUM_PAIRS); p++)
        for(k = 0; k < (page == 2 ? 4 : 5); k++) {
            const int layout = k == 4 ? 0 : k;
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
            isp.DepthMode = DepthMode_Always;
            isp.ZWriteDis = 1;
            isp.Texture = 1;
            tsp.SrcInstr = BlendInstr_One;
            tsp.DstInstr = BlendInstr_Zero;
            tsp.FogCtrl = FogCtrl_None;
            tsp.UseAlpha = 1;
            tsp.ShadInstr = ShadInstr_Decal;
            tsp.FilterMode = k == 4 ? FilterMode_Point : FilterMode_Bilinear;
            tsp.MipMapD = 4;
            tsp.TexU = 0;                           /* 8 texels */
            tsp.TexV = 0;
            tcw.PixelFmt = page ? PixelFmt_565 : PixelFmt_YUV422;
            tcw.ScanOrder = (layout & 2) != 0;      /* 1: not twiddled */
            tcw.TexAddr = (page ? rgb_txr[p][layout] : yuv_txr[p][layout]) >> 3;
            if(page == 2) {
                tsp.FilterMode = k >= 2 ? FilterMode_Bilinear : FilterMode_Point;
                tcw.PixelFmt = PixelFmt_YUV422;
                tcw.ScanOrder = !(k & 1);
                tcw.TexAddr = all_txr[p][k & 1] >> 3;
                ta_polygon(pcw, isp, tsp, tcw);
                quad3(cell_px(2 * p * COLS + 2 * k), cell_py(2 * p * COLS + 2 * k), 64, 64, F32B_ONE, F32B_ZERO, F32B_ZERO, F32B_ONE, F32B_ONE,
                      0xffffffff, 0);
                continue;
            }
            ta_polygon(pcw, isp, tsp, tcw);
            /* texels 3 and 4 in both directions: u, v from 3/8 to 5/8 */
            quad3(cell_px(p * COLS + k), cell_py(p * COLS + k), 32, 32, F32B_ONE, f32b_scaled(3, -3), f32b_scaled(3, -3),
                  f32b_scaled(5, -3), f32b_scaled(5, -3), 0xffffffff, 0);
        }
    ta_list_end();
}
