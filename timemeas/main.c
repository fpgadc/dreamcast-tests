/* How long the core takes: the ISP and the TSP per triangle, per span and per pixel, by format.
   (Timing exists on a console only. A PC run draws the pages and measures nothing.)

   Method. The region array used for timing has ONE tile, tile (0, 0). Interrupts and the display are off
   while it runs. The "end of render" bits of the interrupt status are polled, and the cycle counter of the
   CPU (200 MHz: 5 ns) is read when each appears; the poll loop takes about 140 ns a turn. A scene is sent to
   the tile accelerator once and rendered REPS times; the least and the middle time of each are printed
   (lines "T ..."; the times are in 5 ns counts from the write to STARTRENDER). analyze.py makes the tables;
   REPORT.md is what they say.

   Only the TSP's bit is a usable event: it is the end of the tile's write-out. The ISP's bit is not the end
   of the ISP's work (it comes later when small triangles are seen). So every cost is a difference of two
   scenes' TSP bits:
     ISP   the background polygon alone, against N objects behind it (the TSP's work is the same);
     TSP   a scene against its twin with the same polygons behind the background (the ISP's work is the same).
     runs  the same scene with a modifier volume closed as "inside" and as "outside". Punch through polygons
           inside an opaque modifier volume are shaded twice, so there the difference is the TSP's work
           alone. Opaque polygons are shaded once; there the part of a polygon inside a volume is a
           triangle of its own to the TSP, which gives the cost of runs.
   The background polygon is always shaded by the TSP where nothing covers it, in every region array entry,
   also one that draws nothing and is not written out; it is flat and untextured here, the cheapest.

   TSP scenes: a format (texture, shading, list: formats[]) times a layout: the tile's 1024 pixels covered by
   polygons of that format in different ways, so that the numbers of pixels, of runs (pixels of one triangle
   next to each other on one line) and of triangles change independently. The pages of this test are the
   layouts drawn once each with every triangle in its own colour. */
/* #define TM_QUICK */                 /* only the scenes before the TM_QUICK line in run_all: a few seconds */
/* #define TM_DCALC */                 /* only the dcalc scenes: a few seconds */
/* #define TM_AHEAD */                 /* only the ahead scenes: a few seconds */
/* #define TM_SPAN */                  /* only the spanahead scenes: a few seconds */
/* #define TM_SPANFMT */               /* only the spanfmt scenes: a few seconds */
/* #define TM_LINE */                  /* only the lineshare scenes: a few seconds */
/* #define TM_VOL2 */                  /* only the vol2off and vol2pitch scenes: a few seconds */
/* #define TM_OFFS */                  /* only the offs2 and offs3 scenes: a few seconds */
#define NUM_LAYOUTS 17
#define NUM_PAGES NUM_LAYOUTS
#define TEST_MODIFIERS              /* the two modifier volume lists exist */
#include "pvrtest2.h"
#ifdef _arch_dreamcast
#include <arch/irq.h>
#include <dc/perfctr.h>
#endif

#define ONE_TILE    0x3e0000u       /* 32 bit area, unused: the region array of one tile */
#define ONE_TILE_NW 0x3e0100u       /* the same with the tile not written out */
#define MANY_TILES  0x3e0200u       /* region arrays of n tiles */
#define ONE_VAR     0x3e1000u       /* the one tile with a chosen number of entries that draw nothing in front */
#define REPS 11

static uint32_t tex;                /* 256 KB that every texture format reads in its own way */

typedef struct { const char *name; int list, textured, gouraud, offset, PixelFmt, ScanOrder, VQ, MipMapped, filter, scale, fog; } format_t;
#define OP ListType_Opaque
#define PT ListType_Punch_Through
#define TR ListType_Translucent
#define P0 FilterMode_Point
#define BI FilterMode_Bilinear
static const format_t formats[] = {
    /*                                           list tex gou ofs  format           scan vq mip filter         texels/pixel fog */
    { "untextured flat",                          OP, 0, 0, 0, 0,                0, 0, 0, P0, 1, 0 },
    { "untextured gouraud",                       OP, 0, 1, 0, 0,                0, 0, 0, P0, 1, 0 },
    { "565 flat",                                 OP, 1, 0, 0, PixelFmt_565,     0, 0, 0, P0, 1, 0 },
    { "565 gouraud",                              OP, 1, 1, 0, PixelFmt_565,     0, 0, 0, P0, 1, 0 },
    { "565 flat offset",                          OP, 1, 0, 1, PixelFmt_565,     0, 0, 0, P0, 1, 0 },
    { "565 gouraud offset",                       OP, 1, 1, 1, PixelFmt_565,     0, 0, 0, P0, 1, 0 },
    { "565 gouraud bilinear",                     OP, 1, 1, 0, PixelFmt_565,     0, 0, 0, BI, 1, 0 },
    { "565 gouraud mipmapped point",              OP, 1, 1, 0, PixelFmt_565,     0, 0, 1, P0, 1, 0 },
    { "565 gouraud mipmapped bilinear",           OP, 1, 1, 0, PixelFmt_565,     0, 0, 1, BI, 1, 0 },
    { "565 gouraud trilinear A",                  OP, 1, 1, 0, PixelFmt_565,     0, 0, 1, FilterMode_TrilinearA, 1, 0 },
    { "565 gouraud trilinear B",                  OP, 1, 1, 0, PixelFmt_565,     0, 0, 1, FilterMode_TrilinearB, 1, 0 },
    { "1555 gouraud",                             OP, 1, 1, 0, PixelFmt_1555,    0, 0, 0, P0, 1, 0 },
    { "4444 gouraud",                             OP, 1, 1, 0, PixelFmt_4444,    0, 0, 0, P0, 1, 0 },
    { "YUV422 gouraud",                           OP, 1, 1, 0, PixelFmt_YUV422,  0, 0, 0, P0, 1, 0 },
    { "bump gouraud offset",                      OP, 1, 1, 1, PixelFmt_BumpMap, 0, 0, 0, P0, 1, 0 },
    { "palette 4 bpp gouraud",                    OP, 1, 1, 0, PixelFmt_Pal4,    0, 0, 0, P0, 1, 0 },
    { "palette 8 bpp gouraud",                    OP, 1, 1, 0, PixelFmt_Pal8,    0, 0, 0, P0, 1, 0 },
    { "565 VQ gouraud",                           OP, 1, 1, 0, PixelFmt_565,     0, 1, 0, P0, 1, 0 },
    { "565 VQ mipmapped gouraud",                 OP, 1, 1, 0, PixelFmt_565,     0, 1, 1, P0, 1, 0 },
    { "565 not twiddled gouraud",                 OP, 1, 1, 0, PixelFmt_565,     1, 0, 0, P0, 1, 0 },
    { "YUV422 gouraud bilinear",                  OP, 1, 1, 0, PixelFmt_YUV422,  0, 0, 0, BI, 1, 0 },
    { "palette 8 bpp gouraud bilinear",           OP, 1, 1, 0, PixelFmt_Pal8,    0, 0, 0, BI, 1, 0 },
    { "565 VQ gouraud bilinear",                  OP, 1, 1, 0, PixelFmt_565,     0, 1, 0, BI, 1, 0 },
    { "565 gouraud, 8 texels a pixel",            OP, 1, 1, 0, PixelFmt_565,     0, 0, 0, P0, 8, 0 },
    { "565 gouraud mipmapped, 8 texels a pixel",  OP, 1, 1, 0, PixelFmt_565,     0, 0, 1, P0, 8, 0 },
    { "565 gouraud trilinear A, 6 texels a pixel", OP, 1, 1, 0, PixelFmt_565,    0, 0, 1, FilterMode_TrilinearA, 6, 0 },
    { "565 gouraud, fog table",                   OP, 1, 1, 0, PixelFmt_565,     0, 0, 0, P0, 1, 1 },
    { "565 gouraud, punch through list",          PT, 1, 1, 0, PixelFmt_565,     0, 0, 0, P0, 1, 0 },
    { "565 gouraud, translucent list",            TR, 1, 1, 0, PixelFmt_565,     0, 0, 0, P0, 1, 0 },
    { "untextured gouraud, translucent list",     TR, 0, 1, 0, 0,                0, 0, 0, P0, 1, 0 },
    /* added later (30 ..): the code above this line refers to formats by their numbers */
    { "YUV422 not twiddled gouraud",              OP, 1, 1, 0, PixelFmt_YUV422,  1, 0, 0, P0, 1, 0 },
    { "YUV422 not twiddled gouraud bilinear",     OP, 1, 1, 0, PixelFmt_YUV422,  1, 0, 0, BI, 1, 0 },
    { "565 not twiddled gouraud bilinear",        OP, 1, 1, 0, PixelFmt_565,     1, 0, 0, BI, 1, 0 },
    { "1555 gouraud bilinear",                    OP, 1, 1, 0, PixelFmt_1555,    0, 0, 0, BI, 1, 0 },
    { "4444 gouraud bilinear",                    OP, 1, 1, 0, PixelFmt_4444,    0, 0, 0, BI, 1, 0 },
    { "YUV422 gouraud mipmapped bilinear",        OP, 1, 1, 0, PixelFmt_YUV422,  0, 0, 1, BI, 1, 0 },
    { "YUV422 gouraud, 8 texels a pixel",         OP, 1, 1, 0, PixelFmt_YUV422,  0, 0, 0, P0, 8, 0 },
    { "YUV422 gouraud bilinear, 8 texels a pixel", OP, 1, 1, 0, PixelFmt_YUV422, 0, 0, 0, BI, 8, 0 },
    { "565 gouraud bilinear, 8 texels a pixel",   OP, 1, 1, 0, PixelFmt_565,     0, 0, 0, BI, 8, 0 },
    { "YUV422 VQ gouraud",                        OP, 1, 1, 0, PixelFmt_YUV422,  0, 1, 0, P0, 1, 0 },
    { "YUV422 VQ gouraud bilinear",               OP, 1, 1, 0, PixelFmt_YUV422,  0, 1, 0, BI, 1, 0 },
    { "palette 4 bpp VQ gouraud",                 OP, 1, 1, 0, PixelFmt_Pal4,    0, 1, 0, P0, 1, 0 },
    { "palette 8 bpp VQ gouraud",                 OP, 1, 1, 0, PixelFmt_Pal8,    0, 1, 0, P0, 1, 0 },
    { "palette 8 bpp VQ gouraud bilinear",        OP, 1, 1, 0, PixelFmt_Pal8,    0, 1, 0, BI, 1, 0 },
    { "palette 4 bpp gouraud bilinear",           OP, 1, 1, 0, PixelFmt_Pal4,    0, 0, 0, BI, 1, 0 },
    { "565 VQ gouraud, 8 texels a pixel",         OP, 1, 1, 0, PixelFmt_565,     0, 1, 0, P0, 8, 0 },
    { "palette 8 bpp gouraud, 8 texels a pixel",  OP, 1, 1, 0, PixelFmt_Pal8,    0, 0, 0, P0, 8, 0 },
    { "565 gouraud offset, 8 texels a pixel",     OP, 1, 1, 1, PixelFmt_565,     0, 0, 0, P0, 8, 0 },
    { "565 gouraud offset mipmapped point",       OP, 1, 1, 1, PixelFmt_565,     0, 0, 1, P0, 1, 0 },
};
#define NUM_FORMATS ((int)(sizeof(formats) / sizeof(formats[0])))

static const char *layout_names[NUM_LAYOUTS] = {
    "nothing (the background)", "1 sprite 32x32", "1 quad 32x32", "32 quads 32x1", "32 quads 1x32", "1024 quads 1x1",
    "32 sprites 32x1", "32 sprites 1x32", "1024 sprites 1x1", "16 quads 8x8", "64 quads 4x4", "256 quads 2x2",
    "4 quads 8x32", "4 quads 32x8", "1 quad 32x16", "1 quad 16x32", "256 sprites 2x2",
};
static int layout_has_sprites(int L) { return L == 1 || L == 6 || L == 7 || L == 8 || L == 16; }

static void test_setup(void) {
    static uint8_t data[1 << 18];
    uint32_t words[12], s = 12345;
    RegionArrayEntry e;
    ListPointer *p[5] = { &e.opaque, &e.opaque_mod, &e.trans, &e.trans_mod, &e.puncht };
    int i;

    for(i = 0; i < (int)sizeof(data); i++) {        /* any bytes are texels, indices and code books */
        s = s * 1664525u + 1013904223u;
        data[i] = (uint8_t)(s >> 24);
    }
    tex = pvr_tex_alloc(sizeof(data));
    pvr_tex_load(tex, data, sizeof(data));
    pvr_pal_format(PAL_RAM_CTRL_ARGB8888);
    for(i = 0; i < 1024; i++)
        pvr_pal_entry(i, 0xff000000u | (uint32_t)i * 0x00040201u);

    memset(&e, 0, sizeof(e));                       /* the entry that draws nothing, as KallistiOS has it first */
    e.control.no_writeout = 1;
    for(i = 0; i < 5; i++) p[i]->empty = 1;
    memcpy(&words[0], &e, sizeof(e));
    memset(&e, 0, sizeof(e));
    e.control.last_region = 1;
    e.control.pre_sort = pvr_cfg.pre_sort;
    for(i = 0; i < 5; i++) {
        if(pvr_cfg.opb_words[i])
            p[i]->ptr_in_words = pvr_list_base[i] >> 2;
        else
            p[i]->empty = 1;
    }
    memcpy(&words[6], &e, sizeof(e));
    pvrio_w32(ONE_TILE, words, 12);
    e.control.no_writeout = 1;                      /* the same, but the tile is not written to the framebuffer */
    memcpy(&words[6], &e, sizeof(e));
    pvrio_w32(ONE_TILE_NW, words, 12);
}

static void test_describe(void) {
    int p;
    for(p = 0; p < NUM_PAGES; p++)
        label(p, 0, "layout %d: %s", p, layout_names[p]);
}

/* ---- one rectangle of the tile in a format: a strip of two triangles, or a sprite ---- */
static uint32_t own_colour(int n) { return 0xff000080u | (uint32_t)(n & 255) << 16 | (uint32_t)((n >> 8) & 255) << 8; }

static int shadow_bit;              /* polygons are sent with the shadow bit: modifier volumes act on them */
static int dcalc_ctrl;              /* the polygons' DCalcCtrl bit: where the mipmap level's numerators are taken */
static int two_volumes;             /* strips are sent "with two volumes": a second TSP, TCW and colours for inside */
static int vol1_blend;              /* the second volume blends 0: as the first; 1: ONE, ONE; 2: ZERO, ONE; 3: SRC_ALPHA, INV_SRC_ALPHA */
static int vol_flat;                /* colours 0x404040 outside and 0x202020 (blend 3: white, alpha 0x80) inside, to read the result */
static int sheet_list = ListType_Opaque_Modifier_Volume;
static int vol_m;                   /* vol_flat: the first volume's colour tells the scenes apart (a stale picture shows) */
static const uint32_t vol_tint[3] = { 0x00404040u, 0x00406080u, 0x00804020u };
static f32b sheet_z;                /* depth of the modifier sheet; 0: 2.0, in front of everything */
static int presort = -1;            /* one_tile_region: the entry's pre-sort bit; -1: the configuration's */
static uint32_t vol_a = 0xff000000u; /* its alpha: half in the translucent list, so that what is under it shows */

typedef struct {                    /* vertex, textured, packed colour, 32 bit UV, with two volumes (64 bytes) */
    PCW pcw;
    f32b xyz[3];
    f32b u0, v0;
    uint32_t BaseCol0, OffsCol0;
    f32b u1, v1;
    uint32_t BaseCol1, OffsCol1;
    uint32_t ignore[4];
} vertex11_t;

static void header(const format_t *f, int DepthMode, PCW *pcw, ISP_TSP *isp, TSP *tsp, TCW *tcw) {
    pcw->full = isp->full = tsp->full = tcw->full = 0;
    pcw->Shadow = shadow_bit;
    pcw->ListType = f->list;
    pcw->Texture = f->textured;
    pcw->Offset = f->offset;
    pcw->Gouraud = f->gouraud;
    isp->DepthMode = DepthMode;
    isp->CullMode = CullMode_None;
    isp->DCalcCtrl = dcalc_ctrl;
    tsp->FogCtrl = f->fog ? FogCtrl_Table : FogCtrl_None;
    if(f->list == ListType_Translucent) {
        tsp->SrcInstr = BlendInstr_SrcAlpha;
        tsp->DstInstr = BlendInstr_InvSrcAlpha;
        tsp->UseAlpha = 1;
    } else {
        tsp->SrcInstr = BlendInstr_One;
        tsp->DstInstr = BlendInstr_Zero;
        tsp->UseAlpha = f->list == ListType_Punch_Through;
    }
    if(f->textured) {
        tsp->ShadInstr = ShadInstr_Modulate;
        tsp->FilterMode = f->filter;
        tsp->MipMapD = 4;
        tsp->TexU = 5;                          /* 256 texels */
        tsp->TexV = 5;
        tcw->PixelFmt = f->PixelFmt;
        tcw->ScanOrder = f->ScanOrder;
        tcw->VQ_Comp = f->VQ;
        tcw->MipMapped = f->MipMapped;
        tcw->TexAddr = tex >> 3;
    }
}

/* the offset colour of vertex k: offs_mode -1 the scene's own (dflt), 0 zero, 1 0x202020, 2 0x101010 (k + 1),
   3 0xff000000, 4 0x000020, 5 0x202020 at vertex 0 only, 6 at vertex 3 only */
static int offs_mode = -1;
static uint32_t offs_col(int k, uint32_t dflt) {
    switch(offs_mode) {
    case 0: return 0;
    case 1: return 0x00202020u;
    case 2: return 0x00101010u * (uint32_t)(k + 1);
    case 3: return 0xff000000u;                         /* alpha only */
    case 4: return 0x00000020u;                         /* blue only */
    case 5: return k == 0 ? 0x00202020u : 0;            /* the first vertex only */
    case 6: return k == 3 ? 0x00202020u : 0;            /* the last vertex only */
    default: return dflt;
    }
}

/* own: every triangle (or sprite) in a colour of its own, flat; otherwise four different corner colours */
static void rect(const format_t *f, int sprite, int DepthMode, int x, int y, int w, int h, f32b z, int n, int own) {
    static const uint32_t corner[4] = { 0xffff4040u, 0xff40ff40u, 0xff4040ffu, 0xffffffffu };
    PCW pcw; ISP_TSP isp; TSP tsp; TCW tcw;
    int k;

    header(f, DepthMode, &pcw, &isp, &tsp, &tcw);
    if(sprite) {
        TA_SpriteParam s;
        f32b xs[4] = { f32b_int(x), f32b_int(x + w), f32b_int(x + w), f32b_int(x) };
        f32b ys[4] = { f32b_int(y), f32b_int(y), f32b_int(y + h), f32b_int(y + h) };
        memset(&s, 0, sizeof(s));
        pcw.ParaType = ParamType_Sprite;
        pcw.Gouraud = 0;
        pcw.UV_16bit = 1;
        isp.Texture = f->textured;
        isp.Offset = f->offset;
        s.pcw = pcw; s.isp = isp; s.tsp = tsp; s.tcw = tcw;
        s.BaseCol = own ? own_colour(2 * n) : corner[3];
        s.OffsCol = 0x00202020u;
        ta_send(&s, sizeof(s));
        if(f->textured) {
            TA_Sprite1 v;
            memset(&v, 0, sizeof(v));
            v.pcw = pcw_vertex(1);
            v.x0 = xs[0]; v.y0 = ys[0]; v.z0 = z; v.x1 = xs[1]; v.y1 = ys[1]; v.z1 = z; v.x2 = xs[2]; v.y2 = ys[2]; v.z2 = z; v.x3 = xs[3]; v.y3 = ys[3];
            v.uv0 = uv16(f32b_scaled(x * f->scale, -8), f32b_scaled(y * f->scale, -8));
            v.uv1 = uv16(f32b_scaled((x + w) * f->scale, -8), f32b_scaled(y * f->scale, -8));
            v.uv2 = uv16(f32b_scaled((x + w) * f->scale, -8), f32b_scaled((y + h) * f->scale, -8));
            ta_send(&v, sizeof(v));
        } else {
            TA_Sprite0 v;
            memset(&v, 0, sizeof(v));
            v.pcw = pcw_vertex(1);
            v.x0 = xs[0]; v.y0 = ys[0]; v.z0 = z; v.x1 = xs[1]; v.y1 = ys[1]; v.z1 = z; v.x2 = xs[2]; v.y2 = ys[2]; v.z2 = z; v.x3 = xs[3]; v.y3 = ys[3];
            ta_send(&v, sizeof(v));
        }
        return;
    }
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    if(two_volumes) {
        TA_PolyParam3 p;
        memset(&p, 0, sizeof(p));
        pcw.Shadow = 1;
        pcw.Volume = 1;
        p.pcw = pcw; p.isp = isp; p.tsp = tsp; p.tcw = tcw; p.tsp1 = tsp; p.tcw1 = tcw;
        if(vol1_blend) {
            p.tsp1.SrcInstr = vol1_blend == 1 ? BlendInstr_One : vol1_blend == 2 ? BlendInstr_Zero : BlendInstr_SrcAlpha;
            p.tsp1.DstInstr = vol1_blend == 3 ? BlendInstr_InvSrcAlpha : BlendInstr_One;
            p.tsp1.UseAlpha = 1;
        }
        ta_send(&p, sizeof(p));
        for(k = 0; k < 4; k++) {
            int vx = x + (k & 1) * w, vy = y + (k >> 1) * h;
            if(f->textured) {
                vertex11_t v;
                memset(&v, 0, sizeof(v));
                v.pcw = pcw_vertex(k == 3);
                v.xyz[0] = f32b_int(vx); v.xyz[1] = f32b_int(vy); v.xyz[2] = z;
                v.u0 = v.u1 = f32b_scaled(vx * f->scale, -8);
                v.v0 = v.v1 = f32b_scaled(vy * f->scale, -8);
                v.BaseCol0 = vol_flat ? vol_tint[vol_m] | vol_a : corner[k];
                v.BaseCol1 = !vol_flat ? corner[k] & 0xff7f7f7fu : vol1_blend == 3 ? 0x80ffffffu : 0xff202020u;
                v.OffsCol0 = v.OffsCol1 = offs_col(k, 0);
                ta_send(&v, sizeof(v));
            } else {
                TA_Vertex9 v;
                memset(&v, 0, sizeof(v));
                v.pcw = pcw_vertex(k == 3);
                v.xyz[0] = f32b_int(vx); v.xyz[1] = f32b_int(vy); v.xyz[2] = z;
                v.BaseCol0 = vol_flat ? vol_tint[vol_m] | vol_a : corner[k];
                v.BaseCol1 = !vol_flat ? corner[k] & 0xff7f7f7fu : vol1_blend == 3 ? 0x80ffffffu : 0xff202020u;
                ta_send(&v, sizeof(v));
            }
        }
        return;
    }
    ta_polygon(pcw, isp, tsp, tcw);
    for(k = 0; k < 4; k++) {
        int vx = x + (k & 1) * w, vy = y + (k >> 1) * h;
        uint32_t c = vol_flat ? (vol_tint[vol_m] | vol_a) : own ? own_colour(2 * n + (k == 3)) : corner[k];
        if(f->textured)
            ta_vertex3(f32b_int(vx), f32b_int(vy), z, f32b_scaled(vx * f->scale, -8), f32b_scaled(vy * f->scale, -8), c, offs_col(k, 0x00101010u * (uint32_t)k), k == 3);
        else
            ta_vertex0(f32b_int(vx), f32b_int(vy), z, c, k == 3);
    }
}

/* ---- the layouts: the tile covered by rectangles of one size, each a strip of two triangles or a sprite ---- */
static const format_t *cur;         /* the format polygons are sent in */
static int cur_own;                 /* every triangle in a colour of its own (the pages) */
static int vis = DepthMode_Always;  /* DepthMode_Greater, with the background's depth in front of them: the same
                                       polygons, but none of them is seen (the scene's twin) */

static void tiles(int sprite, int w, int h) {
    int x, y, n = 0;
    for(y = 0; y < 32; y += h)
        for(x = 0; x < 32; x += w)
            rect(cur, sprite, vis, x, y, w, h, F32B_ONE, n++, cur_own);
}

static void layout(int L) {
    switch(L) {
    case 1: tiles(1, 32, 32); break;
    case 2: tiles(0, 32, 32); break;
    case 3: tiles(0, 32, 1); break;
    case 4: tiles(0, 1, 32); break;
    case 5: tiles(0, 1, 1); break;
    case 6: tiles(1, 32, 1); break;
    case 7: tiles(1, 1, 32); break;
    case 8: tiles(1, 1, 1); break;
    case 9: tiles(0, 8, 8); break;
    case 10: tiles(0, 4, 4); break;
    case 11: tiles(0, 2, 2); break;
    case 12: tiles(0, 8, 32); break;
    case 13: tiles(0, 32, 8); break;
    case 14: rect(cur, 0, vis, 0, 0, 32, 16, F32B_ONE, 0, cur_own); break;
    case 15: rect(cur, 0, vis, 0, 0, 16, 32, F32B_ONE, 0, cur_own); break;
    case 16: tiles(1, 2, 2); break;
    default: break;                             /* 0: nothing but the background */
    }
}

#ifdef _arch_dreamcast
/* ---- a render of the one tile, timed ---- */
typedef struct { uint32_t isp, tsp; } times_t;

/* pvr_frame_render of ../common/pvrsetup.h up to the write to STARTRENDER (the region array is set by measure) */
static const format_t *bg_fmt;      /* the background polygon's format. NULL: untextured and flat, the cheapest */
static uint32_t bg_depth;           /* ISP_BACKGND_D of the next renders. 0: the frame's (behind everything) */
static uint32_t clip_x, clip_y;     /* FB_X_CLIP and FB_Y_CLIP of the next renders. 0: the frame's */
static int lists_present = 5;       /* one_tile_region: so many of the tile's five lists have a pointer */
static int bg_shadow;               /* the background polygon has the shadow bit */
static uint32_t bg_xor;             /* changes the background's colour (0xc0c0c0), to see that it was drawn */
static int bg_ustep = 1, bg_uofs;   /* the background's texture: texels per pixel along x, and texels of offset */
static int bg_vstep = 1, bg_vofs;   /* the same along y */
static int no_opaque;               /* one_tile_region: the opaque list's pointer is marked empty */
static uint32_t shad_scale = 1;     /* FPU_SHAD_SCALE. 0x180: cheap shadow, half intensity */

static void prepare(void) {
    static const int corner[3][2] = { { 0, 480 }, { 0, 0 }, { 640, 480 } };
    const format_t *bf = bg_fmt ? bg_fmt : &formats[0];
    PCW pcw; ISP_TSP isp; TSP tsp; TCW tcw;
    ISP_BACKGND_T_type tag = { .full = 0 };
    uint32_t bg[24], events = 0;
    int i, n = 3;

    if(pvr_list_open >= 0)
        ta_list_end();
    for(i = 0; i < 5; i++)
        if(pvr_cfg.opb_words[i]) {
            if(!(pvr_lists_sent & (1u << i))) { ta_list_begin(i); ta_list_end(); }
            events |= PVRIO_LIST_DONE(i);
        }
    pvrio_wait(events);
    /* the background polygon: the words of a polygon in the parameter memory, three corners; the tile's every
       pixel that nothing else covers is shaded from it by the TSP like any other polygon's */
    header(bf, DepthMode_Greater, &pcw, &isp, &tsp, &tcw);
    isp.CullMode = CullMode_Negative;
    isp.Texture = bf->textured;
    isp.Offset = bf->offset;
    isp.Gouraud = bf->gouraud;
    bg[0] = isp.full; bg[1] = tsp.full; bg[2] = tcw.full;
    for(i = 0; i < 3; i++) {
        bg[n++] = f32b_int(corner[i][0]); bg[n++] = f32b_int(corner[i][1]); bg[n++] = 0x34000000;
        if(bf->textured) {
            bg[n++] = f32b_scaled(corner[i][0] * bf->scale * bg_ustep + bg_uofs, -8);
            bg[n++] = f32b_scaled(corner[i][1] * bf->scale * bg_vstep + bg_vofs, -8);
        }
        bg[n++] = (0xff000000u | 0x00404040u * (uint32_t)(i + 1)) ^ bg_xor;
        if(bf->offset)
            bg[n++] = 0x00101010u * (uint32_t)i;
    }
    pvrio_w32(SETUP_BACKGROUND, bg, n);
    tag.param_offs_in_words = (SETUP_BACKGROUND - SETUP_PARAMS) >> 2;
    tag.skip = 1 + 2 * bf->textured + bf->offset;       /* words of a corner after x, y, z */
    tag.shadow = bg_shadow;
    pvrio_reg(FPU_SHAD_SCALE_addr, shad_scale);
    pvrio_reg(PARAM_BASE_addr, SETUP_PARAMS);
    pvrio_reg(FB_W_SOF1_addr, SETUP_FB);
    pvrio_reg(ISP_BACKGND_T_addr, tag.full);
    pvrio_reg(ISP_BACKGND_D_addr, bg_depth ? bg_depth : pvr_frame.bg_depth);
    pvrio_reg(FB_X_CLIP_addr, clip_x ? clip_x : pvr_frame.FB_X_CLIP);
    pvrio_reg(FB_Y_CLIP_addr, clip_y ? clip_y : pvr_frame.FB_Y_CLIP);
}

static times_t go(void) {
    volatile uint32_t *status = (volatile uint32_t *)0xa05f6900;        /* SB_ISTNRM: bit 0 video, 1 ISP, 2 TSP */
    volatile uint32_t *start = (volatile uint32_t *)(0xa05f8000 + STARTRENDER_addr);
    times_t t = { 0, 0 };
    uint32_t seen = 0, polls = 0;
    uint64_t t0;
    int old = irq_disable();

    *status = 7;
    t0 = perf_cntr_count(PRFC0);
    *start = 0xffffffff;
    while((seen & 6) != 6 && polls < 200000) {
        uint32_t now = *status & 6 & ~seen;
        if(now) {
            uint32_t c = (uint32_t)(perf_cntr_count(PRFC0) - t0);
            if(now & 2) t.isp = c;
            if(now & 4) t.tsp = c;
            seen |= now;
        }
        polls++;
    }
    for(polls = 0; polls < 400 && !(*status & 1); polls++) ;           /* the video bit follows, when the tile is written out */
    *status = 7;
    irq_restore(old);
    return t;
}

static void sort(uint32_t *v, int n) {
    int i, j;
    for(i = 1; i < n; i++)
        for(j = i; j > 0 && v[j - 1] > v[j]; j--) { uint32_t s = v[j]; v[j] = v[j - 1]; v[j - 1] = s; }
}

static uint32_t region_addr = ONE_TILE;

/* a region array of the first n tiles in KallistiOS's order (column by column), at MANY_TILES */
static void tiles_region(int n) {
    static uint32_t words[6 * 65];
    RegionArrayEntry e;
    ListPointer *p[5] = { &e.opaque, &e.opaque_mod, &e.trans, &e.trans_mod, &e.puncht };
    int i, k;

    memset(&e, 0, sizeof(e));
    e.control.no_writeout = 1;
    for(i = 0; i < 5; i++) p[i]->empty = 1;
    memcpy(&words[0], &e, sizeof(e));
    for(k = 0; k < n; k++) {
        const int x = k / SETUP_TILES_Y, y = k % SETUP_TILES_Y;
        memset(&e, 0, sizeof(e));
        e.control.tilex = x;
        e.control.tiley = y;
        e.control.last_region = k == n - 1;
        e.control.pre_sort = pvr_cfg.pre_sort;
        for(i = 0; i < 5; i++) {
            if(pvr_cfg.opb_words[i])
                p[i]->ptr_in_words = (pvr_list_base[i] + (uint32_t)pvr_cfg.opb_words[i] * 4 * (uint32_t)(SETUP_TILES_X * y + x)) >> 2;
            else
                p[i]->empty = 1;
        }
        memcpy(&words[6 * (k + 1)], &e, sizeof(e));
    }
    pvrio_w32(MANY_TILES, words, 6 * (n + 1));
}

/* the one tile, with so many entries in front of its own that draw nothing and are not written out (ONE_TILE
   has one, as KallistiOS does) */
static void one_tile_region(int dummies) {
    static uint32_t words[6 * 9];
    RegionArrayEntry e;
    ListPointer *p[5] = { &e.opaque, &e.opaque_mod, &e.trans, &e.trans_mod, &e.puncht };
    int i, k;

    for(k = 0; k < dummies; k++) {
        memset(&e, 0, sizeof(e));
        e.control.no_writeout = 1;
        for(i = 0; i < 5; i++) p[i]->empty = 1;
        memcpy(&words[6 * k], &e, sizeof(e));
    }
    memset(&e, 0, sizeof(e));
    e.control.last_region = 1;
    e.control.pre_sort = presort >= 0 ? presort : pvr_cfg.pre_sort;
    for(i = 0; i < 5; i++) {
        if(pvr_cfg.opb_words[i] && i < lists_present && !(i == 0 && no_opaque))
            p[i]->ptr_in_words = pvr_list_base[i] >> 2;
        else
            p[i]->empty = 1;
    }
    memcpy(&words[6 * dummies], &e, sizeof(e));
    pvrio_w32(ONE_VAR, words, 6 * (dummies + 1));
}

/* Region arrays for the "keep Z" bit (z_keep): up to 8 entries, all of tile (0, 0). lists: 0 no list pointer;
   1 all five as usual; 2 the opaque pointer only; 3 the opaque pointer only, and it points to the objects of
   the TRANSLUCENT list (so that a second entry of the same tile can draw other polygons than the first). */
typedef struct { int z_keep, no_writeout, lists; } zk_entry_t;

static void zk_region(const zk_entry_t *en, int n) {
    static uint32_t words[6 * 8];
    RegionArrayEntry e;
    ListPointer *p[5] = { &e.opaque, &e.opaque_mod, &e.trans, &e.trans_mod, &e.puncht };
    int i, k;

    for(k = 0; k < n; k++) {
        memset(&e, 0, sizeof(e));
        e.control.last_region = k == n - 1;
        e.control.z_keep = en[k].z_keep;
        e.control.no_writeout = en[k].no_writeout;
        e.control.pre_sort = pvr_cfg.pre_sort;
        for(i = 0; i < 5; i++) {
            const int present = en[k].lists == 1 ? pvr_cfg.opb_words[i] != 0 : (i == 0 && en[k].lists >= 2);
            if(present)
                p[i]->ptr_in_words = pvr_list_base[en[k].lists == 3 ? ListType_Translucent : i] >> 2;
            else
                p[i]->empty = 1;
        }
        memcpy(&words[6 * k], &e, sizeof(e));
    }
    pvrio_w32(ONE_VAR, words, 6 * n);
}

/* The lists are sent: render REPS times; the least and the middle of the ISP's time, of the TSP's, and of TSP
   minus ISP. (A tile that is not written out, no_writeout in its region array entry, never raises the TSP's
   bit: that bit is the end of the write-out. So there is no measuring the TSP without it.) */
static void measure(const char *what, int a, int b, int c, int d) {
    const volatile uint32_t *fb = (const volatile uint32_t *)(0xa5000000 + SETUP_FB);
    uint32_t isp[REPS], tsp[REPS], dif[REPS];
    int k;

    prepare();
    pvrio_reg(REGION_BASE_addr, region_addr);
    go();
    for(k = 0; k < REPS; k++) {
        times_t t = go();
        isp[k] = t.isp; tsp[k] = t.tsp; dif[k] = t.tsp - t.isp;
    }
    sort(isp, REPS); sort(tsp, REPS); sort(dif, REPS);
    printf("T %s %d %d %d %d W %lu %lu %lu %lu %lu %lu P %06lx %06lx\n", what, a, b, c, d, (unsigned long)isp[0], (unsigned long)isp[REPS / 2], (unsigned long)tsp[0],
           (unsigned long)tsp[REPS / 2], (unsigned long)dif[0], (unsigned long)dif[REPS / 2],
           (unsigned long)(fb[0] & 0xffffff), (unsigned long)(fb[20 * 640 + 20] & 0xffffff));       /* the 0888 pixels (0, 0) and (20, 20) */
    pvr_frame_begin();
}

/* The opaque list of the TSP scenes starts with 48 quads that never pass the depth test: ONE_TILE has an entry
   in front that draws nothing, whose background the TSP shades for about 2070 counts, and the ISP's work on
   the tile is free for that long. With the 48 quads the ISP is the later one in a scene and in its twin. */
static void prefix(void) {
    int n;
    ta_list_begin(ListType_Opaque);
    for(n = 0; n < 48; n++)
        rect(&formats[0], 0, DepthMode_Never, 0, 0, 1, 1, F32B_ONE, n, 0);
}

static void begin(int list) {
    prefix();
    if(list != ListType_Opaque)
        ta_list_begin(list);
}

/* an untextured strip of n triangles, each w wide and h tall (a zigzag of n + 2 corners) */
static void strip(int DepthMode, int cull, int n, int x, int y, int w, int h, f32b z) {
    PCW pcw; ISP_TSP isp; TSP tsp; TCW tcw;
    int k;
    header(&formats[0], DepthMode, &pcw, &isp, &tsp, &tcw);
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    isp.CullMode = cull;
    ta_polygon(pcw, isp, tsp, tcw);
    for(k = 0; k < n + 2; k++)
        ta_vertex0(f32b_int(x + (k >> 1) * w), f32b_int(y + (k & 1) * h), z, 0xff000000u | 0x00404040u * (uint32_t)(k & 3), k == n + 1);
}

/* A modifier volume of one sheet in front of everything: the rectangle as two triangles (or, w = 0, one
   triangle that covers the tile). The last triangle carries the volume's mode: INSIDE_LAST puts the pixels
   under the sheet into the volume, OUTSIDE_LAST takes them out of it (none is in: nothing changes). */
static void sheet(int mode, int x, int y, int w, int h) {
    const f32b z = sheet_z ? sheet_z : f32b_int(2);
    int k;
    for(k = w ? 0 : 1; k < 2; k++) {
        TA_ModVolParam m;
        TA_ModVol v;
        memset(&m, 0, sizeof(m));
        m.pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
        m.pcw.ListType = sheet_list;
        m.isp.modvol.VolumeMode = k ? mode : VolumeMode_Normal;
        m.isp.modvol.CullMode = CullMode_None;
        ta_send(&m, sizeof(m));
        memset(&v, 0, sizeof(v));
        v.pcw = pcw_vertex(1);
        if(!w) {
            v.x0 = f32b_int(0);  v.y0 = f32b_int(0);  v.z0 = z;
            v.x1 = f32b_int(64); v.y1 = f32b_int(0);  v.z1 = z;
            v.x2 = f32b_int(0);  v.y2 = f32b_int(64); v.z2 = z;
        } else {
            v.x0 = f32b_int(x);              v.y0 = f32b_int(y);              v.z0 = z;
            v.x1 = f32b_int(x + w);          v.y1 = f32b_int(k ? y + h : y);  v.z1 = z;
            v.x2 = f32b_int(k ? x : x + w);  v.y2 = f32b_int(y + h);          v.z2 = z;
        }
        ta_send(&v, sizeof(v));
    }
}

static void run_all(void) {
    static const int tsp_layouts[10] = { 0, 2, 14, 5, 10, 4, 3, 12, 1, 8 };
    static const int ks[14] = { 0, 1, 2, 3, 4, 6, 8, 12, 14, 16, 20, 24, 28, 32 };
    static const int pads[5] = { 0, 1, 2, 3, 5 };
    static const int blend_sizes[6][2] = { { 32, 32 }, { 32, 16 }, { 16, 16 }, { 8, 8 }, { 32, 1 }, { 1, 1 } };
    static const int heights[11] = { 1, 2, 4, 8, 12, 16, 17, 20, 24, 28, 32 };
    static const int places[10][2] = { { 0, 0 }, { 0, 8 }, { 0, 12 }, { 0, 16 }, { 0, 24 }, { 24, 0 }, { 24, 8 }, { 24, 12 }, { 24, 16 }, { 24, 24 } };
    static const int counts[2] = { 64, 256 };
    const f32b front = F32B_ONE;
    int f, L, i, k, n, p, v, c, s;

    /* The display reads the framebuffer once a scan line, and the core waits while it does: with the display on,
       times jitter in steps of about 600 counts (3 microseconds). So the display is off while this runs. */
    volatile uint32_t *fb_r_ctrl = (volatile uint32_t *)(0xa05f8000 + 0x44);
    const uint32_t fb_r = *fb_r_ctrl;
    *fb_r_ctrl = fb_r & ~1u;
    printf("T display %lx off\n", (unsigned long)fb_r);
    for(f = 0; f < NUM_FORMATS; f++) printf("F %d %s\n", f, formats[f].name);
    for(L = 0; L < NUM_LAYOUTS; L++) printf("L %d %s\n", L, layout_names[L]);

    /* ---- offs2 f q a b: does the offset colour share anything with mipmapping? 565 Gouraud f 3, offset 5,
       mipmapped point 7, offset mipmapped point 48. q 0: setup, one tile, a = layout (2: 1 quad, 5: 1024 quads of
       1x1), b 0; q 1: run pitch, 64 tiles, a triangle cut into 1-pixel runs by 15 cheap-shadow sheets over lines
       0 .. a - 1 (a 24 or 32), pass b; offset colours 0x202020 (n + 1). */
    {
        volatile uint32_t *fb_w_ctrl = (volatile uint32_t *)(0xa05f8000 + FB_W_CTRL_addr);
        const uint32_t fbw = *fb_w_ctrl;
        static const int of[4] = { 3, 5, 7, 48 };
        int fi, li, ps, t;
        for(fi = 0; fi < 4; fi++)
            for(li = 0; li < 2; li++) {
                cur = &formats[of[fi]]; cur_own = 0;
                ta_list_begin(ListType_Opaque);
                layout(li ? 5 : 2);
                measure("offs2", of[fi], 0, li ? 5 : 2, 0);
            }
        cur = &formats[0];
        *fb_w_ctrl = (fbw & ~15u) | 1u;
        shadow_bit = 1; shad_scale = 0x180;
        for(ps = 0; ps < 2; ps++)
            for(fi = 0; fi < 4; fi++)
                for(li = 0; li < 2; li++) {
                    const format_t *fm = &formats[of[fi]];
                    const int L = li ? 32 : 24;
                    tiles_region(64);
                    region_addr = MANY_TILES;
                    ta_list_begin(ListType_Opaque);
                    for(t = 0; t < 64; t++) {
                        const int x0 = (t / SETUP_TILES_Y) * 32, y0 = (t % SETUP_TILES_Y) * 32;
                        static const int cx[3] = { 0, 64, 0 }, cy[3] = { 0, 0, 64 };
                        PCW pcw; ISP_TSP isp; TSP tsp; TCW tcw;
                        header(fm, DepthMode_Always, &pcw, &isp, &tsp, &tcw);
                        pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
                        pcw.Group_En = 1;
                        pcw.Strip_Len = 1;
                        ta_polygon(pcw, isp, tsp, tcw);
                        for(n = 0; n < 3; n++) {
                            const int vx = x0 + cx[n], vy = y0 + cy[n];
                            ta_vertex3(f32b_int(vx), f32b_int(vy), front, f32b_scaled(vx, -8), f32b_scaled(vy, -8),
                                       0xffc0c0c0u, 0x00202020u * (uint32_t)(n + 1), n == 2);
                        }
                    }
                    ta_list_begin(ListType_Opaque_Modifier_Volume);
                    for(t = 0; t < 64; t++) {
                        const int x0 = (t / SETUP_TILES_Y) * 32, y0 = (t % SETUP_TILES_Y) * 32;
                        for(n = 0; n < 15; n++)
                            sheet(VolumeMode_Inside_Last, x0 + 2 * n + 1, y0, 1, L);
                    }
                    measure("offs2", of[fi], 1, L, ps);
                }
        shadow_bit = 0; shad_scale = 1;
        *fb_w_ctrl = fbw;
        region_addr = ONE_TILE;
    }
    /* ---- offs3 f o L: what makes an offset colour 0? One tile, 565 flat (f 2, 4 with offset) or Gouraud (3, 5 with
       offset), layout L 2 (1 quad) or 11 (256 quads of 2x2: both triangles of a quad seen), offset colours offs_mode o
       (0 zero, 1 0x202020, 3 alpha only, 4 blue only, 5 vertex 0 only, 6 vertex 3 only). A quad is a strip v0 (0,0),
       v1 (w,0), v2 (0,h), v3 (w,h): triangles v0 v1 v2 (flat colour of v2) and v1 v2 v3 (of v3). */
    {
        static const int o3[6] = { 0, 1, 3, 4, 5, 6 }, f3[4] = { 2, 4, 3, 5 };
        int fi, oi, li;
        for(fi = 0; fi < 4; fi++)
            for(oi = 0; oi < 6; oi++)
                for(li = 0; li < 2; li++) {
                    if((f3[fi] == 2 || f3[fi] == 3) && oi > 1)
                        continue;
                    offs_mode = o3[oi];
                    cur = &formats[f3[fi]]; cur_own = 0;
                    ta_list_begin(ListType_Opaque);
                    layout(li ? 11 : 2);
                    measure("offs3", f3[fi], o3[oi], li ? 11 : 2, 0);
                }
        offs_mode = -1; cur = &formats[0];
    }
#ifdef TM_OFFS
    *fb_r_ctrl = fb_r;
    printf("T end\n");
    return;
#endif
    /* ---- vol2off k o f Lm: does a polygon with two volumes pay for its offset colour, and is the offset colour seen?
       k 0: ordinary polygons (shadow bit off, FPU_SHAD_SCALE 1); 1: with two volumes (FPU_SHAD_SCALE 0x080: not
       intensity mode). o: offset colour 0 zero, 1 0x202020, 2 0x101010 (k + 1) at vertex k (both sets alike). f 2 (565
       flat) or 4 (565 flat with offset colour). Lm = 10 L + m: layout 2 (1 quad) or 5 (1024 quads 1x1, 2048
       triangles); m 0 no volume, 1 inside a sheet over the tile (second set). One tile; the logged pixels show
       what was drawn. */
    {
        static const int vf[2] = { 2, 4 }, vl[2] = { 2, 5 };
        int kq, o, fi, li, m;
        for(kq = 0; kq < 2; kq++)
            for(o = 0; o < 3; o++)
                for(fi = 0; fi < 2; fi++)
                    for(li = 0; li < 2; li++)
                        for(m = 0; m < 2; m++) {
                            shad_scale = kq ? 0x080 : 1; shadow_bit = kq; bg_shadow = 0; two_volumes = kq;
                            offs_mode = o;
                            cur = &formats[vf[fi]]; cur_own = 0;
                            ta_list_begin(ListType_Opaque);
                            layout(vl[li]);
                            if(m) {
                                ta_list_begin(ListType_Opaque_Modifier_Volume);
                                sheet(VolumeMode_Inside_Last, 0, 0, 32, 32);
                            }
                            measure("vol2off", kq, o, vf[fi], 10 * vl[li] + m);
                        }
        offs_mode = -1; shad_scale = 1; shadow_bit = 0; two_volumes = 0; cur = &formats[0];
    }
    /* ---- vol2pitch f o L pass: spanshad's two-volume scenes (m 5, 6) again, now with FPU_SHAD_SCALE 0x080 (spanshad
       had 0x180, intensity mode, and drew them in the base colour only). 64 tiles, a 565 triangle with two volumes
       over each, cut into 1-pixel runs by 15 sheets of 1 x L at odd columns; f 2 or 4 (offset colour), o offset
       colour 0 or 0x202020 in both sets. 16 bit framebuffer as in spanshad. */
    {
        volatile uint32_t *fb_w_ctrl = (volatile uint32_t *)(0xa05f8000 + FB_W_CTRL_addr);
        const uint32_t fbw = *fb_w_ctrl;
        static const int sL[3] = { 16, 24, 32 };
        int ps, fi, o, li, t;
        *fb_w_ctrl = (fbw & ~15u) | 1u;
        shadow_bit = 1; shad_scale = 0x080;
        for(ps = 0; ps < 2; ps++)
            for(fi = 0; fi < 2; fi++)
                for(o = 0; o < 2; o++)
                    for(li = 0; li < 3; li++) {
                        const format_t *fm = &formats[fi ? 4 : 2];
                        tiles_region(64);
                        region_addr = MANY_TILES;
                        ta_list_begin(ListType_Opaque);
                        for(t = 0; t < 64; t++) {
                            const int x0 = (t / SETUP_TILES_Y) * 32, y0 = (t % SETUP_TILES_Y) * 32;
                            static const int cx[3] = { 0, 64, 0 }, cy[3] = { 0, 0, 64 };
                            PCW pcw; ISP_TSP isp; TSP tsp; TCW tcw;
                            TA_PolyParam3 pp;
                            header(fm, DepthMode_Always, &pcw, &isp, &tsp, &tcw);
                            pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
                            pcw.Group_En = 1;
                            pcw.Strip_Len = 1;
                            pcw.Volume = 1;
                            isp.Texture = 1;
                            isp.Offset = fm->offset;
                            memset(&pp, 0, sizeof(pp));
                            pp.pcw = pcw; pp.isp = isp; pp.tsp = tsp; pp.tcw = tcw; pp.tsp1 = tsp; pp.tcw1 = tcw;
                            ta_send(&pp, sizeof(pp));
                            for(n = 0; n < 3; n++) {
                                const int vx = x0 + cx[n], vy = y0 + cy[n];
                                vertex11_t vv;
                                memset(&vv, 0, sizeof(vv));
                                vv.pcw = pcw_vertex(n == 2);
                                vv.xyz[0] = f32b_int(vx); vv.xyz[1] = f32b_int(vy); vv.xyz[2] = front;
                                vv.u0 = vv.u1 = f32b_scaled(vx, -8);
                                vv.v0 = vv.v1 = f32b_scaled(vy, -8);
                                vv.BaseCol0 = vv.BaseCol1 = 0xffc0c0c0u;
                                vv.OffsCol0 = vv.OffsCol1 = o ? 0x00202020u : 0;
                                ta_send(&vv, sizeof(vv));
                            }
                        }
                        ta_list_begin(ListType_Opaque_Modifier_Volume);
                        for(t = 0; t < 64; t++) {
                            const int x0 = (t / SETUP_TILES_Y) * 32, y0 = (t % SETUP_TILES_Y) * 32;
                            for(n = 0; n < 15; n++)
                                sheet(VolumeMode_Inside_Last, x0 + 2 * n + 1, y0, 1, sL[li]);
                        }
                        measure("vol2pitch", fi ? 4 : 2, o, sL[li], ps);
                    }
        shadow_bit = 0; shad_scale = 1;
        *fb_w_ctrl = fbw;
        region_addr = ONE_TILE;
    }
#ifdef TM_VOL2
    *fb_r_ctrl = fb_r;
    printf("T end\n");
    return;
#endif

    /* ---- lineshare v o L pass: is part of a run start made once per line and triangle? Untextured sprites, each tile
       of the first 64 the same, nine runs of the first L lines at columns o .. o + 9 and one to the end of the line:
         v 0 (R): a sprite T over the whole tile, sprites S1..S4 of 1 x L in front at o + 1, 3, 5, 7: on a line T has
                  five runs (its first and four more) and S1..S4 one each;
         v 1 (D): the same runs from nine sprites: U0 over columns 0 .. o, U1..U3 of 1 x L at o + 2, 4, 6, U4 from
                  o + 8 to the end of the line, and S1..S4: every run is a sprite's only run on the line.
       The lines from L on: T (R) or the background (D). o 0 or 16: the same runs further right. The slope over L
       is the cost of a line of these runs; the setups (5 against 9 sprites a tile) drop out. 16 bit framebuffer. */
    {
        volatile uint32_t *fb_w_ctrl = (volatile uint32_t *)(0xa05f8000 + FB_W_CTRL_addr);
        const uint32_t fbw = *fb_w_ctrl;
        static const int lL[4] = { 8, 16, 24, 32 };
        int vv, oi, li, ps, t;
        *fb_w_ctrl = (fbw & ~15u) | 1u;
        for(ps = 0; ps < 2; ps++)
            for(vv = 0; vv < 2; vv++)
                for(oi = 0; oi < 2; oi++)
                    for(li = 0; li < 4; li++) {
                        const int o = oi * 16, L = lL[li];
                        tiles_region(64);
                        region_addr = MANY_TILES;
                        ta_list_begin(ListType_Opaque);
                        for(t = 0; t < 64; t++) {
                            const int x0 = (t / SETUP_TILES_Y) * 32, y0 = (t % SETUP_TILES_Y) * 32;
                            if(vv == 0)
                                rect(&formats[0], 1, DepthMode_Always, x0, y0, 32, 32, front, 0, 0);
                            else {
                                rect(&formats[0], 1, DepthMode_Always, x0, y0, o + 1, L, front, 1, 0);
                                for(n = 1; n <= 3; n++)
                                    rect(&formats[0], 1, DepthMode_Always, x0 + o + 2 * n, y0, 1, L, front, 1 + n, 0);
                                rect(&formats[0], 1, DepthMode_Always, x0 + o + 8, y0, 24 - o, L, front, 5, 0);
                            }
                            for(n = 0; n < 4; n++)
                                rect(&formats[0], 1, DepthMode_Always, x0 + o + 1 + 2 * n, y0, 1, L, front, 6 + n, 0);
                        }
                        measure("lineshare", vv, o, L, ps);
                    }
        /* lineshare2 f (10 K + v) L pass: the same, better made. Format f 0 (untextured) or 4 (565 with offset colour:
           run starts of about 8 clocks against about 45 of shading a line, so the lines are bound by run starts),
           K 3 or 4 sprites in front, and in D (v 1) a sprite T' over the whole tile behind everything, hidden on the
           first L lines, so that the lines from L on are the same in R and D (T, or T'). R has K + 1 first runs and K
           repeated runs of T a line, D has 2K + 1 first runs: R - D a line is K (repeated - first). Format 2 (565
           without offset colour) added later: 7 planes against 5 (format 0) and 11 (format 4). */
        {
            static const int fmt2[3] = { 0, 2, 4 };
            int fi2, K;
            for(ps = 0; ps < 2; ps++)
                for(fi2 = 0; fi2 < 3; fi2++)
                    for(K = 3; K <= 4; K++)
                        for(vv = 0; vv < 2; vv++)
                            for(li = 0; li < 4; li++) {
                                const format_t *fm = &formats[fmt2[fi2]];
                                const int L = lL[li];
                                tiles_region(64);
                                region_addr = MANY_TILES;
                                ta_list_begin(ListType_Opaque);
                                for(t = 0; t < 64; t++) {
                                    const int x0 = (t / SETUP_TILES_Y) * 32, y0 = (t % SETUP_TILES_Y) * 32;
                                    rect(fm, 1, DepthMode_Always, x0, y0, 32, 32, front, 0, 0);          /* T, or T' */
                                    if(vv == 1) {
                                        for(n = 0; n < K; n++)                                          /* U0 .. U(K-1) */
                                            rect(fm, 1, DepthMode_Always, x0 + 2 * n, y0, 1, L, front, 1 + n, 0);
                                        rect(fm, 1, DepthMode_Always, x0 + 2 * K, y0, 32 - 2 * K, L, front, 1 + K, 0);   /* UK */
                                    }
                                    for(n = 0; n < K; n++)                                              /* S1 .. SK */
                                        rect(fm, 1, DepthMode_Always, x0 + 1 + 2 * n, y0, 1, L, front, 8 + n, 0);
                                }
                                measure("lineshare2", fmt2[fi2], 10 * K + vv, L, ps);
                            }
        }
        /* lineshare3 4 (10 M + v) L pass: is what a repeated run saves kept for the line, or only for the last few
           triangles? 565 with offset colour. R (v 0): T over the tile behind M sprites of 1 x L at columns 1 .. M, so
           that a pattern line is T, S1 .. SM, T and T's second run comes after M other triangles' runs. D (v 1): T'
           behind everything, U0 at column 0, S1 .. SM, U1 from column M + 1: the same runs, every one a first run.
           R - D a line is (repeated - first) for one run: the same for every M if it is kept for the line, 0 from
           some M on if only the last few triangles' are kept. */
        {
            static const int mM[5] = { 1, 2, 4, 6, 8 };
            int mi;
            for(ps = 0; ps < 2; ps++)
                for(mi = 0; mi < 5; mi++)
                    for(vv = 0; vv < 2; vv++)
                        for(li = 0; li < 4; li++) {
                            const format_t *fm = &formats[4];
                            const int L = lL[li], M = mM[mi];
                            tiles_region(64);
                            region_addr = MANY_TILES;
                            ta_list_begin(ListType_Opaque);
                            for(t = 0; t < 64; t++) {
                                const int x0 = (t / SETUP_TILES_Y) * 32, y0 = (t % SETUP_TILES_Y) * 32;
                                rect(fm, 1, DepthMode_Always, x0, y0, 32, 32, front, 0, 0);              /* T, or T' */
                                if(vv == 1) {
                                    rect(fm, 1, DepthMode_Always, x0, y0, 1, L, front, 1, 0);            /* U0 */
                                    rect(fm, 1, DepthMode_Always, x0 + M + 1, y0, 31 - M, L, front, 2, 0);   /* U1 */
                                }
                                for(n = 0; n < M; n++)                                                  /* S1 .. SM */
                                    rect(fm, 1, DepthMode_Always, x0 + 1 + n, y0, 1, L, front, 8 + n, 0);
                            }
                            measure("lineshare3", 4, 10 * M + vv, L, ps);
                        }
        }
        /* lineshare4 k (10 M + v) L pass: what bounds the distance over which lineshare3's saving holds: spans, triangles
           or pixels? 565 with offset colour, T over the tile behind everything; a pattern line is T at column 0, a middle
           part, and from column e on T again (v 0, R) or a sprite U in front (v 1, R'). R - R' a line is what T's
           second run on the line saves. Middle parts:
             k 1: M sprites of 1 x L at columns 1 .. M (M spans, M triangles, M pixels between T's runs; as lineshare3)
             k 2: M sprites of 2 x L at columns 1 .. 2M (M spans, M triangles, 2M pixels)
             k 3: a sprite Q over columns 1 .. 2M + 1 behind M sprites of 1 x L at columns 2, 4, .. 2M (2M + 1 spans,
                  M + 1 triangles, 2M + 1 pixels) */
        {
            static const int kk[8] = { 1, 1, 2, 2, 2, 3, 3, 3 }, mm[8] = { 5, 7, 3, 4, 6, 2, 3, 4 };
            int ci;
            for(ps = 0; ps < 2; ps++)
                for(ci = 0; ci < 8; ci++)
                    for(vv = 0; vv < 2; vv++)
                        for(li = 0; li < 4; li++) {
                            const format_t *fm = &formats[4];
                            const int L = lL[li], k = kk[ci], M = mm[ci];
                            const int e = k == 1 ? M + 1 : 2 * M + 1 + (k == 3);
                            tiles_region(64);
                            region_addr = MANY_TILES;
                            ta_list_begin(ListType_Opaque);
                            for(t = 0; t < 64; t++) {
                                const int x0 = (t / SETUP_TILES_Y) * 32, y0 = (t % SETUP_TILES_Y) * 32;
                                rect(fm, 1, DepthMode_Always, x0, y0, 32, 32, front, 0, 0);              /* T */
                                if(k == 3)
                                    rect(fm, 1, DepthMode_Always, x0 + 1, y0, 2 * M + 1, L, front, 1, 0);    /* Q */
                                for(n = 0; n < M; n++) {
                                    if(k == 1)
                                        rect(fm, 1, DepthMode_Always, x0 + 1 + n, y0, 1, L, front, 8 + n, 0);
                                    else if(k == 2)
                                        rect(fm, 1, DepthMode_Always, x0 + 1 + 2 * n, y0, 2, L, front, 8 + n, 0);
                                    else
                                        rect(fm, 1, DepthMode_Always, x0 + 2 + 2 * n, y0, 1, L, front, 8 + n, 0);
                                }
                                if(vv == 1)
                                    rect(fm, 1, DepthMode_Always, x0 + e, y0, 32 - e, L, front, 2, 0);       /* U */
                            }
                            measure("lineshare4", k, 10 * M + vv, L, ps);
                        }
        }
        /* lineshare5 s (10 M + v) L pass: lineshare3 and 4 in one sweep, M 1 .. 8, at two parities. 565 with offset
           colour, T (T' for v 2) over the tile behind everything; a pattern line is a first part over columns 0 .. s,
           M sprites S1 .. SM of 1 x L at columns s + 1 .. s + M, and a tail from column s + M + 1:
             v 0 (R):  T, S1 .. SM, T          v 1 (R'): T, S1 .. SM, U          v 2 (D): U0, S1 .. SM, U */
        {
            int sh, M;
            for(ps = 0; ps < 2; ps++)
                for(sh = 0; sh < 2; sh++)
                    for(M = 1; M <= 8; M++)
                        for(vv = 0; vv < 3; vv++)
                            for(li = 0; li < 4; li++) {
                                const format_t *fm = &formats[4];
                                const int L = lL[li], e = sh + M + 1;
                                tiles_region(64);
                                region_addr = MANY_TILES;
                                ta_list_begin(ListType_Opaque);
                                for(t = 0; t < 64; t++) {
                                    const int x0 = (t / SETUP_TILES_Y) * 32, y0 = (t % SETUP_TILES_Y) * 32;
                                    rect(fm, 1, DepthMode_Always, x0, y0, 32, 32, front, 0, 0);          /* T, or T' */
                                    if(vv == 2)
                                        rect(fm, 1, DepthMode_Always, x0, y0, sh + 1, L, front, 1, 0);   /* U0 */
                                    for(n = 0; n < M; n++)                                              /* S1 .. SM */
                                        rect(fm, 1, DepthMode_Always, x0 + sh + 1 + n, y0, 1, L, front, 8 + n, 0);
                                    if(vv >= 1)
                                        rect(fm, 1, DepthMode_Always, x0 + e, y0, 32 - e, L, front, 2, 0);   /* U */
                                }
                                measure("lineshare5", sh, 10 * M + vv, L, ps);
                            }
        }
        *fb_w_ctrl = fbw;
        region_addr = ONE_TILE;
    }
#ifdef TM_LINE
    *fb_r_ctrl = fb_r;
    printf("T end\n");
    return;
#endif

    /* ---- spanfmt f L pass: what a run start costs by format. In each of the first 64 tiles one triangle of format f
       covers the tile (shadow bit, cheap shadow), 15 opaque modifier volumes of one pixel's width (columns 1, 3, ..
       29) cut lines 0 .. L - 1 into 31 runs (30 of one pixel), L 0, 8, 16, 24, 32. The volumes are the same for
       every L but 0; what L adds is runs: the slope over L is 30 run starts a line. 16 bit framebuffer. */
    {
        volatile uint32_t *fb_w_ctrl = (volatile uint32_t *)(0xa05f8000 + FB_W_CTRL_addr);
        const uint32_t fbw = *fb_w_ctrl;
        static const int ff[6] = { 0, 1, 2, 3, 4, 5 };
        static const int fL[5] = { 0, 8, 16, 24, 32 };
        int fi, li, ps, t;
        *fb_w_ctrl = (fbw & ~15u) | 1u;
        shadow_bit = 1; shad_scale = 0x180;
        for(ps = 0; ps < 2; ps++)
            for(fi = 0; fi < 6; fi++)
                for(li = 0; li < 5; li++) {
                    const format_t *fm = &formats[ff[fi]];
                    tiles_region(64);
                    region_addr = MANY_TILES;
                    ta_list_begin(ListType_Opaque);
                    for(t = 0; t < 64; t++) {
                        const int x0 = (t / SETUP_TILES_Y) * 32, y0 = (t % SETUP_TILES_Y) * 32;
                        static const int cx[3] = { 0, 64, 0 }, cy[3] = { 0, 0, 64 };
                        static const uint32_t col[3] = { 0xffc04040u, 0xff40c040u, 0xff4040c0u };
                        PCW pcw; ISP_TSP isp; TSP tsp; TCW tcw;
                        header(fm, DepthMode_Always, &pcw, &isp, &tsp, &tcw);
                        pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
                        pcw.Group_En = 1;
                        pcw.Strip_Len = 1;
                        ta_polygon(pcw, isp, tsp, tcw);
                        for(n = 0; n < 3; n++) {
                            const int vx = x0 + cx[n], vy = y0 + cy[n];
                            if(fm->textured)
                                ta_vertex3(f32b_int(vx), f32b_int(vy), front, f32b_scaled(vx * fm->scale, -8), f32b_scaled(vy * fm->scale, -8),
                                           col[n], 0x00202020u * (uint32_t)(n + 1), n == 2);
                            else
                                ta_vertex0(f32b_int(vx), f32b_int(vy), front, col[n], n == 2);
                        }
                    }
                    ta_list_begin(ListType_Opaque_Modifier_Volume);
                    for(t = 0; t < 64; t++) {
                        const int x0 = (t / SETUP_TILES_Y) * 32, y0 = (t % SETUP_TILES_Y) * 32;
                        if(fL[li])
                            for(n = 0; n < 15; n++)
                                sheet(VolumeMode_Inside_Last, x0 + 2 * n + 1, y0, 1, fL[li]);
                    }
                    measure("spanfmt", ff[fi], fL[li], ps, 0);
                }
        /* spanshad m L pass: what the pitch of run starts depends on. 565 triangle, cut as above over lines 0 .. L - 1.
           m 0: modulate (format 2); 1: decal (the base colour not used); 2: modulate alpha; 3: offset, modulate
           (format 4); 4: offset, decal; 5: two volumes (both sets 565 modulate); 6: two volumes with offset. m 5 and 6
           with FPU_SHAD_SCALE 0x080 (two-volume mode) since run27; run21 had 0x180 and drew them in the base colour. */
        {
            static const int sL[3] = { 16, 24, 32 };
            for(ps = 0; ps < 2; ps++)
                for(fi = 0; fi < 7; fi++)
                    for(li = 0; li < 3; li++) {
                        const format_t *fm = &formats[fi == 3 || fi == 4 || fi == 6 ? 4 : 2];
                        shad_scale = fi >= 5 ? 0x080 : 0x180;
                        tiles_region(64);
                        region_addr = MANY_TILES;
                        ta_list_begin(ListType_Opaque);
                        for(t = 0; t < 64; t++) {
                            const int x0 = (t / SETUP_TILES_Y) * 32, y0 = (t % SETUP_TILES_Y) * 32;
                            static const int cx[3] = { 0, 64, 0 }, cy[3] = { 0, 0, 64 };
                            PCW pcw; ISP_TSP isp; TSP tsp; TCW tcw;
                            header(fm, DepthMode_Always, &pcw, &isp, &tsp, &tcw);
                            pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
                            pcw.Group_En = 1;
                            pcw.Strip_Len = 1;
                            tsp.ShadInstr = fi == 1 || fi == 4 ? ShadInstr_Decal : fi == 2 ? ShadInstr_ModulateAlpha : ShadInstr_Modulate;
                            if(fi >= 5) {
                                TA_PolyParam3 pp;
                                memset(&pp, 0, sizeof(pp));
                                pcw.Volume = 1;
                                isp.Texture = 1;
                                isp.Offset = fm->offset;
                                pp.pcw = pcw; pp.isp = isp; pp.tsp = tsp; pp.tcw = tcw; pp.tsp1 = tsp; pp.tcw1 = tcw;
                                ta_send(&pp, sizeof(pp));
                                for(n = 0; n < 3; n++) {
                                    const int vx = x0 + cx[n], vy = y0 + cy[n];
                                    vertex11_t vv;
                                    memset(&vv, 0, sizeof(vv));
                                    vv.pcw = pcw_vertex(n == 2);
                                    vv.xyz[0] = f32b_int(vx); vv.xyz[1] = f32b_int(vy); vv.xyz[2] = front;
                                    vv.u0 = vv.u1 = f32b_scaled(vx, -8);
                                    vv.v0 = vv.v1 = f32b_scaled(vy, -8);
                                    vv.BaseCol0 = vv.BaseCol1 = 0xffc0c0c0u;
                                    vv.OffsCol0 = vv.OffsCol1 = 0x00202020u;
                                    ta_send(&vv, sizeof(vv));
                                }
                                continue;
                            }
                            ta_polygon(pcw, isp, tsp, tcw);
                            for(n = 0; n < 3; n++) {
                                const int vx = x0 + cx[n], vy = y0 + cy[n];
                                ta_vertex3(f32b_int(vx), f32b_int(vy), front, f32b_scaled(vx, -8), f32b_scaled(vy, -8),
                                           0xffc0c0c0u, 0x00202020u * (uint32_t)(n + 1), n == 2);
                            }
                        }
                        ta_list_begin(ListType_Opaque_Modifier_Volume);
                        for(t = 0; t < 64; t++) {
                            const int x0 = (t / SETUP_TILES_Y) * 32, y0 = (t % SETUP_TILES_Y) * 32;
                            for(n = 0; n < 15; n++)
                                sheet(VolumeMode_Inside_Last, x0 + 2 * n + 1, y0, 1, sL[li]);
                        }
                        measure("spanshad", fi, sL[li], ps, 0);
                    }
        }
        shadow_bit = 0; shad_scale = 1;
        *fb_w_ctrl = fbw;
        region_addr = ONE_TILE;
    }
#ifdef TM_SPANFMT
    *fb_r_ctrl = fb_r;
    printf("T end\n");
    return;
#endif

    /* ---- spanahead v k f: how many runs ahead can the TSP start? In each of the first 64 tiles one triangle covers the
       whole tile (corners at the tile's origin, 64 to the right and 64 down), with the shadow bit, cheap shadow on.
       Every line of it is one run of 32 pixels, except line B, which k opaque modifier volumes of one pixel each
       (at columns 1, 3, .. 2k - 1) cut into 2k runs of one pixel (runs end where the volume state changes) and a
       last one. No other triangle: the runs need no setups. v 0: 565 with texels 8 apart (about 3.3 clocks a
       pixel), B 24; v 1: 565, B 24; v 2: 565, B 1. f: the triangle without offset colour, and with it (format
       47 / 5): a run costs at least about 6 without and 8 with, the pixels the same, so the offset colour adds 2
       for every run whose start the pixels wait for. 16 bit framebuffer: the TSP sets the pace.
       v 3: 565, the k volumes are columns of one pixel over all 32 lines: every line is cut into 2k + 1 runs
       (sustained: nothing to hide in). */
    {
        volatile uint32_t *fb_w_ctrl = (volatile uint32_t *)(0xa05f8000 + FB_W_CTRL_addr);
        const uint32_t fbw = *fb_w_ctrl;
        static const int sfm[4][2] = { { 23, 47 }, { 3, 5 }, { 3, 5 }, { 3, 5 } };
        static const int sB[4] = { 24, 24, 1, 0 };
        int t, q;
        *fb_w_ctrl = (fbw & ~15u) | 1u;
        shadow_bit = 1; shad_scale = 0x180;
        for(v = 0; v < 4; v++)
            for(k = 0; k <= 15; k++)
                for(q = 0; q < 2; q++) {
                    const format_t *fm = &formats[sfm[v][q]];
                    tiles_region(64);
                    region_addr = MANY_TILES;
                    ta_list_begin(ListType_Opaque);
                    for(t = 0; t < 64; t++) {
                        const int x0 = (t / SETUP_TILES_Y) * 32, y0 = (t % SETUP_TILES_Y) * 32;
                        static const int cx[3] = { 0, 64, 0 }, cy[3] = { 0, 0, 64 };
                        PCW pcw; ISP_TSP isp; TSP tsp; TCW tcw;
                        header(fm, DepthMode_Always, &pcw, &isp, &tsp, &tcw);
                        pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
                        pcw.Group_En = 1;
                        pcw.Strip_Len = 1;
                        ta_polygon(pcw, isp, tsp, tcw);
                        for(n = 0; n < 3; n++) {
                            const int vx = x0 + cx[n], vy = y0 + cy[n];
                            ta_vertex3(f32b_int(vx), f32b_int(vy), front, f32b_scaled(vx * fm->scale, -8), f32b_scaled(vy * fm->scale, -8),
                                       0xffc0c0c0u, 0x00202020u, n == 2);
                        }
                    }
                    ta_list_begin(ListType_Opaque_Modifier_Volume);
                    for(t = 0; t < 64; t++) {
                        const int x0 = (t / SETUP_TILES_Y) * 32, y0 = (t % SETUP_TILES_Y) * 32;
                        for(n = 0; n < k; n++)       /* v 3: columns of one pixel over all 32 lines: every line cut */
                            sheet(VolumeMode_Inside_Last, x0 + 2 * n + 1, y0 + sB[v], 1, v == 3 ? 32 : 1);
                    }
                    measure("spanahead", v, k, sfm[v][q], 0);
                }
        /* spanburst L f: the same triangle (565, f as above), 15 columns of one pixel over lines 8 .. 8 + L - 1: L
           lines of 30 one-pixel runs and a last one; how many of the 30 L runs does the shading wait for? */
        {
            static const int Ls[12] = { 1, 2, 3, 4, 5, 6, 8, 10, 12, 16, 20, 24 };
            int li;
            for(li = 0; li < 12; li++)
                for(q = 0; q < 2; q++) {
                    const format_t *fm = &formats[q ? 5 : 3];
                    tiles_region(64);
                    region_addr = MANY_TILES;
                    ta_list_begin(ListType_Opaque);
                    for(t = 0; t < 64; t++) {
                        const int x0 = (t / SETUP_TILES_Y) * 32, y0 = (t % SETUP_TILES_Y) * 32;
                        static const int cx[3] = { 0, 64, 0 }, cy[3] = { 0, 0, 64 };
                        PCW pcw; ISP_TSP isp; TSP tsp; TCW tcw;
                        header(fm, DepthMode_Always, &pcw, &isp, &tsp, &tcw);
                        pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
                        pcw.Group_En = 1;
                        pcw.Strip_Len = 1;
                        ta_polygon(pcw, isp, tsp, tcw);
                        for(n = 0; n < 3; n++) {
                            const int vx = x0 + cx[n], vy = y0 + cy[n];
                            ta_vertex3(f32b_int(vx), f32b_int(vy), front, f32b_scaled(vx * fm->scale, -8), f32b_scaled(vy * fm->scale, -8),
                                       0xffc0c0c0u, 0x00202020u, n == 2);
                        }
                    }
                    ta_list_begin(ListType_Opaque_Modifier_Volume);
                    for(t = 0; t < 64; t++) {
                        const int x0 = (t / SETUP_TILES_Y) * 32, y0 = (t % SETUP_TILES_Y) * 32;
                        for(n = 0; n < 15; n++)
                            sheet(VolumeMode_Inside_Last, x0 + 2 * n + 1, y0 + 8, 1, Ls[li]);
                    }
                    measure("spanburst", Ls[li], q ? 5 : 3, 0, 0);
                }
        }
        /* spanpos B L pass: the same, plain 565, the 15 columns over lines B .. B + L - 1 (L 0: no cut). Does what is
           hidden depend on how many lines come before the cut lines? */
        {
            static const int Bs[3] = { 1, 8, 20 };
            static const int Ls[7] = { 0, 1, 2, 3, 4, 6, 8 };
            int bi, li, ps;
            for(ps = 0; ps < 2; ps++)
                for(bi = 0; bi < 3; bi++)
                    for(li = 0; li < 7; li++) {
                        const format_t *fm = &formats[3];
                        tiles_region(64);
                        region_addr = MANY_TILES;
                        ta_list_begin(ListType_Opaque);
                        for(t = 0; t < 64; t++) {
                            const int x0 = (t / SETUP_TILES_Y) * 32, y0 = (t % SETUP_TILES_Y) * 32;
                            static const int cx[3] = { 0, 64, 0 }, cy[3] = { 0, 0, 64 };
                            PCW pcw; ISP_TSP isp; TSP tsp; TCW tcw;
                            header(fm, DepthMode_Always, &pcw, &isp, &tsp, &tcw);
                            pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
                            pcw.Group_En = 1;
                            pcw.Strip_Len = 1;
                            ta_polygon(pcw, isp, tsp, tcw);
                            for(n = 0; n < 3; n++) {
                                const int vx = x0 + cx[n], vy = y0 + cy[n];
                                ta_vertex3(f32b_int(vx), f32b_int(vy), front, f32b_scaled(vx * fm->scale, -8), f32b_scaled(vy * fm->scale, -8),
                                           0xffc0c0c0u, 0x00202020u, n == 2);
                            }
                        }
                        ta_list_begin(ListType_Opaque_Modifier_Volume);
                        for(t = 0; t < 64; t++) {
                            const int x0 = (t / SETUP_TILES_Y) * 32, y0 = (t % SETUP_TILES_Y) * 32;
                            if(Ls[li])
                                for(n = 0; n < 15; n++)
                                    sheet(VolumeMode_Inside_Last, x0 + 2 * n + 1, y0 + Bs[bi], 1, Ls[li]);
                        }
                        measure("spanpos", Bs[bi], Ls[li], ps, 0);
                    }
        }
        /* spanq P L pass: the cut lines at 20 .. 20 + L - 1 as in spanpos, and lines 0-19 cut into P runs of 32 / P
           pixels (P 1, 2, 4; volumes over columns 16-31, or 8-15 and 24-31). Does the hidden part depend on how long
           the runs queued before the cut lines are (a queue of runs) or not (a queue of pixels)? */
        {
            static const int Ps[3] = { 1, 2, 4 };
            static const int Ls[4] = { 0, 4, 6, 8 };
            int pi, li, ps;
            for(ps = 0; ps < 2; ps++)
                for(pi = 0; pi < 3; pi++)
                    for(li = 0; li < 4; li++) {
                        const format_t *fm = &formats[3];
                        tiles_region(64);
                        region_addr = MANY_TILES;
                        ta_list_begin(ListType_Opaque);
                        for(t = 0; t < 64; t++) {
                            const int x0 = (t / SETUP_TILES_Y) * 32, y0 = (t % SETUP_TILES_Y) * 32;
                            static const int cx[3] = { 0, 64, 0 }, cy[3] = { 0, 0, 64 };
                            PCW pcw; ISP_TSP isp; TSP tsp; TCW tcw;
                            header(fm, DepthMode_Always, &pcw, &isp, &tsp, &tcw);
                            pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
                            pcw.Group_En = 1;
                            pcw.Strip_Len = 1;
                            ta_polygon(pcw, isp, tsp, tcw);
                            for(n = 0; n < 3; n++) {
                                const int vx = x0 + cx[n], vy = y0 + cy[n];
                                ta_vertex3(f32b_int(vx), f32b_int(vy), front, f32b_scaled(vx * fm->scale, -8), f32b_scaled(vy * fm->scale, -8),
                                           0xffc0c0c0u, 0x00202020u, n == 2);
                            }
                        }
                        ta_list_begin(ListType_Opaque_Modifier_Volume);
                        for(t = 0; t < 64; t++) {
                            const int x0 = (t / SETUP_TILES_Y) * 32, y0 = (t % SETUP_TILES_Y) * 32;
                            if(Ps[pi] == 2)
                                sheet(VolumeMode_Inside_Last, x0 + 16, y0, 16, 20);
                            if(Ps[pi] == 4) {
                                sheet(VolumeMode_Inside_Last, x0 + 8, y0, 8, 20);
                                sheet(VolumeMode_Inside_Last, x0 + 24, y0, 8, 20);
                            }
                            if(Ls[li])
                                for(n = 0; n < 15; n++)
                                    sheet(VolumeMode_Inside_Last, x0 + 2 * n + 1, y0 + 20, 1, Ls[li]);
                        }
                        measure("spanq", Ps[pi], Ls[li], ps, 0);
                    }
        }
        shadow_bit = 0; shad_scale = 1;
        *fb_w_ctrl = fbw;
        region_addr = ONE_TILE;
    }
#ifdef TM_SPAN
    *fb_r_ctrl = fb_r;
    printf("T end\n");
    return;
#endif

    /* ---- ahead a N d: how many primitives can the TSP set up ahead of its pixels? First a block keeps the pixels
       busy: a 0: an untextured sprite over lines 0-23 (about 780 clocks of pixels); 1: the same in 565 with
       texels 8 apart (about 2500); 2: an untextured sprite over lines 16-23 only (about 260). Then on line 24 N
       untextured sprites of one pixel at x = 0 .. N - 1: N first runs at once, each needing its setup; the rest
       of the tile is the background. Setups done during the block cost nothing there; each beyond what the
       setup can do ahead costs a setup. d: DCalcCtrl of the N sprites (5 clocks more setup each): the time it
       adds is 5 for every sprite whose setup was not done ahead. */
    {
        int pass, nn;
        for(pass = 0; pass < 2; pass++)
            for(i = 0; i < 3; i++)
                for(nn = 0; nn <= 32; nn++)
                    for(p = 0; p < 2; p++) {
                        begin(ListType_Opaque);
                        dcalc_ctrl = 0;
                        if(i == 2)
                            rect(&formats[0], 1, DepthMode_Always, 0, 16, 32, 8, front, 0, 0);
                        else
                            rect(&formats[i ? 23 : 0], 1, DepthMode_Always, 0, 0, 32, 24, front, 0, 0);
                        dcalc_ctrl = p;
                        for(k = 0; k < nn; k++)
                            rect(&formats[0], 1, DepthMode_Always, k, 24, 1, 1, front, k + 1, 0);
                        dcalc_ctrl = 0;
                        measure("ahead", i, nn, p, pass);
                    }
    }
    /* ---- ahead2 a N d: the same in each of the first 64 tiles, 16 bit framebuffer, so that a difference of 5
       clocks a sprite becomes 320 for the render. The block is 565 with texels 8 apart (the TSP sets the pace):
       a 0: lines 0-23 of the tile (about 2500 clocks of pixels); a 1: lines 16-23 (about 840, the background
       above it); a 2: lines 20-23 (about 420). N sprites of one pixel on line 24, DCalcCtrl d. */
    {
        volatile uint32_t *fb_w_ctrl = (volatile uint32_t *)(0xa05f8000 + FB_W_CTRL_addr);
        const uint32_t fbw = *fb_w_ctrl;
        static const int top[3] = { 0, 16, 20 };
        int nn, t;
        *fb_w_ctrl = (fbw & ~15u) | 1u;
        for(i = 0; i < 3; i++)
            for(nn = 0; nn <= 32; nn++)
                for(p = 0; p < 2; p++) {
                    tiles_region(64);
                    region_addr = MANY_TILES;
                    ta_list_begin(ListType_Opaque);
                    for(t = 0; t < 64; t++) {
                        const int x0 = (t / SETUP_TILES_Y) * 32, y0 = (t % SETUP_TILES_Y) * 32;
                        dcalc_ctrl = 0;
                        rect(&formats[23], 1, DepthMode_Always, x0, y0 + top[i], 32, 24 - top[i], front, 0, 0);
                        dcalc_ctrl = p;
                        for(k = 0; k < nn; k++)
                            rect(&formats[0], 1, DepthMode_Always, x0 + k, y0 + 24, 1, 1, front, k + 1, 0);
                    }
                    dcalc_ctrl = 0;
                    measure("ahead2", i, nn, p, 0);
                }
        *fb_w_ctrl = fbw;
        region_addr = ONE_TILE;
    }
#ifdef TM_AHEAD
    *fb_r_ctrl = fb_r;
    printf("T end\n");
    return;
#endif

    /* ---- dcalc f L d: DCalcCtrl d (0: the mipmap level's numerators are taken at the screen's origin, 1: at the
       first vertex) for format f in layout L, seen (dcalc) and its twin behind the background (dcalcH). The
       numerators are the polygon's: in theory the bit costs setup time only, if any. Formats: untextured and
       565 without mipmaps (the bit means nothing), 565 mipmapped point, bilinear and trilinear A. Layouts from
       one quad over the tile to 1024 quads of one pixel. Two passes, to see how well the times repeat. */
    {
        static const int df[5] = { 1, 3, 7, 8, 9 };
        static const int dl[5] = { 2, 9, 10, 11, 5 };
        int pass;
        for(pass = 0; pass < 2; pass++)
            for(i = 0; i < 5; i++)
                for(k = 0; k < 5; k++)
                    for(p = 0; p < 2; p++)
                        for(c = 0; c < 2; c++) {
                            dcalc_ctrl = p;
                            vis = c ? DepthMode_Greater : DepthMode_Always; bg_depth = c ? f32b_int(2) : 0;
                            cur = &formats[df[i]]; cur_own = 0;
                            begin(formats[df[i]].list);
                            layout(dl[k]);
                            measure(c ? "dcalcH" : "dcalc", df[i], dl[k], p, pass);
                        }
        dcalc_ctrl = 0; vis = DepthMode_Always; bg_depth = 0;
    }

    /* ---- dcalcs f (100 w + h) d: the same for sprites of w x h covering the tile, untextured (f 0) and 565 (f 2),
       whose times are known from the sprites table: setup-bound ones should pay the setup's extra in full,
       ones whose runs take a little longer than the setup only part of it, and the rest nothing. */
    {
        static const int sh[13][2] = { { 1, 1 }, { 2, 2 }, { 4, 4 }, { 8, 2 }, { 16, 1 }, { 16, 2 }, { 32, 1 }, { 8, 4 },
                                       { 2, 8 }, { 4, 8 }, { 1, 8 }, { 8, 8 }, { 32, 2 } };
        static const int sf[2] = { 0, 2 };
        int pass;
        for(pass = 0; pass < 2; pass++)
            for(i = 0; i < 2; i++)
                for(k = 0; k < 13; k++)
                    for(p = 0; p < 2; p++)
                        for(c = 0; c < 2; c++) {
                            dcalc_ctrl = p;
                            vis = c ? DepthMode_Greater : DepthMode_Always; bg_depth = c ? f32b_int(2) : 0;
                            cur = &formats[sf[i]]; cur_own = 0;
                            begin(ListType_Opaque);
                            tiles(1, sh[k][0], sh[k][1]);
                            measure(c ? "dcalcsH" : "dcalcs", sf[i], sh[k][0] * 100 + sh[k][1], p, pass);
                        }
        dcalc_ctrl = 0; vis = DepthMode_Always; bg_depth = 0;
    }

    /* ---- dcalct f (10 n + q) d: latency. The first n tiles of the screen (n 1 or 64), each covered by one
       primitive of its own: q 1 a sprite, q 2 a quad (two triangles), in format f, 16 bit framebuffer (RGB565)
       so that a tile is bound by the TSP, not by its write-out. Nothing in a tile is shaded before that
       primitive's first pixel: if the first pixel waits for the whole setup, DCalcCtrl 1 costs 5 a tile; if it
       starts as soon as the planes it needs are there, an untextured primitive does not wait for the bit's work. */
    {
        volatile uint32_t *fb_w_ctrl = (volatile uint32_t *)(0xa05f8000 + FB_W_CTRL_addr);
        const uint32_t fbw = *fb_w_ctrl;
        static const int tf[4] = { 0, 2, 7, 8 };
        static const int tn[2] = { 1, 64 };
        int pass, q, t;
        *fb_w_ctrl = (fbw & ~15u) | 1u;
        for(pass = 0; pass < 2; pass++)
            for(i = 0; i < 4; i++)
                for(q = 1; q <= 2; q++)
                    for(k = 0; k < 2; k++)
                        for(p = 0; p < 2; p++) {
                            dcalc_ctrl = p;
                            tiles_region(tn[k]);
                            region_addr = MANY_TILES;
                            ta_list_begin(ListType_Opaque);
                            for(t = 0; t < tn[k]; t++)
                                rect(&formats[tf[i]], q == 1, DepthMode_Always, (t / SETUP_TILES_Y) * 32, (t % SETUP_TILES_Y) * 32, 32, 32, front, t, 0);
                            measure("dcalct", tf[i], 10 * tn[k] + q, p, pass);
                        }
        *fb_w_ctrl = fbw;
        dcalc_ctrl = 0;
        region_addr = ONE_TILE;
    }
#ifdef TM_DCALC
    *fb_r_ctrl = fb_r;
    printf("T end\n");
    return;
#endif

    /* ---- align2 c (1000 su + 100 sv + 10 ou + ov) d: where the bilinear footprint starts, in x and in y. The
       background polygon's texture stepped su texels a pixel along x and sv along y, offset by ou and ov texels,
       d entries in front that draw nothing. With a step of 2 every pixel's 2x2 footprint starts at an even
       texel for an offset of 0 and at an odd one for 1; with a step of 4 it is the other way round.
       c: 0 YUV422 bilinear, 1 565 bilinear, 2 YUV422 point, 3 palette 8 bpp bilinear with ARGB8888 entries,
       4 the same with RGB565 entries, 5 palette 8 bpp point with ARGB8888 entries, 6 with RGB565 entries. */
    {
        static const struct { int fmt, pal; } cf[7] = { { 20, 3 }, { 6, 3 }, { 13, 3 }, { 21, 3 }, { 21, 1 }, { 16, 3 }, { 16, 1 } };
        static const int st[13][4] = { { 1, 1, 0, 0 }, { 2, 2, 0, 0 }, { 2, 2, 1, 0 }, { 2, 2, 0, 1 }, { 2, 2, 1, 1 }, { 4, 4, 0, 0 }, { 4, 4, 1, 0 },
                                       { 4, 4, 0, 1 }, { 4, 4, 1, 1 }, { 2, 1, 0, 0 }, { 2, 1, 1, 0 }, { 4, 1, 0, 0 }, { 4, 1, 1, 0 } };
        for(i = 0; i < 7; i++)
            for(v = 0; v < 13; v++)
                for(k = 2; k <= 4; k += 2) {
                    bg_fmt = &formats[cf[i].fmt];
                    bg_ustep = st[v][0]; bg_vstep = st[v][1]; bg_uofs = st[v][2]; bg_vofs = st[v][3];
                    one_tile_region(k);
                    region_addr = ONE_VAR;
                    ta_list_begin(ListType_Opaque);
                    pvrio_reg(PAL_RAM_CTRL_addr, (uint32_t)cf[i].pal);
                    measure("align2", i, st[v][0] * 1000 + st[v][1] * 100 + st[v][2] * 10 + st[v][3], k, 0);
                    pvrio_reg(PAL_RAM_CTRL_addr, 3);
                }
        bg_fmt = NULL; bg_ustep = bg_vstep = 1; bg_uofs = bg_vofs = 0;
        region_addr = ONE_TILE;
    }
#ifdef TM_QUICK
    *fb_r_ctrl = fb_r;
    printf("T end\n");
    return;
#endif

    /* ---- The background polygon. It is always there, and the TSP shades it wherever nothing covers it: 32 runs
       of 32 pixels in an empty tile. And an entry of the region array that draws nothing and is not written out
       (KallistiOS puts one first) is a tile like any other to the ISP and the TSP: its background is shaded too.
       bgonly f d: nothing in the lists, background of format f, d such entries in front of the tile's own.
       bgcover f d: the same with one untextured sprite over the tile (the background is not seen there). */
    {
        static const int dummies[4] = { 0, 1, 2, 4 };
        for(v = 0; v < 2; v++)
            for(i = 0; i < NUM_FORMATS; i++)
                for(k = 0; k < 4; k++) {
                    if(v && i != 0 && i != 2 && i != 16 && i != 23)
                        continue;
                    bg_fmt = &formats[i];
                    one_tile_region(dummies[k]);
                    region_addr = ONE_VAR;
                    ta_list_begin(ListType_Opaque);
                    if(v)
                        rect(&formats[0], 1, DepthMode_Always, 0, 0, 32, 32, front, 0, 0);
                    measure(v ? "bgcover" : "bgonly", i, dummies[k], 0, 0);
                }
        bg_fmt = NULL;
    }

    /* ---- The palette's entry format. PAL_RAM_CTRL: 0 ARGB1555, 1 RGB565, 2 ARGB4444, 3 ARGB8888 (what every
       other scene has). palfmt m f d: entry format m, the background polygon in format f, d entries in front
       of the tile's that draw nothing: the TSP's time per entry is (d 4 - d 2) / 2. */
    {
        static const int pf[7] = { 15, 16, 21, 44, 42, 46, 3 };
        int m;
        for(m = 0; m < 4; m++)
            for(i = 0; i < 7; i++)
                for(k = 2; k <= 4; k += 2) {
                    bg_fmt = &formats[pf[i]];
                    one_tile_region(k);
                    region_addr = ONE_VAR;
                    ta_list_begin(ListType_Opaque);
                    pvrio_reg(PAL_RAM_CTRL_addr, (uint32_t)m);
                    measure("palfmt", m, pf[i], k, 0);
                    pvrio_reg(PAL_RAM_CTRL_addr, 3);
                }
        bg_fmt = NULL;
        region_addr = ONE_TILE;
    }

    /* ---- Where the bilinear footprint starts. The background polygon's texture with s texels per pixel along x
       (1 along y) and o texels of offset: with s = 2 every pixel's 2x2 footprint starts at an even x for o = 0
       and at an odd x for o = 1 (s = 4: odd, even, odd, even for o = 0 .. 3). In a YUV422 texture the two
       texels of a row share their U and V when the footprint starts at an even x, and belong to two pairs
       when it starts at an odd one. align f (10 s + o) d: format f, d entries in front that draw nothing. */
    {
        static const int af[7] = { 20, 13, 6, 31, 32, 3, 40 };
        static const int so[7][2] = { { 1, 0 }, { 2, 0 }, { 2, 1 }, { 4, 0 }, { 4, 1 }, { 4, 2 }, { 4, 3 } };
        for(i = 0; i < 7; i++)
            for(v = 0; v < 7; v++)
                for(k = 2; k <= 4; k += 2) {
                    bg_fmt = &formats[af[i]];
                    bg_ustep = so[v][0]; bg_uofs = so[v][1];
                    one_tile_region(k);
                    region_addr = ONE_VAR;
                    ta_list_begin(ListType_Opaque);
                    measure("align", af[i], so[v][0] * 10 + so[v][1], k, 0);
                }
        bg_fmt = NULL; bg_ustep = 1; bg_uofs = 0;
        region_addr = ONE_TILE;
    }

    /* ---- The write-out. Background only.
       wout m n: framebuffer pack mode m (FB_W_CTRL; 8 + m: with dither), n = 1: the tile alone; n = 8: the first
                 eight tiles of the screen after an entry that draws nothing.
       clip k:   the tile alone, the framebuffer clip leaving 0: all of it; 1: 16 lines; 2: 16 columns; 3: 16 x 16;
                 4: nothing.
       lists n:  the tile alone, with pointers to the first n of its five lists (opaque, opaque modifier,
                 translucent, translucent modifier, punch through) and the others marked empty. */
    {
        volatile uint32_t *fb_w_ctrl = (volatile uint32_t *)(0xa05f8000 + FB_W_CTRL_addr);
        const uint32_t fbw = (uint32_t)pvr_cfg.fb_packmode | (uint32_t)(pvr_cfg.fb_dither ? 8 : 0);
        static const uint32_t clips[5][2] = { { 0, 0 }, { 639u << 16, 15u << 16 }, { 15u << 16, 479u << 16 }, { 15u << 16, 15u << 16 },
                                              { 100u | 200u << 16, 479u << 16 } };
        for(i = 0; i < 16; i++) {
            if((i & 7) == 7)
                continue;
            for(k = 0; k < 2; k++) {
                ta_list_begin(ListType_Opaque);
                if(k) { tiles_region(8); region_addr = MANY_TILES; }
                else { one_tile_region(0); region_addr = ONE_VAR; }
                *fb_w_ctrl = (fbw & ~15u) | (uint32_t)i;
                measure("wout", i, k ? 8 : 1, 0, 0);
                *fb_w_ctrl = fbw;
            }
        }
        for(i = 0; i < 5; i++) {
            clip_x = clips[i][0]; clip_y = clips[i][1];
            one_tile_region(0); region_addr = ONE_VAR;
            ta_list_begin(ListType_Opaque);
            measure("clip", i, 0, 0, 0);
        }
        clip_x = clip_y = 0;
        for(i = 0; i <= 5; i++) {
            lists_present = i;
            one_tile_region(0); region_addr = ONE_VAR;
            ta_list_begin(ListType_Opaque);
            measure("lists", i, 0, 0, 0);
        }
        lists_present = 5;
        region_addr = ONE_TILE;
    }

    /* ---- Modifier volumes: the TSP's work without the ISP's. The polygons and the background have the shadow
       bit (cheap shadow), and one sheet of an opaque modifier volume lies in front of everything. Closed as
       INSIDE_LAST the pixels under it are in the volume and are shaded again; closed as OUTSIDE_LAST nothing
       is. The ISP's work is the same in both, so the difference is the TSP's for those pixels: making the
       runs, sorting them, shading.
         mv f L m       format f in layout L; m 0: no volume, 1: inside, 2: outside; the sheet covers the tile
         mvshape v w h m  the sheet covers w x h at the tile's corner. v 0: background only; 1: an untextured
                        sprite over the tile; 2: a 565 sprite; 3: 1024 untextured sprites of 1x1
         mvn v n m      n such volumes over the tile, one after the other. v 0: background only; 1: a sprite
         mvflags v m    v 0: a sprite over the tile WITHOUT the shadow bit; 1: the background without it
         mvtri v m      the volume is one triangle over the tile instead of two. v as mvn */
    {
        static const int mv_formats[6] = { 0, 2, 4, 3, 16, 23 };
        static const int mv_layouts[11] = { 0, 1, 2, 8, 5, 4, 3, 10, 12, 7, 16 };
        static const int shapes[16][2] = { { 32, 32 }, { 32, 16 }, { 32, 8 }, { 32, 4 }, { 32, 2 }, { 32, 1 }, { 16, 32 }, { 8, 32 },
                                           { 4, 32 }, { 2, 32 }, { 1, 32 }, { 16, 16 }, { 8, 8 }, { 4, 4 }, { 2, 2 }, { 1, 1 } };
        int m;
        shad_scale = 0x180; shadow_bit = 1; bg_shadow = 1;
        vis = DepthMode_Always; cur_own = 0;
        one_tile_region(0); region_addr = ONE_VAR;
        for(f = 0; f < 6; f++)
            for(i = 0; i < 11; i++) {
                L = mv_layouts[i];
                if(layout_has_sprites(L) && formats[mv_formats[f]].gouraud)
                    continue;
                for(m = 0; m < 3; m++) {
                    cur = &formats[mv_formats[f]];
                    ta_list_begin(ListType_Opaque);
                    layout(L);
                    if(m) {
                        ta_list_begin(ListType_Opaque_Modifier_Volume);
                        sheet(m == 1 ? VolumeMode_Inside_Last : VolumeMode_Outside_Last, 0, 0, 32, 32);
                    }
                    measure("mv", mv_formats[f], L, m, 0);
                }
            }
        for(v = 0; v < 4; v++)
            for(i = 0; i < 16; i++)
                for(m = 1; m <= 2; m++) {
                    cur = &formats[v == 2 ? 2 : 0];
                    ta_list_begin(ListType_Opaque);
                    if(v)
                        layout(v == 3 ? 8 : 1);
                    ta_list_begin(ListType_Opaque_Modifier_Volume);
                    sheet(m == 1 ? VolumeMode_Inside_Last : VolumeMode_Outside_Last, 0, 0, shapes[i][0], shapes[i][1]);
                    measure("mvshape", v, shapes[i][0], shapes[i][1], m);
                }
        cur = &formats[0];
        for(v = 0; v < 2; v++)
            for(i = 0; i < 4; i++)
                for(m = 1; m <= 2; m++) {
                    ta_list_begin(ListType_Opaque);
                    if(v)
                        layout(1);
                    ta_list_begin(ListType_Opaque_Modifier_Volume);
                    for(k = 0; k < (1 << i); k++)
                        sheet(m == 1 ? VolumeMode_Inside_Last : VolumeMode_Outside_Last, 0, 0, 32, 32);
                    measure("mvn", v, 1 << i, m, 0);
                }
        for(v = 0; v < 2; v++)
            for(m = 1; m <= 2; m++) {
                shadow_bit = v != 0; bg_shadow = v != 1;
                ta_list_begin(ListType_Opaque);
                if(v == 0)
                    layout(1);
                ta_list_begin(ListType_Opaque_Modifier_Volume);
                sheet(m == 1 ? VolumeMode_Inside_Last : VolumeMode_Outside_Last, 0, 0, 32, 32);
                measure("mvflags", v, m, 0, 0);
            }
        shadow_bit = 1; bg_shadow = 1;
        for(v = 0; v < 2; v++)
            for(m = 1; m <= 2; m++) {
                ta_list_begin(ListType_Opaque);
                if(v)
                    layout(1);
                ta_list_begin(ListType_Opaque_Modifier_Volume);
                sheet(m == 1 ? VolumeMode_Inside_Last : VolumeMode_Outside_Last, 0, 0, 0, 0);
                measure("mvtri", v, m, 0, 0);
            }
        /* the same with polygons "with two volumes" (intensity mode off): inside the volume they are shaded with
           their second set of parameters. mv2 f L m and mv2shape v w h m (v 0: an untextured quad over the
           tile; 1: a 565 Gouraud one). */
        {
            static const int f2[3] = { 0, 3, 4 };
            static const int l2[6] = { 2, 5, 4, 3, 10, 12 };
            shad_scale = 0x080; shadow_bit = 1; bg_shadow = 0; two_volumes = 1;
            for(f = 0; f < 3; f++)
                for(i = 0; i < 6; i++)
                    for(m = 0; m < 3; m++) {
                        cur = &formats[f2[f]];
                        ta_list_begin(ListType_Opaque);
                        layout(l2[i]);
                        if(m) {
                            ta_list_begin(ListType_Opaque_Modifier_Volume);
                            sheet(m == 1 ? VolumeMode_Inside_Last : VolumeMode_Outside_Last, 0, 0, 32, 32);
                        }
                        measure("mv2", f2[f], l2[i], m, 0);
                    }
            for(v = 0; v < 2; v++)
                for(i = 0; i < 16; i++)
                    for(m = 1; m <= 2; m++) {
                        cur = &formats[v ? 3 : 0];
                        ta_list_begin(ListType_Opaque);
                        layout(2);
                        ta_list_begin(ListType_Opaque_Modifier_Volume);
                        sheet(m == 1 ? VolumeMode_Inside_Last : VolumeMode_Outside_Last, 0, 0, shapes[i][0], shapes[i][1]);
                        measure("mv2shape", v, shapes[i][0], shapes[i][1], m);
                    }
            /* The second volume blends differently from the first. If the pixels in the volume are shaded
               twice (first volume, then the second on top), ONE + ONE gives 0x40 + 0x20 = 0x60 and the time
               grows; if the second volume's parameters only take the first's place, it adds to whatever was in
               the tile's buffer. mvb (10 l + b) t L m: l 0 opaque list, 1 punch through, 2 translucent (with the
               translucent modifier volume list); b the second volume's blend (vol1_blend); t 0 untextured, 1 565
               Gouraud; layout L; m as before. */
            {
                static const int lists3[3] = { ListType_Opaque, ListType_Punch_Through, ListType_Translucent };
                static const int lb[2] = { 2, 5 };
                int l, b, t;
                vol_flat = 1;
                for(l = 0; l < 3; l++)
                    for(b = 0; b < 4; b++)
                        for(t = 0; t < 2; t++)
                            for(i = 0; i < 2; i++)
                                for(m = 0; m < 3; m++) {
                                    format_t g = formats[t ? 3 : 0];
                                    g.list = lists3[l];
                                    vol1_blend = b;
                                    vol_m = m;
                                    vol_a = l == 2 ? 0x80000000u : 0xff000000u;
                                    sheet_list = l == 2 ? ListType_Translucent_Modifier_Volume : ListType_Opaque_Modifier_Volume;
                                    cur = &g;
                                    ta_list_begin(g.list);
                                    layout(lb[i]);
                                    if(m) {
                                        ta_list_begin(sheet_list);
                                        sheet(m == 1 ? VolumeMode_Inside_Last : VolumeMode_Outside_Last, 0, 0, 32, 32);
                                    }
                                    measure("mvb", l * 10 + b, t, lb[i], m);
                                    cur = &formats[0];
                                }
                vol_flat = 0; vol1_blend = 0; vol_m = 0; sheet_list = ListType_Opaque_Modifier_Volume;
            }
            /* Punch through polygons with two volumes ARE shaded twice inside an opaque modifier volume (first
               volume, then the second on top). So there "inside - outside" is the TSP's work for those pixels
               and nothing else: the ISP's work is the same.
                 mvp f L m       format f as punch through polygons with two volumes (the second like the first),
                                 layout L, sheet over the tile
                 mvpshape v w h m  one such quad over the tile (v 0 untextured, 1 565 Gouraud), sheet of w x h */
            {
                static const int fp[6] = { 0, 3, 4, 6, 16, 23 };
                static const int lp[6] = { 2, 12, 10, 3, 4, 5 };
                for(f = 0; f < 6; f++)
                    for(i = 0; i < 6; i++)
                        for(m = 0; m < 3; m++) {
                            format_t g = formats[fp[f]];
                            g.list = ListType_Punch_Through;
                            cur = &g;
                            ta_list_begin(g.list);
                            layout(lp[i]);
                            if(m) {
                                ta_list_begin(ListType_Opaque_Modifier_Volume);
                                sheet(m == 1 ? VolumeMode_Inside_Last : VolumeMode_Outside_Last, 0, 0, 32, 32);
                            }
                            measure("mvp", fp[f], lp[i], m, 0);
                            cur = &formats[0];
                        }
                for(v = 0; v < 2; v++)
                    for(i = 0; i < 16; i++)
                        for(m = 1; m <= 2; m++) {
                            format_t g = formats[v ? 3 : 0];
                            g.list = ListType_Punch_Through;
                            cur = &g;
                            ta_list_begin(g.list);
                            layout(2);
                            ta_list_begin(ListType_Opaque_Modifier_Volume);
                            sheet(m == 1 ? VolumeMode_Inside_Last : VolumeMode_Outside_Last, 0, 0, shapes[i][0], shapes[i][1]);
                            measure("mvpshape", v, shapes[i][0], shapes[i][1], m);
                            cur = &formats[0];
                        }
            }
            two_volumes = 0;
            /* mvtc t L m: translucent polygons with the shadow bit and cheap shadow, the sheet in the translucent
               modifier volume list. t 0 untextured Gouraud, 1 565 Gouraud. */
            {
                static const int lt[2] = { 2, 5 };
                int t;
                shad_scale = 0x180;
                sheet_list = ListType_Translucent_Modifier_Volume;
                for(t = 0; t < 2; t++)
                    for(i = 0; i < 2; i++)
                        for(m = 0; m < 3; m++) {
                            cur = &formats[t ? 28 : 29];
                            ta_list_begin(ListType_Translucent);
                            layout(lt[i]);
                            if(m) {
                                ta_list_begin(sheet_list);
                                sheet(m == 1 ? VolumeMode_Inside_Last : VolumeMode_Outside_Last, 0, 0, 32, 32);
                            }
                            measure("mvtc", t, lt[i], m, 0);
                            cur = &formats[0];
                        }
                sheet_list = ListType_Opaque_Modifier_Volume;
            }
            /* Translucent modifier volumes. A half transparent quad over the tile (its colour tells the scenes
               apart), a sheet in the translucent modifier volume list.
                 mvtr (10 k + o) p m: k 0 the quad has no shadow bit, 1 shadow bit and cheap shadow, 2 two volumes
                 (second volume 0x202020); o 1: an opaque quad under it (0 the background only);
                 p bit 0: the region array entry's pre-sort bit, bit 1: bit 0 of ISP_FEED_CFG;
                 m 0 no sheet, 1 inside, 2 outside, 3 inside with the sheet BEHIND the translucent quad */
            {
                volatile uint32_t *feed = (volatile uint32_t *)(0xa05f8000 + ISP_FEED_CFG_addr);
                int kk, o, pp;
                sheet_list = ListType_Translucent_Modifier_Volume;
                printf("T feedcfg %08lx" "\n", (unsigned long)*feed);
                /* mvtrs i p m o: a sequence, to see what the tile's behaviour depends on. A half transparent quad
                   with two volumes; p the entry's pre-sort bit; m 1: a sheet in front (inside); o 1: an opaque
                   quad under it. Until here every render of this program was pre-sorted. */
                {
                    static const int seq[10][3] = { { 1, 1, 1 }, { 1, 1, 0 }, { 0, 0, 0 }, { 1, 1, 0 }, { 0, 1, 0 }, { 1, 1, 0 },
                                                    { 1, 0, 0 }, { 1, 1, 0 }, { 1, 1, 1 }, { 0, 1, 1 } };
                    for(i = 0; i < 10; i++) {
                        format_t g = formats[0];
                        g.list = ListType_Translucent;
                        presort = seq[i][0];
                        one_tile_region(0);
                        if(seq[i][2]) {
                            shadow_bit = 0; two_volumes = 0; vol_flat = 0;
                            cur = &formats[0];
                            ta_list_begin(ListType_Opaque);
                            rect(cur, 0, DepthMode_Always, 0, 0, 32, 32, f32b_scaled(1, -1), 0, 0);
                        }
                        shadow_bit = 1; two_volumes = 1; shad_scale = 0x080;
                        vol_flat = 1; vol_m = seq[i][1]; vol_a = 0x80000000u; vol1_blend = 0;
                        cur = &g;
                        ta_list_begin(ListType_Translucent);
                        layout(2);
                        if(seq[i][1]) {
                            ta_list_begin(sheet_list);
                            sheet(VolumeMode_Inside_Last, 0, 0, 32, 32);
                        }
                        measure("mvtrs", i, seq[i][0], seq[i][1], seq[i][2]);
                        cur = &formats[0];
                    }
                    presort = -1; one_tile_region(0);
                }
                /* which of the two things mvtr does differently from the scenes before it matters: variant 0
                   neither, 1 the region array entry written again, 2 ISP_FEED_CFG written (the same value),
                   3 both, 4 the entry written with the pre-sort bit 0; then 5 with the bit 1 again, 6 as 0, and
                   7 with the bit 1 and the sheet behind the quad, 8 as 5 */
                for(o = 0; o < 9; o++)
                    for(m = 0; m < 3; m++) {
                        format_t g = formats[0];
                        g.list = ListType_Translucent;
                        if(o == 1 || o == 3 || o == 6) { presort = -1; one_tile_region(0); }
                        if(o == 4) { presort = 0; one_tile_region(0); }
                        if(o == 5 || o == 7 || o == 8) { presort = 1; one_tile_region(0); }
                        sheet_z = o == 7 ? f32b_scaled(3, -2) : 0;
                        shadow_bit = 1; two_volumes = 1; shad_scale = 0x080;
                        vol_flat = 1; vol_m = m; vol_a = 0x80000000u; vol1_blend = 0;
                        cur = &g;
                        ta_list_begin(ListType_Translucent);
                        layout(2);
                        if(m) {
                            ta_list_begin(sheet_list);
                            sheet(m == 2 ? VolumeMode_Outside_Last : VolumeMode_Inside_Last, 0, 0, 32, 32);
                        }
                        if(o == 2 || o == 3) *feed = 0x00800408u;
                        measure("mvtrv", o, m, 0, 0);
                        cur = &formats[0];
                    }
                for(kk = 0; kk < 3; kk++)
                    for(o = 0; o < 2; o++)
                        for(pp = 0; pp < 4; pp++)
                            for(m = 0; m < 4; m++) {
                                format_t g = formats[0];
                                g.list = ListType_Translucent;
                                presort = pp & 1;
                                one_tile_region(0);
                                if(o) {
                                    shadow_bit = 0; two_volumes = 0; vol_flat = 0;
                                    cur = &formats[0];
                                    ta_list_begin(ListType_Opaque);
                                    rect(cur, 0, DepthMode_Always, 0, 0, 32, 32, f32b_scaled(1, -1), 0, 0);
                                }
                                shadow_bit = kk != 0; two_volumes = kk == 2; shad_scale = kk == 2 ? 0x080 : 0x180;
                                vol_flat = 1; vol_m = m == 3 ? 1 : m; vol_a = 0x80000000u; vol1_blend = 0;
                                sheet_z = m == 3 ? f32b_scaled(3, -2) : 0;
                                cur = &g;
                                ta_list_begin(ListType_Translucent);
                                layout(2);
                                if(m) {
                                    ta_list_begin(sheet_list);
                                    sheet(m == 2 ? VolumeMode_Outside_Last : VolumeMode_Inside_Last, 0, 0, 32, 32);
                                }
                                *feed = 0x00800408u | (uint32_t)(pp >> 1);
                                measure("mvtr", kk * 10 + o, pp, m, 0);
                                *feed = 0x00800408u;
                                cur = &formats[0];
                            }
                presort = -1; one_tile_region(0);
                sheet_z = 0; vol_flat = 0; vol_m = 0; vol_a = 0xff000000u; shadow_bit = 1; two_volumes = 0;
                sheet_list = ListType_Opaque_Modifier_Volume;
            }
            /* mvpc t L m: punch through polygons with the shadow bit and cheap shadow (not two volumes).
               t 0 untextured flat, 1 565 Gouraud. */
            {
                static const int lc[3] = { 1, 2, 5 };
                int t;
                shad_scale = 0x180;
                for(t = 0; t < 2; t++)
                    for(i = t; i < 3; i++)
                        for(m = 0; m < 3; m++) {
                            format_t g = formats[t ? 3 : 0];
                            g.list = ListType_Punch_Through;
                            cur = &g;
                            ta_list_begin(g.list);
                            layout(lc[i]);
                            if(m) {
                                ta_list_begin(ListType_Opaque_Modifier_Volume);
                                sheet(m == 1 ? VolumeMode_Inside_Last : VolumeMode_Outside_Last, 0, 0, 32, 32);
                            }
                            measure("mvpc", t, lc[i], m, 0);
                            cur = &formats[0];
                        }
            }
        }
        shad_scale = 1; shadow_bit = 0; bg_shadow = 0;
        region_addr = ONE_TILE;
    }

    /* ---- The ISP's time without its end-of-render bit: the tile alone in the region array (no entry in front),
       background flat and untextured, and N objects that are not seen. The TSP's work is the background's
       32 runs whatever N is, so the TSP's bit moves by the ISP's time for the objects.
         behind  the background's depth is 1 and the objects are at 0.5 with compare GREATER: they fail against it
         never   the objects have compare NEVER
         pass    the objects pass (ALWAYS) and write depth, and one sprite over the tile is drawn after them
       b = 1: the same with one entry in front that draws nothing (the TSP is busy with that one's background
       while the ISP works on the tile). */
    {
        typedef struct { int fmt, sprite, w, h, strip; } object_t;
        static const object_t objects[13] = {
            { 0, 0, 8, 8, 1 },      /*  0 a triangle */
            { 0, 0, 8, 8, 0 },      /*  1 a quad: a strip of two */
            { 0, 1, 8, 8, 0 },      /*  2 a sprite */
            { 2, 0, 8, 8, 0 },      /*  3 a textured quad */
            { 4, 0, 8, 8, 0 },      /*  4 a textured quad with offset colour */
            { 0, 0, 1, 1, 0 },      /*  5 quads by size: 1x1 */
            { 0, 0, 32, 32, 0 },    /*  6 32x32 */
            { 0, 0, 32, 1, 0 },     /*  7 32x1 */
            { 0, 0, 1, 32, 0 },     /*  8 1x32 */
            { 0, 1, 32, 32, 0 },    /*  9 a sprite 32x32 */
            { 0, 1, 1, 1, 0 },      /* 10 a sprite 1x1 */
            { 0, 0, 4, 8, 6 },      /* 11 a strip of six triangles */
            { 0, 0, 1, 1, 1 },      /* 12 a triangle of half a pixel */
        };
        static const int ns[10] = { 0, 1, 2, 4, 8, 16, 32, 64, 128, 256 };
        static const char *modes[3] = { "behind", "never", "pass" };
        const f32b half = f32b_scaled(1, -1);
        int m, o, b;
        for(b = 0; b < 2; b++)
            for(m = 0; m < 3; m++)
                for(o = 0; o < 13; o++)
                    for(i = 0; i < 10; i++) {
                        const object_t *ob = &objects[o];
                        const int dm = m == 0 ? DepthMode_Greater : m == 1 ? DepthMode_Never : DepthMode_Always;
                        if((m || b) && i != 0 && i != 5 && i != 7 && i != 9)
                            continue;
                        bg_depth = m == 0 ? f32b_int(1) : 0;
                        one_tile_region(b);
                        region_addr = ONE_VAR;
                        ta_list_begin(ListType_Opaque);
                        for(n = 0; n < ns[i]; n++) {
                            if(ob->strip)
                                strip(dm, CullMode_None, ob->strip, 0, 0, ob->w, ob->h, half);
                            else
                                rect(&formats[ob->fmt], ob->sprite, dm, 0, 0, ob->w, ob->h, half, n, 0);
                        }
                        if(m == 2)
                            rect(&formats[0], 1, DepthMode_Always, 0, 0, 32, 32, front, 0, 0);
                        measure(modes[m], o, ns[i], b, 0);
                    }
        bg_depth = 0;
        region_addr = ONE_TILE;
    }

    /* When can the TSP start? Without the 48 quads in front: nothing but the background, one sprite, one quad. */
    for(i = 0; i < 3; i++) {
        cur = &formats[0]; cur_own = 0;
        ta_list_begin(ListType_Opaque);
        layout(i);
        measure("start", i, 0, 0, 0);
    }

    /* More tiles: the first n tiles of the screen, with nothing in them (v 0) or one untextured sprite over all
       of them (v 1). What a tile costs when it is one of many. */
    for(v = 0; v < 2; v++)
        for(i = 0; i < 7; i++) {
            tiles_region(1 << i);
            region_addr = MANY_TILES;
            ta_list_begin(ListType_Opaque);
            if(v)
                rect(&formats[0], 1, DepthMode_Always, 0, 0, 160, 480, front, 0, 0);
            measure("tiles", v, 1 << i, 0, 0);
            region_addr = ONE_TILE;
        }

    /* TSP: every format in a few layouts (sprites have one colour: not for the Gouraud formats) */
    for(f = 0; f < NUM_FORMATS; f++)
        for(i = 0; i < 10; i++) {
            L = tsp_layouts[i];
            if(layout_has_sprites(L) && formats[f].gouraud)
                continue;
            for(c = 0; c < 2; c++) {                /* seen, and the twin that is not */
                vis = c ? DepthMode_Greater : DepthMode_Always; bg_depth = c ? f32b_int(2) : 0;
                cur = &formats[f]; cur_own = 0;
                begin(formats[f].list);
                layout(L);
                measure(c ? "tspH" : "tsp", f, L, 0, 0);
            }
            vis = DepthMode_Always; bg_depth = 0;
        }

    /* The TSP's memory of triangles: a quad over the tile and K columns one pixel wide over it, so that every
       line has K + 1 runs, of the same triangles as the line before. v 0: untextured quads (a column is two
       triangles, one above the other); 1: untextured sprites; 2: 565 Gouraud quads; 3: 565 sprites; 4: 565 sprites
       with offset colour. pad: so many quads that are
       never seen between two columns in the list, which moves the columns' parameters apart in memory. */
    for(v = 0; v < 5; v++)
        for(i = 0; i < 14; i++)
            for(p = 0; p < 5; p++) {
                const format_t *fm = &formats[v == 2 ? 3 : v == 3 ? 2 : v == 4 ? 4 : 0];
                if(p > 0 && ks[i] != 8 && ks[i] != 16)
                    continue;
                for(c = 0; c < 2; c++) {
                    vis = c ? DepthMode_Greater : DepthMode_Always; bg_depth = c ? f32b_int(2) : 0;
                    begin(ListType_Opaque);
                    rect(fm, 0, vis, 0, 0, 32, 32, front, 0, 0);
                    for(k = 0; k < ks[i]; k++) {
                        rect(fm, v == 1 || v >= 3, vis, k, 0, 1, 32, front, k, 0);
                        for(n = 0; n < pads[p]; n++)
                            rect(&formats[0], 0, DepthMode_Never, 0, 0, 1, 1, front, n, 0);
                    }
                    measure(c ? "combH" : "comb", v, ks[i], pads[p], 0);
                }
                vis = DepthMode_Always; bg_depth = 0;
            }

    /* translucent layers: N quads of w x h on top of each other. v 0: untextured Gouraud; 1: 565 Gouraud */
    for(v = 0; v < 2; v++)
        for(s = 0; s < 6; s++)
            for(c = 0; c < 2; c++) {
                const int layers = c ? 12 : 4;
                begin(ListType_Translucent);
                for(n = 0; n < layers; n++)
                    rect(&formats[v ? 28 : 29], 0, DepthMode_Always, 0, 0, blend_sizes[s][0], blend_sizes[s][1], front, n, 0);
                measure("blend", v, blend_sizes[s][0], blend_sizes[s][1], layers);
            }

    /* grids: the tile covered by sprites or quads of w x h, for every w and h that is a power of two. v 0:
       untextured sprites; 1: untextured quads; 2: 565 Gouraud quads; 3: 565 sprites */
    for(v = 0; v < 4; v++)
        for(i = 0; i < 6; i++)
            for(k = 0; k < 6; k++) {
                for(c = 0; c < 2; c++) {
                    vis = c ? DepthMode_Greater : DepthMode_Always; bg_depth = c ? f32b_int(2) : 0;
                    cur = &formats[v == 2 ? 3 : v == 3 ? 2 : 0]; cur_own = 0;
                    begin(ListType_Opaque);
                    tiles(v == 0 || v == 3, 1 << i, 1 << k);
                    measure(c ? "gridH" : "grid", v, 1 << i, 1 << k, 0);
                }
                vis = DepthMode_Always; bg_depth = 0;
            }

    /* every opaque format as translucent layers over the whole tile: 4 and 12 quads on top of each other */
    for(f = 0; f < NUM_FORMATS; f++)
        for(c = 0; c < 2; c++) {
            format_t g = formats[f];
            const int layers = c ? 12 : 4;
            if(g.list != ListType_Opaque)
                continue;
            g.list = ListType_Translucent;
            begin(ListType_Translucent);
            for(n = 0; n < layers; n++)
                rect(&g, 0, DepthMode_Always, 0, 0, 32, 32, front, n, 0);
            measure("layerfmt", f, layers, 0, 0);
        }

    /* Cascades: rectangles drawn over each other, each narrower than the one before, so that a line shows a
       sliver of every one of them: many spans from few polygons, and (centred) two separate runs of each polygon
       on every line.
         kind 0, 1  centred, 32x32, 30x32 .. 2x32 and a 1x32 at column 16: 17 polygons, 32 runs a line (1024 spans);
                    untextured sprites, untextured quads
              2, 3  left edges together, 32x32, 31x32 .. 1x32: 32 polygons, 32 runs a line; sprites, quads
              4, 5  as 1 and 0 with 565 Gouraud quads, 565 sprites
              6     the other way: 32x32, 32x30 .. 32x2 centred: 16 sprites, one run a line (32 spans)
              7, 8  centred in steps of 4 and of 8: 8 sprites with 16 runs a line of 2 pixels; 4 with 8 of 4 pixels
              9 .. 12  the 17 polygons of kinds 0 and 1 turned by a quarter (32x32, 32x30 .. 32x2 and a 32x1 at row 16):
                    the same triangles and pixels, but every line is one polygon. Untextured quads, untextured
                    sprites, 565 Gouraud quads, 565 sprites
              13, 14   as 7 and 8 (steps of 4 and of 8) with 565 sprites
              15 .. 17 as 0, 7 and 8 with 565 sprites with offset colour */
    for(v = 0; v < 18; v++) {
        const format_t *fm = &formats[v == 4 || v == 11 ? 3 : v == 5 || v == 12 || v == 13 || v == 14 ? 2 : v >= 15 ? 4 : 0];
        const int sprite = v == 0 || v == 2 || (v >= 5 && v <= 8) || v == 10 || v >= 12;
        for(c = 0; c < 2; c++) {
        vis = c ? DepthMode_Greater : DepthMode_Always; bg_depth = c ? f32b_int(2) : 0;
        begin(ListType_Opaque);
        if(v == 2 || v == 3) {
            for(k = 0; k < 32; k++)
                rect(fm, sprite, vis, 0, 0, 32 - k, 32, front, k, 0);
        } else if(v == 6) {
            for(k = 0; k < 16; k++)
                rect(fm, sprite, vis, 0, k, 32, 32 - 2 * k, front, k, 0);
        } else if(v >= 9 && v <= 12) {              /* the 17 of kinds 0 and 1 turned by a quarter */
            for(k = 0; k < 16; k++)
                rect(fm, sprite, vis, 0, k, 32, 32 - 2 * k, front, k, 0);
            rect(fm, sprite, vis, 0, 16, 32, 1, front, 16, 0);
        } else {
            const int step = v == 7 || v == 13 || v == 16 ? 2 : v == 8 || v == 14 || v == 17 ? 4 : 1;
            for(k = 0; k < 16; k += step)
                rect(fm, sprite, vis, k, 0, 32 - 2 * k, 32, front, k, 0);
            if(step == 1)
                rect(fm, sprite, vis, 16, 0, 1, 32, front, 16, 0);
        }
        measure(c ? "cascadeH" : "cascade", v, 0, 0, 0);
        }
        vis = DepthMode_Always; bg_depth = 0;
    }

    /* ISP: N quads that never pass the depth test, w x h at (x, y) */
    for(c = 0; c < 2; c++) {
        for(v = 0; v < 2; v++)
            for(i = 0; i < 11; i++) {
                ta_list_begin(ListType_Opaque);
                for(n = 0; n < counts[c]; n++)
                    rect(&formats[0], 0, DepthMode_Never, 0, 0, v ? 32 : 1, heights[i], front, n, 0);
                measure("isp", v ? 32 : 1, heights[i], 0, counts[c]);
            }
        for(i = 0; i < 10; i++) {
            ta_list_begin(ListType_Opaque);
            for(n = 0; n < counts[c]; n++)
                rect(&formats[0], 0, DepthMode_Never, places[i][0], places[i][1], 8, 8, front, n, 0);
            measure("isp", 8, 8, places[i][0] * 64 + places[i][1], counts[c]);
        }
        /* strips of k triangles 4 wide and 8 tall; sprites; textured quads; culled triangles */
        for(k = 1; k <= 6; k++) {
            ta_list_begin(ListType_Opaque);
            for(n = 0; n < counts[c]; n++)
                strip(DepthMode_Never, CullMode_None, k, 0, 0, 4, 8, front);
            measure("strip", k, 0, 0, counts[c]);
        }
        for(k = 0; k < 7; k++) {
            ta_list_begin(ListType_Opaque);
            for(n = 0; n < counts[c]; n++)
                switch(k) {
                case 0: rect(&formats[0], 1, DepthMode_Never, 0, 0, 8, 8, front, n, 0); break;        /* untextured sprite */
                case 1: rect(&formats[2], 1, DepthMode_Never, 0, 0, 8, 8, front, n, 0); break;        /* textured sprite */
                case 2: rect(&formats[2], 0, DepthMode_Never, 0, 0, 8, 8, front, n, 0); break;        /* textured quad */
                case 3: rect(&formats[4], 0, DepthMode_Never, 0, 0, 8, 8, front, n, 0); break;        /* textured quad, offset colour */
                case 4: strip(DepthMode_Always, CullMode_Negative, 1, 0, 0, 8, 8, front); break;      /* one of 4 and 5 is culled, */
                case 5: strip(DepthMode_Always, CullMode_Positive, 1, 0, 0, 8, 8, front); break;      /* the other is drawn */
                default: strip(DepthMode_Always, CullMode_Small, 1, 0, 0, 1, 1, front); break;        /* area 0.5: culled as small */
                }
            measure("prim", k, 0, 0, counts[c]);
        }
    }
    {
        /* Is the background drawn when the tile has no opaque list? Its colour changes from scene to scene
           (0xc0c0c0, 0xc0ffc0, 0xffc0c0), so a tile that was not shaded shows the colour before.
             bgcol v c p: v 0 pointers to all five lists (empty lists); 1 no pointer at all; 2 none to the opaque
             list, the others present; 3 as 2 with a half transparent quad 0x406080 in the translucent list;
             4 as 0 with that quad. c the colour; p the entry's pre-sort bit. */
        {
            static const uint32_t xors[3] = { 0, 0x00003f00u, 0x003f0000u };
            int pp;
            for(pp = 0; pp < 2; pp++)
                for(v = 0; v < 5; v++)
                    for(i = 0; i < 3; i++) {
                        format_t g = formats[0];
                        g.list = ListType_Translucent;
                        lists_present = v == 1 ? 0 : 5;
                        no_opaque = v == 2 || v == 3;
                        presort = pp;
                        one_tile_region(0); region_addr = ONE_VAR;
                        bg_xor = xors[i];
                        ta_list_begin(ListType_Opaque);
                        if(v >= 3) {
                            vol_flat = 1; vol_m = 1; vol_a = 0x80000000u;
                            cur = &g;
                            ta_list_begin(ListType_Translucent);
                            layout(2);
                            vol_flat = 0; vol_m = 0; vol_a = 0xff000000u;
                            cur = &formats[0];
                        }
                        measure("bgcol", v, i, pp, 0);
                    }
            bg_xor = 0; no_opaque = 0; presort = -1; lists_present = 5;
        }
        region_addr = ONE_TILE;
    }
    /* ---- The region array entry's "keep Z" bit: the depth buffer is not cleared and the background polygon is
       not drawn. zk v c: scene v (below), background colour c (0xc0c0c0, 0xc0ffc0, 0xffc0c0).
       Contents: A nothing in the lists; B a white sprite over the tile in the opaque list (z 1, ALWAYS) and a
       quad 0x406080 over the tile among the translucent list's objects (compare GREATER, z 0.5, or 2 in v 8);
       C that quad 16x16 at the corner (ALWAYS, z 1) and nothing in the opaque list. */
    {
        static const struct { zk_entry_t e[5]; int n, content; } zk[15] = {
            { { { 0, 0, 1 } }, 1, 0 },                              /*  0 the tile as usual */
            { { { 1, 0, 1 } }, 1, 0 },                              /*  1 keep Z, five empty lists */
            { { { 1, 0, 0 } }, 1, 0 },                              /*  2 keep Z, no list pointer */
            { { { 0, 1, 0 }, { 0, 0, 1 } }, 2, 0 },                 /*  3 an entry in front that draws nothing */
            { { { 1, 1, 0 }, { 0, 0, 1 } }, 2, 0 },                 /*  4 that entry with keep Z */
            { { { 1, 1, 0 }, { 1, 1, 0 }, { 1, 1, 0 }, { 1, 1, 0 }, { 0, 0, 1 } }, 5, 0 },      /*  5 four of them */
            { { { 0, 0, 2 }, { 1, 0, 3 } }, 2, 1 },                 /*  6 B: sprite, then keep Z and the quad behind it */
            { { { 0, 0, 2 }, { 0, 0, 3 } }, 2, 1 },                 /*  7 B: the same without keep Z */
            { { { 0, 0, 2 }, { 1, 0, 3 } }, 2, 2 },                 /*  8 B: keep Z, the quad in front */
            { { { 0, 1, 2 }, { 1, 0, 3 } }, 2, 1 },                 /*  9 as 6, the first entry not written out */
            { { { 0, 1, 0 }, { 1, 0, 3 } }, 2, 3 },                 /* 10 C: background (not written out), then keep Z and the 16x16 */
            { { { 0, 0, 0 }, { 1, 0, 3 } }, 2, 3 },                 /* 11 C: the same, the first entry written out */
            { { { 1, 0, 3 } }, 1, 3 },                              /* 12 C: keep Z alone */
            { { { 1, 0, 2 } }, 1, 1 },                              /* 13 B: keep Z alone, the sprite */
            { { { 0, 0, 2 } }, 1, 1 },                              /* 14 B: the sprite, as usual */
        };
        static const uint32_t xors[3] = { 0, 0x00003f00u, 0x003f0000u };
        for(v = 0; v < 15; v++)
            for(i = 0; i < 3; i++) {
                const int ct = zk[v].content;
                zk_region(zk[v].e, zk[v].n);
                region_addr = ONE_VAR;
                bg_xor = xors[i];
                ta_list_begin(ListType_Opaque);
                if(ct == 1 || ct == 2)
                    rect(&formats[0], 1, DepthMode_Always, 0, 0, 32, 32, front, 0, 0);
                if(ct) {
                    format_t g = formats[0];
                    g.list = ListType_Translucent;
                    vol_flat = 1; vol_m = 1; vol_a = 0xff000000u;
                    ta_list_begin(ListType_Translucent);
                    if(ct == 3)
                        rect(&g, 0, DepthMode_Always, 0, 0, 16, 16, front, 0, 0);
                    else
                        rect(&g, 0, DepthMode_Greater, 0, 0, 32, 32, ct == 2 ? f32b_int(2) : f32b_scaled(1, -1), 0, 0);
                    vol_flat = 0; vol_m = 0;
                }
                measure("zk", v, i, 0, 0);
            }
        bg_xor = 0;
        one_tile_region(0);
        region_addr = ONE_TILE;
    }
    *fb_r_ctrl = fb_r;
    printf("T end\n");
}
#endif

static void test_page(int page) {
#ifdef _arch_dreamcast
    if(page == 0)
        run_all();
#endif
    cur = &formats[0]; cur_own = 1;
    ta_list_begin(ListType_Opaque);
    layout(page);
}
