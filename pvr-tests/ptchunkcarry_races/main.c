/* When pixels a punch-through chunk resolved take part in the next chunk's runs (2026-11-19; a _races test; ptchunkrun_races:
   with A alone (or A and one filler) in the chunk before B they do, with A and 8 or 9 fillers they do not, and the
   chunk size, A's shape and discard do not matter). ptchunkrun's readout: A (depth 2, fails on columns 8-15) over the
   cell, then k filler triangles (untextured, alpha 0xff: they pass, depth 3, in front of A), then B (uvmeas's steep
   triangle, depth 1, the reading); the chunk size 3 (k + 1) + 2 puts A and the k fillers in the first chunk and B in
   the second.
     kinds 0 .. 9    k = 0 .. 9 fillers all on one spot (the cell's lower right corner: stacked, one pass each)
     kinds 10 .. 19  k = 0 .. 9 fillers side by side along the cell's bottom (no two overlap)
   Two pages per kind (page = 2 kind + 0 or 1; B's triangles of ptlayers' page (page mod 18)). */
#define KINDS 20
#define NUM_PAGES (2 * KINDS)
#include "pvr_params.h"
#include "cases.h"
#include "pvrtest2.h"
#include "texgen.h"

static uint32_t tex_read, tex_read_a, tex_fail[4], tex_pass;

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
static uint32_t pass_texel(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)y; return 0x8000u | 0x03e0u | (uint32_t)(x & 31); }

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
    /* 1555: texel i: green i >> 5, blue i & 31, alpha 0 where (i mod 64) < 16 */
    for(j = 0; j < 8; j++)
        for(i = 0; i < 1024; i += 2) {
            const uint32_t a = (uint32_t)(((i >> 5) << 5) | (i & 31)) | ((i % 64) < 16 ? 0 : 0x8000u);
            const uint32_t b = (uint32_t)((((i + 1) >> 5) << 5) | ((i + 1) & 31)) | (((i + 1) % 64) < 16 ? 0 : 0x8000u);
            words[(j * 1024 + i) / 2] = a | b << 16;
        }
    tex_read_a = pvr_tex_alloc(sizeof(words));
    pvrio_w64(tex_read_a, words, sizeof(words) / 4);
    for(i = 0; i < 4; i++)
        tex_fail[i] = tex_build(&l, fail_texel, &pat[i]);
    tex_pass = tex_build(&l, pass_texel, NULL);
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

/* columns 0-15 of the cell, all rows: a quad as a strip of two triangles would count 4 vertices; a triangle covering
   the left half is enough: corners (x, y), (x + 16, y), (x, y + 64) cover columns 0-15 down to row 31 only partly, so
   two triangles, each its own primitive, would be two chunks; use one sprite-free triangle (x, y) (x + 32, y) (x, y + 64) */
static void left_half(int x, int y, f32b z)
{
    pt_poly(1, tex_pass, 2, PixelFmt_1555);
    ta_vertex3(f32b_int(x), f32b_int(y), z, F32B_ZERO, F32B_ZERO, 0xffffffff, 0, 0);
    ta_vertex3(f32b_int(x + 32), f32b_int(y), z, F32B_ONE, F32B_ZERO, 0xffffffff, 0, 0);
    ta_vertex3(f32b_int(x), f32b_int(y + 64), z, F32B_ZERO, F32B_ZERO, 0xffffffff, 0, 1);
}

static void filler(int x, int y)
{
    pt_poly(0, 0, 0, 0);
    ta_vertex0(f32b_int(x + 28), f32b_int(y + 31), F(3.0f), 0xff00ff00u, 0);
    ta_vertex0(f32b_int(x + 32), f32b_int(y + 28), F(3.0f), 0xff00ff00u, 0);
    ta_vertex0(f32b_int(x + 32), f32b_int(y + 32), F(3.0f), 0xff00ff00u, 1);
}

static void reading(const tri_t *t, f32b z, int banded)
{
    int k;
    pt_poly(1, banded ? tex_read_a : tex_read, 7, banded ? PixelFmt_1555 : PixelFmt_565);
    for(k = 0; k < 3; k++)
        ta_vertex3(t->v[k][0], t->v[k][1], z == 0 ? t->v[k][3] : z, t->v[k][2], F32B_HALF, 0xffffffff, 0, k == 2);
}

static void test_describe(void)
{
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 300; c++)
            if((c % 20) % 2 == 0 && (c / 20) % 2 == 0)
                label(p, c, "A, %d fillers %s | B (chunk size %d)", (p / 2) % 10, p / 2 < 10 ? "on one spot" : "side by side", 3 * ((p / 2) % 10 + 1) + 2);
}

static void filler_at(int x, int y)
{
    pt_poly(0, 0, 0, 0);
    ta_vertex0(f32b_int(x), f32b_int(y + 3), F(3.0f), 0xff00ff00u, 0);
    ta_vertex0(f32b_int(x + 3), f32b_int(y), F(3.0f), 0xff00ff00u, 0);
    ta_vertex0(f32b_int(x + 3), f32b_int(y + 3), F(3.0f), 0xff00ff00u, 1);
}

static void test_page(int page)
{
    const int kind = page / 2, k = kind % 10, spread = kind >= 10;
    int i, n, f;
    pvrio_reg(PT_ALPHA_REF_addr, 0x80);
    pvrio_reg(ISP_FEED_CFG_addr, 0x00800008u | (uint32_t)(3 * (k + 1) + 2) << 4);
    pvr_frame.bg_color = 0x000040;
    ta_list_begin(ListType_Punch_Through);
    for(i = 0, n = 0; i < NUM_TRIS; i++) {
        const tri_t *t = &tris[i];
        int x, y;
        if(t->page != page % 18)
            continue;
        x = cell_px(n); y = cell_py(n);
        n++;
        if((x / 32) % 2 || (y / 32) % 2)
            continue;
        front(x, y, F(2.0f), tex_fail[0]);
        for(f = 0; f < k; f++)
            filler_at(spread ? x + 1 + 3 * f : x + 28, y + 28);
        reading(t, F32B_ONE, 0);
    }
    ta_list_end();
}
