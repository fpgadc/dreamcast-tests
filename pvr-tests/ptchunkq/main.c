/* The punch-through ISP cache with sprites (quads to the ISP) and triangles mixed, chunk sizes 0x20-0x25: which
   primitive goes to the next chunk (2026-11-18; pvr-tests/ptchunk, whose A, B and fillers this uses, and its kinds
   16-21). One page per configuration: A (a triangle or a sprite over the cell), filler sprites, filler triangles, B;
   the first 72 pages are chunk 0x20-0x25 with sprites or triangles as fillers, the rest every mix within a few
   vertices of the boundary at 0x20, 0x23 and 0x40 (the games').
   Whether B is in A's chunk shows as in ptchunk: compare with refsw2-offline rendering with no chunks and with every
   primitive its own chunk (RSW_PTCHUNKCAP=100000 / 3). Configurations: CONF below. */
#define NUM_PAGES 200
#include "pvr_params.h"
#include "../uvrun/cases.h"
#include "pvrtest2.h"
#include "texgen.h"

static const struct { int chunk, a_sprite, n_sprites, n_tris; } CONF[] = {
    { 0x20, 0, 5, 0 },
    { 0x20, 0, 6, 0 },
    { 0x20, 0, 7, 0 },
    { 0x20, 0, 8, 0 },
    { 0x20, 0, 0, 7 },
    { 0x20, 0, 0, 8 },
    { 0x20, 1, 5, 0 },
    { 0x20, 1, 6, 0 },
    { 0x20, 1, 7, 0 },
    { 0x20, 1, 8, 0 },
    { 0x20, 1, 0, 7 },
    { 0x20, 1, 0, 8 },
    { 0x21, 0, 5, 0 },
    { 0x21, 0, 6, 0 },
    { 0x21, 0, 7, 0 },
    { 0x21, 0, 8, 0 },
    { 0x21, 0, 0, 7 },
    { 0x21, 0, 0, 8 },
    { 0x21, 1, 5, 0 },
    { 0x21, 1, 6, 0 },
    { 0x21, 1, 7, 0 },
    { 0x21, 1, 8, 0 },
    { 0x21, 1, 0, 7 },
    { 0x21, 1, 0, 8 },
    { 0x22, 0, 5, 0 },
    { 0x22, 0, 6, 0 },
    { 0x22, 0, 7, 0 },
    { 0x22, 0, 8, 0 },
    { 0x22, 0, 0, 7 },
    { 0x22, 0, 0, 8 },
    { 0x22, 1, 5, 0 },
    { 0x22, 1, 6, 0 },
    { 0x22, 1, 7, 0 },
    { 0x22, 1, 8, 0 },
    { 0x22, 1, 0, 7 },
    { 0x22, 1, 0, 8 },
    { 0x23, 0, 5, 0 },
    { 0x23, 0, 6, 0 },
    { 0x23, 0, 7, 0 },
    { 0x23, 0, 8, 0 },
    { 0x23, 0, 0, 7 },
    { 0x23, 0, 0, 8 },
    { 0x23, 1, 5, 0 },
    { 0x23, 1, 6, 0 },
    { 0x23, 1, 7, 0 },
    { 0x23, 1, 8, 0 },
    { 0x23, 1, 0, 7 },
    { 0x23, 1, 0, 8 },
    { 0x24, 0, 5, 0 },
    { 0x24, 0, 6, 0 },
    { 0x24, 0, 7, 0 },
    { 0x24, 0, 8, 0 },
    { 0x24, 0, 0, 7 },
    { 0x24, 0, 0, 8 },
    { 0x24, 1, 5, 0 },
    { 0x24, 1, 6, 0 },
    { 0x24, 1, 7, 0 },
    { 0x24, 1, 8, 0 },
    { 0x24, 1, 0, 7 },
    { 0x24, 1, 0, 8 },
    { 0x25, 0, 5, 0 },
    { 0x25, 0, 6, 0 },
    { 0x25, 0, 7, 0 },
    { 0x25, 0, 8, 0 },
    { 0x25, 0, 0, 7 },
    { 0x25, 0, 0, 8 },
    { 0x25, 1, 5, 0 },
    { 0x25, 1, 6, 0 },
    { 0x25, 1, 7, 0 },
    { 0x25, 1, 8, 0 },
    { 0x25, 1, 0, 7 },
    { 0x25, 1, 0, 8 },
    { 0x20, 0, 0, 7 },
    { 0x20, 0, 0, 8 },
    { 0x20, 0, 0, 9 },
    { 0x20, 0, 0, 10 },
    { 0x20, 0, 1, 5 },
    { 0x20, 0, 1, 6 },
    { 0x20, 0, 1, 7 },
    { 0x20, 0, 1, 8 },
    { 0x20, 0, 1, 9 },
    { 0x20, 0, 2, 4 },
    { 0x20, 0, 2, 5 },
    { 0x20, 0, 2, 6 },
    { 0x20, 0, 2, 7 },
    { 0x20, 0, 3, 3 },
    { 0x20, 0, 3, 4 },
    { 0x20, 0, 3, 5 },
    { 0x20, 0, 3, 6 },
    { 0x20, 0, 4, 1 },
    { 0x20, 0, 4, 2 },
    { 0x20, 0, 4, 3 },
    { 0x20, 0, 4, 4 },
    { 0x20, 0, 4, 5 },
    { 0x20, 0, 5, 0 },
    { 0x20, 0, 5, 1 },
    { 0x20, 0, 5, 2 },
    { 0x20, 0, 5, 3 },
    { 0x20, 0, 6, 0 },
    { 0x20, 0, 6, 1 },
    { 0x20, 0, 6, 2 },
    { 0x20, 0, 7, 0 },
    { 0x20, 0, 7, 1 },
    { 0x23, 0, 0, 8 },
    { 0x23, 0, 0, 9 },
    { 0x23, 0, 0, 10 },
    { 0x23, 0, 0, 11 },
    { 0x23, 0, 1, 6 },
    { 0x23, 0, 1, 7 },
    { 0x23, 0, 1, 8 },
    { 0x23, 0, 1, 9 },
    { 0x23, 0, 1, 10 },
    { 0x23, 0, 2, 5 },
    { 0x23, 0, 2, 6 },
    { 0x23, 0, 2, 7 },
    { 0x23, 0, 2, 8 },
    { 0x23, 0, 3, 4 },
    { 0x23, 0, 3, 5 },
    { 0x23, 0, 3, 6 },
    { 0x23, 0, 3, 7 },
    { 0x23, 0, 4, 2 },
    { 0x23, 0, 4, 3 },
    { 0x23, 0, 4, 4 },
    { 0x23, 0, 4, 5 },
    { 0x23, 0, 4, 6 },
    { 0x23, 0, 5, 1 },
    { 0x23, 0, 5, 2 },
    { 0x23, 0, 5, 3 },
    { 0x23, 0, 5, 4 },
    { 0x23, 0, 6, 0 },
    { 0x23, 0, 6, 1 },
    { 0x23, 0, 6, 2 },
    { 0x23, 0, 6, 3 },
    { 0x23, 0, 7, 0 },
    { 0x23, 0, 7, 1 },
    { 0x23, 0, 7, 2 },
    { 0x40, 0, 0, 17 },
    { 0x40, 0, 0, 18 },
    { 0x40, 0, 0, 19 },
    { 0x40, 0, 0, 20 },
    { 0x40, 0, 0, 21 },
    { 0x40, 0, 1, 16 },
    { 0x40, 0, 1, 17 },
    { 0x40, 0, 1, 18 },
    { 0x40, 0, 1, 19 },
    { 0x40, 0, 2, 15 },
    { 0x40, 0, 2, 16 },
    { 0x40, 0, 2, 17 },
    { 0x40, 0, 2, 18 },
    { 0x40, 0, 3, 13 },
    { 0x40, 0, 3, 14 },
    { 0x40, 0, 3, 15 },
    { 0x40, 0, 3, 16 },
    { 0x40, 0, 3, 17 },
    { 0x40, 0, 4, 12 },
    { 0x40, 0, 4, 13 },
    { 0x40, 0, 4, 14 },
    { 0x40, 0, 4, 15 },
    { 0x40, 0, 5, 11 },
    { 0x40, 0, 5, 12 },
    { 0x40, 0, 5, 13 },
    { 0x40, 0, 5, 14 },
    { 0x40, 0, 6, 9 },
    { 0x40, 0, 6, 10 },
    { 0x40, 0, 6, 11 },
    { 0x40, 0, 6, 12 },
    { 0x40, 0, 6, 13 },
    { 0x40, 0, 7, 8 },
    { 0x40, 0, 7, 9 },
    { 0x40, 0, 7, 10 },
    { 0x40, 0, 7, 11 },
    { 0x40, 0, 8, 7 },
    { 0x40, 0, 8, 8 },
    { 0x40, 0, 8, 9 },
    { 0x40, 0, 8, 10 },
    { 0x40, 0, 9, 5 },
    { 0x40, 0, 9, 6 },
    { 0x40, 0, 9, 7 },
    { 0x40, 0, 9, 8 },
    { 0x40, 0, 9, 9 },
    { 0x40, 0, 10, 4 },
    { 0x40, 0, 10, 5 },
    { 0x40, 0, 10, 6 },
    { 0x40, 0, 10, 7 },
    { 0x40, 0, 11, 3 },
    { 0x40, 0, 11, 4 },
    { 0x40, 0, 11, 5 },
    { 0x40, 0, 11, 6 },
    { 0x40, 0, 12, 1 },
    { 0x40, 0, 12, 2 },
    { 0x40, 0, 12, 3 },
    { 0x40, 0, 12, 4 },
    { 0x40, 0, 12, 5 },
    { 0x40, 0, 13, 0 },
    { 0x40, 0, 13, 1 },
    { 0x40, 0, 13, 2 },
    { 0x40, 0, 13, 3 },
    { 0x40, 0, 14, 0 },
    { 0x40, 0, 14, 1 },
    { 0x40, 0, 14, 2 }
};

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


static void test_describe(void)
{
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 300; c++)
            if((c % 20) % 2 == 0 && (c / 20) % 2 == 0)
                label(p, c, "chunk 0x%02x: A %s, %d filler sprites, %d filler triangles, B", CONF[p].chunk,
                      CONF[p].a_sprite ? "sprite" : "triangle", CONF[p].n_sprites, CONF[p].n_tris);
}

static void test_page(int page)
{
    int i, n;
    pvrio_reg(ISP_FEED_CFG_addr, 0x00800008u | (uint32_t)CONF[page].chunk << 4);
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
        if(CONF[page].a_sprite) sprite_A(x, y); else tri_A(x, y, 0);
        sprite_fillers(x, y, CONF[page].n_sprites);
        fillers(x, y, CONF[page].n_tris, 0);
        tri_B(t);
    }
    ta_list_end();
}
