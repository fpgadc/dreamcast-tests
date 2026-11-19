/* SupSample: the colours, the fog, and the bit with other features (2026-11-19; pvr-tests/supsample2 found the rules).
   Each of the four points of a SupSample pixel (its centre, half a pixel right, down, both; the planes evaluated in
   half pixel steps) has its own base and offset colours and its own fog (table: its own 1/w; vertex: its own offset
   alpha), and the four finished colours are averaged, each row's two then the rows. The bit works on a polygon
   without a texture too.

   Cells 0 .. 149 with SupSample, 150 .. 299 the same without (the control). Cells are 32x32 quads; colours at
   their corners are arbitrary (all 32 bits), so the slopes are anything up to 255 / 32 a pixel, either sign.
   "Steep along x": the left corners share one colour, the right ones another (the plane changes along x only);
   "along y" likewise. Perspective: every corner its own z, 1/8 .. 2.
   Blend: opaque (ONE, ZERO) on 0x000040; "over": translucent (SRC_ALPHA, INV_SRC_ALPHA) over 0x803020.
   Fog: FOG_DENSITY 16, an arbitrary table, FOG_COL_RAM 0xc0e57f19, FOG_COL_VERT 0x0033cc66.
     page  0  no texture, Gouraud, steep along x, flat z, opaque
     page  1  no texture, Gouraud, steep along y, flat z, opaque
     page  2  no texture, Gouraud, perspective, over
     page  3  no texture, flat shaded, perspective, table fog, opaque (only the fog changes in the cell)
     page  4  no texture, Gouraud, perspective, table fog mode 2, over
     page  5  no texture, Gouraud, perspective, table fog, colour clamp (MIN 0x30405060, MAX 0xc0b0a090), over
     page  6  white 565 texture, modulate + Gouraud offset, steep along x, opaque
     page  7  white 565 texture, modulate + Gouraud offset, vertex fog (the offset alpha), perspective, opaque
   The bit with other features:
     page  8  punch-through, 4444 texture of arbitrary alpha, bilinear, modulate alpha, Gouraud (alpha 0x80 ..
              0xff at the corners), PT_ALPHA_REF 0x80 (is the average alpha tested, or each point's?)
     page  9  punch-through, no texture, Gouraud (alpha 0x80 .. 0xff at the corners), PT_ALPHA_REF 0x80
     page 10  an intensity shadow (FPU_SHAD_SCALE 0x180) over the left 20 pixels of every cell: arbitrary 565
              texture, bilinear, modulate + Gouraud offset, perspective
     page 11  two volumes (a modifier volume over the left 20 pixels): outside an arbitrary 565 texture, inside an
              arbitrary 4444 one, each with its own Gouraud colours; cells 0 .. 149 SupSample outside only,
              150 .. 299 inside only; modulate (outside) and modulate alpha (inside), both with the offset colour,
              bilinear
     page 12  the same, cells 0 .. 149 SupSample in both volumes, 150 .. 299 in neither
     page 13  the same as page 11 with point sampling and decal (no colours) in both volumes
     page 14  the same as page 11 with decal (no colours)
     page 15  the same as page 11 with point sampling */
#define NUM_PAGES 16
#define TEST_MODIFIERS
#include "pvrtest2.h"
#include "texgen.h"

enum { OPAQUE, OVER, PT };
enum { NOTEX, WHITE, R4444, R565 };
enum { STEEPX = 1, STEEPY = 2, PERSP = 4, CLAMP = 8, SHADOW = 16, TWOVOL = 32, BOTHVOL = 64, DECAL2 = 128 };
typedef struct { int tex, gouraud, shad, fog, blend, filter, flags; } page_t;
static const page_t pages[NUM_PAGES] = {
    { NOTEX, 1, ShadInstr_Decal, FogCtrl_None, OPAQUE, 0, STEEPX },
    { NOTEX, 1, ShadInstr_Decal, FogCtrl_None, OPAQUE, 0, STEEPY },
    { NOTEX, 1, ShadInstr_Decal, FogCtrl_None, OVER, 0, PERSP },
    { NOTEX, 0, ShadInstr_Decal, FogCtrl_Table, OPAQUE, 0, PERSP },
    { NOTEX, 1, ShadInstr_Decal, FogCtrl_Table2, OVER, 0, PERSP },
    { NOTEX, 1, ShadInstr_Decal, FogCtrl_Table, OVER, 0, PERSP | CLAMP },
    { WHITE, 1, ShadInstr_Modulate, FogCtrl_None, OPAQUE, FilterMode_Point, STEEPX },
    { WHITE, 1, ShadInstr_Modulate, FogCtrl_Vertex, OPAQUE, FilterMode_Point, PERSP },
    { R4444, 1, ShadInstr_ModulateAlpha, FogCtrl_None, PT, FilterMode_Bilinear, PERSP },
    { NOTEX, 1, ShadInstr_Decal, FogCtrl_None, PT, 0, PERSP },
    { R565, 1, ShadInstr_Modulate, FogCtrl_None, OPAQUE, FilterMode_Bilinear, PERSP | SHADOW },
    { R565, 1, ShadInstr_Modulate, FogCtrl_None, OPAQUE, FilterMode_Bilinear, PERSP | TWOVOL },
    { R565, 1, ShadInstr_Modulate, FogCtrl_None, OPAQUE, FilterMode_Bilinear, PERSP | TWOVOL | BOTHVOL },
    { R565, 1, ShadInstr_Decal, FogCtrl_None, OPAQUE, FilterMode_Point, PERSP | TWOVOL | DECAL2 },
    { R565, 1, ShadInstr_Decal, FogCtrl_None, OPAQUE, FilterMode_Bilinear, PERSP | TWOVOL | DECAL2 },
    { R565, 1, ShadInstr_Modulate, FogCtrl_None, OPAQUE, FilterMode_Point, PERSP | TWOVOL },
};
#define CELLS 150

/* textured, packed colour, two volumes (vertex parameter type 11) */
typedef struct TA_Vertex11 {
    PCW pcw;
    f32b xyz[3];
    f32b u0, v0;
    uint32_t BaseCol0, OffsCol0;
    f32b u1, v1;
    uint32_t BaseCol1, OffsCol1;
    uint32_t ignore[4];
} TA_Vertex11;

static uint32_t tex[4];
static uint32_t fog_table[128];

static uint32_t hsh(uint32_t v) { v *= 2654435761u; v ^= v >> 15; v *= 2246822519u; v ^= v >> 13; v *= 3266489917u; v ^= v >> 16; return v; }

static uint32_t rnd_texel(const void *ctx, int size, int x, int y)
{
    const uint32_t s = *(const uint32_t *)ctx;
    return s == 0 ? 0xffff : hsh((uint32_t)(size * 4096 + y * 64 + x) ^ s) & 0xffff;
}

static void test_setup(void)
{
    static const uint32_t seed[4] = { 0, 0, 0x2b2b2b2u, 0x6c6c6c6u };     /* (no texture), white, 4444, 565 */
    const tex_layout_t l = { TEX_TWIDDLED, 16, 64, 64, 0 };
    int k;
    for(k = 1; k < 4; k++)
        tex[k] = tex_build(&l, rnd_texel, &seed[k]);
    for(k = 0; k < 128; k++)
        fog_table[k] = hsh((uint32_t)k + 0xaa00u) & 0xffff;
}

typedef struct { int32_t pu, sux, suy, pv, svx, svy; uint32_t zm[4], base[2][4], offs[2][4]; } cell_t;

static void cell(int page, int c, cell_t *p)
{
    const page_t *g = &pages[page];
    const uint32_t s = (uint32_t)(c % CELLS) * 977u + (uint32_t)page * 100003u;
    int k, vol;
    for(k = 0; k < 4; k++) {
        const uint32_t h = hsh(s + 50 + (uint32_t)k);
        const int32_t m = (int32_t)((1u << 14) + (h & ((1u << 17) - (1u << 14))));     /* 1/4 .. 2 texels */
        const int32_t v = (h >> 31) ? -m : m;
        if(k == 0) p->sux = v; else if(k == 1) p->svy = v; else if(k == 2) p->suy = v / 4; else p->svx = v / 4;
    }
    p->pu = (int32_t)(hsh(s + 1) & 0x3fffff);
    p->pv = (int32_t)(hsh(s + 2) & 0x3fffff);
    for(k = 0; k < 4; k++) {
        /* the corner whose colour this one takes: steep along x, the left or right; along y, the top or bottom */
        const int src = g->flags & STEEPX ? (k & 1) : g->flags & STEEPY ? (k & 2) : k;
        p->zm[k] = g->flags & PERSP ? (1u << 19) + hsh(s + 10 + (uint32_t)k) % ((1u << 23) - (1u << 19)) : 1u << 22;
        for(vol = 0; vol < 2; vol++) {
            p->base[vol][k] = hsh(s + 20 + (uint32_t)src + 100u * (uint32_t)vol) | (g->blend == PT ? 0x80000000u : 0);
            p->offs[vol][k] = hsh(s + 30 + (uint32_t)src + 100u * (uint32_t)vol);
        }
    }
}

static void test_describe(void)
{
    static const char *desc[NUM_PAGES] = {
        "no texture, Gouraud, steep along x, opaque", "no texture, Gouraud, steep along y, opaque",
        "no texture, Gouraud, perspective, over", "no texture, flat shaded, perspective, table fog, opaque",
        "no texture, Gouraud, perspective, table fog 2, over", "no texture, Gouraud, perspective, table fog, colour clamp, over",
        "white texture, modulate + Gouraud offset, steep along x", "white texture, modulate + Gouraud offset, vertex fog, perspective",
        "punch-through, 4444 bilinear, modulate alpha, Gouraud, perspective", "punch-through, no texture, Gouraud, perspective",
        "intensity shadow left 20 px, 565 bilinear, modulate + Gouraud offset, perspective",
        "two volumes (inside: left 20 px), 565 outside, 4444 inside, perspective",
        "two volumes (inside: left 20 px), 565 outside, 4444 inside, perspective",
        "two volumes (inside: left 20 px), point, decal", "two volumes (inside: left 20 px), bilinear, decal",
        "two volumes (inside: left 20 px), point, modulate + offset" };
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 300; c++) {
            if(pages[p].flags & BOTHVOL)
                label(p, c, "%s: SupSample %s", desc[p], c < CELLS ? "in both volumes" : "in neither");
            else if(pages[p].flags & TWOVOL)
                label(p, c, "%s: SupSample %s", desc[p], c < CELLS ? "outside only" : "inside only");
            else
                label(p, c, "%s%s", c < CELLS ? "" : "no SupSample: ", desc[p]);
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
        /* in front of every polygon: z 4 (polygons are at 1/8 .. 2) */
        v.x0 = f32b_int(x);       v.y0 = f32b_int(y);                  v.z0 = f32b_int(4);
        v.x1 = f32b_int(x + 20);  v.y1 = f32b_int(k ? y + 32 : y);     v.z1 = f32b_int(4);
        v.x2 = f32b_int(k ? x : x + 20);  v.y2 = f32b_int(y + 32);     v.z2 = f32b_int(4);
        ta_send(&v, sizeof(v));
    }
}

static void test_page(int page)
{
    const page_t *g = &pages[page];
    const int list = g->blend == OVER ? ListType_Translucent : g->blend == PT ? ListType_Punch_Through : ListType_Opaque;
    const int tx = g->tex != NOTEX;
    int c, k;
    pvr_frame.bg_color = g->blend == OPAQUE ? 0x000040 : 0x803020;
    pvrio_reg(FOG_COL_RAM_addr, 0xc0e57f19u);
    pvrio_reg(FOG_COL_VERT_addr, 0x0033cc66u);
    pvrio_reg(FOG_DENSITY_addr, 0x00008004u);
    pvrio_reg(FOG_CLAMP_MAX_addr, g->flags & CLAMP ? 0xc0b0a090u : 0xffffffffu);
    pvrio_reg(FOG_CLAMP_MIN_addr, g->flags & CLAMP ? 0x30405060u : 0);
    pvrio_reg(PT_ALPHA_REF_addr, 0x80);
    pvrio_reg(FPU_SHAD_SCALE_addr, g->flags & SHADOW ? 0x180 : 0x080);
    for(c = 0; c < 128; c++)
        pvrio_reg(FOG_TABLE_START_addr + 4 * (uint32_t)c, fog_table[c]);
    ta_list_begin(list);
    for(c = 0; c < 300; c++) {
        PCW pcw = { .full = 0 };
        ISP_TSP isp = { .full = 0 };
        TSP tsp = { .full = 0 };
        TCW tcw = { .full = 0 };
        const int x = cell_px(c), y = cell_py(c);
        cell_t q;
        cell(page, c, &q);
        pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
        pcw.ListType = (uint32_t)list;
        pcw.Group_En = 1;
        pcw.Strip_Len = 1;
        pcw.Texture = tx;
        pcw.Offset = tx;
        pcw.Gouraud = g->gouraud;
        pcw.Shadow = (g->flags & (SHADOW | TWOVOL)) != 0;
        pcw.Volume = (g->flags & TWOVOL) != 0;
        isp.DepthMode = DepthMode_Always;
        isp.CullMode = CullMode_None;
        isp.Texture = pcw.Texture;
        isp.Offset = pcw.Offset;
        isp.Gouraud = pcw.Gouraud;
        tsp.SrcInstr = g->blend == OPAQUE ? BlendInstr_One : BlendInstr_SrcAlpha;
        tsp.DstInstr = g->blend == OPAQUE ? BlendInstr_Zero : BlendInstr_InvSrcAlpha;
        tsp.FogCtrl = (uint32_t)g->fog;
        tsp.UseAlpha = 1;
        tsp.ColorClamp = (g->flags & CLAMP) != 0;
        tsp.ShadInstr = (uint32_t)g->shad;
        tsp.FilterMode = (uint32_t)g->filter;
        tsp.SupSample = c < CELLS;
        tsp.MipMapD = 4;
        tsp.TexU = 3;
        tsp.TexV = 3;
        tcw.PixelFmt = g->tex == R4444 ? PixelFmt_4444 : PixelFmt_565;
        tcw.TexAddr = tex[tx ? g->tex : WHITE] >> 3;
        if(g->flags & TWOVOL) {
            TA_PolyParam3 p;
            TSP tsp1 = tsp;
            TCW tcw1 = tcw;
            tsp.SupSample = c < CELLS;
            tsp1.SupSample = g->flags & BOTHVOL ? c < CELLS : c >= CELLS;
            tsp1.ShadInstr = g->flags & DECAL2 ? ShadInstr_Decal : ShadInstr_ModulateAlpha;
            tcw1.PixelFmt = PixelFmt_4444;
            tcw1.TexAddr = tex[R4444] >> 3;
            memset(&p, 0, sizeof(p));
            p.pcw = pcw; p.isp = isp; p.tsp = tsp; p.tcw = tcw; p.tsp1 = tsp1; p.tcw1 = tcw1;
            ta_send(&p, sizeof(p));
        } else
            ta_polygon(pcw, isp, tsp, tcw);
        for(k = 0; k < 4; k++) {
            const int X = (k & 1) * 32, Y = (k >> 1) * 32;
            const f32b vxx = f32b_int(x + X), vyy = f32b_int(y + Y), vz = f32b_scaled((int32_t)q.zm[k], -22);
            const f32b u = f32b_scaled(q.pu + X * q.sux + Y * q.suy, -22), v = f32b_scaled(q.pv + X * q.svx + Y * q.svy, -22);
            if(g->flags & TWOVOL) {
                TA_Vertex11 w;
                memset(&w, 0, sizeof(w));
                w.pcw = pcw_vertex(k == 3);
                w.xyz[0] = vxx; w.xyz[1] = vyy; w.xyz[2] = vz;
                w.u0 = u; w.v0 = v; w.BaseCol0 = q.base[0][k]; w.OffsCol0 = q.offs[0][k];
                w.u1 = v; w.v1 = u; w.BaseCol1 = q.base[1][k]; w.OffsCol1 = q.offs[1][k];
                ta_send(&w, sizeof(w));
            } else if(tx)
                ta_vertex3(vxx, vyy, vz, u, v, q.base[0][k], q.offs[0][k], k == 3);
            else
                ta_vertex0(vxx, vyy, vz, q.base[0][k], k == 3);
        }
    }
    ta_list_end();
    ta_list_begin(ListType_Opaque_Modifier_Volume);
    if(g->flags & (SHADOW | TWOVOL))
        for(c = 0; c < 300; c++)
            sheet(cell_px(c), cell_py(c));
    ta_list_end();
}
