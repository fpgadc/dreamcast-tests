/* Rules found by replaying game frames on refsw2 and on a console (dreamster's polly2-data dumps,
   ../../polly2dumps.py, ../../dumprender.sh), each one on its own here (2026-11-17). The scene a rule came from
   is given; the cells say what the rule predicts.

   page 0  (pre-sort; FPU_SHAD_SCALE 0x1b3: intensity mode, 0xb3; PT_ALPHA_REF 0x80; FOG_COL_VERT 0xe05010)
     cells 0-5    a modifier volume triangle at exactly the depth of the pixel is not in front of it (strictly
                  greater, not greater or equal): jgr_tree_shadows, a shadow volume's triangle with the depth of
                  the ground polygon, bit for bit. A cheap shadow polygon (0x40a0e0) over the cell, and a sheet of
                  the polygon's own corners (sheet(): the same depth arithmetic, so the same depth); shadowed where
                  the volume counts as inside.
                  0 at 0.5: outside      1 at 0.5, and a second sheet at 0.75 over the left 24 pixels: inside there
                  2 at 0.3: outside      3 sloped 0.3 .. 0.7 across: outside
                  4 sloped, the sheet closing an exclusion volume: shadowed (not inside)
                  5 at 0.5, a sheet at 0.5 over the left 24 pixels only (other corners: the depth arithmetic decides;
                    refsw2's, from zmeas, puts it just behind)
                  (two-volume polygons: page 1; in intensity mode the core reads their parameters as one volume's)
     cells 20-23  intensity shadow: the vertex colours are scaled, each with (c * scale) >> 8, before they are
                  interpolated, and alpha is not scaled (sc_ingame2, jgr_tree_shadows2). A volume (0.75) over the
                  left 24 pixels of a polygon at 0.5.
                  20 Gouraud colours    21 textured, modulate, Gouraud base and offset colours
                  22 punch through, alpha 0 at the left to 0xff at the right, over grey at 0.25; the volume over the
                     top 16 rows: the alpha test's edge at the same place inside and outside
                  23 flat (reference)
     cells 40-43  vertex fog: (c * (256 - a) + f * a) >> 8, a the offset colour's alpha (sc_ingame). Textured,
                  modulate (an offset colour comes only with a texture: refta, ta-tests)
                  40 white texture, 0x204080, a 0 .. 0xff across   41 white, Gouraud colours, a 0x10 .. 0xf0 down
                  42 a texture, white, a 0 .. 0xff along the diagonal   43 white, 0x204080, a 0x7e .. 0x82 across
     cells 60-67  trilinear filtering of a texture without mipmaps: point sampled (shenmue_intro). Point,
                  bilinear, trilinear A, trilinear B: 60-63 an 8x8 texture magnified, 64-67 64x64 minified.
     cells 80-85  punch through polygons in one plane (sc_ingame, a line of text): the last one in the list where
                  its alpha test passes, then where it fails the one before it, and so on. Over grey at 0.25;
                  textures with alpha 1 over all (full), the left half (left), the top half (top); blend SRC_ALPHA,
                  INV_SRC_ALPHA (a pixel that fails the alpha test is still blended: ONE, ZERO would write it).
                  80 at 0.5: red full, green left, blue top     81 red full, blue top, green left
                  82 red full, green left                       83 green left, red full
                  84 the order of 80 at 0.5, 0.6, 0.7 (reference) 85 green left, blue top (nothing full)
                  86, 87 red (alpha 0xff) at exactly the depth of grey opaque polygons (their corners; 0.5, sloped
                  0.3 .. 0.7): drawn (compare greater or equal with the opaque depth)
   page 1  (autosort; FPU_SHAD_SCALE 1) translucent polygons in one plane after one behind them later in the list (shenmue_menu):
           all are drawn. Alpha 0x80, red over the left 24 pixels, green over the right 24, blue over the middle 16
           rows, sent in the order given.
     cells 0-3    0 R 0.25, G 0.25, B 0.2    1 B 0.2, R 0.25, G 0.25    2 R 0.25, G 0.25, B 0.3
                  3 R 0.25, B 0.2, G 0.25
     cells 4-6    translucent polygons at exactly the depth of grey opaque ones (their corners): drawn, the opaque
                  depth being below every fragment at its depth. 4 at 0.5, 5 sloped 0.3 .. 0.7, 6 green then red
     cells 20-21  the modifier volume tie with two-volume polygons (0x4080c0, 0xc04020 inside), sheets of their own
                  corners: 20 at 0.5, outside; 21 sloped 0.3 .. 0.7, and a sheet at 0.75 over the left 24: inside there
   page 8  (pre-sort) depth write off (ZWriteDis) on translucent, opaque and punch through polygons: page_zwrite
   pages 2, 3  (pre-sort, autosort) ISP_BACKGND_D 0.5, in front of what is drawn at 0.3. In autosort, the
           translucent polygons are not hidden by the background's depth (xtreme_intro); opaque ones are.
     cells 0-6    0 opaque 0.3   1 opaque 0.7   2 translucent 0.3   3 translucent 0.7   4 translucent 0.3 compare
                  always   5 punch through 0.3   6 punch through 0.7
     cells 7-10   (both pages) 7 opaque 0.7 over the left 16 pixels, translucent 0.3 over the cell
                  8 translucent R 0.3, G 0.35   9 translucent R 0.6, G 0.3   10 opaque 0.2, translucent 0.3 */
#define NUM_PAGES 9
#define TEST_MODIFIERS
#include "pvrtest2.h"
#include "texgen.h"

#define SHADOW  1                   /* cheap shadow: affected by modifier volumes */
#define GOUR    2
#define TEX     4
#define OFFS    8
#define ZWD     16                  /* depth write off */
#define VOL2    32                  /* two volumes */

#define OP      ListType_Opaque
#define OPMOD   ListType_Opaque_Modifier_Volume
#define TR      ListType_Translucent
#define PT      ListType_Punch_Through

typedef struct { int list, depth, flags, src, dst, fog, shad, filt, size, fmt; uint32_t tex; } poly_t;

static uint32_t tex_col8, tex_col64, tex_white, tex_a[3];

static uint32_t col_texel(const void *ctx, int size, int x, int y) {
    (void)ctx; (void)size;
    return (uint32_t)(((x * 9 + 5) & 31) << 11 | ((y * 21 + 7) & 63) << 5 | (((x ^ y) * 5 + 3) & 31));
}

/* 1555, white; alpha 1 over all (0), the left half (1), the top half (2) */
static uint32_t a_texel(const void *ctx, int size, int x, int y) {
    const int k = *(const int *)ctx;
    (void)size;
    return ((k == 0 || (k == 1 && x < 4) || (k == 2 && y < 4)) ? 0x8000u : 0) | 0x7fffu;
}

static uint32_t white_texel(const void *ctx, int size, int x, int y) {
    (void)ctx; (void)size; (void)x; (void)y;
    return 0xffff;
}

static void test_setup(void) {
    static const int pattern[3] = { 0, 1, 2 };
    tex_layout_t l = { TEX_TWIDDLED, 16, 8, 8, 0 };
    int k;
    tex_col8 = tex_build(&l, col_texel, NULL);
    tex_white = tex_build(&l, white_texel, NULL);
    l.w = l.h = 64;
    tex_col64 = tex_build(&l, col_texel, NULL);
    l.w = l.h = 8;
    for(k = 0; k < 3; k++)
        tex_a[k] = tex_build(&l, a_texel, &pattern[k]);
}

static poly_t P(int list, int depth, int flags) {
    poly_t p;
    memset(&p, 0, sizeof(p));
    p.list = list; p.depth = depth; p.flags = flags;
    p.src = BlendInstr_One; p.dst = BlendInstr_Zero;
    p.fog = FogCtrl_None; p.shad = ShadInstr_Modulate; p.filt = FilterMode_Point; p.fmt = PixelFmt_565;
    if(list == TR || list == PT) {            /* punch through: what fails the alpha test leaves what is behind */
        p.src = BlendInstr_SrcAlpha; p.dst = BlendInstr_InvSrcAlpha;
    }
    return p;
}

static void begin(const poly_t *p) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };

    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = p->list;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Gouraud = (p->flags & GOUR) != 0;
    pcw.Texture = (p->flags & TEX) != 0;
    pcw.Offset = (p->flags & OFFS) != 0;
    pcw.Shadow = (p->flags & (SHADOW | VOL2)) != 0;
    pcw.Volume = (p->flags & VOL2) != 0;
    isp.DepthMode = p->depth;
    isp.ZWriteDis = (p->flags & ZWD) != 0;
    isp.CullMode = CullMode_None;
    isp.Gouraud = pcw.Gouraud;
    isp.Texture = pcw.Texture;
    isp.Offset = pcw.Offset;
    tsp.SrcInstr = p->src;
    tsp.DstInstr = p->dst;
    tsp.FogCtrl = p->fog;
    tsp.UseAlpha = 1;
    tsp.ShadInstr = p->shad;
    tsp.FilterMode = p->filt;
    tsp.MipMapD = 4;
    if(p->flags & TEX) {
        tsp.TexU = tsp.TexV = p->size;
        tcw.PixelFmt = p->fmt;
        tcw.TexAddr = p->tex >> 3;
    }
    if(p->flags & VOL2) {
        TA_PolyParam3 q;
        memset(&q, 0, sizeof(q));
        q.pcw = pcw; q.isp = isp; q.tsp = tsp; q.tcw = tcw;
        q.tsp1 = tsp; q.tcw1 = tcw;
        ta_send(&q, sizeof(q));
    } else {
        ta_polygon(pcw, isp, tsp, tcw);
    }
}

/* a w x h quad, corners top left, top right, bottom left, bottom right: depth zl on the left, zr on the right,
   colours col[], offset colours off[] (non-textured: only the offset alpha, for vertex fog); a texture is mapped
   whole (0..1). Two-volume polygons: col[] outside, col2 inside, flat. */
static void quad(const poly_t *p, int x, int y, int w, int h, f32b zl, f32b zr, const uint32_t col[4], const uint32_t off[4],
                 uint32_t col2) {
    int k;
    begin(p);
    for(k = 0; k < 4; k++) {
        const f32b vx = f32b_int(x + (k & 1) * w), vy = f32b_int(y + (k >> 1) * h), vz = (k & 1) ? zr : zl;
        if(p->flags & VOL2) {
            TA_Vertex9 v;
            memset(&v, 0, sizeof(v));
            v.pcw = pcw_vertex(k == 3);
            v.xyz[0] = vx; v.xyz[1] = vy; v.xyz[2] = vz;
            v.BaseCol0 = col[k];
            v.BaseCol1 = col2;
            ta_send(&v, sizeof(v));
        } else if(p->flags & TEX) {
            ta_vertex3(vx, vy, vz, (k & 1) ? F32B_ONE : F32B_ZERO, (k >> 1) ? F32B_ONE : F32B_ZERO, col[k], off ? off[k] : 0, k == 3);
        } else {
            TA_Vertex0 v;
            memset(&v, 0, sizeof(v));
            v.pcw = pcw_vertex(k == 3);
            v.xyz[0] = vx; v.xyz[1] = vy; v.xyz[2] = vz;
            v.BaseCol = col[k];
            v.ignore_3 = off ? off[k] : 0;          /* the offset colour's word */
            ta_send(&v, sizeof(v));
        }
    }
}

/* a flat-coloured quad at one depth */
static void flat(const poly_t *p, int x, int y, int w, int h, f32b z, uint32_t c) {
    const uint32_t col[4] = { c, c, c, c };
    quad(p, x, y, w, h, z, z, col, NULL, 0);
}

/* one modifier volume triangle */
static void mv_tri(int mode, int x0, int y0, f32b z0, int x1, int y1, f32b z1, int x2, int y2, f32b z2) {
    TA_ModVolParam m;
    TA_ModVol v;
    memset(&m, 0, sizeof(m));
    m.pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    m.pcw.ListType = OPMOD;
    m.isp.modvol.VolumeMode = mode;
    m.isp.modvol.CullMode = CullMode_None;
    ta_send(&m, sizeof(m));
    memset(&v, 0, sizeof(v));
    v.pcw = pcw_vertex(1);
    v.x0 = f32b_int(x0); v.y0 = f32b_int(y0); v.z0 = z0;
    v.x1 = f32b_int(x1); v.y1 = f32b_int(y1); v.z1 = z1;
    v.x2 = f32b_int(x2); v.y2 = f32b_int(y2); v.z2 = z2;
    ta_send(&v, sizeof(v));
}

/* a w x h sheet as the triangles of a strip of its corners, with the corners in the order the ISP gets a strip's
   (top left, top right, bottom left; bottom left, top right, bottom right): over a quad() of the same corners it
   has the polygon's depth, bit for bit. Depth zl on the left and zr on the right; the second triangle carries
   `last`. */
static void sheet(int last, int x, int y, int w, int h, f32b zl, f32b zr) {
    mv_tri(VolumeMode_Normal, x, y, zl, x + w, y, zr, x, y + h, zl);
    mv_tri(last, x, y + h, zl, x + w, y, zr, x + w, y + h, zr);
}

#define Z(v) F(v)

static const uint32_t gcol[4] = { 0xff0a1b2c, 0xfff0e1d2, 0xff8193a5, 0xff13579b };

/* ---- page 0 ---- */
static void page0_opaque(void) {
    const uint32_t c4[4] = { 0xff40a0e0, 0xff40a0e0, 0xff40a0e0, 0xff40a0e0 };
    const uint32_t white[4] = { 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff };
    const uint32_t offs[4] = { 0x00102030, 0x00302010, 0x00050a0f, 0x00403020 };
    poly_t p;
    int c;

    /* cells 0-5 */
    p = P(OP, DepthMode_GreaterEqual, SHADOW);
    for(c = 0; c < 6; c++) {
        if(c == 3 || c == 4)
            quad(&p, cell_px(c), cell_py(c), 32, 32, Z(0.3f), Z(0.7f), c4, NULL, 0);
        else
            flat(&p, cell_px(c), cell_py(c), 32, 32, c == 2 ? Z(0.3f) : F32B_HALF, 0xff40a0e0);
    }

    /* cells 20-23 */
    p = P(OP, DepthMode_GreaterEqual, SHADOW | GOUR);
    quad(&p, cell_px(20), cell_py(20), 32, 32, F32B_HALF, F32B_HALF, gcol, NULL, 0);
    p = P(OP, DepthMode_GreaterEqual, SHADOW | GOUR | TEX | OFFS);
    p.tex = tex_col8; p.size = 0;
    quad(&p, cell_px(21), cell_py(21), 32, 32, F32B_HALF, F32B_HALF, gcol, offs, 0);
    p = P(OP, DepthMode_GreaterEqual, 0);
    flat(&p, cell_px(22), cell_py(22), 32, 32, Z(0.25f), 0xff202020);
    p = P(OP, DepthMode_GreaterEqual, SHADOW);
    flat(&p, cell_px(23), cell_py(23), 32, 32, F32B_HALF, 0xff8193a5);

    /* cells 40-43: vertex fog */
    {
        const uint32_t a_x[4] = { 0x00000000, 0xff000000, 0x00000000, 0xff000000 };
        const uint32_t a_y[4] = { 0x10000000, 0x10000000, 0xf0000000, 0xf0000000 };
        const uint32_t a_d[4] = { 0x00000000, 0x80000000, 0x80000000, 0xff000000 };
        const uint32_t a_f[4] = { 0x7e000000, 0x82000000, 0x7e000000, 0x82000000 };
        const uint32_t base[4] = { 0xff204080, 0xff204080, 0xff204080, 0xff204080 };
        p = P(OP, DepthMode_GreaterEqual, GOUR | OFFS | TEX);     /* the offset colour: only with a texture (refta) */
        p.fog = FogCtrl_Vertex;
        p.tex = tex_white; p.size = 0;
        quad(&p, cell_px(40), cell_py(40), 32, 32, F32B_HALF, F32B_HALF, base, a_x, 0);
        quad(&p, cell_px(41), cell_py(41), 32, 32, F32B_HALF, F32B_HALF, gcol, a_y, 0);
        quad(&p, cell_px(43), cell_py(43), 32, 32, F32B_HALF, F32B_HALF, base, a_f, 0);
        p = P(OP, DepthMode_GreaterEqual, GOUR | OFFS | TEX);
        p.fog = FogCtrl_Vertex;
        p.tex = tex_col8; p.size = 0;
        quad(&p, cell_px(42), cell_py(42), 32, 32, F32B_HALF, F32B_HALF, white, a_d, 0);
    }

    /* cells 60-67: filtering without mipmaps */
    for(c = 0; c < 8; c++) {
        static const int filt[4] = { FilterMode_Point, FilterMode_Bilinear, FilterMode_TrilinearA, FilterMode_TrilinearB };
        p = P(OP, DepthMode_GreaterEqual, TEX);
        p.shad = ShadInstr_Decal;
        p.filt = filt[c & 3];
        p.tex = c < 4 ? tex_col8 : tex_col64;
        p.size = c < 4 ? 0 : 3;
        quad(&p, cell_px(60 + c), cell_py(60 + c), 32, 32, F32B_HALF, F32B_HALF, white, NULL, 0);
    }

    /* cells 80-85: grey behind the punch through polygons; 86-87 grey at their depth */
    p = P(OP, DepthMode_GreaterEqual, 0);
    for(c = 80; c < 86; c++)
        flat(&p, cell_px(c), cell_py(c), 32, 32, Z(0.25f), 0xff202020);
    {
        const uint32_t grey[4] = { 0xff202020, 0xff202020, 0xff202020, 0xff202020 };
        flat(&p, cell_px(86), cell_py(86), 32, 32, F32B_HALF, 0xff202020);
        quad(&p, cell_px(87), cell_py(87), 32, 32, Z(0.3f), Z(0.7f), grey, NULL, 0);
    }
}

static void page0_volumes(void) {
    int c;
    for(c = 0; c < 6; c++) {
        const int x = cell_px(c), y = cell_py(c);
        switch(c) {
        case 0: sheet(VolumeMode_Inside_Last, x, y, 32, 32, F32B_HALF, F32B_HALF); break;
        case 1: sheet(VolumeMode_Normal, x, y, 32, 32, F32B_HALF, F32B_HALF);
                sheet(VolumeMode_Inside_Last, x, y, 24, 32, Z(0.75f), Z(0.75f)); break;
        case 2: sheet(VolumeMode_Inside_Last, x, y, 32, 32, Z(0.3f), Z(0.3f)); break;
        case 3: sheet(VolumeMode_Inside_Last, x, y, 32, 32, Z(0.3f), Z(0.7f)); break;
        case 4: sheet(VolumeMode_Outside_Last, x, y, 32, 32, Z(0.3f), Z(0.7f)); break;
        case 5: sheet(VolumeMode_Inside_Last, x, y, 24, 32, F32B_HALF, F32B_HALF); break;
        }
    }
    for(c = 20; c < 24; c++) {
        if(c == 22)
            sheet(VolumeMode_Inside_Last, cell_px(c), cell_py(c), 32, 16, Z(0.75f), Z(0.75f));
        else
            sheet(VolumeMode_Inside_Last, cell_px(c), cell_py(c), 24, 32, Z(0.75f), Z(0.75f));
    }
}

static void pt_tex(int cell, int k, f32b z, uint32_t c) {
    const uint32_t col[4] = { c, c, c, c };
    poly_t p = P(PT, DepthMode_GreaterEqual, TEX);
    p.tex = tex_a[k]; p.size = 0; p.fmt = PixelFmt_1555;
    quad(&p, cell_px(cell), cell_py(cell), 32, 32, z, z, col, NULL, 0);
}

#define RED   0xffff0000u
#define GREEN 0xff00ff00u
#define BLUE  0xff0000ffu

static void page0_punch(void) {
    const uint32_t a[4] = { 0x00ff8000, 0xffff8000, 0x00ff8000, 0xffff8000 };
    poly_t p = P(PT, DepthMode_GreaterEqual, SHADOW | GOUR);
    quad(&p, cell_px(22), cell_py(22), 32, 32, F32B_HALF, F32B_HALF, a, NULL, 0);

    pt_tex(80, 0, F32B_HALF, RED);  pt_tex(80, 1, F32B_HALF, GREEN); pt_tex(80, 2, F32B_HALF, BLUE);
    pt_tex(81, 0, F32B_HALF, RED);  pt_tex(81, 2, F32B_HALF, BLUE);  pt_tex(81, 1, F32B_HALF, GREEN);
    pt_tex(82, 0, F32B_HALF, RED);  pt_tex(82, 1, F32B_HALF, GREEN);
    pt_tex(83, 1, F32B_HALF, GREEN); pt_tex(83, 0, F32B_HALF, RED);
    pt_tex(84, 0, F32B_HALF, RED);  pt_tex(84, 1, Z(0.6f), GREEN);   pt_tex(84, 2, Z(0.7f), BLUE);
    pt_tex(85, 1, F32B_HALF, GREEN); pt_tex(85, 2, F32B_HALF, BLUE);
    {
        const uint32_t red[4] = { RED, RED, RED, RED };
        poly_t q = P(PT, DepthMode_GreaterEqual, 0);
        quad(&q, cell_px(86), cell_py(86), 32, 32, F32B_HALF, F32B_HALF, red, NULL, 0);
        quad(&q, cell_px(87), cell_py(87), 32, 32, Z(0.3f), Z(0.7f), red, NULL, 0);
    }
}

/* ---- page 1 ---- */
static void rgb(int cell, char which, f32b z) {
    const int x = cell_px(cell), y = cell_py(cell);
    poly_t p = P(TR, DepthMode_GreaterEqual, 0);
    switch(which) {
    case 'R': flat(&p, x, y, 24, 32, z, 0x80ff0000); break;
    case 'G': flat(&p, x + 8, y, 24, 32, z, 0x8000ff00); break;
    case 'B': flat(&p, x, y + 8, 32, 16, z, 0x800000ff); break;
    }
}

static void page1(void) {
    const f32b Z25 = Z(0.25f), Z20 = Z(0.2f), Z30 = Z(0.3f);
    const uint32_t o[4] = { 0xff4080c0, 0xff4080c0, 0xff4080c0, 0xff4080c0 };
    poly_t p = P(OP, DepthMode_GreaterEqual, VOL2);
    ta_list_begin(OP);
    quad(&p, cell_px(20), cell_py(20), 32, 32, F32B_HALF, F32B_HALF, o, NULL, 0xffc04020);
    quad(&p, cell_px(21), cell_py(21), 32, 32, Z(0.3f), Z(0.7f), o, NULL, 0xffc04020);
    {
        const uint32_t grey[4] = { 0xff808080, 0xff808080, 0xff808080, 0xff808080 };
        p = P(OP, DepthMode_GreaterEqual, 0);
        quad(&p, cell_px(4), cell_py(4), 32, 32, F32B_HALF, F32B_HALF, grey, NULL, 0);
        quad(&p, cell_px(5), cell_py(5), 32, 32, Z(0.3f), Z(0.7f), grey, NULL, 0);
        quad(&p, cell_px(6), cell_py(6), 32, 32, F32B_HALF, F32B_HALF, grey, NULL, 0);
    }
    ta_list_end();
    ta_list_begin(OPMOD);
    sheet(VolumeMode_Inside_Last, cell_px(20), cell_py(20), 32, 32, F32B_HALF, F32B_HALF);
    sheet(VolumeMode_Normal, cell_px(21), cell_py(21), 32, 32, Z(0.3f), Z(0.7f));
    sheet(VolumeMode_Inside_Last, cell_px(21), cell_py(21), 24, 32, Z(0.75f), Z(0.75f));
    ta_list_end();
    ta_list_begin(TR);
    rgb(0, 'R', Z25); rgb(0, 'G', Z25); rgb(0, 'B', Z20);
    rgb(1, 'B', Z20); rgb(1, 'R', Z25); rgb(1, 'G', Z25);
    rgb(2, 'R', Z25); rgb(2, 'G', Z25); rgb(2, 'B', Z30);
    rgb(3, 'R', Z25); rgb(3, 'B', Z20); rgb(3, 'G', Z25);
    {
        const uint32_t g4[4] = { 0x8000ff00, 0x8000ff00, 0x8000ff00, 0x8000ff00 }, r4[4] = { 0x80ff0000, 0x80ff0000, 0x80ff0000, 0x80ff0000 };
        poly_t q = P(TR, DepthMode_GreaterEqual, 0);
        quad(&q, cell_px(4), cell_py(4), 32, 32, F32B_HALF, F32B_HALF, g4, NULL, 0);
        quad(&q, cell_px(5), cell_py(5), 32, 32, Z(0.3f), Z(0.7f), g4, NULL, 0);
        quad(&q, cell_px(6), cell_py(6), 32, 32, F32B_HALF, F32B_HALF, g4, NULL, 0);
        quad(&q, cell_px(6), cell_py(6), 32, 32, F32B_HALF, F32B_HALF, r4, NULL, 0);
    }
    ta_list_end();
}

/* ---- pages 4 and 5: depths below 2^-20 (polly2-data xtreme_intro: translucent triangles at about 2^-27,
   behind the background's 1e-8, are drawn whatever the background depth up to 0x1.f8p-21, and none from 2^-20) ---- */
static void page_small(void) {
    poly_t p;
    ta_list_begin(OP);
    p = P(OP, DepthMode_Greater, 0);
    flat(&p, cell_px(2), cell_py(2), 32, 32, 0x33000000, 0xffff0000);        /* 2^-25 */
    flat(&p, cell_px(3), cell_py(3), 32, 32, 0x34800000, 0xffff0000);        /* 2^-22 */
    flat(&p, cell_px(4), cell_py(4), 32, 32, 0x36000000, 0xffff0000);        /* 2^-19 */
    flat(&p, cell_px(5), cell_py(5), 32, 32, 0x35800000, 0xffff0000);        /* 2^-20 */
    flat(&p, cell_px(6), cell_py(6), 32, 32, 0x357fffff, 0xffff0000);        /* just below 2^-20 */
    p = P(OP, DepthMode_Always, 0);
    flat(&p, cell_px(7), cell_py(7), 32, 32, 0x33800000, 0xffff0000);        /* A 2^-24 */
    flat(&p, cell_px(8), cell_py(8), 32, 32, 0x36000000, 0xffff0000);        /* A 2^-19 */
    flat(&p, cell_px(9), cell_py(9), 16, 32, 0x33800000, 0xffff0000);        /* A 2^-24, left 16 */
    p = P(OP, DepthMode_Greater, 0);
    flat(&p, cell_px(7), cell_py(7), 16, 32, 0x34800000, 0xffffff00);        /* B 2^-22 over A */
    flat(&p, cell_px(8), cell_py(8), 16, 32, 0x36800000, 0xffffff00);        /* B 2^-18 over A */
    p = P(OP, DepthMode_GreaterEqual, 0);
    flat(&p, cell_px(10), cell_py(10), 32, 32, 0x32000000, 0xffff0000);      /* 2^-27, greater or equal */
    ta_list_end();
    ta_list_begin(TR);
    p = P(TR, DepthMode_Greater, 0);
    flat(&p, cell_px(0), cell_py(0), 32, 32, 0x32000000, 0x8000ff00);        /* 2^-27 */
    flat(&p, cell_px(1), cell_py(1), 32, 32, 0x33000000, 0x8000ff00);        /* 2^-25 */
    flat(&p, cell_px(9), cell_py(9), 32, 32, 0x32000000, 0x8000ff00);        /* 2^-27 over A 2^-24 */
    ta_list_end();
}

/* pages 6 and 7 (pre-sort): background depth just below 2^-20, and 2^-20 */
static void page_edge(void) {
    poly_t p;
    ta_list_begin(OP);
    p = P(OP, DepthMode_Greater, 0);
    flat(&p, cell_px(0), cell_py(0), 32, 32, 0x34800000, 0xffff0000);        /* 2^-22 */
    flat(&p, cell_px(1), cell_py(1), 32, 32, 0x00800000, 0xffff0000);        /* 2^-126 */
    flat(&p, cell_px(2), cell_py(2), 32, 32, 0x30000000, 0xffff0000);        /* 2^-31 */
    flat(&p, cell_px(4), cell_py(4), 32, 32, 0x357ffffe, 0xffff0000);        /* 0x1.fffffcp-21 */
    p = P(OP, DepthMode_GreaterEqual, 0);
    flat(&p, cell_px(5), cell_py(5), 32, 32, 0x00000000, 0xffff0000);        /* 0 */
    ta_list_end();
    ta_list_begin(TR);
    p = P(TR, DepthMode_Greater, 0);
    flat(&p, cell_px(3), cell_py(3), 32, 32, 0x34800000, 0x8000ff00);        /* 2^-22 */
    ta_list_end();
}

/* page 8 (pre-sort): depth write off. Grey opaque at 0.3, a stripe at 0.7 over x 8..15 (depth write off in
   the even cells, on in the odd ones), then green translucent at 0.5 over the cell, compare greater: green over
   the stripe only where the stripe left no depth. Cells 0-1 the stripe translucent, 2-3 opaque, 4-5 punch through. */
static void page_zwrite(void) {
    static const int lists[3] = { TR, OP, PT };
    const uint32_t green = 0x8000ff00;
    poly_t p;
    int c, l;
    for(l = 0; l < 3; l++) {
        ta_list_begin(lists[l]);
        for(c = 0; c < 6; c++) {
            if(lists[l] == OP) {
                p = P(OP, DepthMode_GreaterEqual, 0);
                flat(&p, cell_px(c), cell_py(c), 32, 32, Z(0.3f), 0xff808080);
            }
            if(lists[l] == lists[c / 2]) {
                p = P(lists[l], DepthMode_Greater, (c & 1) ? 0 : ZWD);
                flat(&p, cell_px(c) + 8, cell_py(c), 8, 32, Z(0.7f), lists[l] == TR ? 0x80ffffff : 0xffffffff);
            }
            if(lists[l] == TR) {
                p = P(TR, DepthMode_Greater, 0);
                flat(&p, cell_px(c), cell_py(c), 32, 32, F32B_HALF, green);
            }
        }
        ta_list_end();
    }
}

/* ---- pages 2 and 3 ---- */
static void page_bg(void) {
    const f32b Z3 = Z(0.3f), Z7 = Z(0.7f);
    poly_t p;
    ta_list_begin(OP);
    p = P(OP, DepthMode_Greater, 0);
    flat(&p, cell_px(0), cell_py(0), 32, 32, Z3, 0xffff0000);
    flat(&p, cell_px(1), cell_py(1), 32, 32, Z7, 0xffff0000);
    flat(&p, cell_px(7), cell_py(7), 16, 32, Z7, 0xffff0000);
    flat(&p, cell_px(10), cell_py(10), 32, 32, Z(0.2f), 0xffff0000);
    ta_list_end();
    ta_list_begin(TR);
    p = P(TR, DepthMode_Greater, 0);
    flat(&p, cell_px(2), cell_py(2), 32, 32, Z3, 0x8000ff00);
    flat(&p, cell_px(3), cell_py(3), 32, 32, Z7, 0x8000ff00);
    flat(&p, cell_px(7), cell_py(7), 32, 32, Z3, 0x8000ff00);
    rgb(8, 'R', Z3); rgb(8, 'G', Z(0.35f));
    rgb(9, 'R', Z(0.6f)); rgb(9, 'G', Z3);
    flat(&p, cell_px(10), cell_py(10), 32, 32, Z3, 0x8000ff00);
    p = P(TR, DepthMode_Always, 0);
    flat(&p, cell_px(4), cell_py(4), 32, 32, Z3, 0x8000ff00);
    ta_list_end();
    ta_list_begin(PT);
    p = P(PT, DepthMode_Greater, 0);
    flat(&p, cell_px(5), cell_py(5), 32, 32, Z3, 0xff0000ff);
    flat(&p, cell_px(6), cell_py(6), 32, 32, Z7, 0xff0000ff);
    ta_list_end();
}

static void test_describe(void) {
    static const char *mv[6] = { "0.5, a sheet of its corners: outside?", "0.5, a sheet of its corners and one at 0.75 (left 24): inside?",
                                 "0.3, a sheet of its corners: outside?", "sloped 0.3..0.7, a sheet of its corners: outside?",
                                 "sloped, a sheet of its corners closing an exclusion volume: shadowed?",
                                 "0.5, a sheet at 0.5 over the left 24 (other corners)" };
    static const char *in[4] = { "Gouraud", "textured, modulate, Gouraud base and offset", "punch through alpha 0..ff across, volume over the top 16 rows",
                                 "flat (reference)" };
    static const char *fog[4] = { "white texture, 204080, a 0..ff across", "white texture, Gouraud colours, a 10..f0 down",
                                  "a texture, white, a 0..ff along the diagonal", "white texture, 204080, a 7e..82 across" };
    static const char *flt[4] = { "point", "bilinear", "trilinear A", "trilinear B" };
    static const char *pt[8] = { "red full, green left, blue top, all at 0.5", "red full, blue top, green left, at 0.5",
                                 "red full, green left, at 0.5", "green left, red full, at 0.5",
                                 "red full 0.5, green left 0.6, blue top 0.7 (reference)", "green left, blue top, at 0.5",
                                 "red at exactly the depth of grey opaque 0.5 (its corners): drawn?",
                                 "red at exactly the depth of grey opaque sloped 0.3..0.7: drawn?" };
    static const char *as[4] = { "R 0.25, G 0.25, B 0.2", "B 0.2, R 0.25, G 0.25", "R 0.25, G 0.25, B 0.3", "R 0.25, B 0.2, G 0.25" };
    static const char *bg[11] = { "opaque 0.3", "opaque 0.7", "translucent 0.3", "translucent 0.7", "translucent 0.3, compare always",
                                  "punch through 0.3", "punch through 0.7", "opaque 0.7 left 16, translucent 0.3",
                                  "translucent R 0.3, G 0.35", "translucent R 0.6, G 0.3", "opaque 0.2, translucent 0.3" };
    static const char *small[11] = { "translucent 2^-27", "translucent 2^-25", "opaque 2^-25 greater", "opaque 2^-22 greater",
                                     "opaque 2^-19 greater", "opaque 2^-20 greater", "opaque 0x1.fffffep-21 greater",
                                     "opaque 2^-24, opaque 2^-22 greater over the left 16",
                                     "opaque 2^-19, opaque 2^-18 greater over the left 16",
                                     "opaque 2^-24 left 16, translucent 2^-27 over the cell", "opaque 2^-27 greater or equal" };
    static const char *edge[6] = { "opaque 2^-22 greater", "opaque 2^-126 greater", "opaque 2^-31 greater", "translucent 2^-22 greater",
                                   "opaque 0x1.fffffcp-21 greater", "opaque 0 greater or equal" };
    int c, p;
    for(c = 0; c < 6; c++)
        label(0, c, "modifier volume tie: %s", mv[c]);
    for(c = 0; c < 4; c++)
        label(0, 20 + c, "intensity shadow 0xb3, inside on the left: %s", in[c]);
    for(c = 0; c < 4; c++)
        label(0, 40 + c, "vertex fog e05010: %s", fog[c]);
    for(c = 0; c < 8; c++)
        label(0, 60 + c, "no mipmaps, %s, %s", c < 4 ? "8x8 magnified" : "64x64 minified", flt[c & 3]);
    for(c = 0; c < 8; c++)
        label(0, 80 + c, "punch through: %s", pt[c]);
    for(c = 0; c < 4; c++)
        label(1, c, "autosort, coplanar: %s", as[c]);
    label(1, 4, "autosort: green translucent at exactly the depth of grey opaque 0.5 (its corners): drawn?");
    label(1, 5, "autosort: green translucent at exactly the depth of grey opaque sloped 0.3..0.7: drawn?");
    label(1, 6, "autosort: green then red translucent at exactly the depth of grey opaque 0.5: both, red over green?");
    label(1, 20, "modifier volume tie: two-volume polygon (4080c0 / c04020 inside) at 0.5, a sheet of its corners: outside?");
    label(1, 21, "modifier volume tie: two-volume polygon sloped 0.3..0.7, a sheet of its corners and one at 0.75 (left 24): inside there?");
    for(p = 2; p < 4; p++)
        for(c = 0; c < 11; c++)
            label(p, c, "%s, background depth 0.5: %s", p == 2 ? "pre-sort" : "autosort", bg[c]);
    for(c = 0; c < 6; c++)
        label(8, c, "pre-sort: grey 0.3, %s stripe 0.7 depth write %s, green translucent 0.5 (greater): green over the stripe%s?",
              c < 2 ? "translucent" : c < 4 ? "opaque" : "punch through", (c & 1) ? "on" : "off", (c & 1) ? " not" : "");
    for(p = 6; p < 8; p++)
        for(c = 0; c < 6; c++)
            label(p, c, "pre-sort, background depth %s: %s", p == 6 ? "0x1.fffffep-21" : "2^-20", edge[c]);
    for(p = 4; p < 6; p++)
        for(c = 0; c < 11; c++)
            label(p, c, "%s, background depth 2^-26: %s", p == 5 ? "pre-sort" : "autosort", small[c]);
}

static void test_page(int page) {
    pvr_cfg.pre_sort = page == 1 || page == 3 || page == 4 ? 0 : 1;
    pvr_region_write(0);
    pvr_frame.bg_color = 0x000040;
    pvr_frame.bg_depth = page == 7 ? 0x35800000 : page == 6 ? 0x357fffff : page >= 4 ? 0x32800000 : page >= 2 ? F32B_HALF : 0x38d1b717;
    pvrio_reg(PT_ALPHA_REF_addr, 0x80);
    pvrio_reg(FPU_SHAD_SCALE_addr, page == 0 ? 0x1b3 : 0x1);
    pvrio_reg(FOG_COL_VERT_addr, 0x00e05010);
    switch(page) {
    case 0:
        ta_list_begin(OP);
        page0_opaque();
        ta_list_end();
        ta_list_begin(OPMOD);
        page0_volumes();
        ta_list_end();
        ta_list_begin(PT);
        page0_punch();
        ta_list_end();
        break;
    case 1:
        page1();
        break;
    case 2:
    case 3:
        page_bg();
        break;
    case 4:
    case 5:
        page_small();
        break;
    case 8:
        page_zwrite();
        break;
    default:
        page_edge();
        break;
    }
}
