/* The depth buffer read out bit by bit (2026-11-18): the depth each pixel of a tile has when the core is done with a
   scene, by depth compare probes and a binary search, one render per step.

   A render: the scene's region array entries for the tile, not written out, and after them a keep Z entry (no Z
   clear, no background: pvr-tests/keepz) at tile (0, 0), whose opaque list has a black quad over that tile, compare
   always and depth write off, and then a probe per pixel at depth d, compare greater, in that pixel's own colour:
   the pixel is not black where d is greater than the depth kept. The probes are in tile (0, 0) because only there
   do triangles exist whose depth the setup computes exactly (zexfil_probes.h, made by pvr-tests/zexfil/gen): a 1x1
   quad elsewhere comes out some steps of the stored depth off what it was given. The keep Z entry at another tile
   than the scene's takes the tile's depths as they are (one depth buffer: refsw3-core; the console: zexfil page 0's
   quads at depths that store exactly, read back).

   Tile (0, 0) as written out is read back (pvrio_read32), and each pixel's next d halves what is left of its
   interval: a binary search over the depths of 19 significant bits (the bit patterns k << 5, k = 0 .. 2^26: the
   positive floats in their order, the infinities and NaNs at the top), 27 renders. The result is the largest d
   that does not pass, the depth kept itself as the ISP stores it (19 significant bits); 0xffffffff when d = 0
   passes already (a depth below +0). The probes are exact for depths of 2^-100 .. 2^100 (gen.cpp checks those;
   outside that the setup's limits turn them to zero or infinity), and are kept in that range: a depth kept below
   2^-100 reads as 0, one above 2^100 as 0x7fffffe0. A pixel that shows another colour than its own probe's (a
   probe the console covers differently than the model) is counted in zexfil_foreign.

   The probes' parameters, their object list and zexfil's region array are written to video memory directly (no
   tile accelerator), in an area of ZEXFIL_AREA bytes that the render does not otherwise use, at or above
   PARAM_BASE's 1 MB page (the object list's parameter offsets are counted from there: a console does not wrap them
   around the 8 MB of video memory, refsw3-core does). Each side searches with its own readbacks: a console with the
   depths the hardware keeps, the offline renderer with its own; so the probes differ where the depths do, and they
   are left out of the PVRIO checksum (pvrio_unhashed), as are the results.

   Use: fill zexfil_cfg (zexfil_use_pvrsetup() does for a test on pvrtest2.h, pvr-tests/dumprender for a game's
   frame), then zexfil_tile() for each tile. The ISP has to sample at pixel centers (HALF_OFFSET bit 0). */
#ifndef ZEXFIL_H
#define ZEXFIL_H

#include "pvrio.h"
#include "zexfil_probes.h"

#define ZEXFIL_STEPS        27
#define ZEXFIL_MAX_ENTRIES  16          /* the scene's region array entries per tile */
#define ZEXFIL_AREA         0x13000u    /* bytes: parameters 64 kB, object list, region array, tile (0, 0) */

static struct {
    uint32_t param_base;                /* the render's PARAM_BASE */
    uint32_t area;                      /* ZEXFIL_AREA bytes of video memory (32 bit area) the render does not use */
    int entry_words;                    /* region array entries of 5 or 6 words (FPU_PARAM_CFG bit 21) */
    uint32_t fb, stride;                /* where tile (0, 0) is written out (0888: 32 bit words) and the line stride
                                           in bytes; render() has the framebuffer registers say so */
    /* the scene's region array entries for tile (x, y), entry_words each, into w (at most ZEXFIL_MAX_ENTRIES); how
       many. zexfil sets no_writeout and clears last_region in them. */
    int (*entries)(int x, int y, uint32_t *w);
    void (*render)(uint32_t region);    /* one render of the scene with REGION_BASE = region; done when it returns */
} zexfil_cfg;

static uint32_t zexfil_lo[1024], zexfil_hi[1024];      /* the search, in units of 2^5 of the bit patterns */
static int zexfil_foreign;                              /* pixels that showed another colour than their probe's */

static uint32_t zexfil_params(void) { return zexfil_cfg.area; }
static uint32_t zexfil_ol(void)     { return zexfil_cfg.area + 0x10000u; }
static uint32_t zexfil_region(void) { return zexfil_cfg.area + 0x11000u + 18 * 4; }    /* after 18 zero words */
static uint32_t zexfil_fb(void)     { return zexfil_cfg.area + 0x12000u; }              /* 32 lines of 128 bytes */

static uint32_t zexfil_colour(int pixel) {
    return 0xff800000u | (uint32_t)pixel;
}

/* the region array: 18 zero words, the entry that draws nothing, the scene's entries of tile (x, y), the keep Z
   entry at tile (0, 0) with the probes */
static void zexfil_region_write(int x, int y) {
    static uint32_t region[18 + 6 * (2 + ZEXFIL_MAX_ENTRIES)];
    const int ew = zexfil_cfg.entry_words;
    int n = 18, i, c;

    memset(region, 0, sizeof(region));
    region[n] = 1u << 28;                               /* not written out, every list empty */
    for(i = 1; i < ew; i++)
        region[n + i] = 0x80000000u;
    n += ew;
    c = zexfil_cfg.entries(x, y, &region[n]);
    for(i = 0; i < c; i++) {
        region[n] = (region[n] & 0x7fffffffu) | 1u << 28;
        n += ew;
    }
    region[n] = 1u << 31 | 1u << 30;                    /* the last entry, keep Z, tile (0, 0) */
    region[n] |= region[n - ew] & (1u << 29);           /* pre_sort as the scene's last entry has it */
    region[n + 1] = zexfil_ol() & 0xfffffcu;            /* opaque */
    for(i = 2; i < ew; i++)
        region[n + i] = 0x80000000u;
    n += ew;
    pvrio_w32(zexfil_region() - 18 * 4, region, n);
}

static uint32_t zexfil_mid(int i) {
    return zexfil_lo[i] + ((zexfil_hi[i] - zexfil_lo[i]) >> 1);
}

/* the probe's depth for a step k: k << 5, but within the probes' exact range: a depth of 0 < d < 2^-100 is probed
   with 2^-100 (so a depth kept below 2^-100 reads as 0), one above 2^100 with 2^100 (so one above reads as
   0x7fffffe0) */
#define ZEXFIL_LOW   0x0d800000u       /* 2^-100 */
#define ZEXFIL_HIGH  0x71800000u       /* 2^100 */
static uint32_t zexfil_depth(uint32_t k) {
    const uint32_t d = k << 5;
    return d && d < ZEXFIL_LOW ? ZEXFIL_LOW : d > ZEXFIL_HIGH ? ZEXFIL_HIGH : d;
}

/* a polygon's parameters (flat, untextured, packed colour: vertices of x, y, z, colour) and its object list
   entry, a strip of 3 or 4 vertices */
static uint32_t zexfil_isp(int depth_mode) {
    ISP_TSP isp = { .full = 0 };
    isp.DepthMode = depth_mode;
    isp.ZWriteDis = 1;
    isp.CullMode = CullMode_None;
    return isp.full;
}
static uint32_t zexfil_tsp(void) {
    TSP tsp = { .full = 0 };
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    return tsp.full;
}
static uint32_t zexfil_entry(uint32_t at, int triangles) {
    const uint32_t offset = ((at - (zexfil_cfg.param_base & 0xf00000u)) & 0x7fffffu) >> 2;
    return offset | 1u << 21 | (triangles == 2 ? 0x30u : 0x20u) << 25;     /* skip 1; triangle 0 (and 1) */
}

/* the probes of the pixels still searched, the black quad before them */
static void zexfil_probes_write(void) {
    static uint32_t p[19 + 15 * 1024], ol[2 + 1024];
    int n = 0, m = 0, k, i, q;

    ol[m++] = zexfil_entry(zexfil_params(), 2);
    p[n++] = zexfil_isp(DepthMode_Always); p[n++] = zexfil_tsp(); p[n++] = 0;
    for(k = 0; k < 4; k++) {
        p[n++] = f32b_int((k & 1) * 32); p[n++] = f32b_int((k >> 1) * 32); p[n++] = F32B_ONE; p[n++] = 0xff000000u;
    }
    for(i = 0; i < 1024; i++) {
        q = zexfil_probe[i].pixel;
        if(zexfil_lo[q] >= zexfil_hi[q])
            continue;
        ol[m++] = zexfil_entry(zexfil_params() + 4u * (uint32_t)n, 1);
        p[n++] = zexfil_isp(DepthMode_Greater); p[n++] = zexfil_tsp(); p[n++] = 0;
        for(k = 0; k < 3; k++) {
            p[n++] = zexfil_probe[i].x[k]; p[n++] = zexfil_probe[i].y[k]; p[n++] = zexfil_depth(zexfil_mid(q));
            p[n++] = zexfil_colour(q);
        }
    }
    ol[m++] = 0xf0000000u;                              /* the end */
    pvrio_unhashed = 1;
    pvrio_w32(zexfil_params(), p, n);
    pvrio_w32(zexfil_ol(), ol, m);
    pvrio_unhashed = 0;
}

/* tile (0, 0) read back: a probe drawn (not the black quad) halves the interval downward, black upward. A probe
   shows its own colour exactly (at a depth of 0, a 1/w of 0, the TSP's colour is anything); any other colour is
   counted in zexfil_foreign. */
static void zexfil_update(void) {
    static uint32_t rb[32 * 640];
    const int stride = (int)zexfil_cfg.stride / 4;
    uint32_t c;
    int i;

    pvrio_read32(zexfil_cfg.fb, 31 * stride + 32, rb);
    for(i = 0; i < 1024; i++) {
        if(zexfil_lo[i] >= zexfil_hi[i])
            continue;
        c = rb[(i / 32) * stride + i % 32] & 0xffffffu;
        if(c && c != (zexfil_colour(i) & 0xffffffu) && zexfil_mid(i))
            zexfil_foreign++;
        if(c)
            zexfil_hi[i] = zexfil_mid(i);
        else
            zexfil_lo[i] = zexfil_mid(i) + 1;
    }
}

/* the depths of tile (x, y) into z (row by row, 32 wide) */
static void zexfil_tile(int x, int y, uint32_t *z) {
    int i, step;

    for(i = 0; i < 1024; i++) {
        zexfil_lo[i] = 0;
        zexfil_hi[i] = 1u << 26;
    }
    zexfil_region_write(x, y);
    for(step = 0; step < ZEXFIL_STEPS; step++) {
        zexfil_probes_write();
        zexfil_cfg.render(zexfil_region());
        zexfil_update();
    }
    for(i = 0; i < 1024; i++)
        z[i] = (zexfil_hi[i] - 1) << 5 | (zexfil_hi[i] ? 0 : 0x1fu);
}

#ifdef PVRSETUP_H
/* ---- a test on pvrsetup.h: the scene sent by a function, the area given (free video memory) ---- */

static void (*zexfil_scene)(void);

static int zexfil_pvrsetup_entries(int x, int y, uint32_t *w) {
    RegionArrayEntry e;
    ListPointer *p[5] = { &e.opaque, &e.opaque_mod, &e.trans, &e.trans_mod, &e.puncht };
    uint32_t bytes;
    int i;
    memset(&e, 0, sizeof(e));
    e.control.tilex = x;
    e.control.tiley = y;
    e.control.pre_sort = pvr_cfg.pre_sort;
    for(i = 0; i < 5; i++) {
        bytes = (uint32_t)pvr_cfg.opb_words[i] * 4;
        if(bytes)
            p[i]->ptr_in_words = (pvr_list_base[i] + bytes * (uint32_t)(SETUP_TILES_X * y + x)) >> 2;
        else
            p[i]->empty = 1;
    }
    memcpy(w, &e, sizeof(e));
    return 1;
}

static void zexfil_pvrsetup_render(uint32_t region) {
    pvr_frame_begin();
    zexfil_scene();
    pvr_frame_lists();
    pvr_frame_start(region);
}

/* scene() sends the lists between pvr_frame_begin and the lists' end, the same every time; area: ZEXFIL_AREA bytes
   of video memory the test does not use */
static void zexfil_use_pvrsetup(void (*scene)(void), uint32_t area) {
    zexfil_scene = scene;
    zexfil_cfg.param_base = SETUP_PARAMS;
    zexfil_cfg.area = area;
    zexfil_cfg.entry_words = 6;
    zexfil_cfg.fb = SETUP_FB;
    zexfil_cfg.stride = 640 * 4;
    zexfil_cfg.entries = zexfil_pvrsetup_entries;
    zexfil_cfg.render = zexfil_pvrsetup_render;
}

/* the depths z of tile (x, y) into the framebuffer at its place, one word per pixel: a page that dumps them */
static void zexfil_show(int x, int y, const uint32_t *z) {
    int j;
    pvrio_unhashed = 1;
    for(j = 0; j < 32; j++)
        pvrio_w32(SETUP_FB + ((uint32_t)(y * 32 + j) * 640 + (uint32_t)x * 32) * 4, z + j * 32, 32);
    pvrio_unhashed = 0;
}
#endif

#endif
