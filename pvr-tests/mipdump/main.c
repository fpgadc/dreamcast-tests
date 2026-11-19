/* Game triangles and their mipmap level (2026-11-19): the triangles of tris.h (gen.py, from refsw3-offline's render
   log of a polly2-data dump) drawn at their place with the corners exactly as the game sent them, and a 64x64
   mipmapped texture whose every level is one colour, so that each pixel shows the level the TSP chose there and,
   through a trilinear pass of white levels, the fraction it blends with.

   Found on sydney_race and sydney_stadium: the kerb along the track is triangles with two corners at about 2^36 and
   1/w of 10^6 (geometry behind the camera, projected), mipmapped VQ, bilinear, D adjust 2/4. Their U and V are the
   console's on every pixel (uvmeas, sets dump_sydney1 and 2), their depth too (dumprender z), and still a third of
   their pixels differ in the frame.

   Every page: the triangles in the order of tris.h, depth ALWAYS, no culling, decal, on 0x000040.
     page 0  point sampled, levels in colours, the game's D adjust: the level
     page 1  trilinear pass B, every level white: 255 times the fraction (k / 256)
     page 2  trilinear pass A, every level white: 255 times (256 - k) / 256
     page 3  page 0 with D adjust 4/4 */
#define NUM_PAGES 4
#include "pvrtest2.h"
#include "texgen.h"
#include "tris.h"

static uint32_t tex_col, tex_white;

static uint32_t hsh(uint32_t v) { v *= 2654435761u; v ^= v >> 15; v *= 2246822519u; v ^= v >> 13; return v; }
static uint32_t level_colour(const void *ctx, int size, int x, int y)
{
    (void)ctx; (void)x; (void)y;
    return hsh((uint32_t)size * 31u + 7u) & 0xffff;
}
static uint32_t white(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0xffff; }

static void test_setup(void)
{
    const tex_layout_t l = { TEX_MIP, 16, 64, 64, 0 };
    tex_col = tex_build(&l, level_colour, NULL);
    tex_white = tex_build(&l, white, NULL);
}

static void test_describe(void)
{
    static const char *what[NUM_PAGES] = { "level, the game's D", "trilinear B: fraction", "trilinear A: 1 - fraction", "level, D 4/4" };
    int p;
    for(p = 0; p < NUM_PAGES; p++)
        label_span(p, 0, 20, 15, "%s (%d triangles of tris.h)", what[p], NUM_DTRIS);
}

static void test_page(int page)
{
    int i, k;
    pvr_frame.bg_color = 0x000040;
    ta_list_begin(ListType_Opaque);
    for(i = 0; i < NUM_DTRIS; i++) {
        PCW pcw = { .full = 0 };
        ISP_TSP isp = { .full = 0 };
        TSP tsp = { .full = 0 }, game = { .full = dtris[i].tsp };
        TCW tcw = { .full = 0 };
        pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
        pcw.ListType = ListType_Opaque;
        pcw.Group_En = 1;
        pcw.Strip_Len = 1;
        pcw.Texture = 1;
        isp.DepthMode = DepthMode_Always;
        isp.CullMode = CullMode_None;
        isp.Texture = 1;
        tsp.SrcInstr = BlendInstr_One;
        tsp.DstInstr = BlendInstr_Zero;
        tsp.FogCtrl = FogCtrl_None;
        tsp.UseAlpha = 1;
        tsp.ShadInstr = ShadInstr_Decal;
        tsp.FilterMode = page == 1 ? FilterMode_TrilinearB : page == 2 ? FilterMode_TrilinearA : FilterMode_Point;
        tsp.MipMapD = page == 3 ? 4 : game.MipMapD;
        tsp.TexU = game.TexU;
        tsp.TexV = game.TexV;
        tcw.PixelFmt = PixelFmt_565;
        tcw.MipMapped = 1;
        tcw.TexAddr = (page == 1 || page == 2 ? tex_white : tex_col) >> 3;
        ta_polygon(pcw, isp, tsp, tcw);
        for(k = 0; k < 3; k++)                  /* x, y, z, u, v */
            ta_vertex3(dtris[i].v[k][0], dtris[i].v[k][1], dtris[i].v[k][2], dtris[i].v[k][3], dtris[i].v[k][4], 0xffffffff, 0, k == 2);
    }
    ta_list_end();
}
