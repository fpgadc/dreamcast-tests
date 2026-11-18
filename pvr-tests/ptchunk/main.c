/* The punch-through ISP cache: the polygons of a tile in chunks of ISP_FEED_CFG's "Punch Through chunk size"
   vertices, each chunk peeled on its own (the hardware manual, 3.4.3.1 "ISP Cache Size"; 2026-11-18).

   polly2-data shenmue_intro*, jgr_tree_shadows2: tiles of 49 and more punch-through triangles (chunk size 64: 21
   triangles a chunk) whose later passes fit no rule while all of a tile's polygons are peeled together. A chunk
   boundary between two layers changes the pass a pixel is shaded in, and with it where its run starts: that is
   what this test reads.

   Read as pvr-tests/ptlayers: every cell (one tile) has, in the punch-through list, B (a triangle of uvrun's steep
   mix over the cell's upper left half: ../uvrun/cases.h, depth 1; texel i holds the number i, so a pixel shows the
   coordinate the TSP computed there; alpha 1, it passes) and A (the same half of the cell, depth 2, 32 texels across,
   alpha 0 on columns 8-15: B shows there), and filler triangles that pass, small, in the cell's lower right corner
   outside both (depth 3). Peeled together, B's run on columns 8-15 starts at its first pixel (ptlayers kind 0);
   with A in a chunk before B's, B is drawn in its own chunk only where A failed, from column 8.
   ISP_FEED_CFG = 0x00800000 | chunk << 4 | discard << 3 (the games' value is chunk 0x40 with discard):
     kind 0  A, B; chunk 0x20 (32 vertices)
     kind 1  A, 9 fillers, B; chunk 0x20: B is the 11th triangle (33 vertices)
     kind 2  A, 8 fillers, B; chunk 0x20: B is the 10th (30)
     kind 3  kind 1 with chunk 0x21 (33)
     kind 4  kind 1 with chunk 0x200
     kind 5  B, 9 fillers, A; chunk 0x20
     kind 6  kind 1 with the 9 fillers one strip (the tile accelerator cuts it into strips of 6 and 3)
     kind 7  kind 1 without discard
     kind 8  A, 6 fillers as one strip, B, chunk 0x20: B is the 8th triangle, 24 vertices as triangles, 11 as strips
     kind 9  A, 7 fillers, B, 1 filler; chunk 0x20 with A failing on all columns
     kind 10-12  kind 1 with chunk 0x22, 0x23, 0x24
     kind 13 9 fillers, then A as the first triangle of a strip of two over the whole cell (vertices 28-33 as
             triangles: a chunk boundary in the strip entry), B; chunk 0x20
     kind 14 A, 20 fillers, B; chunk 0x40 (the games'): B is the 22nd triangle
     kind 15 A, 19 fillers, B; chunk 0x40: B is the 21st
   Sprites (quads to the ISP: 4 vertices each?), chunk 0x20 unless said:
     kind 16 A, 6 filler sprites, B
     kind 17 A, 7 filler sprites, B
     kind 18 A as a sprite over the whole cell, 6 filler sprites, B
     kind 19 A as a sprite, 5 filler sprites, B
     kind 20 kind 18 with chunk 0x21
     kind 21 kind 18 with chunk 0x22
   Two pages of triangles per kind (page = 2 kind + 0 or 1); only every other cell, so that the fillers and A stay in
   their tile. Blend SRC_ALPHA, INV_SRC_ALPHA. */
#define KINDS 22
#define NUM_PAGES (2 * KINDS)
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

static void test_describe(void)
{
    static const char *what[KINDS] = { "A, B; chunk 0x20", "A, 9 fillers, B; chunk 0x20", "A, 8 fillers, B; chunk 0x20",
                                       "A, 9 fillers, B; chunk 0x21", "A, 9 fillers, B; chunk 0x200", "B, 9 fillers, A; chunk 0x20",
                                       "A, 9 fillers as a strip, B; chunk 0x20", "A, 9 fillers, B; chunk 0x20, no discard",
                                       "A, 6 fillers as a strip, B; chunk 0x20", "A fails all, 7 fillers, B, 1 filler; chunk 0x20",
                                       "A, 9 fillers, B; chunk 0x22", "A, 9 fillers, B; chunk 0x23", "A, 9 fillers, B; chunk 0x24",
                                       "9 fillers, A in a strip of 2, B; chunk 0x20", "A, 20 fillers, B; chunk 0x40", "A, 19 fillers, B; chunk 0x40",
                                       "A, 6 filler sprites, B; chunk 0x20", "A, 7 filler sprites, B; chunk 0x20",
                                       "A sprite, 6 filler sprites, B; chunk 0x20", "A sprite, 5 filler sprites, B; chunk 0x20",
                                       "A sprite, 6 filler sprites, B; chunk 0x21", "A sprite, 6 filler sprites, B; chunk 0x22" };
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 300; c++)
            if((c % 20) % 2 == 0 && (c / 20) % 2 == 0)
                label(p, c, "kind %d: %s", p / 2, what[p / 2]);
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

/* n filler triangles in the cell's lower right corner, depth 3, alpha 0xff; one strip if strip */
static void fillers(int x, int y, int n, int strip)
{
    int k;
    if(n <= 0)
        return;
    if(strip) {
        pt_poly(0, 0, 0, 0, 1);
        for(k = 0; k < n + 2; k++)                      /* a zigzag along the corner's bottom row */
            ta_vertex0(f32b_int(x + 24 + k / 2), f32b_int(y + 30 + (k & 1)), F(3.0f), 0xff00ff00u, k == n + 1);
        return;
    }
    for(k = 0; k < n; k++) {
        pt_poly(0, 0, 0, 0, 0);
        const int fx = x + 20 + k % 10, fy = y + 28 + 2 * (k / 10 % 2);      /* rows 28 and 30, columns 20-30 */
        ta_vertex0(f32b_int(fx), f32b_int(fy), F(3.0f), 0xff00ff00u, 0);
        ta_vertex0(f32b_int(fx + 1), f32b_int(fy), F(3.0f), 0xff00ff00u, 0);
        ta_vertex0(f32b_int(fx + 1), f32b_int(fy + 1), F(3.0f), 0xff00ff00u, 1);
    }
}

/* n filler sprites in the cell's lower right corner, depth 3, alpha 0xff */
static void sprite_fillers(int x, int y, int n)
{
    int k;
    for(k = 0; k < n; k++) {
        const int fx = x + 20 + k % 10, fy = y + 28 + 2 * (k / 10 % 2);
        TA_SpriteParam sp;
        TA_Sprite0 v;
        memset(&sp, 0, sizeof(sp));
        sp.pcw.ParaType = ParamType_Sprite;
        sp.pcw.ListType = ListType_Punch_Through;
        sp.isp.DepthMode = DepthMode_GreaterEqual;
        sp.isp.CullMode = CullMode_None;
        sp.tsp.SrcInstr = BlendInstr_SrcAlpha;
        sp.tsp.DstInstr = BlendInstr_InvSrcAlpha;
        sp.tsp.FogCtrl = FogCtrl_None;
        sp.tsp.UseAlpha = 1;
        sp.BaseCol = 0xff00ff00u;
        ta_send(&sp, sizeof(sp));
        memset(&v, 0, sizeof(v));
        v.pcw = pcw_vertex(1);
        v.x0 = f32b_int(fx); v.y0 = f32b_int(fy); v.z0 = F(3.0f);
        v.x1 = f32b_int(fx + 1); v.y1 = f32b_int(fy); v.z1 = F(3.0f);
        v.x2 = f32b_int(fx + 1); v.y2 = f32b_int(fy + 1); v.z2 = F(3.0f);
        v.x3 = f32b_int(fx); v.y3 = f32b_int(fy + 1);
        ta_send(&v, sizeof(v));
    }
}

/* A as a textured sprite over the whole cell (u = column / 32) */
static void sprite_A(int x, int y)
{
    TA_SpriteParam sp;
    TA_Sprite1 v;
    memset(&sp, 0, sizeof(sp));
    sp.pcw.ParaType = ParamType_Sprite;
    sp.pcw.ListType = ListType_Punch_Through;
    sp.pcw.Texture = 1;
    sp.pcw.UV_16bit = 1;
    sp.isp.DepthMode = DepthMode_GreaterEqual;
    sp.isp.CullMode = CullMode_None;
    sp.isp.Texture = 1;
    sp.isp.UV_16b = 1;
    sp.tsp.SrcInstr = BlendInstr_SrcAlpha;
    sp.tsp.DstInstr = BlendInstr_InvSrcAlpha;
    sp.tsp.FogCtrl = FogCtrl_None;
    sp.tsp.UseAlpha = 1;
    sp.tsp.ShadInstr = ShadInstr_Decal;
    sp.tsp.FilterMode = FilterMode_Point;
    sp.tsp.MipMapD = 4;
    sp.tsp.TexU = 2;
    sp.tsp.TexV = 0;
    sp.tcw.PixelFmt = PixelFmt_1555;
    sp.tcw.ScanOrder = 1;
    sp.tcw.TexAddr = tex_fail[0] >> 3;
    sp.BaseCol = 0xffffffffu;
    ta_send(&sp, sizeof(sp));
    memset(&v, 0, sizeof(v));
    v.pcw = pcw_vertex(1);
    v.x0 = f32b_int(x); v.y0 = f32b_int(y); v.z0 = F(2.0f);
    v.x1 = f32b_int(x + 32); v.y1 = f32b_int(y); v.z1 = F(2.0f);
    v.x2 = f32b_int(x + 32); v.y2 = f32b_int(y + 32); v.z2 = F(2.0f);
    v.x3 = f32b_int(x); v.y3 = f32b_int(y + 32);
    v.uv0 = uv16(F32B_ZERO, F32B_ZERO); v.uv1 = uv16(F32B_ONE, F32B_ZERO); v.uv2 = uv16(F32B_ONE, F32B_ZERO);
    ta_send(&v, sizeof(v));
}

static void tri_A(int x, int y, int all)
{
    pt_poly(1, tex_fail[all], 2, PixelFmt_1555, 0);
    ta_vertex3(f32b_int(x), f32b_int(y), F(2.0f), F32B_ZERO, F32B_ZERO, 0xffffffff, 0, 0);
    ta_vertex3(f32b_int(x + 32), f32b_int(y), F(2.0f), F32B_ONE, F32B_ZERO, 0xffffffff, 0, 0);
    ta_vertex3(f32b_int(x), f32b_int(y + 32), F(2.0f), F32B_ZERO, F32B_ZERO, 0xffffffff, 0, 1);
}

static void tri_B(const tri_t *t)
{
    int k;
    pt_poly(1, tex_read, 7, PixelFmt_565, 0);
    for(k = 0; k < 3; k++)
        ta_vertex3(t->v[k][0], t->v[k][1], t->v[k][3], t->v[k][2], F32B_HALF, 0xffffffff, 0, k == 2);
}

static void test_page(int page)
{
    static const int chunk[KINDS] = { 0x20, 0x20, 0x20, 0x21, 0x200, 0x20, 0x20, 0x20, 0x20, 0x20, 0x22, 0x23, 0x24, 0x20, 0x40, 0x40,
                                      0x20, 0x20, 0x20, 0x20, 0x21, 0x22 };
    const int kind = page / 2;
    int i, n;
    pvrio_reg(ISP_FEED_CFG_addr, 0x00800000u | (uint32_t)chunk[kind] << 4 | (kind == 7 ? 0u : 8u));
    pvrio_reg(PT_ALPHA_REF_addr, 0x80);
    pvr_frame.bg_color = 0x000040;
    ta_list_begin(ListType_Punch_Through);
    for(i = 0, n = 0; i < NUM_TRIS; i++) {
        const tri_t *t = &tris[i];
        int x, y;
        if(t->page != (page & 1))
            continue;
        x = cell_px(n); y = cell_py(n);
        n++;
        if((x / 32) % 2 || (y / 32) % 2)
            continue;
        switch(kind) {
        case 0: tri_A(x, y, 0); tri_B(t); break;
        case 1: case 3: case 4: case 7: case 10: case 11: case 12: tri_A(x, y, 0); fillers(x, y, 9, 0); tri_B(t); break;
        case 13:
            fillers(x, y, 9, 0);
            pt_poly(1, tex_fail[0], 2, PixelFmt_1555, 1);       /* A, then the cell's other half: one strip */
            ta_vertex3(f32b_int(x), f32b_int(y), F(2.0f), F32B_ZERO, F32B_ZERO, 0xffffffff, 0, 0);
            ta_vertex3(f32b_int(x + 32), f32b_int(y), F(2.0f), F32B_ONE, F32B_ZERO, 0xffffffff, 0, 0);
            ta_vertex3(f32b_int(x), f32b_int(y + 32), F(2.0f), F32B_ZERO, F32B_ZERO, 0xffffffff, 0, 0);
            ta_vertex3(f32b_int(x + 32), f32b_int(y + 32), F(2.0f), F32B_ONE, F32B_ZERO, 0xffffffff, 0, 1);
            tri_B(t);
            break;
        case 14: tri_A(x, y, 0); fillers(x, y, 20, 0); tri_B(t); break;
        case 15: tri_A(x, y, 0); fillers(x, y, 19, 0); tri_B(t); break;
        case 16: tri_A(x, y, 0); sprite_fillers(x, y, 6); tri_B(t); break;
        case 17: tri_A(x, y, 0); sprite_fillers(x, y, 7); tri_B(t); break;
        case 18: case 20: case 21: sprite_A(x, y); sprite_fillers(x, y, 6); tri_B(t); break;
        case 19: sprite_A(x, y); sprite_fillers(x, y, 5); tri_B(t); break;
        case 2: tri_A(x, y, 0); fillers(x, y, 8, 0); tri_B(t); break;
        case 5: tri_B(t); fillers(x, y, 9, 0); tri_A(x, y, 0); break;
        case 6: tri_A(x, y, 0); fillers(x, y, 9, 1); tri_B(t); break;
        case 8: tri_A(x, y, 0); fillers(x, y, 6, 1); tri_B(t); break;
        case 9: tri_A(x, y, 1); fillers(x, y, 7, 0); tri_B(t); fillers(x, y, 1, 0); break;
        }
    }
    ta_list_end();
}
