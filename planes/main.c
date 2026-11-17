/* Which of the TSP's planes are evaluated and divided by 1/w at each pixel? Every cell has one triangle with
   corners (0, 0) (32, 0) (0, 32) of the cell and a different 1/w at each corner (0.25 .. 1.75): perspective. A
   value that is a plane divided by 1/w comes out curved along the triangle; one that is not, straight or
   flat. The pictures are compared with refsw2-offline, which takes all of these as planes divided by 1/w.
     page 0  base colour, Gouraud (untextured)            page 1  base colour, flat (untextured)
     page 2  offset colour, Gouraud (565 texture of black texels, modulate: the picture is the offset colour)
     page 3  offset colour, flat
     page 4  base alpha, Gouraud (white, blend SRC_ALPHA, ZERO: the picture is 255 x alpha)
     page 5  base alpha, flat
     page 6  U (uvmeas's texture: texel i holds the number i, 1024 wide)
     page 7  V (the same texture turned: 8 wide, 1024 tall, row i holds the number i)
   Random colours, alphas and texture coordinates (pvrtest2.h's rng: the same every run). */
#define NUM_PAGES 8
#include "pvrtest2.h"

static uint32_t tex_black, tex_u, tex_v;     /* 64 bit area addresses */

static void test_setup(void) {
    static uint32_t words[1024 * 8 / 2];
    int i, j;
    memset(words, 0, 8 * 8 * 2);
    tex_black = pvr_tex_alloc(8 * 8 * 2);
    pvrio_w64(tex_black, words, 8 * 8 * 2 / 4);
    for(j = 0; j < 8; j++)                       /* 1024 x 8: texel (i, j) holds i */
        for(i = 0; i < 1024; i += 2)
            words[(j * 1024 + i) / 2] = (uint32_t)i | (uint32_t)(i + 1) << 16;
    tex_u = pvr_tex_alloc(sizeof(words));
    pvrio_w64(tex_u, words, sizeof(words) / 4);
    for(j = 0; j < 1024; j++)                    /* 8 x 1024: texel (i, j) holds j */
        for(i = 0; i < 8; i += 2)
            words[(j * 8 + i) / 2] = (uint32_t)j | (uint32_t)j << 16;
    tex_v = pvr_tex_alloc(sizeof(words));
    pvrio_w64(tex_v, words, sizeof(words) / 4);
}

static void test_describe(void) {
    static const char *what[NUM_PAGES] = { "base colour Gouraud", "base colour flat", "offset colour Gouraud", "offset colour flat",
                                           "base alpha Gouraud", "base alpha flat", "U", "V" };
    int p;
    for(p = 0; p < NUM_PAGES; p++)
        label_span(p, 0, 20, 15, "%s, perspective", what[p]);
}

static void test_page(int page) {
    const int offset = page == 2 || page == 3, alpha = page == 4 || page == 5, tex = offset || page >= 6;
    const int gouraud = page == 0 || page == 2 || page == 4 || page >= 6;
    int c, k;

    rng_state = 1000u + (uint32_t)page;
    ta_list_begin(ListType_Translucent);
    for(c = 0; c < 300; c++) {
        PCW pcw = { .full = 0 };
        ISP_TSP isp = { .full = 0 };
        TSP tsp = { .full = 0 };
        TCW tcw = { .full = 0 };
        static const int cx[3] = { 0, 32, 0 }, cy[3] = { 0, 0, 32 };
        pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
        pcw.ListType = ListType_Translucent;
        pcw.Group_En = 1;
        pcw.Strip_Len = 1;
        pcw.Texture = tex;
        pcw.Offset = offset;
        pcw.Gouraud = gouraud;
        isp.DepthMode = DepthMode_Always;
        isp.ZWriteDis = 1;
        isp.Texture = tex;
        isp.Offset = offset;
        isp.Gouraud = gouraud;
        tsp.SrcInstr = alpha ? BlendInstr_SrcAlpha : BlendInstr_One;
        tsp.DstInstr = BlendInstr_Zero;
        tsp.FogCtrl = FogCtrl_None;
        tsp.UseAlpha = 1;
        tsp.ShadInstr = offset ? ShadInstr_Modulate : ShadInstr_Decal;
        tsp.FilterMode = FilterMode_Point;
        tsp.MipMapD = 4;
        if(tex) {
            tsp.TexU = page == 7 ? 0 : page == 6 ? 7 : 0;       /* U: 1024 x 8; V: 8 x 1024; black: 8 x 8 */
            tsp.TexV = page == 7 ? 7 : 0;
            tcw.PixelFmt = PixelFmt_565;
            tcw.ScanOrder = 1;
            tcw.TexAddr = (page == 6 ? tex_u : page == 7 ? tex_v : tex_black) >> 3;
        }
        ta_polygon(pcw, isp, tsp, tcw);
        for(k = 0; k < 3; k++) {
            const f32b x = f32b_int(cell_px(c) + cx[k]), y = f32b_int(cell_py(c) + cy[k]);
            const f32b z = f32b_scaled((int)(1 + rng() % 7), -2);               /* 0.25 .. 1.75 */
            uint32_t base = rng(), ofs = rng() & 0x00ffffffu;
            if(alpha)
                base = ((rng() & 0xffu) << 24) | 0x00ffffffu;       /* rng() has no top byte */
            if(!tex)
                ta_vertex0(x, y, z, base, k == 2);
            else {
                /* U and V: 64 .. 128 plus up to 2 across the cell, 24 bits (the reading is exact there) */
                const f32b t = f32b_scaled((int)(0x400000 + (rng() & 0xfffff)), -16);
                ta_vertex3(x, y, z, page == 6 ? t : F32B_HALF, page == 7 ? t : F32B_HALF, base | 0xff000000u, ofs, k == 2);
            }
        }
    }
    ta_list_end();
}
