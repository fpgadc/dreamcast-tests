/* What a punch-through primitive costs in a chunk (2026-11-19). pvr-tests/ptchunk measured a triangle at 3 vertices and a
   sprite at 4 (with a lag after a quad); polly2-data f1wgp_driver, f1wgp_race and vrally_rally4 have chunk boundaries
   that fit no count of 3 per triangle: their lists have triangles the ISP culls (CullMode) and strip entries whose
   first triangle is in another tile (mask 010000). Read as ptchunk: A (over the cell's upper left half, depth 2,
   fails on columns 8-15), N fillers of one kind, then B (uvrun's steep triangle, depth 1); chunk size 0x20 (30
   vertices: A, N fillers and B in one chunk while their cost is at most 30). In one chunk B's run on columns 8-15
   starts at its first pixel; with B in the next chunk it starts at column 8 (the first chunk holds 6 or more
   primitives: no carry, ptchunkcarry_races). One page per (group, N); used cells only (every other one), so that
   everything stays in its tile:
     pages  0 ..  7  N = 5 .. 12 triangles (cost 3: B leaves A's chunk from N = 9)
     pages  8 .. 15  N = 5 .. 12 triangles culled facing away (CullMode negative, wound the other way)
     pages 16 .. 23  N = 5 .. 12 triangles culled as small (CullMode small, area 9/32 pixel; FPU_CULL_VAL 1.0)
     pages 24 .. 31  N = 5 .. 12 strips of two triangles whose first lies in the unused cell to the left: in the
                     tile their object list entries have the mask 010000
     pages 32 .. 39  N = 1 .. 8 strips of two triangles in the cell (mask 110000) */
#define GROUPS 5
#define NUM_PAGES (8 * GROUPS)

#include "pvr_params.h"
#include "../uvrun/cases.h"
#include "pvrtest2.h"
#include "texgen.h"

static uint32_t tex_read, tex_fail[2];

static uint32_t fail_texel(const void *ctx, int size, int x, int y)
{
    const int all = *(const int *)ctx;
    (void)size; (void)y;
    return ((all || (x >= 8 && x <= 15)) ? 0 : 0x8000u) | 0x7c00u | (uint32_t)(x & 31);
}

static void test_setup(void)
{
    static uint32_t words[1024 * 8 / 2];
    static const int all[2] = { 0, 1 };
    tex_layout_t l = { TEX_RASTER, 16, 32, 8, 0 };
    int i, j;
    for(j = 0; j < 8; j++)
        for(i = 0; i < 1024; i += 2)
            words[(j * 1024 + i) / 2] = (uint32_t)i | (uint32_t)(i + 1) << 16;
    tex_read = pvr_tex_alloc(sizeof(words));
    pvrio_w64(tex_read, words, sizeof(words) / 4);
    for(i = 0; i < 2; i++)
        tex_fail[i] = tex_build(&l, fail_texel, &all[i]);
}

static void pt_poly(int textured, uint32_t tex, int texu, int pixfmt, int strip)
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
    isp.CullMode = CullMode_None;
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

static void test_describe(void)
{
    static const char *what[GROUPS] = { "triangles", "triangles culled facing away", "triangles culled as small",
                                        "strips with their first triangle in the cell to the left (mask 010000)", "strips of two in the cell" };
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 300; c++)
            if((c % 20) % 2 == 0)
                label(p, c, "A, %d %s, B; chunk 0x20", (p < 32 ? 5 : 1) + p % 8, what[p / 8]);
}

/* filler n of a kind in the cell at (x, y), depth 3, alpha 0xff */
static void filler(int x, int y, int kind, int n)
{
    const int fx = x + 20 + n % 10, fy = y + 28 + 2 * (n / 10 % 2);
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Punch_Through;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    isp.DepthMode = DepthMode_GreaterEqual;
    isp.CullMode = kind == 1 ? CullMode_Negative : kind == 2 ? CullMode_Small : CullMode_None;
    tsp.SrcInstr = BlendInstr_SrcAlpha;
    tsp.DstInstr = BlendInstr_InvSrcAlpha;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    ta_polygon(pcw, isp, tsp, tcw);
    switch(kind) {
    case 0: case 1:         /* legs of 2 pixels over the pixel at (fx, fy); kind 1 wound the way CullMode negative drops */
        ta_vertex0(f32b_int(fx), f32b_int(fy), F(3.0f), 0xff00ff00u, 0);
        ta_vertex0(f32b_int(kind ? fx : fx + 2), f32b_int(kind ? fy + 2 : fy), F(3.0f), 0xff00ff00u, 0);
        ta_vertex0(f32b_int(kind ? fx + 2 : fx), f32b_int(kind ? fy : fy + 2), F(3.0f), 0xff00ff00u, 1);
        break;
    case 2:                 /* legs of 3/4 pixel over the same pixel's centre: area 9/32 */
        ta_vertex0(f32b_scaled(4 * fx + 1, -2), f32b_scaled(4 * fy + 1, -2), F(3.0f), 0xff00ff00u, 0);
        ta_vertex0(f32b_int(fx + 1), f32b_scaled(4 * fy + 1, -2), F(3.0f), 0xff00ff00u, 0);
        ta_vertex0(f32b_scaled(4 * fx + 1, -2), f32b_int(fy + 1), F(3.0f), 0xff00ff00u, 1);
        break;
    case 3:                 /* first triangle in the cell to the left, the second reaching into this one's corner */
        ta_vertex0(f32b_int(x - 30), f32b_int(fy), F(3.0f), 0xff00ff00u, 0);
        ta_vertex0(f32b_int(x - 28), f32b_int(fy + 1), F(3.0f), 0xff00ff00u, 0);
        ta_vertex0(f32b_int(x - 26), f32b_int(fy), F(3.0f), 0xff00ff00u, 0);
        ta_vertex0(f32b_int(fx), f32b_int(fy + 1), F(3.0f), 0xff00ff00u, 1);
        break;
    case 4:                 /* two triangles in the corner */
        ta_vertex0(f32b_int(fx), f32b_int(fy), F(3.0f), 0xff00ff00u, 0);
        ta_vertex0(f32b_int(fx), f32b_int(fy + 1), F(3.0f), 0xff00ff00u, 0);
        ta_vertex0(f32b_int(fx + 1), f32b_int(fy), F(3.0f), 0xff00ff00u, 0);
        ta_vertex0(f32b_int(fx + 1), f32b_int(fy + 1), F(3.0f), 0xff00ff00u, 1);
        break;
    }
}

static void test_page(int page)
{
    const int group = page / 8, n = (group < 4 ? 5 : 1) + page % 8;
    int i, c, k;
    pvrio_reg(PT_ALPHA_REF_addr, 0x80);
    pvrio_reg(ISP_FEED_CFG_addr, 0x00800008u | 0x20u << 4);
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
        pt_poly(1, tex_fail[0], 2, PixelFmt_1555, 0);          /* A: the cell's upper left half */
        ta_vertex3(f32b_int(x), f32b_int(y), F(2.0f), F32B_ZERO, F32B_ZERO, 0xffffffff, 0, 0);
        ta_vertex3(f32b_int(x + 32), f32b_int(y), F(2.0f), F32B_ONE, F32B_ZERO, 0xffffffff, 0, 0);
        ta_vertex3(f32b_int(x), f32b_int(y + 32), F(2.0f), F32B_ZERO, F32B_ZERO, 0xffffffff, 0, 1);
        for(k = 0; k < n; k++)
            filler(x, y, group, k);
        pt_poly(1, tex_read, 7, PixelFmt_565, 0);              /* B: the reading */
        for(k = 0; k < 3; k++)
            ta_vertex3(t->v[k][0], t->v[k][1], t->v[k][3], t->v[k][2], F32B_HALF, 0xffffffff, 0, k == 2);
    }
    ta_list_end();
}
