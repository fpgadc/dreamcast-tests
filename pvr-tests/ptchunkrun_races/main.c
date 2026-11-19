/* Runs of punch-through layers across chunks (2026-11-19; a _races test: which result a cell gets depends on the
   pipeline's timing, README "ptchunkrun_races"). polly2-data f1wgp_driver, tile (16, 9): a layer in the first
   pass of a new chunk whose run on the console starts further left than refsw3's, as if pixels an earlier chunk
   resolved took part in it; and a layer that fails the alpha test on the first columns of its run, whose run on the
   console starts further right. ptlayers measured the runs of later passes inside one chunk; this is across them.

   The readout is ptlayers': in every used cell (both cell coordinates even) a triangle B of uvmeas's steep mix at the
   back (cases.h, copied from ptlayers; texture: texel i holds the number i, point sampled) and single triangles A over
   the cell (64 pixels: they reach into the unused cells right and below) whose 32 texel wide textures fail the alpha
   test (PT_ALPHA_REF 0x80, 1555 alpha 0) on some columns. B's reading where it shows tells where its run started.
   Chunks (ISP_FEED_CFG bits 13..4, the chunk size in vertices; pvr-tests/ptchunk: a triangle is taken while the
   vertices before it and its own come to at most size - 2): size 5 makes every triangle a chunk of its own, size 8
   two triangles a chunk, 0x40 (the games') everything one chunk. Triangles are sent in the order the kind says.
     kind 0  A (depth 2, fails 8-15), then B (depth 1): one chunk each
     kind 1  kind 0 in one chunk (ptlayers kind 7)
     kind 2  A1 (depth 3, fails 8-15), A2 (depth 2, fails 12-19), B: one chunk each
     kind 3  A1 and A2 in one chunk, B in the next (size 8)
     kind 4  A (fails 8-15), a filler triangle (untextured, depth 0.5, behind everything, a corner of the cell), B:
             one chunk each (B two chunks after A)
     kind 5  A (depth 2, fails 8-15), then B in front of it (depth 3): one chunk each (a later chunk draws over)
     kind 6  A' (depth 0.5, passes, columns 0-15 only), then B in front (depth 1) with a 1555 reading texture that
             fails where (texel mod 64) < 16: one chunk each (f1wgp_driver's 89A1 over 4B61)
     kind 7  B with that texture alone
     kind 8  kind 6 in one chunk
   What separates kind 0 from ptchunk's kind 1 (there B's run starts at column 8: A and 9 fillers in the first chunk,
   size 0x20, A over half the cell):
     kind 9   kind 0 with size 6                         kind 10  A, a filler | B, size 8
     kind 11  A, 9 fillers | B, size 0x20                kind 12  A, 8 fillers, B in one chunk, size 0x20
     kind 13  kind 0 with A over the upper left half of the cell only (ptchunk's A)
     kind 14  kind 11 without discard (ISP_FEED_CFG bit 3)
   Fillers: untextured triangles that pass (alpha 0xff), depth 3, in the cell's lower right corner.
   Two pages of triangles per kind (page = 2 kind + 0 or 1; pages from 18 on take the triangles of page - 18).
   Blend SRC_ALPHA, INV_SRC_ALPHA, compare GREATER_EQUAL. */
#define KINDS 15
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

static void test_describe(void)
{
    static const char *what[KINDS] = { "A fails 8-15, then B: a chunk each", "A fails 8-15, then B: one chunk",
        "A1 fails 8-15, A2 fails 12-19, B: a chunk each", "A1, A2 in one chunk, B in the next",
        "A fails 8-15, a filler, B: a chunk each", "A fails 8-15, then B in front of A: a chunk each",
        "A' passes on 0-15 at the back, then B in front failing in bands: a chunk each", "B failing in bands alone",
        "A' passes on 0-15 at the back, then B failing in bands: one chunk", "A, then B: a chunk each, size 6",
        "A, a filler | B, size 8", "A, 9 fillers | B, size 0x20", "A, 8 fillers, B in one chunk, size 0x20",
        "A over the upper left half, then B: a chunk each", "A, 9 fillers | B, size 0x20, no discard" };
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

static void test_page(int page)
{
    static const int size[KINDS] = { 5, 0x40, 5, 8, 5, 5, 5, 5, 0x40, 6, 8, 0x20, 0x20, 5, 0x20 };
    const int kind = page / 2;
    int i, n, f;
    pvrio_reg(PT_ALPHA_REF_addr, 0x80);
    pvrio_reg(ISP_FEED_CFG_addr, 0x00800000u | (kind == 14 ? 0 : 8u) | (uint32_t)size[kind] << 4);
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
        switch(kind) {
        case 0: case 1:
            front(x, y, F(2.0f), tex_fail[0]); reading(t, F32B_ONE, 0); break;
        case 2: case 3:
            front(x, y, F(3.0f), tex_fail[0]); front(x, y, F(2.0f), tex_fail[3]); reading(t, F32B_ONE, 0); break;
        case 4:
            front(x, y, F(2.0f), tex_fail[0]);
            pt_poly(0, 0, 0, 0);
            ta_vertex0(f32b_int(x + 30), f32b_int(y + 30), F32B_HALF, 0xff00ff00u, 0);
            ta_vertex0(f32b_int(x + 32), f32b_int(y + 30), F32B_HALF, 0xff00ff00u, 0);
            ta_vertex0(f32b_int(x + 30), f32b_int(y + 32), F32B_HALF, 0xff00ff00u, 1);
            reading(t, F32B_ONE, 0); break;
        case 5:
            front(x, y, F(2.0f), tex_fail[0]); reading(t, F(3.0f), 0); break;
        case 6: case 8:
            left_half(x, y, F32B_HALF); reading(t, F32B_ONE, 1); break;
        case 7:
            reading(t, F32B_ONE, 1); break;
        case 9:
            front(x, y, F(2.0f), tex_fail[0]); reading(t, F32B_ONE, 0); break;
        case 10:
            front(x, y, F(2.0f), tex_fail[0]); filler(x, y); reading(t, F32B_ONE, 0); break;
        case 11: case 14:
            front(x, y, F(2.0f), tex_fail[0]); for(f = 0; f < 9; f++) filler(x, y); reading(t, F32B_ONE, 0); break;
        case 12:
            front(x, y, F(2.0f), tex_fail[0]); for(f = 0; f < 8; f++) filler(x, y); reading(t, F32B_ONE, 0); break;
        case 13:
            pt_poly(1, tex_fail[0], 2, PixelFmt_1555);
            ta_vertex3(f32b_int(x), f32b_int(y), F(2.0f), F32B_ZERO, F32B_ZERO, 0xffffffff, 0, 0);
            ta_vertex3(f32b_int(x + 32), f32b_int(y), F(2.0f), F32B_ONE, F32B_ZERO, 0xffffffff, 0, 0);
            ta_vertex3(f32b_int(x), f32b_int(y + 32), F(2.0f), F32B_ZERO, F32B_ZERO, 0xffffffff, 0, 1);
            reading(t, F32B_ONE, 0); break;
        }
    }
    ta_list_end();
}
