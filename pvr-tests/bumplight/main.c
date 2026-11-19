/* A bump map's light (2026-11-19; pvr-tests/supsample2 pages 29 to 31 found it): K1 K2 K3 Q, the offset colour, are
   the LAST vertex's offset colour as it is. Not interpolated when the polygon is Gouraud shaded (supsample2 page 30
   and its control: 123,578 pixels missed), and not taken through the 1/w planes when it is flat (attribute / w
   divided by 1/w again: a few pixels next to a corner of large w were one or two levels off). The base colour of a
   bump map is Gouraud shaded as any other.

   Every cell is a strip of four triangles (six vertices, three columns of 16 pixels: v0 v2 v4 on the top edge, v1 v3
   v5 on the bottom edge), every vertex its own base and offset colour, so each triangle shows whose light it took.
   The bump texture is 64x64 of arbitrary S and R, coordinates u = (P + X Sx + Y Sy) / 2^22 at the vertex (X, Y),
   1/4 .. 2 texels a pixel. The result of a bump map is white with the intensity in alpha: shown "x alpha",
   (SRC_ALPHA, ZERO) on black, or "over", the translucent list (SRC_ALPHA, INV_SRC_ALPHA) over 0x803020.
   Depths: flat (z = 1 everywhere) or extreme (every vertex its own z, 2^-8 .. 4: w far apart within a triangle).
     page  0  point, flat, Gouraud, decal, x alpha
     page  1  the same, not Gouraud (flat shaded)
     page  2  bilinear, extreme, Gouraud, modulate, x alpha
     page  3  the same, flat shaded
     page  4  bilinear, extreme, Gouraud, modulate alpha, SupSample, over
     page  5  point, flat, Gouraud, modulate alpha, vertex fog (the offset alpha is K1 here), over
     page  6  bilinear, extreme, Gouraud, modulate alpha, table fog, over
     page  7  point, z = 1/2, Gouraud, modulate, x alpha, an intensity shadow (FPU_SHAD_SCALE 0x180) over the left 20
              pixels: the shadow scales the vertices' colours
   Not bump maps, the same strips (is a flat polygon's colour taken as it is, or through the planes, as refsw3 does?):
     page  8  a white 565 texture, modulate + offset, extreme, flat shaded
     page  9  the same, Gouraud (the control)
     page 10  no texture, extreme, flat shaded, opaque
     page 11  no texture, extreme, flat shaded, over
   Fog: FOG_DENSITY 16, an arbitrary table, FOG_COL_RAM 0xc0e57f19, FOG_COL_VERT 0x0033cc66. */
#define NUM_PAGES 12
#define TEST_MODIFIERS
#include "pvrtest2.h"
#include "texgen.h"

enum { OPAQUE, OVER, XALPHA };
enum { BUMPT, WHITE, NONE };
typedef struct { int tex, filter, extreme, gouraud, shad, fog, blend, super, shadow; } page_t;
static const page_t pages[NUM_PAGES] = {
    { BUMPT, FilterMode_Point, 0, 1, ShadInstr_Decal, FogCtrl_None, XALPHA, 0, 0 },
    { BUMPT, FilterMode_Point, 0, 0, ShadInstr_Decal, FogCtrl_None, XALPHA, 0, 0 },
    { BUMPT, FilterMode_Bilinear, 1, 1, ShadInstr_Modulate, FogCtrl_None, XALPHA, 0, 0 },
    { BUMPT, FilterMode_Bilinear, 1, 0, ShadInstr_Modulate, FogCtrl_None, XALPHA, 0, 0 },
    { BUMPT, FilterMode_Bilinear, 1, 1, ShadInstr_ModulateAlpha, FogCtrl_None, OVER, 1, 0 },
    { BUMPT, FilterMode_Point, 0, 1, ShadInstr_ModulateAlpha, FogCtrl_Vertex, OVER, 0, 0 },
    { BUMPT, FilterMode_Bilinear, 1, 1, ShadInstr_ModulateAlpha, FogCtrl_Table, OVER, 0, 0 },
    { BUMPT, FilterMode_Point, 0, 1, ShadInstr_Modulate, FogCtrl_None, XALPHA, 0, 1 },
    { WHITE, FilterMode_Point, 1, 0, ShadInstr_Modulate, FogCtrl_None, OPAQUE, 0, 0 },
    { WHITE, FilterMode_Point, 1, 1, ShadInstr_Modulate, FogCtrl_None, OPAQUE, 0, 0 },
    { NONE, FilterMode_Point, 1, 0, ShadInstr_Decal, FogCtrl_None, OPAQUE, 0, 0 },
    { NONE, FilterMode_Point, 1, 0, ShadInstr_Decal, FogCtrl_None, OVER, 0, 0 },
};

static uint32_t tex_bump, tex_white;
static uint32_t fog_table[128];

static uint32_t hsh(uint32_t v) { v *= 2654435761u; v ^= v >> 15; v *= 2246822519u; v ^= v >> 13; v *= 3266489917u; v ^= v >> 16; return v; }

static uint32_t rnd_texel(const void *ctx, int size, int x, int y)
{
    return hsh((uint32_t)(size * 4096 + y * 64 + x) ^ *(const uint32_t *)ctx) & 0xffff;
}
static uint32_t white_texel(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0xffff; }

static void test_setup(void)
{
    static const uint32_t seed = 0x4d4d4d4u;
    const tex_layout_t l = { TEX_TWIDDLED, 16, 64, 64, 0 };
    int k;
    tex_bump = tex_build(&l, rnd_texel, &seed);
    tex_white = tex_build(&l, white_texel, &seed);
    for(k = 0; k < 128; k++)
        fog_table[k] = hsh((uint32_t)k + 0x9900u) & 0xffff;
}

/* vertex i of the strip: column i / 2 (0, 16, 32), top (even) or bottom (odd) */
static int vx(int i) { return (i >> 1) * 16; }
static int vy(int i) { return (i & 1) * 32; }

typedef struct { int32_t pu, sux, suy, pv, svx, svy; f32b z[6]; uint32_t base[6], offs[6]; } cell_t;

static void cell(int page, int c, cell_t *p)
{
    const page_t *g = &pages[page];
    const uint32_t s = (uint32_t)c * 977u + (uint32_t)page * 100003u;
    int i;
    for(i = 0; i < 4; i++) {
        const uint32_t h = hsh(s + 50 + (uint32_t)i);
        const int32_t m = (int32_t)((1u << 14) + (h & ((1u << 17) - (1u << 14))));     /* 1/4 .. 2 texels */
        const int32_t v = (h >> 31) ? -m : m;
        if(i == 0) p->sux = v; else if(i == 1) p->svy = v; else if(i == 2) p->suy = v / 4; else p->svx = v / 4;
    }
    p->pu = (int32_t)(hsh(s + 1) & 0x3fffff);
    p->pv = (int32_t)(hsh(s + 2) & 0x3fffff);
    for(i = 0; i < 6; i++) {
        const uint32_t h = hsh(s + 10 + (uint32_t)i);
        /* extreme: a mantissa 1 .. 2 at 2^-8 .. 2^1 */
        p->z[i] = g->extreme ? f32b_scaled((int32_t)((1u << 22) + (h & ((1u << 22) - 1))), -22 - 8 + (int)((h >> 24) % 10))
                             : g->shadow ? F32B_HALF : F32B_ONE;
        p->base[i] = hsh(s + 20 + (uint32_t)i);
        p->offs[i] = hsh(s + 30 + (uint32_t)i);
    }
}

static void test_describe(void)
{
    static const char *tn[3] = { "bump", "white 565", "no texture" };
    static const char *fn[2] = { "point", "bilinear" };
    static const char *sn[4] = { "decal", "modulate", "decal alpha", "modulate alpha" };
    static const char *fogn[4] = { ", table fog", ", vertex fog", "", ", table fog 2" };
    static const char *bn[3] = { "", ", over", ", x alpha" };
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 300; c++) {
            const page_t *g = &pages[p];
            label(p, c, "%s %s, %s, %s, %s%s%s%s%s", tn[g->tex], fn[g->filter], g->extreme ? "extreme depths" : "flat",
                  g->gouraud ? "Gouraud" : "flat shaded", sn[g->shad], fogn[g->fog], bn[g->blend], g->super ? ", SupSample" : "",
                  g->shadow ? ", intensity shadow over the left 20 pixels" : "");
        }
}

static void sheet(int x, int y)
{
    int k;
    for(k = 0; k < 2; k++) {
        TA_ModVolParam m;
        TA_ModVol v;
        memset(&m, 0, sizeof(m));
        m.pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
        m.pcw.ListType = ListType_Opaque_Modifier_Volume;
        m.isp.modvol.VolumeMode = k ? VolumeMode_Inside_Last : VolumeMode_Normal;
        m.isp.modvol.CullMode = CullMode_None;
        ta_send(&m, sizeof(m));
        memset(&v, 0, sizeof(v));
        v.pcw = pcw_vertex(1);
        v.x0 = f32b_int(x);       v.y0 = f32b_int(y);                  v.z0 = F(0.75f);
        v.x1 = f32b_int(x + 20);  v.y1 = f32b_int(k ? y + 32 : y);     v.z1 = F(0.75f);
        v.x2 = f32b_int(k ? x : x + 20);  v.y2 = f32b_int(y + 32);     v.z2 = F(0.75f);
        ta_send(&v, sizeof(v));
    }
}

static void test_page(int page)
{
    const page_t *g = &pages[page];
    const int tr = g->blend == OVER;
    int c, i;
    pvr_frame.bg_color = tr ? 0x803020 : g->blend == XALPHA ? 0 : 0x000040;
    pvrio_reg(FOG_COL_RAM_addr, 0xc0e57f19u);
    pvrio_reg(FOG_COL_VERT_addr, 0x0033cc66u);
    pvrio_reg(FOG_DENSITY_addr, 0x00008004u);
    pvrio_reg(FPU_SHAD_SCALE_addr, g->shadow ? 0x180 : 0x080);
    for(c = 0; c < 128; c++)
        pvrio_reg(FOG_TABLE_START_addr + 4 * (uint32_t)c, fog_table[c]);
    ta_list_begin(tr ? ListType_Translucent : ListType_Opaque);
    for(c = 0; c < 300; c++) {
        PCW pcw = { .full = 0 };
        ISP_TSP isp = { .full = 0 };
        TSP tsp = { .full = 0 };
        TCW tcw = { .full = 0 };
        const int x = cell_px(c), y = cell_py(c), tx = g->tex != NONE;
        cell_t q;
        cell(page, c, &q);
        pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
        pcw.ListType = tr ? ListType_Translucent : ListType_Opaque;
        pcw.Group_En = 1;
        pcw.Strip_Len = 1;
        pcw.Texture = tx;
        pcw.Offset = tx;
        pcw.Gouraud = g->gouraud;
        pcw.Shadow = g->shadow;
        isp.DepthMode = DepthMode_Always;
        isp.CullMode = CullMode_None;
        isp.Texture = pcw.Texture;
        isp.Offset = pcw.Offset;
        isp.Gouraud = pcw.Gouraud;
        tsp.SrcInstr = g->blend == OPAQUE ? BlendInstr_One : BlendInstr_SrcAlpha;
        tsp.DstInstr = tr ? BlendInstr_InvSrcAlpha : BlendInstr_Zero;
        tsp.FogCtrl = (uint32_t)g->fog;
        tsp.UseAlpha = 1;
        tsp.ShadInstr = (uint32_t)g->shad;
        tsp.FilterMode = (uint32_t)g->filter;
        tsp.SupSample = (uint32_t)g->super;
        tsp.MipMapD = 4;
        tsp.TexU = 3;
        tsp.TexV = 3;
        tcw.PixelFmt = g->tex == BUMPT ? PixelFmt_BumpMap : PixelFmt_565;
        tcw.TexAddr = (g->tex == BUMPT ? tex_bump : tex_white) >> 3;
        ta_polygon(pcw, isp, tsp, tcw);
        for(i = 0; i < 6; i++) {
            const int X = vx(i), Y = vy(i);
            if(tx)
                ta_vertex3(f32b_int(x + X), f32b_int(y + Y), q.z[i], f32b_scaled(q.pu + X * q.sux + Y * q.suy, -22),
                           f32b_scaled(q.pv + X * q.svx + Y * q.svy, -22), q.base[i], q.offs[i], i == 5);
            else
                ta_vertex0(f32b_int(x + X), f32b_int(y + Y), q.z[i], q.base[i], i == 5);
        }
    }
    ta_list_end();
    if(g->shadow) {
        ta_list_begin(ListType_Opaque_Modifier_Volume);
        for(c = 0; c < 300; c++)
            sheet(cell_px(c), cell_py(c));
        ta_list_end();
    } else {                            /* the list is in the setup: an empty one */
        ta_list_begin(ListType_Opaque_Modifier_Volume);
        ta_list_end();
    }
}
