/* Where a filtered sample falls in a mip level smaller than the full size (2026-11-17).

   The console makes a texture coordinate at the full size, takes the bilinear half texel off there, and only
   then shifts it down to the level: at level L the footprint sits 1/2 - 1/2^(L+1) texel further on than a
   half texel of the level's own size. That was found replaying three Sonic Adventure frames (dreamster
   polly2-data, sa_slow1-3) and no other test saw it: pvr-tests/mip has one flat colour per level below the
   full size, so a sample there looks the same wherever it falls.

   Here every texel of every level differs from its neighbours (red steps by 11 of 31 along x, green by 23 of
   63 along y, blue a checker, all shifted by the level), so the bilinear weights show in every pixel.

   Seven textures, two rows of cells each (variants[]): 565 mipmapped 64x64, the same VQ, the 64x64 one
   declared 64x16 and 64x32, a 16x16 one declared 16x64, 256x256 and 8x8. A cell is a 32x32 quad:
     cells 0-27   1.375 x 2^L texels of the full size a pixel along u and v (the middle of level L), L = 0 .. 6,
                  each at four starting offsets (5/32, 15/32, 25/32, 35/32 texel of the full size along u,
                  7/32 .. 37/32 along v)
     cells 28-31  more texels a pixel along u than along v (u at 1.375 x 2^L, v a quarter of that), L = 1 .. 4
     cells 32-35  the same the other way round
     cells 36-39  perspective: 1/w 1 at the top, 1/4 at the bottom, 1.375 x 2^L texels a pixel at the top,
                  L = 0 .. 3 (the level grows down the cell)
   Pages: 0 bilinear, 1 trilinear pass A, 2 trilinear pass B, 3 point sampling (where the half texel is not
   taken: a control). Translucent list, replace (ONE, ZERO), decal: the picture is the sample. */
#define NUM_PAGES 4
#include "pvrtest2.h"
#include "texgen.h"

typedef struct { const char *name; int VQ, size, TexU, TexV; } variant_t;      /* size: the texture as stored */
#define NUM_VARIANTS 7
static const variant_t variants[NUM_VARIANTS] = {
    { "565 64x64",                 0,  64, 3, 3 },
    { "565 VQ 64x64",              1,  64, 3, 3 },
    { "565 64x64 declared 64x16",  0,  64, 3, 1 },
    { "565 16x16 declared 16x64",  0,  16, 1, 3 },
    { "565 64x64 declared 64x32",  0,  64, 3, 2 },
    { "565 256x256",               0, 256, 5, 5 },
    { "565 8x8",                   0,   8, 0, 0 },
};
static uint32_t txr[NUM_VARIANTS];

static int log2i(int v) { int k = 0; while((1 << k) < v) k++; return k; }

/* period 8, so that a VQ texture needs few code book entries (16 a level) */
static uint32_t texel(const void *ctx, int size, int x, int y) {
    const int lv = log2i(size) + 1;          /* + 1: no level is black, the 1x1 one included */
    const uint32_t r = (uint32_t)(((x & 7) * 11 + lv * 5) & 31);
    const uint32_t g = (uint32_t)(((y & 7) * 23 + lv * 9) & 63);
    const uint32_t b = (uint32_t)((((x ^ y) & 1) ? lv * 3 + 20 : lv * 7) & 31);
    (void)ctx;
    return (r << 11) | (g << 5) | b;
}

static void test_setup(void) {
    int k;
    for(k = 0; k < NUM_VARIANTS; k++) {
        int j;
        for(j = 0; j < k; j++)                  /* the declared-size variants share the stored texture */
            if(variants[j].VQ == variants[k].VQ && variants[j].size == variants[k].size)
                break;
        if(j < k) {
            txr[k] = txr[j];
        } else {
            const tex_layout_t l = { variants[k].VQ ? TEX_VQ_MIP : TEX_MIP, 16, variants[k].size, variants[k].size, 0 };
            txr[k] = tex_build(&l, texel, NULL);
        }
    }
}

static const char *filter_name[NUM_PAGES] = { "bilinear", "trilinear A", "trilinear B", "point" };
static const int filter_mode[NUM_PAGES] = { FilterMode_Bilinear, FilterMode_TrilinearA, FilterMode_TrilinearB, FilterMode_Point };

static void test_describe(void) {
    int p, k, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(k = 0; k < NUM_VARIANTS; k++)
            for(c = 0; c < 40; c++) {
                const int cell = k * 40 + c;
                const char *n = variants[k].name, *f = filter_name[p];
                if(c < 28)
                    label(p, cell, "%s, %s: 1.375 x 2^%d texels a pixel, offset %d", n, f, c / 4, c % 4);
                else if(c < 32)
                    label(p, cell, "%s, %s: u 1.375 x 2^%d texels a pixel, v a quarter", n, f, c - 27);
                else if(c < 36)
                    label(p, cell, "%s, %s: v 1.375 x 2^%d texels a pixel, u a quarter", n, f, c - 31);
                else
                    label(p, cell, "%s, %s: perspective, 1/w 1 to 1/4, 1.375 x 2^%d texels a pixel at the top", n, f, c - 36);
            }
}

static void textured(const variant_t *v, uint32_t at, int filter) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };

    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Translucent;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Texture = 1;
    pcw.Gouraud = 1;
    isp.DepthMode = DepthMode_Always;
    isp.ZWriteDis = 1;
    isp.Texture = 1;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    tsp.ShadInstr = ShadInstr_Decal;
    tsp.FilterMode = filter;
    tsp.MipMapD = 4;
    tsp.TexU = v->TexU;
    tsp.TexV = v->TexV;
    tcw.PixelFmt = PixelFmt_565;
    tcw.VQ_Comp = v->VQ;
    tcw.MipMapped = 1;
    tcw.TexAddr = at >> 3;
    ta_polygon(pcw, isp, tsp, tcw);
}

/* a 32x32 quad; 1/w zt along the top, zb along the bottom */
static void quad_p(int x, int y, f32b zt, f32b zb, f32b u0, f32b v0, f32b u1, f32b v1) {
    int k;
    for(k = 0; k < 4; k++)
        ta_vertex3(f32b_int(x + (k & 1) * 32), f32b_int(y + (k >> 1) * 32), (k >> 1) ? zb : zt, (k & 1) ? u1 : u0,
                   (k >> 1) ? v1 : v0, 0xffffffff, 0, k == 3);
}

/* p / q texels of a declared size of 8 << tex, as a coordinate (p / (q * size)): exact */
static f32b coord(int32_t p, int32_t q, int tex) { return sf_ratio(p, q * (8 << tex)); }

static void test_page(int page) {
    int k, c;
    ta_list_begin(ListType_Translucent);
    for(k = 0; k < NUM_VARIANTS; k++) {
        const variant_t *v = &variants[k];
        textured(v, txr[k], filter_mode[page]);
        for(c = 0; c < 40; c++) {
            const int cell = k * 40 + c, x = cell_px(cell), y = cell_py(cell);
            /* 32 pixels of 1.375 x 2^L texels of the full size: 44 x 2^L texels; offsets in 32nds of a texel */
            int32_t su, sv;
            int ou = 5, ov = 7, persp = 0;
            if(c < 28) {
                su = sv = 44 << (c / 4);
                ou = 5 + 10 * (c % 4);
                ov = 7 + 10 * (c % 4);
            } else if(c < 32) {
                su = 44 << (c - 27); sv = 11 << (c - 27);
            } else if(c < 36) {
                sv = 44 << (c - 31); su = 11 << (c - 31);
            } else {
                su = sv = 44 << (c - 36); persp = 1;
            }
            {
                const f32b u0 = coord(ou, 32, v->TexU), v0 = coord(ov, 32, v->TexV);
                const f32b u1 = sf_add(u0, coord(su, 1, v->TexU)), v1 = sf_add(v0, coord(sv, 1, v->TexV));
                quad_p(x, y, F32B_ONE, persp ? f32b_scaled(1, -2) : F32B_ONE, u0, v0, u1, v1);
            }
        }
    }
    ta_list_end();
}
