/* rayman_ingame's bump maps on their own (2026-11-18): its four bump triangles (tris.h: corners, depths, U and V
   exact, the 64x64 bump map as the game stored it) drawn as the game draws them, over grey (0x808080, opaque), so
   that one property at a time can be changed. In the frame, refsw2 and the console differ at a few pixels of
   them, by one or two levels of intensity: their filtered R is another (dumprender poke=, README).

   As the game: translucent, flat shaded (the third corner's colours), bump map bilinear, V clamped, modulate
   alpha, SRC_ALPHA, INV_SRC_ALPHA; the light is the third corner's offset colour.
     page 0  the game's lights
     page 1  light K3 = 255, Q = 0 (the intensity shows R; S through cos)
     page 2  page 1 without the V clamp
     page 3  page 1, decal
     page 4  page 1 in the opaque list
     page 5  page 1 with every row of the bump map a copy of row 0 (it varies along U only)
     page 6  light K2 = 255 (the intensity shows S alone)
     page 7  light K3 = 255, Q = 128 */
#define NUM_PAGES 8
#include "pvrtest2.h"
#include "tris.h"

static uint32_t tex, tex_row0;

/* twiddled index of texel (x, y) of a 64x64 map */
static int tw64(int x, int y)
{
    int r = 0, b;
    for(b = 0; b < 6; b++)
        r |= ((y >> b) & 1) << (2 * b) | ((x >> b) & 1) << (2 * b + 1);
    return r;
}

static void test_setup(void)
{
    static uint32_t row0[2048];
    const uint16_t *t = (const uint16_t *)bump_tex;
    uint16_t *r = (uint16_t *)row0;
    int x, y;
    tex = pvr_tex_alloc(sizeof(bump_tex));
    pvrio_w64(tex, bump_tex, (int)(sizeof(bump_tex) / 4));
    for(y = 0; y < 64; y++)
        for(x = 0; x < 64; x++)
            r[tw64(x, y)] = t[tw64(x, 0)];
    tex_row0 = pvr_tex_alloc(sizeof(row0));
    pvrio_w64(tex_row0, row0, (int)(sizeof(row0) / 4));
}

static void test_describe(void)
{
    static const char *what[NUM_PAGES] = { "game lights", "light K3 255", "light K3 255, V not clamped", "light K3 255, decal",
                                           "light K3 255, opaque list", "light K3 255, rows all row 0", "light K2 255",
                                           "light K3 255, Q 128" };
    int p;
    for(p = 0; p < NUM_PAGES; p++)
        label_span(p, 0, 20, 15, "%s", what[p]);
}

static void test_page(int page)
{
    const int list = page == 4 ? ListType_Opaque : ListType_Translucent;
    int i, k;
    pvr_frame.bg_color = 0x000000;
    ta_list_begin(ListType_Opaque);
    {
        PCW pcw = { .full = 0 };
        ISP_TSP isp = { .full = 0 };
        TSP tsp = { .full = 0 };
        TCW tcw = { .full = 0 };
        pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
        pcw.ListType = ListType_Opaque;
        pcw.Group_En = 1;
        pcw.Strip_Len = 1;
        isp.DepthMode = DepthMode_Always;
        isp.CullMode = CullMode_None;
        tsp.SrcInstr = BlendInstr_One;
        tsp.DstInstr = BlendInstr_Zero;
        tsp.FogCtrl = FogCtrl_None;
        ta_polygon(pcw, isp, tsp, tcw);
        quad0(0, 0, 640, 480, 0x38d1b717u, 0xff808080u);        /* depth 0.0001 */
    }
    if(list == ListType_Translucent) {
        ta_list_end();
        ta_list_begin(list);
    }
    for(i = 0; i < NUM_BTRIS; i++) {
        const btri_t *t = &btris[i];
        PCW pcw = { .full = 0 };
        ISP_TSP isp = { .full = 0 };
        TSP tsp = { .full = 0x9490a1dbu };                     /* the game's */
        TCW tcw = { .full = 0 };
        pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
        pcw.ListType = list;
        pcw.Group_En = 1;
        pcw.Strip_Len = 1;
        pcw.Texture = 1;
        pcw.Offset = 1;
        isp.DepthMode = DepthMode_Greater;
        isp.CullMode = CullMode_None;
        isp.Texture = 1;
        isp.Offset = 1;
        if(page == 2)
            tsp.ClampV = 0;
        if(page == 3)
            tsp.ShadInstr = ShadInstr_Decal;
        tcw.PixelFmt = PixelFmt_BumpMap;
        tcw.TexAddr = (page == 5 ? tex_row0 : tex) >> 3;
        ta_polygon(pcw, isp, tsp, tcw);
        for(k = 0; k < 3; k++)
            ta_vertex3(t->v[k][0], t->v[k][1], t->v[k][2], t->v[k][3], t->v[k][4], t->col[k],
                       k == 2 ? (page == 0 ? t->light : page == 6 ? 0x00ff0000u : page == 7 ? 0x0000ff80u : 0x0000ff00u) : 0, k == 2);
    }
    ta_list_end();
}
