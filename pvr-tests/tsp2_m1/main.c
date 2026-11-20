/* tsp2_m1: the mipmap level and the colours at a pixel whose 1/w is exactly -1 grid unit (2026-11-20). See gen.cpp.

   texfar_m1's triangle per cell (TL (x0, y0 + 15) z, TR (x0 + 64, y0 + 15) z t, BL (x0, y0 + 16) -z; 1/w exactly
   -1 unit at the first pixel of the cell's line 15, Z[] from gen.cpp, which checks it with refsw3), drawn on that
   line only. The divider reads that 1/w as 0xFFFF 2^-143 (texfar_m1). Here what the level unit and the colours make
   of it; every other pixel of the line has a 1/w a few units below zero.
     rows 0-9   a mipmapped texture: TexU 0 .. 5 by column (c % 6), D adjust 1/4, 4/4, 15/4 (c / 6), columns 18, 19
                DCalcCtrl 1 (64, D 4/4 and 8, D 15/4); U, V 1/4 at every corner (rows 0-4: flat, so K is zero: the
                level from w alone) or U at TR 1/4 + s (rows 5-9, s from gen.cpp); 1/w 2^k, k = -20, 0, 30, 60, 100
       page 0  point sampled, level colours (texel of the level 2^l wide: red 3 l, green 5 l, blue 31 - 2 l)
       page 1  trilinear pass B of white levels: 255 times the fraction (k / 256)
       page 2  trilinear pass A of white levels: 255 times (256 - k) / 256
     rows 10-14 colours at every corner (columns 14-19: one colour at all three), 1/w 2^k, k = -100, -60, -20, 30, 100
       page 0  Gouraud, untextured: the base colour
       page 1  Gouraud, a black 8x8 texture, decal, the offset bit: the offset colour
       page 2  flat shaded, untextured (the last corner's colour: the control)
   Results (one console, 2026-11-20): refsw3 as it was, 900 of 900 regions: the level unit's w is the divider's
   reciprocal of the -1 unit (0xFFFF 2^-143), every first pixel on the last level with the fraction that gives; the
   colours there 0 or 255 by the sign of the colour / w. That 1/w read as 2^-143 or as a zero by the level unit:
   500 (every fraction region of rows 0-9 wrong).
   Opaque list, depth ALWAYS, on 0x000040. */
#define NUM_PAGES 3
#include "pvrtest2.h"
#include "texgen.h"
#include "cases.h"

static uint32_t tex_col[6], tex_white, tex_black;

static uint32_t c565(int r, int g, int b) { return (uint32_t)(r & 31) << 11 | (uint32_t)(g & 63) << 5 | (uint32_t)(b & 31); }
static uint32_t level_colour(const void *ctx, int size, int x, int y)
{
    int l = 0;
    (void)ctx; (void)x; (void)y;
    while((1 << l) < size) l++;
    return c565(3 * l, 5 * l, 31 - 2 * l);
}
static uint32_t white(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0xffff; }
static uint32_t black(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0; }

static void column(int c, int *texU, int *mipD, int *dcalc)
{
    static const int D[3] = { 1, 4, 15 };
    *texU = c < 18 ? c % 6 : (c == 18 ? 3 : 0);
    *mipD = c < 18 ? D[c / 6] : (c == 18 ? 4 : 15);
    *dcalc = c >= 18;
}

static void test_setup(void)
{
    int k;
    for(k = 0; k <= 5; k++) {
        const tex_layout_t l = { TEX_MIP, 16, 8 << k, 8 << k, 0 };
        tex_col[k] = tex_build(&l, level_colour, NULL);
    }
    {
        const tex_layout_t l = { TEX_MIP, 16, 256, 256, 0 };
        tex_white = tex_build(&l, white, NULL);
    }
    {
        const tex_layout_t l = { TEX_TWIDDLED, 16, 8, 8, 0 };
        tex_black = tex_build(&l, black, NULL);
    }
}

static void test_describe(void)
{
    static const int K[15] = { -20, 0, 30, 60, 100, -20, 0, 30, 60, 100, -100, -60, -20, 30, 100 };
    static const char *mip[3] = { "level", "trilinear B: fraction", "trilinear A: 1 - fraction" };
    static const char *col[3] = { "base colour", "offset colour", "flat colour" };
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 300; c++) {
            const int row = c / 20;
            if(row < 10) {
                int texU, mipD, dcalc;
                column(c % 20, &texU, &mipD, &dcalc);
                label(p, c, "%s, 1/w 2^%d, %s, %d texels, D %d/4%s", mip[p], K[row], row < 5 ? "U flat" : "U sloped", 8 << texU, mipD,
                      dcalc ? ", DCalcCtrl 1" : "");
            } else
                label(p, c, "%s, 1/w 2^%d, %s", col[p], K[row], c % 20 >= 14 ? "one colour" : "a colour per corner");
        }
}

static void test_page(int page)
{
    int c, k;
    pvr_frame.bg_color = 0x000040;
    ta_list_begin(ListType_Opaque);
    for(c = 0; c < 300; c++) {
        PCW pcw = { .full = 0 };
        ISP_TSP isp = { .full = 0 };
        TSP tsp = { .full = 0 };
        TCW tcw = { .full = 0 };
        const int row = c / 20;
        const int textured = row < 10 || page == 1;
        pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
        pcw.ListType = ListType_Opaque;
        pcw.Group_En = 1;
        pcw.Strip_Len = 1;
        pcw.Texture = (uint32_t)textured;
        isp.DepthMode = DepthMode_Always;
        isp.CullMode = CullMode_None;
        isp.Texture = (uint32_t)textured;
        tsp.SrcInstr = BlendInstr_One;
        tsp.DstInstr = BlendInstr_Zero;
        tsp.FogCtrl = FogCtrl_None;
        tsp.UseAlpha = 1;
        tsp.ShadInstr = ShadInstr_Decal;
        if(row < 10) {
            int texU, mipD, dcalc;
            column(c % 20, &texU, &mipD, &dcalc);
            isp.DCalcCtrl = (uint32_t)dcalc;
            tsp.FilterMode = page == 1 ? FilterMode_TrilinearB : page == 2 ? FilterMode_TrilinearA : FilterMode_Point;
            tsp.MipMapD = (uint32_t)mipD;
            tsp.TexU = (uint32_t)texU;
            tsp.TexV = (uint32_t)texU;
            tcw.PixelFmt = PixelFmt_565;
            tcw.MipMapped = 1;
            tcw.TexAddr = (page ? tex_white : tex_col[texU]) >> 3;
        } else {
            pcw.Gouraud = page != 2;
            isp.Gouraud = page != 2;
            if(page == 1) {
                pcw.Offset = 1;
                isp.Offset = 1;
                tsp.FilterMode = FilterMode_Point;
                tsp.TexU = 0;
                tsp.TexV = 0;
                tcw.PixelFmt = PixelFmt_565;
                tcw.TexAddr = tex_black >> 3;
            }
        }
        ta_polygon(pcw, isp, tsp, tcw);
        for(k = 0; k < 3; k++) {
            const uint32_t x = f32b_int(cell_px(c) + (k == 1) * 64), y = f32b_int(cell_py(c) + 15 + (k == 2));
            if(textured)
                ta_vertex3(x, y, Z[c][k], ATT[c].u[k], ATT[c].v[k], row < 10 ? 0xffffffffu : ATT[c].col[k], row < 10 ? 0 : ATT[c].ofs[k], k == 2);
            else
                ta_vertex0(x, y, Z[c][k], ATT[c].col[k], k == 2);
        }
    }
    ta_list_end();
}
