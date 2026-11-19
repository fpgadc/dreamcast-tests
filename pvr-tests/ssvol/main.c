/* SupSample's coordinates at high resolution, and inside a modifier volume (2026-11-19). sscolor left 2 pixels: bilinear
   inside a modifier volume with SupSample in the inside volume only, each a point whose u is within 1/50 of a
   coordinate unit of a bilinear weight step. This test makes such points common, to see where the console and
   refsw3 part and how often.

   The textures are 1024x8 (565, every channel of every texel 0 or all ones, at random): along u a bilinear weight
   step is 1/32768 of the coordinate, and every step shows in some channel. Coordinates u = (P + X Sx + Y Sy) / 2^22
   at the corners X, Y of the cell, 1/4 .. 2 texels a pixel along x (and up to a quarter of that along y); v with
   1/4 .. 1 texel a pixel along y. Perspective: every corner its own z, 1/8 .. 2. Bilinear, decal, opaque.
   Cells 0 .. 149 and 150 .. 299 as each page says (most: with SupSample, then the same without).
     page 0  one volume: SupSample / without
     page 1  two volumes, outside the modifier volume (none drawn): SupSample outside / without
     page 2  two volumes, inside (a modifier volume over the whole cell): SupSample inside only / without
     page 3  inside: SupSample in both volumes / in neither
     page 4  inside: SupSample outside only / inside only
     page 5  inside, flat z (z = 1): SupSample inside only / without
     page 6  one volume inside an intensity shadow (FPU_SHAD_SCALE 0x180) over the whole cell: SupSample / without
     page 7  inside, the inside volume with the outside's coordinates and texture: SupSample inside only / without
     page 8  one volume, flat z: SupSample / without
   The same with u = 0 in the middle of every cell (P chosen so; the coordinate crosses zero, where the texture
   wraps, along a line through the cell), and pages 15 and 16 with v crossing zero instead:
     page  9  one volume: SupSample / without           page 10  one volume, flat z
     page 11  two volumes, outside: SupSample outside / without
     page 12  two volumes, inside: SupSample inside only / without
     page 13  inside: SupSample in both volumes / in neither
     page 14  inside, flat z: SupSample inside only / without
     page 15  one volume, v crossing zero               page 16  two volumes, inside, v crossing zero: inside only */
#define NUM_PAGES 17
#define TEST_MODIFIERS
#include "pvrtest2.h"
#include "texgen.h"

enum { ONEVOL = 1, TWOVOL = 2, SHEET = 4, SHADOW = 8, FLATZ = 16, SAMEUV = 32, UZERO = 64, VZERO = 128 };
typedef struct { int flags; int s0[2], s1[2]; } page_t;      /* SupSample outside / inside, first half, second half */
static const page_t pages[NUM_PAGES] = {
    { ONEVOL,                    { 1, 0 }, { 0, 0 } },
    { TWOVOL,                    { 1, 0 }, { 0, 0 } },
    { TWOVOL | SHEET,            { 0, 0 }, { 1, 0 } },
    { TWOVOL | SHEET,            { 1, 0 }, { 1, 0 } },
    { TWOVOL | SHEET,            { 1, 0 }, { 0, 1 } },
    { TWOVOL | SHEET | FLATZ,    { 0, 0 }, { 1, 0 } },
    { ONEVOL | SHEET | SHADOW,   { 1, 0 }, { 0, 0 } },
    { TWOVOL | SHEET | SAMEUV,   { 0, 0 }, { 1, 0 } },
    { ONEVOL | FLATZ,            { 1, 0 }, { 0, 0 } },
    { ONEVOL | UZERO,            { 1, 0 }, { 0, 0 } },
    { ONEVOL | FLATZ | UZERO,    { 1, 0 }, { 0, 0 } },
    { TWOVOL | UZERO,            { 1, 0 }, { 0, 0 } },
    { TWOVOL | SHEET | UZERO,    { 0, 0 }, { 1, 0 } },
    { TWOVOL | SHEET | UZERO,    { 1, 0 }, { 1, 0 } },
    { TWOVOL | SHEET | FLATZ | UZERO, { 0, 0 }, { 1, 0 } },
    { ONEVOL | VZERO,            { 1, 0 }, { 0, 0 } },
    { TWOVOL | SHEET | VZERO,    { 0, 0 }, { 1, 0 } },
};
#define CELLS 150

typedef struct TA_Vertex11 {            /* textured, packed colour, two volumes (vertex parameter type 11) */
    PCW pcw;
    f32b xyz[3];
    f32b u0, v0;
    uint32_t BaseCol0, OffsCol0;
    f32b u1, v1;
    uint32_t BaseCol1, OffsCol1;
    uint32_t ignore[4];
} TA_Vertex11;

static uint32_t tex[2];

static uint32_t hsh(uint32_t v) { v *= 2654435761u; v ^= v >> 15; v *= 2246822519u; v ^= v >> 13; v *= 3266489917u; v ^= v >> 16; return v; }

/* 565, each channel 0 or all ones */
static uint32_t bw_texel(const void *ctx, int size, int x, int y)
{
    const uint32_t h = hsh((uint32_t)(y * 1024 + x) ^ *(const uint32_t *)ctx);
    (void)size;
    return (h & 1 ? 0xf800u : 0) | (h & 2 ? 0x07e0u : 0) | (h & 4 ? 0x001fu : 0);
}

static void test_setup(void)
{
    static const uint32_t seed[2] = { 0x1357913u, 0x2468024u };
    const tex_layout_t l = { TEX_TWIDDLED, 16, 1024, 8, 0 };
    int k;
    for(k = 0; k < 2; k++)
        tex[k] = tex_build(&l, bw_texel, &seed[k]);
}

typedef struct { int32_t pu[2], sux[2], suy[2], pv[2], svx[2], svy[2]; uint32_t zm[4]; } cell_t;

static void cell(int page, int c, cell_t *p)
{
    const page_t *g = &pages[page];
    const uint32_t s = (uint32_t)(c % CELLS) * 977u + (uint32_t)page * 100003u;
    int k, vol;
    for(vol = 0; vol < 2; vol++) {
        const uint32_t b = s + 100u * (uint32_t)(g->flags & SAMEUV ? 0 : vol);
        const uint32_t h = hsh(b + 4), h2 = hsh(b + 5);
        /* along x: 1/4 .. 2 texels of 1024: 2^10 .. 2^13 units of 2^-22 */
        const int32_t m = (int32_t)((1u << 10) + (h & ((1u << 13) - (1u << 10))));
        /* v along y: 1/4 .. 1 texel of 8: 2^17 .. 2^19 */
        const int32_t mv = (int32_t)((1u << 17) + (h2 & ((1u << 19) - (1u << 17))));
        p->sux[vol] = (h >> 31) ? -m : m;
        p->suy[vol] = (int32_t)(hsh(b + 6) % (uint32_t)(m / 2 + 1)) - m / 4;
        p->svy[vol] = (h2 >> 31) ? -mv : mv;
        p->svx[vol] = (int32_t)(hsh(b + 7) % (uint32_t)(mv / 2 + 1)) - mv / 4;
        p->pu[vol] = (int32_t)(hsh(b + 1) & 0x3fffff);
        p->pv[vol] = (int32_t)(hsh(b + 2) & 0x3fffff);
        /* zero in the middle of the cell, give or take 4 pixels */
        if(g->flags & UZERO)
            p->pu[vol] = -(16 * p->sux[vol] + 16 * p->suy[vol]) + (int32_t)(hsh(b + 8) % (uint32_t)(8 * (m < 0 ? -m : m) + 1)) - 4 * (m < 0 ? -m : m);
        if(g->flags & VZERO)
            p->pv[vol] = -(16 * p->svx[vol] + 16 * p->svy[vol]) + (int32_t)(hsh(b + 9) % (uint32_t)(8 * mv + 1)) - 4 * mv;
    }
    for(k = 0; k < 4; k++)
        p->zm[k] = g->flags & FLATZ ? 1u << 22 : (1u << 19) + hsh(s + 10 + (uint32_t)k) % ((1u << 23) - (1u << 19));
}

static void test_describe(void)
{
    static const char *desc[NUM_PAGES] = { "one volume", "two volumes, outside", "two volumes, inside", "two volumes, inside",
        "two volumes, inside", "two volumes, inside, flat z", "one volume, intensity shadow", "two volumes, inside, same uv inside",
        "one volume, flat z", "u crossing 0, one volume", "u crossing 0, one volume, flat z", "u crossing 0, two volumes, outside",
        "u crossing 0, two volumes, inside", "u crossing 0, two volumes, inside", "u crossing 0, two volumes, inside, flat z",
        "v crossing 0, one volume", "v crossing 0, two volumes, inside" };
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 300; c++) {
            const page_t *g = &pages[p];
            const int h = c >= CELLS;
            if(g->flags & TWOVOL)
                label(p, c, "%s: SupSample outside %d inside %d", desc[p], g->s0[h], g->s1[h]);
            else
                label(p, c, "%s: SupSample %d", desc[p], g->s0[h]);
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
        v.x0 = f32b_int(x);       v.y0 = f32b_int(y);                  v.z0 = f32b_int(4);
        v.x1 = f32b_int(x + 32);  v.y1 = f32b_int(k ? y + 32 : y);     v.z1 = f32b_int(4);
        v.x2 = f32b_int(k ? x : x + 32);  v.y2 = f32b_int(y + 32);     v.z2 = f32b_int(4);
        ta_send(&v, sizeof(v));
    }
}

static void test_page(int page)
{
    const page_t *g = &pages[page];
    int c, k;
    pvr_frame.bg_color = 0x000040;
    pvrio_reg(FPU_SHAD_SCALE_addr, g->flags & SHADOW ? 0x180 : 0x080);
    ta_list_begin(ListType_Opaque);
    for(c = 0; c < 300; c++) {
        PCW pcw = { .full = 0 };
        ISP_TSP isp = { .full = 0 };
        TSP tsp = { .full = 0 };
        TCW tcw = { .full = 0 };
        const int x = cell_px(c), y = cell_py(c), h = c >= CELLS;
        cell_t q;
        cell(page, c, &q);
        pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
        pcw.ListType = ListType_Opaque;
        pcw.Group_En = 1;
        pcw.Strip_Len = 1;
        pcw.Texture = 1;
        pcw.Shadow = (g->flags & (SHADOW | TWOVOL)) != 0;
        pcw.Volume = (g->flags & TWOVOL) != 0;
        isp.DepthMode = DepthMode_Always;
        isp.CullMode = CullMode_None;
        isp.Texture = 1;
        tsp.SrcInstr = BlendInstr_One;
        tsp.DstInstr = BlendInstr_Zero;
        tsp.FogCtrl = FogCtrl_None;
        tsp.UseAlpha = 1;
        tsp.ShadInstr = ShadInstr_Decal;
        tsp.FilterMode = FilterMode_Bilinear;
        tsp.SupSample = (uint32_t)g->s0[h];
        tsp.MipMapD = 4;
        tsp.TexU = 7;                   /* 1024 */
        tsp.TexV = 0;                   /* 8 */
        tcw.PixelFmt = PixelFmt_565;
        tcw.TexAddr = tex[0] >> 3;
        if(g->flags & TWOVOL) {
            TA_PolyParam3 p;
            TSP tsp1 = tsp;
            TCW tcw1 = tcw;
            tsp1.SupSample = (uint32_t)g->s1[h];
            tcw1.TexAddr = tex[g->flags & SAMEUV ? 0 : 1] >> 3;
            memset(&p, 0, sizeof(p));
            p.pcw = pcw; p.isp = isp; p.tsp = tsp; p.tcw = tcw; p.tsp1 = tsp1; p.tcw1 = tcw1;
            ta_send(&p, sizeof(p));
        } else
            ta_polygon(pcw, isp, tsp, tcw);
        for(k = 0; k < 4; k++) {
            const int X = (k & 1) * 32, Y = (k >> 1) * 32;
            const f32b vxx = f32b_int(x + X), vyy = f32b_int(y + Y), vz = f32b_scaled((int32_t)q.zm[k], -22);
            const f32b u0 = f32b_scaled(q.pu[0] + X * q.sux[0] + Y * q.suy[0], -22), v0 = f32b_scaled(q.pv[0] + X * q.svx[0] + Y * q.svy[0], -22);
            const f32b u1 = f32b_scaled(q.pu[1] + X * q.sux[1] + Y * q.suy[1], -22), v1 = f32b_scaled(q.pv[1] + X * q.svx[1] + Y * q.svy[1], -22);
            if(g->flags & TWOVOL) {
                TA_Vertex11 w;
                memset(&w, 0, sizeof(w));
                w.pcw = pcw_vertex(k == 3);
                w.xyz[0] = vxx; w.xyz[1] = vyy; w.xyz[2] = vz;
                w.u0 = u0; w.v0 = v0; w.BaseCol0 = 0xffffffffu;
                w.u1 = u1; w.v1 = v1; w.BaseCol1 = 0xffffffffu;
                ta_send(&w, sizeof(w));
            } else
                ta_vertex3(vxx, vyy, vz, u0, v0, 0xffffffffu, 0, k == 3);
        }
    }
    ta_list_end();
    ta_list_begin(ListType_Opaque_Modifier_Volume);
    if(g->flags & SHEET)
        for(c = 0; c < 300; c++)
            sheet(cell_px(c), cell_py(c));
    ta_list_end();
}
