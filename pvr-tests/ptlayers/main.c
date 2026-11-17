/* Runs of the later punch-through passes when some pixels of the line were resolved before (2026-11-18).

   polly2-data shenmue_intro*: punch-through foliage several layers deep, PT_ALPHA_REF 0xff; the console's
   coordinates in the later passes fit runs that start further left than refsw2's, as if pixels resolved in an
   earlier pass took part in the later passes' runs. Every used cell (both cell coordinates even: the front
   polygons reach into the cells to the right and below) has, at the back, a triangle B of uvmeas's steep mix
   (cases.h, gen.py; texture: texel i holds the number i, point sampled, alpha 1: it passes) at depth 1, and in
   front of it single triangles (one tag each) over the whole cell whose 32 texel wide textures fail the alpha
   test (PT_ALPHA_REF 0x80, 1555 alpha 0) on some columns: B is shaded in a later pass on those columns, and its
   reading there tells where its run started.
     kind 0  A (depth 2) fails on columns 8-15
     kind 1  A fails on columns 8-15 and 20-23
     kind 2  A fails on columns 0-15
     kind 3  A fails on columns 8-15; D (untextured, alpha 0xff, depth 1.5) over columns 0-3 between A and B
     kind 4  A1 (depth 3) fails on columns 8-15, A2 (depth 2) fails on columns 12-19: B on 12-15 in the third pass
     kind 5  A fails on columns 0-15; E (untextured, alpha 0: fails, depth 2.5) over columns 0-3 in front: one layer
             more there (polly2-data shenmue_intro4, a polygon over two columns of a stack)
     kind 6  the same with E alpha 0xff (it passes)
   Two pages of triangles per kind (page = 2 kind + 0 or 1). Blend SRC_ALPHA, INV_SRC_ALPHA. */
#define KINDS 7
#define NUM_PAGES (2 * KINDS)
#include "pvr_params.h"
#include "cases.h"
#include "pvrtest2.h"
#include "texgen.h"

static uint32_t tex_read, tex_fail[4];      /* fail patterns: 8-15; 8-15 + 20-23; 0-15; 12-19 */

static int fails(int k, int i)
{
    switch(k) {
    case 0: return i >= 8 && i <= 15;
    case 1: return (i >= 8 && i <= 15) || (i >= 20 && i <= 23);
    case 2: return i <= 15;
    default: return i >= 12 && i <= 19;
    }
}
static uint32_t fail_texel(const void *ctx, int size, int x, int y)
{
    (void)size; (void)y;
    return (fails(*(const int *)ctx, x) ? 0 : 0x8000u) | 0x7c00u | (uint32_t)(x & 31);      /* red, column in blue */
}

static void test_setup(void)
{
    static uint32_t words[1024 * 8 / 2];
    static const int pat[4] = { 0, 1, 2, 3 };
    tex_layout_t l = { TEX_RASTER, 16, 32, 8, 0 };
    int i, j;
    for(j = 0; j < 8; j++)
        for(i = 0; i < 1024; i += 2)
            words[(j * 1024 + i) / 2] = (uint32_t)i | (uint32_t)(i + 1) << 16;
    tex_read = pvr_tex_alloc(sizeof(words));
    pvrio_w64(tex_read, words, sizeof(words) / 4);
    for(i = 0; i < 4; i++)
        tex_fail[i] = tex_build(&l, fail_texel, &pat[i]);
}

static void test_describe(void)
{
    static const char *what[KINDS] = { "A fails 8-15", "A fails 8-15, 20-23", "A fails 0-15", "A fails 8-15, D over 0-3 between",
                                       "A1 fails 8-15, A2 fails 12-19", "A fails 0-15, E over 0-3 in front fails",
                                       "A fails 0-15, E over 0-3 in front passes" };
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 300; c++)
            if((c % 20) % 2 == 0 && (c / 20) % 2 == 0)
                label(p, c, "kind %d: %s", p / 2, what[p / 2]);
}

static void pt_poly(int textured, uint32_t tex, int texu, int pixfmt)
{
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Punch_Through;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
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

/* one triangle covering the cell at (x, y) (corners (x, y), (x + 64, y), (x, y + 64)), u = column / 32, depth z */
static void front(int x, int y, f32b z, uint32_t tex)
{
    pt_poly(1, tex, 2, PixelFmt_1555);
    ta_vertex3(f32b_int(x), f32b_int(y), z, F32B_ZERO, F32B_ZERO, 0xffffffff, 0, 0);
    ta_vertex3(f32b_int(x + 64), f32b_int(y), z, f32b_int(2), F32B_ZERO, 0xffffffff, 0, 0);
    ta_vertex3(f32b_int(x), f32b_int(y + 64), z, F32B_ZERO, F32B_ZERO, 0xffffffff, 0, 1);
}

static void test_page(int page)
{
    const int kind = page / 2;
    int i, n;
    pvrio_reg(PT_ALPHA_REF_addr, 0x80);
    pvr_frame.bg_color = 0x000040;
    ta_list_begin(ListType_Punch_Through);
    for(i = 0, n = 0; i < NUM_TRIS; i++) {
        const tri_t *t = &tris[i];
        int x, y, k;
        if(t->page != page)
            continue;
        x = cell_px(n); y = cell_py(n);
        n++;
        if((x / 32) % 2 || (y / 32) % 2)
            continue;
        pt_poly(1, tex_read, 7, PixelFmt_565);           /* B: the reading */
        for(k = 0; k < 3; k++)
            ta_vertex3(t->v[k][0], t->v[k][1], t->v[k][3], t->v[k][2], F32B_HALF, 0xffffffff, 0, k == 2);
        if(kind == 4) {
            front(x, y, F(3.0f), tex_fail[0]);
            front(x, y, F(2.0f), tex_fail[3]);
        } else {
            front(x, y, F(2.0f), tex_fail[kind == 3 ? 0 : kind >= 5 ? 2 : kind]);
        }
        if(kind == 3) {
            pt_poly(0, 0, 0, 0);
            quad0(x, y, 4, 32, F(1.5f), 0xff00ff00u);
        }
        if(kind == 5 || kind == 6) {
            pt_poly(0, 0, 0, 0);
            quad0(x, y, 4, 32, F(2.5f), kind == 5 ? 0x0000ff00u : 0xff00ff00u);
        }
    }
    ta_list_end();
}
