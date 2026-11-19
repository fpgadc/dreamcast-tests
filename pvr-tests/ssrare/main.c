/* What makes sscolor's 2 pixels (2026-11-19): bilinear inside a modifier volume, SupSample in the inside volume only,
   where the console's u at the point half a line down (hx 0, hy 1) is a little larger than refsw3's (one weight
   step, both pixels). pvr-tests/ssvol, as sensitive in u and with the same volumes, has none; this test measures
   how often it happens in sscolor's own setting and which of sscolor's differences from ssvol it needs.

   Every cell is sscolor page 14's kind of cell (two volumes, a modifier volume over the left 20 pixels, SupSample
   inside only, perspective, decal, bilinear, 64x64 textures; here each channel of each texel 0 or all ones, so that
   every weight step shows) unless the page says otherwise; 4 pages of each, all 300 cells alike:
     pages  0 ..  3  as sscolor page 14: Gouraud base and offset colours, the inside volume's u and v the outside's
                     v and u (u changes mostly down the cell)
     pages  4 ..  7  the modifier volume over the whole cell
     pages  8 .. 11  white, no offset colour
     pages 12 .. 15  the inside volume's u and v not swapped (u changes mostly along x)
     pages 16 .. 19  one volume (no modifier volume), SupSample, the coordinates as the inside's of pages 0 .. 3 */
#define NUM_PAGES 20
#define TEST_MODIFIERS
#include "pvrtest2.h"
#include "texgen.h"

enum { FULLSHEET = 1, WHITE = 2, NOSWAP = 4, ONEVOL = 8 };
static int pg_flags(int page) { static const int f[5] = { 0, FULLSHEET, WHITE, NOSWAP, ONEVOL }; return f[page / 4]; }

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

static uint32_t bw_texel(const void *ctx, int size, int x, int y)
{
    const uint32_t h = hsh((uint32_t)(y * 64 + x) ^ *(const uint32_t *)ctx);
    const uint32_t s = *(const uint32_t *)ctx;
    (void)size;
    if(s & 1)                           /* 4444 */
        return (h & 1 ? 0xf000u : 0) | (h & 2 ? 0x0f00u : 0) | (h & 4 ? 0x00f0u : 0) | (h & 8 ? 0x000fu : 0);
    return (h & 1 ? 0xf800u : 0) | (h & 2 ? 0x07e0u : 0) | (h & 4 ? 0x001fu : 0);
}

static void test_setup(void)
{
    static const uint32_t seed[2] = { 0x3579bdeu, 0x4681ac1u };       /* 565 outside, 4444 inside */
    const tex_layout_t l = { TEX_TWIDDLED, 16, 64, 64, 0 };
    int k;
    for(k = 0; k < 2; k++)
        tex[k] = tex_build(&l, bw_texel, &seed[k]);
}

/* as sscolor's cells: 1/4 .. 2 texels a pixel along one axis, a quarter of that along the other */
typedef struct { int32_t pu, sux, suy, pv, svx, svy; uint32_t zm[4], base[2][4], offs[2][4]; } cell_t;

static void cell(int page, int c, cell_t *p)
{
    const uint32_t s = (uint32_t)c * 977u + (uint32_t)page * 100003u;
    int k, vol;
    for(k = 0; k < 4; k++) {
        const uint32_t h = hsh(s + 50 + (uint32_t)k);
        const int32_t m = (int32_t)((1u << 14) + (h & ((1u << 17) - (1u << 14))));
        const int32_t v = (h >> 31) ? -m : m;
        if(k == 0) p->sux = v; else if(k == 1) p->svy = v; else if(k == 2) p->suy = v / 4; else p->svx = v / 4;
    }
    p->pu = (int32_t)(hsh(s + 1) & 0x3fffff);
    p->pv = (int32_t)(hsh(s + 2) & 0x3fffff);
    for(k = 0; k < 4; k++) {
        p->zm[k] = (1u << 19) + hsh(s + 10 + (uint32_t)k) % ((1u << 23) - (1u << 19));
        for(vol = 0; vol < 2; vol++) {
            p->base[vol][k] = pg_flags(page) & WHITE ? 0xffffffffu : hsh(s + 20 + (uint32_t)k + 100u * (uint32_t)vol);
            p->offs[vol][k] = pg_flags(page) & WHITE ? 0 : hsh(s + 30 + (uint32_t)k + 100u * (uint32_t)vol);
        }
    }
}

static void test_describe(void)
{
    static const char *d[5] = { "as sscolor page 14", "modifier volume over the whole cell", "white, no offset",
                                "inside u, v not swapped", "one volume, SupSample, coordinates swapped" };
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 300; c++)
            label(p, c, "%s", d[p / 4]);
}

static void sheet(int x, int y, int w)
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
        v.x1 = f32b_int(x + w);   v.y1 = f32b_int(k ? y + 32 : y);     v.z1 = f32b_int(4);
        v.x2 = f32b_int(k ? x : x + w);   v.y2 = f32b_int(y + 32);     v.z2 = f32b_int(4);
        ta_send(&v, sizeof(v));
    }
}

static void test_page(int page)
{
    const int fl = pg_flags(page);
    int c, k;
    pvr_frame.bg_color = 0x000040;
    ta_list_begin(ListType_Opaque);
    for(c = 0; c < 300; c++) {
        PCW pcw = { .full = 0 };
        ISP_TSP isp = { .full = 0 };
        TSP tsp = { .full = 0 };
        TCW tcw = { .full = 0 };
        const int x = cell_px(c), y = cell_py(c);
        cell_t q;
        cell(page, c, &q);
        pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
        pcw.ListType = ListType_Opaque;
        pcw.Group_En = 1;
        pcw.Strip_Len = 1;
        pcw.Texture = 1;
        pcw.Offset = 1;
        pcw.Gouraud = 1;
        pcw.Shadow = !(fl & ONEVOL);
        pcw.Volume = !(fl & ONEVOL);
        isp.DepthMode = DepthMode_Always;
        isp.CullMode = CullMode_None;
        isp.Texture = 1;
        isp.Offset = 1;
        isp.Gouraud = 1;
        tsp.SrcInstr = BlendInstr_One;
        tsp.DstInstr = BlendInstr_Zero;
        tsp.FogCtrl = FogCtrl_None;
        tsp.UseAlpha = 1;
        tsp.ShadInstr = ShadInstr_Decal;
        tsp.FilterMode = FilterMode_Bilinear;
        tsp.SupSample = (fl & ONEVOL) != 0;
        tsp.MipMapD = 4;
        tsp.TexU = 3;
        tsp.TexV = 3;
        tcw.PixelFmt = PixelFmt_565;
        tcw.TexAddr = tex[0] >> 3;
        if(!(fl & ONEVOL)) {
            TA_PolyParam3 p;
            TSP tsp1 = tsp;
            TCW tcw1 = tcw;
            tsp1.SupSample = 1;
            tcw1.PixelFmt = PixelFmt_4444;
            tcw1.TexAddr = tex[1] >> 3;
            memset(&p, 0, sizeof(p));
            p.pcw = pcw; p.isp = isp; p.tsp = tsp; p.tcw = tcw; p.tsp1 = tsp1; p.tcw1 = tcw1;
            ta_send(&p, sizeof(p));
        } else
            ta_polygon(pcw, isp, tsp, tcw);
        for(k = 0; k < 4; k++) {
            const int X = (k & 1) * 32, Y = (k >> 1) * 32;
            const f32b vxx = f32b_int(x + X), vyy = f32b_int(y + Y), vz = f32b_scaled((int32_t)q.zm[k], -22);
            const f32b u = f32b_scaled(q.pu + X * q.sux + Y * q.suy, -22), v = f32b_scaled(q.pv + X * q.svx + Y * q.svy, -22);
            if(!(fl & ONEVOL)) {
                TA_Vertex11 w;
                memset(&w, 0, sizeof(w));
                w.pcw = pcw_vertex(k == 3);
                w.xyz[0] = vxx; w.xyz[1] = vyy; w.xyz[2] = vz;
                w.u0 = u; w.v0 = v; w.BaseCol0 = q.base[0][k]; w.OffsCol0 = q.offs[0][k];
                w.u1 = fl & NOSWAP ? u : v; w.v1 = fl & NOSWAP ? v : u; w.BaseCol1 = q.base[1][k]; w.OffsCol1 = q.offs[1][k];
                ta_send(&w, sizeof(w));
            } else                      /* one volume: the swapped coordinates, the inside's colours */
                ta_vertex3(vxx, vyy, vz, v, u, q.base[1][k], q.offs[1][k], k == 3);
        }
    }
    ta_list_end();
    ta_list_begin(ListType_Opaque_Modifier_Volume);
    if(!(fl & ONEVOL))
        for(c = 0; c < 300; c++)
            sheet(cell_px(c), cell_py(c), fl & FULLSHEET ? 32 : 20);
    ta_list_end();
}
