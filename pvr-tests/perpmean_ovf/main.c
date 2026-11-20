/* Colours and texture coordinates of a perpendicular triangle (FPU_PERP_VAL) whose 1/w sum overflows (2026-11-19,
   after pvr-tests/perpmean_fogw and detinf).

   detinf at FPU_PERP_VAL 2^20 and +infinity: its perpendicular black quads at 1/w = 2^127 and the largest float come
   out white. refsw3 (pvr_tsp_plane.h) has the 1/w plane flat at the mean of the corners' 1/w, (w1 + w2) + w3 / 3,
   an infinity when the sum reaches 2^128, read by the evaluator as 2^-128; each attribute plane the third corner's
   attribute times that (an infinity, or a zero for an attribute of 0), so a colour of 0 is 2^-127 / 2^-128 (white),
   any other colour infinity / infinity, 2^-128 / 2^-128. Only black was seen. Here FPU_PERP_VAL is +infinity (every
   triangle is perpendicular) and each cell has one triangle (corners (2, 2), (30, 2), (2, 30) in the cell), opaque,
   DepthMode Always, Gouraud, with colours from one of 10 sets (third corner: 0, 0x010101, 0x808080, 0xff0000,
   0x00ff00, 0x0000ff, 0xffffff, 0x123456 and two random; the other corners random) and 1/w from one of 6 sets (rows):
     D0 2^127 x 3, D1 the largest float x 3, D3 (2^127, 2^126, 2^126): overflow (the last exactly at 2^128)
     D2 2^126 x 3 (1.5 2^127), D4 (2^127, 2^126, 2^126 - ulp), D5 2^125 x 3: no overflow
   Page 0 untextured; page 1 textured with uvmeas's numbers (1024 x 8, point sampled, repeating, modulate: the texel's
   number in green and blue times the colour), u of the corners 64 .. 128 (u3 = 64.5 + cell / 64), v 0.5; page 2
   the same with decal (the texel alone); page 3 untextured with the Offset bit and offset colours (which an untextured
   polygon does not use: the colours are page 0's), every triangle mirrored (the other winding).
   What a console shows (2026-11-19): the overflowing rows white in every colour set, both windings; texel 1023 (U one
   unit under a whole number) textured; the other rows the third corner's colour, and texel 0 (u3 X itself
   overflows there: 64.5 2^126). refsw3 as described: 240 of 240 regions. */
#define NUM_PAGES 4
#include "pvrtest2.h"

#define PERP_VAL 0x7f800000u            /* +infinity */
#define ND 6
#define NCOL 10
static const uint32_t DZ[ND][3] = {
    { 0x7f000000u, 0x7f000000u, 0x7f000000u },
    { 0x7f7fffffu, 0x7f7fffffu, 0x7f7fffffu },
    { 0x7e800000u, 0x7e800000u, 0x7e800000u },
    { 0x7f000000u, 0x7e800000u, 0x7e800000u },
    { 0x7f000000u, 0x7e800000u, 0x7e7fffffu },
    { 0x7e000000u, 0x7e000000u, 0x7e000000u },
};
static const char *DN[ND] = { "1/w 2^127 x 3", "1/w largest x 3", "1/w 2^126 x 3", "1/w (2^127, 2^126, 2^126)",
                               "1/w (2^127, 2^126, 2^126 - ulp)", "1/w 2^125 x 3" };
static const uint32_t C3[NCOL] = { 0xff000000u, 0xff010101u, 0xff808080u, 0xffff0000u, 0xff00ff00u, 0xff0000ffu, 0xffffffffu,
                                   0xff123456u, 0, 0 };
static uint32_t col[ND][NCOL][3], ofs[ND][NCOL][3], uu[ND][NCOL][3];
static uint32_t texture;

static void test_setup(void)
{
    static uint32_t words[1024 * 8 / 2];
    int d, c, i, j;
    for(j = 0; j < 8; j++)
        for(i = 0; i < 1024; i += 2)
            words[(j * 1024 + i) / 2] = (uint32_t)i | (uint32_t)(i + 1) << 16;
    texture = pvr_tex_alloc(sizeof(words));
    pvrio_w64(texture, words, sizeof(words) / 4);
    for(d = 0; d < ND; d++)
        for(c = 0; c < NCOL; c++) {
            for(i = 0; i < 3; i++) {
                col[d][c][i] = 0xff000000u | (rng() & 0xffffffu);
                ofs[d][c][i] = 0xff000000u | (rng() & 0x7f7f7fu);
                uu[d][c][i] = (127u + 6u) << 23 | (rng() & 0x7fffffu);
            }
            if(C3[c])
                col[d][c][2] = C3[c];
            ofs[d][c][2] = 0xff000000u | (uint32_t)c * 0x0c0c0cu;
            uu[d][c][2] = (127u + 6u) << 23 | ((uint32_t)(d * NCOL + c) << 14 | 1u << 16);   /* 64.5 + cell / 64 */
        }
    pvrio_reg(FPU_PERP_VAL_addr, PERP_VAL);
}

static void test_describe(void)
{
    static const char *pn[NUM_PAGES] = { "untextured", "textured, modulate", "textured, decal", "offset colour, mirrored" };
    int p, d, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(d = 0; d < ND; d++)
            for(c = 0; c < NCOL; c++)
                label(p, (2 * d + 1) * COLS + 2 * c, "%s; colours %d (third corner %06x); %s", DN[d], c, (unsigned)(col[d][c][2] & 0xffffff), pn[p]);
}

static void test_page(int page)
{
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    int d, c, k;
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Opaque;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Gouraud = 1;
    isp.DepthMode = DepthMode_Always;
    isp.CullMode = CullMode_None;
    isp.Gouraud = 1;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    if(page == 1 || page == 2) {
        pcw.Texture = 1;
        isp.Texture = 1;
        tsp.ShadInstr = page == 1 ? ShadInstr_Modulate : ShadInstr_Decal;
        tsp.FilterMode = FilterMode_Point;
        tsp.MipMapD = 4;
        tsp.TexU = 7;
        tsp.TexV = 0;
        tcw.PixelFmt = PixelFmt_565;
        tcw.ScanOrder = 1;
        tcw.TexAddr = texture >> 3;
    }
    if(page == 3) {
        pcw.Offset = 1;
        isp.Offset = 1;
    }
    ta_list_begin(ListType_Opaque);
    ta_polygon(pcw, isp, tsp, tcw);
    for(d = 0; d < ND; d++)
        for(c = 0; c < NCOL; c++) {
            const int x = 64 * c, y = 64 * d + 32;
            const int vx[3] = { x + 2, page == 3 ? x + 2 : x + 30, page == 3 ? x + 30 : x + 2 };
            const int vy[3] = { y + 2, page == 3 ? y + 30 : y + 2, page == 3 ? y + 2 : y + 30 };
            for(k = 0; k < 3; k++)
                ta_vertex3(f32b_int(vx[k]), f32b_int(vy[k]), DZ[d][k], uu[d][c][k], 0x3f000000u, col[d][c][k], ofs[d][c][k], k == 2);
        }
    ta_list_end();
}
