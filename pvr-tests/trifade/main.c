/* The trilinear fade: of what, and with what arithmetic (2026-11-19; pvr-tests/supsample2 page 21 found it).

   A trilinear pass (FilterMode 2: A, 3: B) of a mipmapped texture is faded by the level's fraction k: pass A times
   256 - k, pass B (which reads the next smaller level) times k, >> 8. refsw3 had faded the texel; supsample2's
   control cells say the fade is of the FINISHED colour: after the shading instruction, the offset colour, the
   colour clamp and the fog, on all four channels. Before that every trilinear test had been decal without offset
   or fog, where the two are the same.

   Every cell is a flat quad (z the same at its four corners, so 1/w, the level and k are the same all over it) with
   arithmetic it is easy to follow: the texture's levels are each ONE colour (every level its own, all 16 bits used,
   4444 so with alpha), so the texel is known exactly whatever the filter; the base and offset colours are the
   cell's own (not Gouraud), arbitrary; the cell's slope (1/16 .. 4 texels a pixel) and D adjust (1 .. 15) give it
   its own level and k, and its z (1/4 .. 2) its own fog. So a cell is one colour, and each alternative order
   (fade the texel, the shaded colour, before or after the offset, the clamp, the fog) gives another one.
   Blend: opaque (ONE, ZERO) on 0x000040; "over": the translucent list, (SRC_ALPHA, INV_SRC_ALPHA) over 0x803020
   (the alpha shows); "x alpha": (SRC_ALPHA, ZERO) on black. Fog: FOG_DENSITY 16, an arbitrary table,
   FOG_COL_RAM 0xc0e57f19, FOG_COL_VERT 0x0033cc66. Colour clamp: FOG_CLAMP_MIN 0x30405060, MAX 0xc0b0a090.
     page  0  A decal                               page  1  B decal
     page  2  A decal + offset                      page  3  B modulate + offset
     page  4  A modulate + offset, colour clamp
     page  5  B decal alpha + offset, over          page  6  A modulate alpha + offset, over
     page  7  B modulate alpha + offset, colour clamp, over
     page  8  A modulate + offset, table fog        page  9  B modulate alpha + offset, table fog, over
     page 10  A modulate alpha + offset, table fog mode 2, over
     page 11  B decal + offset, vertex fog          page 12  A modulate alpha + offset, vertex fog, colour clamp, over
     page 13  B modulate alpha + offset, IgnoreTexA, over
     page 14  A modulate alpha, UseAlpha 0 (the base colour's alpha is 255), over
   Textures of other kinds:
     page 15  A, a raster texture with the MipMapped bit (one level; the fraction still fades: pvr-tests/mip),
              modulate + offset, table fog
     page 16  B, the same, modulate alpha + offset, over
     page 17  A, a texture without mipmaps (one texel at full strength, no fade), modulate alpha + offset,
              table fog, over
     page 18  A, a mipmapped bump map (every level one S and R; the light in the offset colour), modulate, x alpha
     page 19  B, the same, decal alpha, over
   The way games draw it, the two passes of the same quad one after the other in the translucent list:
     page 20  A (ONE, ZERO) then B (ONE, ONE), modulate + offset, table fog
     page 21  A (SRC_ALPHA, INV_SRC_ALPHA) then B (SRC_ALPHA, ONE), modulate alpha + offset, vertex fog */
#define NUM_PAGES 22
#include "pvrtest2.h"
#include "texgen.h"

enum { MIP, RASTER, FLAT, BUMP, NTEX };
enum { OPAQUE, OVER, XALPHA, TWOPASS };
enum { CLAMP = 1, IGNA = 2, NOALPHA = 4 };
typedef struct { int pass, shad, offset, fog, blend, extra, tex; } page_t;
static const page_t pages[NUM_PAGES] = {
    { 2, ShadInstr_Decal, 0, FogCtrl_None, OPAQUE, 0, MIP },
    { 3, ShadInstr_Decal, 0, FogCtrl_None, OPAQUE, 0, MIP },
    { 2, ShadInstr_Decal, 1, FogCtrl_None, OPAQUE, 0, MIP },
    { 3, ShadInstr_Modulate, 1, FogCtrl_None, OPAQUE, 0, MIP },
    { 2, ShadInstr_Modulate, 1, FogCtrl_None, OPAQUE, CLAMP, MIP },
    { 3, ShadInstr_DecalAlpha, 1, FogCtrl_None, OVER, 0, MIP },
    { 2, ShadInstr_ModulateAlpha, 1, FogCtrl_None, OVER, 0, MIP },
    { 3, ShadInstr_ModulateAlpha, 1, FogCtrl_None, OVER, CLAMP, MIP },
    { 2, ShadInstr_Modulate, 1, FogCtrl_Table, OPAQUE, 0, MIP },
    { 3, ShadInstr_ModulateAlpha, 1, FogCtrl_Table, OVER, 0, MIP },
    { 2, ShadInstr_ModulateAlpha, 1, FogCtrl_Table2, OVER, 0, MIP },
    { 3, ShadInstr_Decal, 1, FogCtrl_Vertex, OPAQUE, 0, MIP },
    { 2, ShadInstr_ModulateAlpha, 1, FogCtrl_Vertex, OVER, CLAMP, MIP },
    { 3, ShadInstr_ModulateAlpha, 1, FogCtrl_None, OVER, IGNA, MIP },
    { 2, ShadInstr_ModulateAlpha, 0, FogCtrl_None, OVER, NOALPHA, MIP },
    { 2, ShadInstr_Modulate, 1, FogCtrl_Table, OPAQUE, 0, RASTER },
    { 3, ShadInstr_ModulateAlpha, 1, FogCtrl_None, OVER, 0, RASTER },
    { 2, ShadInstr_ModulateAlpha, 1, FogCtrl_Table, OVER, 0, FLAT },
    { 2, ShadInstr_Modulate, 1, FogCtrl_None, XALPHA, 0, BUMP },
    { 3, ShadInstr_DecalAlpha, 1, FogCtrl_None, OVER, 0, BUMP },
    { 2, ShadInstr_Modulate, 1, FogCtrl_Table, TWOPASS, 0, MIP },
    { 2, ShadInstr_ModulateAlpha, 1, FogCtrl_Vertex, TWOPASS, 0, MIP },
};

static uint32_t tex[NTEX];
static uint32_t fog_table[128];

static uint32_t hsh(uint32_t v) { v *= 2654435761u; v ^= v >> 15; v *= 2246822519u; v ^= v >> 13; v *= 3266489917u; v ^= v >> 16; return v; }

/* every level one colour: the level is the size */
static uint32_t level_colour(const void *ctx, int size, int x, int y)
{
    (void)x; (void)y;
    return hsh((uint32_t)size * 31u + *(const uint32_t *)ctx) & 0xffff;
}

static void test_setup(void)
{
    static const uint32_t seed[NTEX] = { 0x3141592u, 0x2718281u, 0x1618033u, 0x1414213u };
    static const int layout[NTEX] = { TEX_MIP, TEX_RASTER, TEX_TWIDDLED, TEX_MIP };
    int k;
    for(k = 0; k < NTEX; k++) {
        const tex_layout_t l = { layout[k], 16, 64, 64, 0 };
        tex[k] = tex_build(&l, level_colour, &seed[k]);
    }
    for(k = 0; k < 128; k++)
        fog_table[k] = hsh((uint32_t)k + 0x7700u) & 0xffff;
}

typedef struct { int32_t pu, sux, suy, pv, svx, svy; uint32_t zm, base, offs; int mipd; } cell_t;

static void cell(int page, int c, cell_t *p)
{
    const uint32_t s = (uint32_t)c * 977u + (uint32_t)page * 100003u;
    const uint32_t h = hsh(s + 4);
    int32_t m;
    uint64_t sl;
    p->zm = (1u << 20) + hsh(s + 10) % ((1u << 23) - (1u << 20));           /* / 2^22: 1/4 .. 2 */
    p->mipd = 1 + (int)(hsh(s + 40) % 15);
    /* the slope for a level of about d = dm / 256 (1/2 .. 16: levels 0 .. 4 with every fraction) at this z and D:
       texels a pixel of the 64 wide level s = d * 4 z^2 / D, in units of 2^-22 of u: s * 2^16 */
    {
        const int e = (int)(h % 5);
        const uint64_t dm = (128u << e) + ((h >> 8) % (128u << e));
        sl = dm * (uint64_t)p->zm * (uint64_t)p->zm / ((uint64_t)p->mipd << 34);
        if(sl > (1u << 24)) sl = 1u << 24;
        if(sl < 16) sl = 16;
        m = (int32_t)sl;
    }
    p->pu = (int32_t)(hsh(s + 1) & 0x3fffff);
    p->pv = (int32_t)(hsh(s + 2) & 0x3fffff);
    p->sux = (h >> 31) ? -m : m;
    p->svy = (int32_t)(hsh(s + 5) % (uint32_t)(m / 4 + 1)) * ((hsh(s + 6) & 1) ? -1 : 1);
    p->suy = (int32_t)(hsh(s + 7) % (uint32_t)(m / 4 + 1)) - m / 8;
    p->svx = (int32_t)(hsh(s + 8) % (uint32_t)(m / 4 + 1)) - m / 8;
    p->base = hsh(s + 20);
    p->offs = hsh(s + 30);
}

static void test_describe(void)
{
    static const char *sn[4] = { "decal", "modulate", "decal alpha", "modulate alpha" };
    static const char *fogn[4] = { ", table fog", ", vertex fog", "", ", table fog 2" };
    static const char *tn[NTEX] = { "mip", "raster with MipMapped", "no mipmaps", "bump mip" };
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 300; c++) {
            const page_t *g = &pages[p];
            cell_t q;
            cell(p, c, &q);
            label(p, c, "%s, pass %s%s, %s%s%s%s, D %d, z %u / 2^22, base %08x offset %08x: u (%d + %d X + %d Y) / 2^22, v (%d + %d X + %d Y) / 2^22",
                  tn[g->tex], g->blend == TWOPASS ? "A then B" : g->pass == 2 ? "A" : "B", g->offset ? " + offset" : "", sn[g->shad], fogn[g->fog],
                  g->extra & CLAMP ? ", colour clamp" : g->extra & IGNA ? ", IgnoreTexA" : g->extra & NOALPHA ? ", UseAlpha 0" : "",
                  g->blend == OVER ? ", over" : g->blend == XALPHA ? ", x alpha" : "", q.mipd, (unsigned)q.zm, (unsigned)q.base, (unsigned)q.offs,
                  (int)q.pu, (int)q.sux, (int)q.suy, (int)q.pv, (int)q.svx, (int)q.svy);
        }
}

static void quad(const page_t *g, int c, const cell_t *q, int pass, int src, int dst)
{
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    const int tr = g->blend == OVER || g->blend == TWOPASS;
    const int x = cell_px(c), y = cell_py(c);
    int k;
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = tr ? ListType_Translucent : ListType_Opaque;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Texture = 1;
    pcw.Offset = g->offset;
    isp.DepthMode = DepthMode_Always;
    isp.ZWriteDis = 1;
    isp.CullMode = CullMode_None;
    isp.Texture = 1;
    isp.Offset = pcw.Offset;
    tsp.SrcInstr = (uint32_t)src;
    tsp.DstInstr = (uint32_t)dst;
    tsp.FogCtrl = (uint32_t)g->fog;
    tsp.UseAlpha = !(g->extra & NOALPHA);
    tsp.ColorClamp = (g->extra & CLAMP) != 0;
    tsp.IgnoreTexA = (g->extra & IGNA) != 0;
    tsp.ShadInstr = (uint32_t)g->shad;
    tsp.FilterMode = (uint32_t)pass;
    tsp.MipMapD = (uint32_t)q->mipd;
    tsp.TexU = 3;                       /* 64 */
    tsp.TexV = 3;
    tcw.PixelFmt = g->tex == BUMP ? PixelFmt_BumpMap : PixelFmt_4444;
    tcw.MipMapped = g->tex != FLAT;
    tcw.ScanOrder = g->tex == RASTER;
    tcw.TexAddr = tex[g->tex] >> 3;
    ta_polygon(pcw, isp, tsp, tcw);
    for(k = 0; k < 4; k++) {
        const int X = (k & 1) * 32, Y = (k >> 1) * 32;
        ta_vertex3(f32b_int(x + X), f32b_int(y + Y), f32b_scaled((int32_t)q->zm, -22),
                   f32b_scaled(q->pu + X * q->sux + Y * q->suy, -22), f32b_scaled(q->pv + X * q->svx + Y * q->svy, -22),
                   q->base, q->offs, k == 3);
    }
}

static void test_page(int page)
{
    const page_t *g = &pages[page];
    const int tr = g->blend == OVER || g->blend == TWOPASS;
    int c;
    pvr_frame.bg_color = tr ? 0x803020 : g->blend == XALPHA ? 0 : 0x000040;
    pvrio_reg(FOG_COL_RAM_addr, 0xc0e57f19u);
    pvrio_reg(FOG_COL_VERT_addr, 0x0033cc66u);
    pvrio_reg(FOG_DENSITY_addr, 0x00008004u);
    pvrio_reg(FOG_CLAMP_MAX_addr, g->extra & CLAMP ? 0xc0b0a090u : 0xffffffffu);
    pvrio_reg(FOG_CLAMP_MIN_addr, g->extra & CLAMP ? 0x30405060u : 0);
    for(c = 0; c < 128; c++)
        pvrio_reg(FOG_TABLE_START_addr + 4 * (uint32_t)c, fog_table[c]);
    ta_list_begin(tr ? ListType_Translucent : ListType_Opaque);
    for(c = 0; c < 300; c++) {
        cell_t q;
        cell(page, c, &q);
        if(g->blend == TWOPASS) {
            const int alpha = g->shad >= ShadInstr_DecalAlpha;
            quad(g, c, &q, 2, alpha ? BlendInstr_SrcAlpha : BlendInstr_One, alpha ? BlendInstr_InvSrcAlpha : BlendInstr_Zero);
            quad(g, c, &q, 3, alpha ? BlendInstr_SrcAlpha : BlendInstr_One, BlendInstr_One);
        } else
            quad(g, c, &q, g->pass, g->blend == OPAQUE ? BlendInstr_One : BlendInstr_SrcAlpha,
                 g->blend == OVER ? BlendInstr_InvSrcAlpha : BlendInstr_Zero);
    }
    ta_list_end();
}
