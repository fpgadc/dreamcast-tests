/* SupSample with what pvr-tests/supsample left out: mipmaps (every filter), perspective (a 1/w that is not flat),
   Gouraud base and offset colours with every shading instruction, and fog (table, table mode 2, vertex).

   Every page has 300 cells of 32x32, each a quad with its own random parameters; cells 150 .. 299 are cells
   0 .. 149 again with SupSample off (the control: what refsw3 had before SupSample). Textures are 64x64 of
   arbitrary texels, every level of a mipmapped one its own.
   Coordinates are given at the corners X, Y (0 or 32) of the cell: u = (P + X Sx + Y Sy) / 2^22, v likewise,
   every cell its own 21 bit P and slopes: along one axis 1/4 .. 2 texels a pixel (a texture with mipmaps: 1/8 ..
   16, perspective 1/8 .. 4 at z = 1; with the D adjusts every level), along the other +- a quarter of that. Flat: z = 1 at all four corners (the coordinates are
   linear on the screen); perspective: every corner its own z, 1/4 .. 2 (the two triangles of the quad have their
   own planes). Gouraud: every corner its own base and offset colour (all 32 bits arbitrary). Mipmaps: every cell
   its own D adjust, 1 .. 15.
   Blend: opaque (ONE, ZERO) on 0x000040; "x alpha": (SRC_ALPHA, ZERO) on black, the colour times its alpha;
   "over": the translucent list, (SRC_ALPHA, INV_SRC_ALPHA) over 0x803020.
   Fog: FOG_DENSITY 16 (the table's index from 16 z: 2 .. 32), an arbitrary table, FOG_COL_RAM 0xc0e57f19,
   FOG_COL_VERT 0x0033cc66; vertex fog with the offset colour's alpha.
     page  0  565 mip, point, flat                  page  1  565 mip, bilinear, flat
     page  2  565 mip, trilinear A, flat            page  3  565 mip, trilinear B, flat
     page  4  4444 VQ mip, point, flat, x alpha
     page  5  565, point, perspective               page  6  565, bilinear, perspective
     page  7  4444 VQ, point, perspective, x alpha
     page  8  565 mip, point, perspective           page  9  565 mip, bilinear, perspective
     page 10  565 mip, trilinear A, perspective     page 11  565 mip, trilinear B, perspective
     page 12  565, bilinear, flat, Gouraud with offset, decal
     page 13  the same, modulate
     page 14  4444, bilinear, flat, Gouraud with offset, decal alpha, over
     page 15  the same, modulate alpha
     page 16  4444 mip, bilinear, perspective, Gouraud with offset, modulate alpha, over
     page 17  565, bilinear, perspective, Gouraud with offset, modulate, table fog
     page 18  4444, bilinear, perspective, Gouraud with offset, modulate alpha, table fog mode 2, over
     page 19  565, bilinear, flat, Gouraud with offset, modulate, vertex fog
     page 20  4444 VQ, point, perspective, Gouraud with offset, modulate alpha, vertex fog, over
     page 21  565 mip, trilinear A, perspective, Gouraud with offset, modulate, table fog
     page 22  565, point, flat, one base and offset colour (not Gouraud), modulate
   The trilinear fade with alpha (refsw3 had it on the texel; it is on the finished colour, page 21's control):
     page 23  4444 mip, trilinear A, perspective, Gouraud with offset, modulate alpha, over
     page 24  4444 mip, trilinear B, perspective, Gouraud with offset, modulate alpha, table fog mode 2, over
     page 25  4444 mip, trilinear A, perspective, Gouraud with offset, modulate alpha, vertex fog, colour clamp
              (FOG_CLAMP_MIN 0x20304050, MAX 0xd0c0b0a0), over
     page 26  4444 mip, trilinear B, perspective, Gouraud with offset, modulate alpha, IgnoreTexA, over
   And the bit on a polygon without a texture:
     page 27  no texture, perspective, Gouraud, table fog, over
   Bump maps (64x64, every texel an arbitrary S and R; the offset colour the light, K1 K2 K3 Q; the result is
   white with the intensity in alpha, shown x alpha):
     page 28  bump, point, flat, one light (base and offset colour) a cell, decal
     page 29  bump, bilinear, perspective, one light a cell, modulate
     page 30  bump, bilinear, perspective, Gouraud (every corner its own light), modulate
     page 31  bump mip, trilinear A, perspective, one light a cell, decal
     page 32  bump mip, trilinear B, perspective, one light a cell, decal */
#define NUM_PAGES 33
#include "pvrtest2.h"
#include "texgen.h"

enum { T565, T4444, TVQ, M565, M4444, MVQ, TBUMP, MBUMP, NTEX };       /* 64x64: twiddled, VQ, mipmapped, bump */
enum { OPAQUE, XALPHA, OVER };
enum { CLAMP = 1, IGNA = 2, NOTEX = 4 };
typedef struct { int tex, filter, persp, gouraud, shad, fog, blend, extra; } page_t;
static const page_t pages[NUM_PAGES] = {
    { M565, FilterMode_Point, 0, 0, ShadInstr_Decal, FogCtrl_None, OPAQUE },
    { M565, FilterMode_Bilinear, 0, 0, ShadInstr_Decal, FogCtrl_None, OPAQUE },
    { M565, FilterMode_TrilinearA, 0, 0, ShadInstr_Decal, FogCtrl_None, OPAQUE },
    { M565, FilterMode_TrilinearB, 0, 0, ShadInstr_Decal, FogCtrl_None, OPAQUE },
    { MVQ, FilterMode_Point, 0, 0, ShadInstr_Decal, FogCtrl_None, XALPHA },
    { T565, FilterMode_Point, 1, 0, ShadInstr_Decal, FogCtrl_None, OPAQUE },
    { T565, FilterMode_Bilinear, 1, 0, ShadInstr_Decal, FogCtrl_None, OPAQUE },
    { TVQ, FilterMode_Point, 1, 0, ShadInstr_Decal, FogCtrl_None, XALPHA },
    { M565, FilterMode_Point, 1, 0, ShadInstr_Decal, FogCtrl_None, OPAQUE },
    { M565, FilterMode_Bilinear, 1, 0, ShadInstr_Decal, FogCtrl_None, OPAQUE },
    { M565, FilterMode_TrilinearA, 1, 0, ShadInstr_Decal, FogCtrl_None, OPAQUE },
    { M565, FilterMode_TrilinearB, 1, 0, ShadInstr_Decal, FogCtrl_None, OPAQUE },
    { T565, FilterMode_Bilinear, 0, 1, ShadInstr_Decal, FogCtrl_None, OPAQUE },
    { T565, FilterMode_Bilinear, 0, 1, ShadInstr_Modulate, FogCtrl_None, OPAQUE },
    { T4444, FilterMode_Bilinear, 0, 1, ShadInstr_DecalAlpha, FogCtrl_None, OVER },
    { T4444, FilterMode_Bilinear, 0, 1, ShadInstr_ModulateAlpha, FogCtrl_None, OVER },
    { M4444, FilterMode_Bilinear, 1, 1, ShadInstr_ModulateAlpha, FogCtrl_None, OVER },
    { T565, FilterMode_Bilinear, 1, 1, ShadInstr_Modulate, FogCtrl_Table, OPAQUE },
    { T4444, FilterMode_Bilinear, 1, 1, ShadInstr_ModulateAlpha, FogCtrl_Table2, OVER },
    { T565, FilterMode_Bilinear, 0, 1, ShadInstr_Modulate, FogCtrl_Vertex, OPAQUE },
    { TVQ, FilterMode_Point, 1, 1, ShadInstr_ModulateAlpha, FogCtrl_Vertex, OVER },
    { M565, FilterMode_TrilinearA, 1, 1, ShadInstr_Modulate, FogCtrl_Table, OPAQUE },
    { T565, FilterMode_Point, 0, 2, ShadInstr_Modulate, FogCtrl_None, OPAQUE },
    { M4444, FilterMode_TrilinearA, 1, 1, ShadInstr_ModulateAlpha, FogCtrl_None, OVER },
    { M4444, FilterMode_TrilinearB, 1, 1, ShadInstr_ModulateAlpha, FogCtrl_Table2, OVER },
    { M4444, FilterMode_TrilinearA, 1, 1, ShadInstr_ModulateAlpha, FogCtrl_Vertex, OVER, CLAMP },
    { M4444, FilterMode_TrilinearB, 1, 1, ShadInstr_ModulateAlpha, FogCtrl_None, OVER, IGNA },
    { T565, FilterMode_Point, 1, 1, ShadInstr_Modulate, FogCtrl_Table, OVER, NOTEX },
    { TBUMP, FilterMode_Point, 0, 2, ShadInstr_Decal, FogCtrl_None, XALPHA },
    { TBUMP, FilterMode_Bilinear, 1, 2, ShadInstr_Modulate, FogCtrl_None, XALPHA },
    { TBUMP, FilterMode_Bilinear, 1, 1, ShadInstr_Modulate, FogCtrl_None, XALPHA },
    { MBUMP, FilterMode_TrilinearA, 1, 2, ShadInstr_Decal, FogCtrl_None, XALPHA },
    { MBUMP, FilterMode_TrilinearB, 1, 2, ShadInstr_Decal, FogCtrl_None, XALPHA },
};
#define CELLS 150                       /* cells with SupSample; the next 150 are the same without */

static uint32_t tex[NTEX];

static uint32_t hsh(uint32_t v) { v *= 2654435761u; v ^= v >> 15; v *= 2246822519u; v ^= v >> 13; v *= 3266489917u; v ^= v >> 16; return v; }

static uint32_t rnd_texel(const void *ctx, int size, int x, int y)
{
    return hsh((uint32_t)(size * 4096 + y * 64 + x) ^ *(const uint32_t *)ctx) & 0xffff;
}

/* VQ: at most 256 different entries of 2x2 texels, so the 2x2 blocks of a level take one of 240 entries (the
   1x1 level is an entry of its own) */
static uint32_t vq_texel(const void *ctx, int size, int x, int y)
{
    const uint32_t s = *(const uint32_t *)ctx;
    const uint32_t e = size == 1 ? 1000u : hsh((uint32_t)(size * 4096 + (y >> 1) * 64 + (x >> 1)) ^ s) % 240u;
    return hsh(e * 4 + (uint32_t)((y & 1) * 2 + (x & 1)) + s) & 0xffff;
}

static uint32_t fog_table[128];

static void test_setup(void)
{
    static const uint32_t seed[NTEX] = { 0x1234567u, 0x89abcdeu, 0x5a5a5a5u, 0x3c3c3c3u, 0x7777777u, 0x2468ace, 0x1b2c3d4u, 0x6e5f4a3u };
    static const int layout[NTEX] = { TEX_TWIDDLED, TEX_TWIDDLED, TEX_VQ, TEX_MIP, TEX_MIP, TEX_VQ_MIP, TEX_TWIDDLED, TEX_MIP };
    int k;
    for(k = 0; k < NTEX; k++) {
        const tex_layout_t l = { layout[k], 16, 64, 64, 0 };
        tex[k] = tex_build(&l, layout[k] == TEX_VQ || layout[k] == TEX_VQ_MIP ? vq_texel : rnd_texel, &seed[k]);
    }
    for(k = 0; k < 128; k++)
        fog_table[k] = hsh((uint32_t)k + 0x5100u) & 0xffff;
}

static int tex_fmt(int t) { return t == T565 || t == M565 ? PixelFmt_565 : t == TBUMP || t == MBUMP ? PixelFmt_BumpMap : PixelFmt_4444; }
static int tex_mip(int t) { return t == M565 || t == M4444 || t == MVQ || t == MBUMP; }
static int tex_vq(int t) { return t == TVQ || t == MVQ; }

/* a cell's parameters, from the page and the cell (the control cells take those of the cell 150 before) */
typedef struct { int32_t pu, sux, suy, pv, svx, svy; uint32_t zm[4], base[4], offs[4]; int mipd; } cell_t;

static int32_t slope(uint32_t h, int mip, int persp)
{
    /* 2^22 is 64 texels over a pixel: 2^12 is 1/16, so 2^14 .. 2^17 is 1/4 .. 2 texels; with mipmaps 2^13 .. 2^20,
       1/8 .. 16 (perspective: 2^13 .. 2^18, 1/8 .. 4, times w) */
    const int e = mip ? 13 + (int)(h % (persp ? 5 : 7)) : 14 + (int)(h % 3);
    return (int32_t)(((1u << e) + ((h >> 8) & ((1u << e) - 1))) * ((h >> 31) ? -1 : 1));
}

static void cell(int page, int c, cell_t *p)
{
    const page_t *g = &pages[page];
    const uint32_t s = (uint32_t)(c % CELLS) * 977u + (uint32_t)page * 100003u;
    int k;
    p->pu = (int32_t)(hsh(s + 1) & 0x1fffff);
    p->pv = (int32_t)(hsh(s + 2) & 0x1fffff);
    if(hsh(s + 3) & 1) {                /* u along x, v along y; or turned */
        p->sux = slope(hsh(s + 4), tex_mip(g->tex), g->persp); p->suy = slope(hsh(s + 5), tex_mip(g->tex), g->persp) / 4;
        p->svx = slope(hsh(s + 6), tex_mip(g->tex), g->persp) / 4; p->svy = slope(hsh(s + 7), tex_mip(g->tex), g->persp);
    } else {
        p->sux = slope(hsh(s + 4), tex_mip(g->tex), g->persp) / 4; p->suy = slope(hsh(s + 5), tex_mip(g->tex), g->persp);
        p->svx = slope(hsh(s + 6), tex_mip(g->tex), g->persp); p->svy = slope(hsh(s + 7), tex_mip(g->tex), g->persp) / 4;
    }
    for(k = 0; k < 4; k++) {
        p->zm[k] = g->persp ? (1u << 20) + hsh(s + 10 + (uint32_t)k) % ((1u << 23) - (1u << 20)) : 0;   /* / 2^22: 1/4 .. 2 */
        p->base[k] = g->gouraud ? hsh(s + 20 + (uint32_t)(g->gouraud == 2 ? 0 : k)) : 0xffffffffu;
        p->offs[k] = g->gouraud ? hsh(s + 30 + (uint32_t)(g->gouraud == 2 ? 0 : k)) : 0;
    }
    p->mipd = tex_mip(g->tex) ? 1 + (int)(hsh(s + 40) % 15) : 4;
}

static void test_describe(void)
{
    static const char *tn[NTEX] = { "565", "4444", "4444 VQ", "565 mip", "4444 mip", "4444 VQ mip", "bump", "bump mip" };
    static const char *fn[4] = { "point", "bilinear", "trilinear A", "trilinear B" };
    static const char *sn[4] = { "decal", "modulate", "decal alpha", "modulate alpha" };
    static const char *fogn[4] = { ", table fog", ", vertex fog", "", ", table fog 2" };
    static const char *bn[3] = { "", ", x alpha", ", over" };
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 300; c++) {
            const page_t *g = &pages[p];
            cell_t q;
            cell(p, c, &q);
            if(g->extra & NOTEX) {
                label(p, c, "%sno texture, perspective, Gouraud, table fog, over, z %u %u %u %u / 2^22", c < CELLS ? "SupSample set: " : "",
                      (unsigned)q.zm[0], (unsigned)q.zm[1], (unsigned)q.zm[2], (unsigned)q.zm[3]);
                continue;
            }
            label(p, c, "%s%s %s %s%s, %s%s%s%s, D %d: u (%d + %d X + %d Y) / 2^22, v (%d + %d X + %d Y) / 2^22, z %u %u %u %u / 2^22",
                  c < CELLS ? "" : "no SupSample: ", tn[g->tex], fn[g->filter], g->persp ? "perspective" : "flat",
                  g->gouraud == 1 ? ", Gouraud with offset" : g->gouraud ? ", base and offset" : "", sn[g->shad], fogn[g->fog], bn[g->blend],
                  g->extra & CLAMP ? ", colour clamp" : g->extra & IGNA ? ", IgnoreTexA" : "", q.mipd,
                  (int)q.pu, (int)q.sux, (int)q.suy, (int)q.pv, (int)q.svx, (int)q.svy,
                  (unsigned)q.zm[0], (unsigned)q.zm[1], (unsigned)q.zm[2], (unsigned)q.zm[3]);
        }
}

static void test_page(int page)
{
    const page_t *g = &pages[page];
    const int tr = g->blend == OVER;
    int c, k;
    pvr_frame.bg_color = tr ? 0x803020 : g->blend == XALPHA ? 0 : 0x000040;
    pvrio_reg(FOG_COL_RAM_addr, 0xc0e57f19u);
    pvrio_reg(FOG_COL_VERT_addr, 0x0033cc66u);
    pvrio_reg(FOG_DENSITY_addr, 0x00008004u);          /* 1.0 x 2^4 */
    pvrio_reg(FOG_CLAMP_MAX_addr, g->extra & CLAMP ? 0xd0c0b0a0u : 0xffffffffu);
    pvrio_reg(FOG_CLAMP_MIN_addr, g->extra & CLAMP ? 0x20304050u : 0);
    for(k = 0; k < 128; k++)
        pvrio_reg(FOG_TABLE_START_addr + 4 * (uint32_t)k, fog_table[k]);
    ta_list_begin(tr ? ListType_Translucent : ListType_Opaque);
    for(c = 0; c < 300; c++) {
        PCW pcw = { .full = 0 };
        ISP_TSP isp = { .full = 0 };
        TSP tsp = { .full = 0 };
        TCW tcw = { .full = 0 };
        const int x = cell_px(c), y = cell_py(c);
        cell_t q;
        cell(page, c, &q);
        pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
        pcw.ListType = tr ? ListType_Translucent : ListType_Opaque;
        pcw.Group_En = 1;
        pcw.Strip_Len = 1;
        pcw.Texture = !(g->extra & NOTEX);
        pcw.Gouraud = g->gouraud == 1;
        pcw.Offset = g->gouraud != 0 && pcw.Texture;
        isp.DepthMode = DepthMode_Always;
        isp.CullMode = CullMode_None;
        isp.Texture = pcw.Texture;
        isp.Gouraud = pcw.Gouraud;
        isp.Offset = pcw.Offset;
        tsp.SrcInstr = g->blend == OPAQUE ? BlendInstr_One : BlendInstr_SrcAlpha;
        tsp.DstInstr = tr ? BlendInstr_InvSrcAlpha : BlendInstr_Zero;
        tsp.FogCtrl = (uint32_t)g->fog;
        tsp.UseAlpha = 1;
        tsp.ColorClamp = (g->extra & CLAMP) != 0;
        tsp.IgnoreTexA = (g->extra & IGNA) != 0;
        tsp.ShadInstr = (uint32_t)g->shad;
        tsp.FilterMode = (uint32_t)g->filter;
        tsp.SupSample = c < CELLS;
        tsp.MipMapD = (uint32_t)q.mipd;
        tsp.TexU = 3;                   /* 64 */
        tsp.TexV = 3;
        tcw.PixelFmt = (uint32_t)tex_fmt(g->tex);
        tcw.VQ_Comp = tex_vq(g->tex);
        tcw.MipMapped = tex_mip(g->tex);
        tcw.TexAddr = tex[g->tex] >> 3;
        ta_polygon(pcw, isp, tsp, tcw);
        for(k = 0; k < 4; k++) {
            const int X = (k & 1) * 32, Y = (k >> 1) * 32;
            if(g->extra & NOTEX) {
                ta_vertex0(f32b_int(x + X), f32b_int(y + Y), f32b_scaled((int32_t)q.zm[k], -22), q.base[k], k == 3);
                continue;
            }
            ta_vertex3(f32b_int(x + X), f32b_int(y + Y), g->persp ? f32b_scaled((int32_t)q.zm[k], -22) : F32B_ONE,
                       f32b_scaled(q.pu + X * q.sux + Y * q.suy, -22), f32b_scaled(q.pv + X * q.svx + Y * q.svy, -22),
                       q.base[k], q.offs[k], k == 3);
        }
    }
    ta_list_end();
}
