/* The stored depth of every pixel of tile (0, 0), by a binary search with exact probes (2026-11-19, hunt depthgaps;
   like common/zexfil.h, but over negative depths as well, and through the tile accelerator).

   A render: the scene (sent by srch_scene, its polygons DepthMode Always, textured green, decal), then at every
   pixel still searched zexfil's probe triangle (common/zexfil_probes.h: exact for any depth of 19 bits) at its
   probe word, DepthMode Greater, textured white; tile (0, 0) read back: white where the probe is above the stored
   depth in the ISP's order. 28 renders, then one with DepthMode Equal at the probe found.
   The probes in the ISP's order (srch_word): the negative words of exponent fields 10 .. 245 whose fraction is a
   multiple of 0x100, or 0x7fffe0 (a negative depth is stored on that grid: pvr-tests/depthgaps_neggrid), each field
   from 0x7fffe0 up to 0, then 0, then the positive words of fields 27 .. 227 on the 19 bit grid (fraction a
   multiple of 0x20). (2^-117 .. 2^118: zexfil's probes are exact in refsw3's depth unit from field 3 to 248.)
   The renders depend on what is read back: not hashed. srch_lo[p]: the number of probes at or below the depth;
   srch_eq[p]: whether the depth equals the last of them. */
#ifndef DEPTHGAPS_SEARCH_H
#define DEPTHGAPS_SEARCH_H
#include "zexfil_probes.h"

#define SRCH_F0 10u
#define SRCH_NF 236u                                   /* fields 10 .. 245 */
#define SRCH_NNEG (SRCH_NF * 0x8001u)
#define SRCH_N (SRCH_NNEG + 1u + SRCH_NF * 0x40000u)

static uint32_t srch_lo[1024], srch_hi[1024], srch_eq[1024];
static uint32_t srch_texG, srch_texW;

static uint32_t srch_word(uint32_t i)
{
    if(i < SRCH_NNEG) {
        const uint32_t f = SRCH_F0 + i / 0x8001u, g = i % 0x8001u;
        return 0x80000000u | f << 23 | (g ? 0x800000u - 0x100u * g : 0x7fffe0u);
    }
    i -= SRCH_NNEG;
    if(!i)
        return 0;
    i--;
    return (SRCH_F0 + (i >> 18)) << 23 | (i & 0x3ffffu) << 5;
}

static uint32_t srch_green(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0x07e0; }
static uint32_t srch_white(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0xffff; }

static void srch_setup(void)
{
    const tex_layout_t l = { TEX_TWIDDLED, 16, 8, 8, 0 };
    srch_texG = tex_build(&l, srch_green, NULL);
    srch_texW = tex_build(&l, srch_white, NULL);
}

static void srch_begin(int mode, uint32_t tex)
{
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Opaque;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Texture = 1;
    isp.DepthMode = mode;
    isp.CullMode = CullMode_None;
    isp.Texture = 1;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    tsp.ShadInstr = ShadInstr_Decal;
    tsp.FilterMode = FilterMode_Point;
    tsp.MipMapD = 4;
    tcw.PixelFmt = PixelFmt_565;
    tcw.TexAddr = tex >> 3;
    ta_polygon(pcw, isp, tsp, tcw);
}

static void srch_vtx(uint32_t x, uint32_t y, uint32_t z, int end)
{
    ta_vertex3(x, y, z, 0x3e800000u, 0x3e800000u, 0xffffffffu, 0, end);
}

/* one render: the scene, then the probes (equal: DepthMode Equal at srch_lo - 1, else Greater at the middle) */
static void srch_render(void (*scene)(void), uint32_t bg, int equal)
{
    static uint32_t rb[32 * 640];
    int i, k;
    pvr_frame.bg_color = 0x000040;
    pvr_frame.bg_depth = bg;
    pvr_frame_begin();
    ta_list_begin(ListType_Opaque);
    scene();
    for(i = 0; i < 1024; i++) {
        const int q = zexfil_probe[i].pixel;
        uint32_t idx;
        if(equal ? srch_lo[q] == 0 : srch_lo[q] >= srch_hi[q])
            continue;
        idx = equal ? srch_lo[q] - 1 : srch_lo[q] + (srch_hi[q] - srch_lo[q]) / 2;
        srch_begin(equal ? DepthMode_Equal : DepthMode_Greater, srch_texW);
        for(k = 0; k < 3; k++)
            srch_vtx(zexfil_probe[i].x[k], zexfil_probe[i].y[k], srch_word(idx), k == 2);
    }
    ta_list_end();
    pvr_frame_render();
    pvrio_read32(SETUP_FB, 31 * 640 + 32, rb);
    for(i = 0; i < 1024; i++) {
        const int white = (rb[(i / 32) * 640 + i % 32] & 0xffffff) == 0xffffff;
        if(equal) {
            srch_eq[i] = srch_lo[i] && white;
            continue;
        }
        if(srch_lo[i] >= srch_hi[i])
            continue;
        /* probe idx above the depth (white): at most idx probes at or below it; else at least idx + 1 */
        if(white)
            srch_hi[i] = srch_lo[i] + (srch_hi[i] - srch_lo[i]) / 2;
        else
            srch_lo[i] = srch_lo[i] + (srch_hi[i] - srch_lo[i]) / 2 + 1;
    }
}

/* the search for a scene */
static void srch_run(void (*scene)(void), uint32_t bg)
{
    int i, step;
    for(i = 0; i < 1024; i++) {
        srch_lo[i] = 0;
        srch_hi[i] = SRCH_N;
        srch_eq[i] = 0;
    }
    pvrio_unhashed = 1;
    for(step = 0; step < 28; step++)
        srch_render(scene, bg, 0);
    srch_render(scene, bg, 1);
    pvrio_unhashed = 0;
}

/* the result of pixel p as a word: the last probe at or below the depth (srch_word(srch_lo - 1); 0xfffffffe: below
   every probe), with bit 0 set where the depth equals it (probe words have bit 0 clear) */
static uint32_t srch_result(int p)
{
    return srch_lo[p] == 0 ? 0xfffffffeu : (srch_word(srch_lo[p] - 1) | srch_eq[p]);
}

/* the results of a search kept (srch_keep) and written into the framebuffer at the end, cell k of a run at cell
   base + k (not hashed) */
#define SRCH_MAXKEEP 16
static uint32_t srch_kept[SRCH_MAXKEEP][1024];
static void srch_keep(int k)
{
    int p;
    for(p = 0; p < 1024; p++)
        srch_kept[k][p] = srch_result(p);
}
static void srch_store(int n, int base)
{
    int k, r;
    pvrio_unhashed = 1;
    for(k = 0; k < n; k++)
        for(r = 0; r < 32; r++)
            pvrio_w32(SETUP_FB + ((uint32_t)(cell_py(base + k) + r) * 640 + (uint32_t)cell_px(base + k)) * 4, &srch_kept[k][r * 32], 32);
    pvrio_unhashed = 0;
}
#endif
