/* The depths of pvr-tests/zbad's triangles read against the background (2026-11-19).

   zbad: a triangle with one corner at a NaN, an infinity or a depth whose plane overflows has depths beyond 2^100
   in size, with a zero 16 pixels into the tile; polygon probes cannot go there (their own planes overflow). The
   background can: a positive background is stored as itself (cut to 19 bits), a negative one of 64 and more too, an
   exponent field of 255 is 254 (pvr-tests/bgneg, bgbig). Every page one background B; every cell a triangle of
   zbad's (textured green, decal), DepthMode Greater: green where its depth is above B in the ISP's order
   (pvr-tests/zsign), the background colour where not.
   Cells: row r < 3, column c: zbad's geometry r (0 TL, 1 TR, 2 BL at S), the bad corner at S[c % 10], the other two
   at +0.5 (c < 10) or -0.5. Pages: B = BG[page]: -2^127 .. -2^100, 2^100 .. 2^127, 1.5 2^127, the largest float. */
#define NUM_PAGES 58
#include "pvrtest2.h"
#include "texgen.h"

static const uint32_t S[10] = { 0x7fc00000u, 0xffc00000u, 0x7f800000u, 0xff800000u, 0x7f7fffffu,
                                0xff7fffffu, 0x7f000000u, 0xff000000u, 0x7149f2cau, 0x71800000u };
static const char *SNAME[10] = { "NaN", "-NaN", "+inf", "-inf", "max", "-max", "2^127", "-2^127", "1e30", "2^100" };
static uint32_t texG;

static uint32_t bg(int page)
{
    if(page < 28)
        return 0x80000000u | (uint32_t)(254 - page) << 23;         /* -2^127 .. -2^100 */
    if(page < 56)
        return (uint32_t)(227 + page - 28) << 23;                    /* 2^100 .. 2^127 */
    return page == 56 ? 0x7f400000u : 0x7f7fffffu;
}

static uint32_t green(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0x07e0; }

static void test_setup(void)
{
    const tex_layout_t l = { TEX_TWIDDLED, 16, 8, 8, 0 };
    texG = tex_build(&l, green, NULL);
}

static void test_describe(void)
{
    static const char *geo[3] = { "TL bad", "TR bad", "BL bad" };
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 60; c++)
            label(p, c, "%s at %s, others %s, background %08x", geo[c / 20], SNAME[c % 10], c % 20 < 10 ? "0.5" : "-0.5", (unsigned)bg(p));
}

static void vtx(int x, int y, uint32_t z, int end)
{
    ta_vertex3(f32b_int(x), f32b_int(y), z, 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, end);
}

static void test_page(int page)
{
    int c;
    pvr_frame.bg_color = 0x000040;
    pvr_frame.bg_depth = bg(page);
    ta_list_begin(ListType_Opaque);
    for(c = 0; c < 60; c++) {
        PCW pcw = { .full = 0 };
        ISP_TSP isp = { .full = 0 };
        TSP tsp = { .full = 0 };
        TCW tcw = { .full = 0 };
        const int x = cell_px(c), y = cell_py(c), geo = c / 20;
        const uint32_t s = S[c % 10], o = c % 20 < 10 ? 0x3f000000u : 0xbf000000u;
        pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
        pcw.ListType = ListType_Opaque;
        pcw.Group_En = 1;
        pcw.Strip_Len = 1;
        pcw.Texture = 1;
        isp.DepthMode = DepthMode_Greater;
        isp.CullMode = CullMode_None;
        isp.Texture = 1;
        tsp.SrcInstr = BlendInstr_One;
        tsp.DstInstr = BlendInstr_Zero;
        tsp.FogCtrl = FogCtrl_None;
        tsp.UseAlpha = 1;
        tsp.ShadInstr = ShadInstr_Decal;
        tsp.FilterMode = FilterMode_Point;
        tsp.MipMapD = 4;
        tcw.PixelFmt = PixelFmt_565;
        tcw.TexAddr = texG >> 3;
        ta_polygon(pcw, isp, tsp, tcw);
        if(geo == 1) { vtx(x, y, o, 0); vtx(x + 32, y, s, 0); vtx(x, y + 32, o, 1); }
        else if(geo == 2) { vtx(x, y, o, 0); vtx(x + 32, y, o, 0); vtx(x, y + 32, s, 1); }
        else { vtx(x, y, s, 0); vtx(x + 32, y, o, 0); vtx(x, y + 32, o, 1); }
    }
    ta_list_end();
}
