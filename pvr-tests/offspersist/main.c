/* The core's persistent offset colour (TODO C13, found by purist 2026-11-19): a bump map drawn WITHOUT the offset bit
   lights itself with the offset colour (K1 K2 K3 Q) the core used last, and the value survives the power going off.
   Which value, kept how long, and who else reads it.

   Every cell (one tile; tiles are rendered column by column, region order 0) has a setter S and a reader R:
     S  a 32x32 quad, a white 565 texture modulated, Gouraud, with the offset bit and four offset colours of its own
        per cell (hashed), opaque; it is what the core keeps
     R  a bump map WITHOUT the offset bit, 16x8 pixels at 1:1 over a random 64x64 bump texture (128 different S, R:
        each kept colour its own fingerprint), shown as intensity x white on black (SRC_ALPHA, ZERO), in front
   The span sorter shades a tile's primitives in the order their runs come in raster order (README spansortcfg,
   timemeas order), so where R is in the tile decides whether it is shaded before or after S:
     page  0  readers only: what the core holds when the program starts (the last program's, or power-on's)
     page  1  S, then R at the bottom right (x 16-31, lines 24-31): R shaded after all of S
     page  2  S, then R at the top left (x 0-15, lines 0-7): R shaded first, before this tile's S
     page  3  R, then S (list order reversed), R at the bottom right in front by depth (GREATER-EQUAL): shaded as page 1
     page  4  as 1, S flat shaded (each of its four vertices another offset colour)
     page  5  as 1, S with SupSample
     page  6  as 1, S a bump map with the offset bit (its light: its last vertex's offset colour as it is)
     page  7  as 1, S only in the tiles of even columns: the odd columns read what an earlier tile left
     page  8  readers only, after page 7's render: across renders
     page  9  the core soft reset (SOFTRESET bit 1), then readers only
     page 10  S, then other readers at the bottom right, by cell column: a bump map (the control), a white 565 texture
              decal, modulate, and modulate with vertex fog (the offset colour's alpha is the fog's), all without the
              offset bit: does anything else read the kept colour?
     page 11  S, then two readers, at the top left and the bottom right
     page 12  as 1, the reader with SupSample */
#define NUM_PAGES 13
#include "pvrtest2.h"
#include "texgen.h"

enum { BR, TL, BOTH };          /* where the reader is */
enum { R_BUMP, R_DECAL, R_MOD, R_FOG };
typedef struct { int setter, rpos, reader_first, s_flat, s_super, s_bump, even_only, reset, other, r_super; } page_t;
static const page_t pages[NUM_PAGES] = {
    /* setter rpos rfirst flat super bump even reset other rsuper */
    { 0, BR, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 1, BR, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 1, TL, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 1, BR, 1, 0, 0, 0, 0, 0, 0, 0 },
    { 1, BR, 0, 1, 0, 0, 0, 0, 0, 0 },
    { 1, BR, 0, 0, 1, 0, 0, 0, 0, 0 },
    { 1, BR, 0, 0, 0, 1, 0, 0, 0, 0 },
    { 1, BR, 0, 0, 0, 0, 1, 0, 0, 0 },
    { 0, BR, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, BR, 0, 0, 0, 0, 0, 1, 0, 0 },
    { 1, BR, 0, 0, 0, 0, 0, 0, 1, 0 },
    { 1, BOTH, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 1, BR, 0, 0, 0, 0, 0, 0, 0, 1 },
};

static uint32_t tex_bump, tex_white;

static uint32_t hsh(uint32_t v) { v *= 2654435761u; v ^= v >> 15; v *= 2246822519u; v ^= v >> 13; v *= 3266489917u; v ^= v >> 16; return v; }

static uint32_t rnd_texel(const void *ctx, int size, int x, int y)
{
    return hsh((uint32_t)(size * 4096 + y * 64 + x) ^ *(const uint32_t *)ctx) & 0xffff;
}
static uint32_t white_texel(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0xffff; }

static void test_setup(void)
{
    static const uint32_t seed = 0x6f666673u;
    const tex_layout_t l = { TEX_TWIDDLED, 16, 64, 64, 0 };
    tex_bump = tex_build(&l, rnd_texel, &seed);
    tex_white = tex_build(&l, white_texel, &seed);
}

static void test_describe(void)
{
    static const char *what[NUM_PAGES] = {
        "readers only: what the core holds at the start", "setter, then reader at the bottom right",
        "setter, then reader at the top left (shaded first)", "reader listed first, at the bottom right, in front by depth",
        "flat shaded setter", "setter with SupSample", "setter a bump map with the offset bit",
        "setters in even columns only", "readers only, after a render", "core soft reset, then readers only",
        "other readers: bump / decal / modulate / vertex fog, no offset bit", "readers at the top left and the bottom right",
        "SupSample reader" };
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 300; c++)
            label(p, c, "%s", what[p]);
}

static uint32_t offs_of(int page, int c, int k) { return hsh((uint32_t)page * 100003u + (uint32_t)c * 977u + 30u + (uint32_t)k); }

/* the setter: a 32x32 quad (a strip of two triangles), white 565 modulated, offset colours per vertex */
static void setter(const page_t *g, int page, int c, int x, int y)
{
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    int k;
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Opaque;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Texture = 1;
    pcw.Offset = 1;
    pcw.Gouraud = !g->s_flat;
    isp.DepthMode = g->reader_first ? DepthMode_GreaterEqual : DepthMode_Always;
    isp.CullMode = CullMode_None;
    isp.Texture = 1;
    isp.Offset = 1;
    isp.Gouraud = pcw.Gouraud;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    tsp.ShadInstr = ShadInstr_Modulate;
    tsp.FilterMode = FilterMode_Point;
    tsp.SupSample = (uint32_t)g->s_super;
    tsp.MipMapD = 4;
    tsp.TexU = 3;
    tsp.TexV = 3;
    tcw.PixelFmt = g->s_bump ? PixelFmt_BumpMap : PixelFmt_565;
    tcw.TexAddr = (g->s_bump ? tex_bump : tex_white) >> 3;
    ta_polygon(pcw, isp, tsp, tcw);
    for(k = 0; k < 4; k++) {
        const int X = (k & 1) * 32, Y = (k >> 1) * 32;
        ta_vertex3(f32b_int(x + X), f32b_int(y + Y), F32B_ONE, f32b_scaled(X, -6), f32b_scaled(Y, -6),
                   0xff808080u, offs_of(page, c, k), k == 3);
    }
}

/* a reader of a kind: 16x8 pixels at (x, y), no offset bit, in front (depth 2) */
static void reader(const page_t *g, int kind, int x, int y, int u0, int v0)
{
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    int k;
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Opaque;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Texture = 1;
    pcw.Offset = 0;
    pcw.Gouraud = 1;
    isp.DepthMode = g->reader_first ? DepthMode_GreaterEqual : DepthMode_Always;
    isp.CullMode = CullMode_None;
    isp.Texture = 1;
    isp.Gouraud = 1;
    tsp.SrcInstr = kind == R_BUMP ? BlendInstr_SrcAlpha : BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = kind == R_FOG ? FogCtrl_Vertex : FogCtrl_None;
    tsp.UseAlpha = 1;
    tsp.ShadInstr = kind == R_DECAL ? ShadInstr_Decal : ShadInstr_Modulate;
    tsp.FilterMode = FilterMode_Point;
    tsp.SupSample = (uint32_t)g->r_super;
    tsp.MipMapD = 4;
    tsp.TexU = 3;
    tsp.TexV = 3;
    tcw.PixelFmt = kind == R_BUMP ? PixelFmt_BumpMap : PixelFmt_565;
    tcw.TexAddr = (kind == R_BUMP ? tex_bump : tex_white) >> 3;
    ta_polygon(pcw, isp, tsp, tcw);
    for(k = 0; k < 4; k++) {
        const int X = (k & 1) * 16, Y = (k >> 1) * 8;
        /* 1:1 over texels (u0 .. u0 + 15, v0 .. v0 + 7) of the 64x64 texture; the base colour grey (modulate
           readers: 0x80, so that an added offset colour shows either way) */
        ta_vertex3(f32b_int(x + X), f32b_int(y + Y), F(2.0f), f32b_scaled(u0 + X, -6), f32b_scaled(v0 + Y, -6),
                   kind == R_BUMP ? 0xffffffffu : 0xff808080u, 0, k == 3);
    }
}

static void test_page(int page)
{
    const page_t *g = &pages[page];
    int c;
    if(g->reset) {                      /* the core (ISP / TSP) soft reset, the TA left alone */
        pvrio_reg(SOFTRESET_addr, 2);
        pvrio_reg(SOFTRESET_addr, 0);
    }
    pvr_frame.bg_color = 0x000000;
    ta_list_begin(ListType_Opaque);
    for(c = 0; c < 300; c++) {
        const int x = cell_px(c), y = cell_py(c);
        const int has_s = g->setter && (!g->even_only || (c % COLS) % 2 == 0);
        const int kind = g->other ? (c % COLS) % 4 : R_BUMP;
        /* each cell reads its own part of the bump texture */
        const int u0 = (c * 7) % 48, v0 = (c * 13) % 56;
        if(g->reader_first)
            reader(g, kind, x + 16, y + 24, u0, v0);
        if(has_s)
            setter(g, page, c, x, y);
        if(!g->reader_first) {
            if(g->rpos == TL || g->rpos == BOTH)
                reader(g, kind, x, y, u0, v0);
            if(g->rpos == BR || g->rpos == BOTH)
                reader(g, kind, x + 16, y + 24, u0, v0);
        }
    }
    ta_list_end();
}
