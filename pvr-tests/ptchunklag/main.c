/* Where a culled punch-through primitive counts in a chunk (2026-11-19). pvr-tests/ptchunkcost: triangles the ISP
   culls (facing away, small) take no room in a chunk; polly2-data f1wgp_race, tile (0, 5): the chunk ends one
   triangle early after three culled ones (two of a strip and one of a tri array, corners at 2^19), where tile (1, 7)
   of the same frame, with the same three, shows they do not take room in the chunk. Both fit one rule: the primitive
   after a culled one is checked against the chunk with the culled one's vertices still counted; then they are gone.
   This test reads it as ptchunkcost (A over the cell's upper left half, failing on columns 8-15; k filler triangles;
   X; B, uvrun's steep triangle; B's run on columns 8-15 starts at its first pixel in A's chunk, at column 8 in the
   next). Chunk size 0x23 (A, fillers, X and B in one chunk while their count is at most 33): with nothing between
   the fillers and B, B leaves A's chunk from k = 10; with the first chunk holding 10 primitives or more, nothing
   carries (ptchunkcarry_races). One page per (kind, k), k = 6 .. 11; used cells only (every other one):
     kind 0  X nothing                                                     B leaves from k = 10
     kind 1  X a triangle culled facing away                               lag: 9; free: 10
     kind 2  X a triangle culled as small (legs of 3/4 pixel)              lag: 9; free: 10
     kind 3  X two triangles culled facing away                            lag: 9; lags adding up: 8
     kind 4  X a triangle culled facing away, then a filler                lag: 9; a culled triangle costing 3: 8
     kind 5  X a sprite culled facing away                                 lag of 4: 8; of 3: 9
     kind 6  X a strip of two (CullMode small): a filler, then one culled as small   lag: 8; free: 9
     kind 7  X a triangle culled facing away, one corner 2^19 up           as kind 1 */
#define KINDS 8
#define KS 6
#define NUM_PAGES (KINDS * KS)

#include "pvr_params.h"
#include "../uvrun/cases.h"
#include "pvrtest2.h"
#include "texgen.h"

static uint32_t tex_read, tex_fail;

static uint32_t fail_texel(const void *ctx, int size, int x, int y)
{
    (void)ctx; (void)size; (void)y;
    return ((x >= 8 && x <= 15) ? 0 : 0x8000u) | 0x7c00u | (uint32_t)(x & 31);
}

static void test_setup(void)
{
    static uint32_t words[1024 * 8 / 2];
    tex_layout_t l = { TEX_RASTER, 16, 32, 8, 0 };
    int i, j;
    for(j = 0; j < 8; j++)
        for(i = 0; i < 1024; i += 2)
            words[(j * 1024 + i) / 2] = (uint32_t)i | (uint32_t)(i + 1) << 16;
    tex_read = pvr_tex_alloc(sizeof(words));
    pvrio_w64(tex_read, words, sizeof(words) / 4);
    tex_fail = tex_build(&l, fail_texel, NULL);
}

static void pt_poly(int textured, uint32_t tex, int texu, int pixfmt, int cull, int strip)
{
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Punch_Through;
    pcw.Group_En = 1;
    pcw.Strip_Len = strip ? 3 : 1;
    pcw.Texture = textured;
    isp.DepthMode = DepthMode_GreaterEqual;
    isp.CullMode = (uint32_t)cull;
    isp.Texture = textured;
    tsp.SrcInstr = BlendInstr_SrcAlpha;
    tsp.DstInstr = BlendInstr_InvSrcAlpha;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    tsp.ShadInstr = ShadInstr_Decal;
    tsp.FilterMode = FilterMode_Point;
    tsp.MipMapD = 4;
    tsp.TexU = texu;
    tsp.TexV = 0;
    tcw.PixelFmt = pixfmt;
    tcw.ScanOrder = 1;
    tcw.TexAddr = tex >> 3;
    ta_polygon(pcw, isp, tsp, tcw);
}

#define GREEN 0xff00ff00u

/* a triangle over the pixel at (fx, fy), legs of 2 pixels: drawn as wound, or wound the way CullMode negative drops */
static void tri2(int fx, int fy, int away)
{
    pt_poly(0, 0, 0, 0, away ? CullMode_Negative : CullMode_None, 0);
    ta_vertex0(f32b_int(fx), f32b_int(fy), F(3.0f), GREEN, 0);
    ta_vertex0(f32b_int(away ? fx : fx + 2), f32b_int(away ? fy + 2 : fy), F(3.0f), GREEN, 0);
    ta_vertex0(f32b_int(away ? fx + 2 : fx), f32b_int(away ? fy : fy + 2), F(3.0f), GREEN, 1);
}

/* the place of filler (or culled primitive) n in the cell at (x, y): rows 28 and 30, columns 20-29 */
static int fpx(int x, int n) { return x + 20 + n % 10; }
static int fpy(int y, int n) { return y + 28 + 2 * (n / 10 % 2); }

static void culled_sprite(int fx, int fy)
{
    TA_SpriteParam sp;
    TA_Sprite0 v;
    memset(&sp, 0, sizeof(sp));
    sp.pcw.ParaType = ParamType_Sprite;
    sp.pcw.ListType = ListType_Punch_Through;
    sp.isp.DepthMode = DepthMode_GreaterEqual;
    sp.isp.CullMode = CullMode_Negative;
    sp.tsp.SrcInstr = BlendInstr_SrcAlpha;
    sp.tsp.DstInstr = BlendInstr_InvSrcAlpha;
    sp.tsp.FogCtrl = FogCtrl_None;
    sp.tsp.UseAlpha = 1;
    sp.BaseCol = GREEN;
    ta_send(&sp, sizeof(sp));
    memset(&v, 0, sizeof(v));
    v.pcw = pcw_vertex(1);
    v.x0 = f32b_int(fx);     v.y0 = f32b_int(fy);     v.z0 = F(3.0f);   /* wound the other way round from ptchunk's */
    v.x1 = f32b_int(fx);     v.y1 = f32b_int(fy + 2); v.z1 = F(3.0f);
    v.x2 = f32b_int(fx + 2); v.y2 = f32b_int(fy + 2); v.z2 = F(3.0f);
    v.x3 = f32b_int(fx + 2); v.y3 = f32b_int(fy);
    ta_send(&v, sizeof(v));
}

/* X of a kind, at filler place n of the cell */
static void x_of(int x, int y, int kind, int n)
{
    const int fx = fpx(x, n), fy = fpy(y, n);
    switch(kind) {
    case 1:
        tri2(fx, fy, 1);
        break;
    case 2:                 /* legs of 3/4 pixel over the pixel's centre: area 9/32 */
        pt_poly(0, 0, 0, 0, CullMode_Small, 0);
        ta_vertex0(f32b_scaled(4 * fx + 1, -2), f32b_scaled(4 * fy + 1, -2), F(3.0f), GREEN, 0);
        ta_vertex0(f32b_int(fx + 1), f32b_scaled(4 * fy + 1, -2), F(3.0f), GREEN, 0);
        ta_vertex0(f32b_scaled(4 * fx + 1, -2), f32b_int(fy + 1), F(3.0f), GREEN, 1);
        break;
    case 3:
        tri2(fx, fy, 1);
        tri2(fpx(x, n + 1), fpy(y, n + 1), 1);
        break;
    case 4:
        tri2(fx, fy, 1);
        tri2(fpx(x, n + 1), fpy(y, n + 1), 0);
        break;
    case 5:
        culled_sprite(fx, fy);
        break;
    case 6:                 /* the first of the strip a filler, the second (v2, v1, v3) of area 1/4 */
        pt_poly(0, 0, 0, 0, CullMode_Small, 1);
        ta_vertex0(f32b_int(fx), f32b_int(fy), F(3.0f), GREEN, 0);
        ta_vertex0(f32b_int(fx + 2), f32b_int(fy), F(3.0f), GREEN, 0);
        ta_vertex0(f32b_int(fx), f32b_int(fy + 2), F(3.0f), GREEN, 0);
        ta_vertex0(f32b_scaled(8 * fx + 9, -3), f32b_scaled(8 * fy + 9, -3), F(3.0f), GREEN, 1);
        break;
    case 7:                 /* kind 1's with its first corner 2^19 up: in the tiles above too, after their own B */
        pt_poly(0, 0, 0, 0, CullMode_Negative, 0);
        ta_vertex0(f32b_int(fx), f32b_int(fy - (1 << 19)), F(3.0f), GREEN, 0);
        ta_vertex0(f32b_int(fx), f32b_int(fy + 2), F(3.0f), GREEN, 0);
        ta_vertex0(f32b_int(fx + 2), f32b_int(fy), F(3.0f), GREEN, 1);
        break;
    }
}

static void test_describe(void)
{
    static const char *what[KINDS] = { "nothing", "a triangle culled facing away", "a triangle culled as small",
                                       "two triangles culled facing away", "a triangle culled facing away and a filler",
                                       "a sprite culled facing away", "a strip of two, the second culled as small",
                                       "a triangle culled facing away, a corner 2^19 up" };
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 300; c++)
            if((c % 20) % 2 == 0)
                label(p, c, "A, %d fillers, %s, B; chunk 0x23", 6 + p % KS, what[p / KS]);
}

static void test_page(int page)
{
    const int kind = page / KS, k = 6 + page % KS;
    int i, c, n;
    pvrio_reg(PT_ALPHA_REF_addr, 0x80);
    pvrio_reg(ISP_FEED_CFG_addr, 0x00800008u | 0x23u << 4);
    pvr_frame.bg_color = 0x000040;
    ta_list_begin(ListType_Punch_Through);
    for(i = 0, c = 0; i < NUM_TRIS; i++) {
        const tri_t *t = &tris[i];
        int x, y;
        if(t->page != (page & 1))
            continue;
        x = cell_px(c); y = cell_py(c);
        c++;
        if((x / 32) % 2)
            continue;
        pt_poly(1, tex_fail, 2, PixelFmt_1555, CullMode_None, 0);       /* A: the cell's upper left half */
        ta_vertex3(f32b_int(x), f32b_int(y), F(2.0f), F32B_ZERO, F32B_ZERO, 0xffffffff, 0, 0);
        ta_vertex3(f32b_int(x + 32), f32b_int(y), F(2.0f), F32B_ONE, F32B_ZERO, 0xffffffff, 0, 0);
        ta_vertex3(f32b_int(x), f32b_int(y + 32), F(2.0f), F32B_ZERO, F32B_ZERO, 0xffffffff, 0, 1);
        for(n = 0; n < k; n++)
            tri2(fpx(x, n), fpy(y, n), 0);
        x_of(x, y, kind, k);
        pt_poly(1, tex_read, 7, PixelFmt_565, CullMode_None, 0);        /* B: the reading */
        for(n = 0; n < 3; n++)
            ta_vertex3(t->v[n][0], t->v[n][1], t->v[n][3], t->v[n][2], F32B_HALF, 0xffffffff, 0, n == 2);
    }
    ta_list_end();
}
