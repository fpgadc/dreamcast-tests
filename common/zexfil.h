/* The depth buffer read out bit by bit (2026-11-18): the depth each pixel of a tile has when the core is done with a
   scene, by depth compare probes and a binary search, one render per step.

   A render: the scene's region array entries for the tile, not written out, and after them a keep Z entry (no Z
   clear, no background: pvr-tests/keepz) at tile (0, 0), whose opaque list has a black quad over that tile, compare
   always and depth write off, and then a probe per pixel at depth d, compare greater, in that pixel's own colour:
   the pixel is not black where d is greater than the depth kept. The probes are in tile (0, 0) because only
   there do triangles exist whose depth the setup computes exactly (zexfil_probes.h, made by pvr-tests/zexfil/gen):
   a 1x1 quad elsewhere comes out some steps of the stored depth off what it was given. The keep Z entry at another
   tile than the scene's takes the tile's depths as they are (one depth buffer: refsw3-core; the console: zexfil
   page 0's quads at depths that store exactly, read back).

   The framebuffer at tile (0, 0) is read back (pvrio_read32), and each pixel's next d halves what is left of its
   interval: a binary search over the depths of 19 significant bits (the bit patterns k << 5, k = 0 .. 2^26: the
   positive floats in their order, the infinities and NaNs at the top), 27 renders. The result is the largest d
   that does not pass, the depth kept itself as the ISP stores it (19 significant bits); 0xffffffff when d = 0
   passes already (a depth below +0). The probes are exact for depths of 2^-100 .. 2^100 (gen.cpp checks those;
   outside that the setup's limits turn them to zero or infinity), and are kept in that range: a depth kept below
   2^-100 reads as 0, one above 2^100 as 0x7fffffe0. A pixel that shows another colour than its own probe's (a
   probe the console covers differently than the model) is counted in zexfil_foreign.

   Each side searches with its own readbacks: a console with the depths the hardware keeps, the offline renderer
   with its own. The probes then differ where the depths do, and they are left out of the PVRIO checksum
   (pvrio_unhashed), as are the results written to the framebuffer.

   The probes are a second pass of the tile accelerator after the scene's lists: TA_LIST_CONT, which keeps
   TA_ISP_CURRENT and the block pointers and takes TA_OL_BASE anew (ta-probe list_cont_base), with the object list
   matrix at the top third of the block pool (below TA_OL_LIMIT: the TA writes nothing past it). Their parameters
   follow the scene's: 61 kB.

   Use: zexfil_run() with a window of tiles and a function that sends the scene's lists (called for every render; it
   has to send the same each time), then zexfil_show() to put the depths into the framebuffer for a dump. The
   framebuffer is 0888 (pvrtest2.h's default), and the ISP samples at pixel centers (HALF_OFFSET bit 0). */
#ifndef ZEXFIL_H
#define ZEXFIL_H

#include "pvrsetup.h"
#include "zexfil_probes.h"

#define ZEXFIL_STEPS        27
#define ZEXFIL_MAX_ENTRIES  4           /* the scene's region array entries per tile */

typedef struct zexfil_window_t {
    int tx, ty, tw, th;                 /* the window, in tiles */
} zexfil_window_t;

/* the scene's region array entries for tile (x, y) into e; how many. zexfil sets no_writeout and clears
   last_region in them. */
typedef int (*zexfil_entries_t)(int x, int y, RegionArrayEntry *e);

static uint32_t zexfil_lo[1024], zexfil_hi[1024];      /* the search, in units of 2^5 of the bit patterns */
static uint32_t zexfil_rb[32 * 640];
static int zexfil_foreign;                              /* pixels that showed another probe's colour */

static uint32_t zexfil_ol(void) {       /* the probes' object list matrix */
    return SETUP_OL + 3 * pvr_ol_bytes;
}

static uint32_t zexfil_region(void) {   /* REGION_BASE of zexfil's region array: after pvr_setup's */
    uint32_t end = pvr_region - 18 * 4 + (18 + 6 * (1 + SETUP_TILES_X * SETUP_TILES_Y)) * 4;
    return ((end + 127) & ~127u) + 18 * 4;
}

static uint32_t zexfil_colour(int pixel) {
    return 0xff800000u | (uint32_t)pixel;
}

/* the entry pvr_region_write gives a tile: the five lists of the scene */
static int zexfil_scene_entries(int x, int y, RegionArrayEntry *e) {
    ListPointer *p[5] = { &e->opaque, &e->opaque_mod, &e->trans, &e->trans_mod, &e->puncht };
    uint32_t bytes;
    int i;
    memset(e, 0, sizeof(*e));
    e->control.tilex = x;
    e->control.tiley = y;
    e->control.pre_sort = pvr_cfg.pre_sort;
    for(i = 0; i < 5; i++) {
        bytes = (uint32_t)pvr_cfg.opb_words[i] * 4;
        if(bytes)
            p[i]->ptr_in_words = (pvr_list_base[i] + bytes * (uint32_t)(SETUP_TILES_X * y + x)) >> 2;
        else
            p[i]->empty = 1;
    }
    return 1;
}

/* the region array: 18 zero words, the entry that draws nothing, the scene's entries of tile (x, y), the keep Z
   entry at tile (0, 0) with the probes */
static void zexfil_region_write(int x, int y, zexfil_entries_t scene) {
    static uint32_t region[18 + 6 * (2 + ZEXFIL_MAX_ENTRIES)];
    RegionArrayEntry e[ZEXFIL_MAX_ENTRIES], k;
    int n = 18, i, c;

    memset(region, 0, sizeof(region));
    memset(&k, 0, sizeof(k));
    k.control.no_writeout = 1;
    k.opaque.empty = k.opaque_mod.empty = k.trans.empty = k.trans_mod.empty = k.puncht.empty = 1;
    memcpy(&region[n], &k, sizeof(k));
    n += 6;
    c = scene(x, y, e);
    for(i = 0; i < c; i++) {
        e[i].control.no_writeout = 1;
        e[i].control.last_region = 0;
        memcpy(&region[n], &e[i], sizeof(e[i]));
        n += 6;
    }
    memset(&k, 0, sizeof(k));
    k.control.z_keep = 1;
    k.control.pre_sort = pvr_cfg.pre_sort;
    k.control.last_region = 1;
    k.opaque.ptr_in_words = (zexfil_ol() + (pvr_list_base[ListType_Opaque] - SETUP_OL)) >> 2;      /* tile (0, 0) */
    k.opaque_mod.empty = k.trans.empty = k.trans_mod.empty = k.puncht.empty = 1;
    memcpy(&region[n], &k, sizeof(k));
    n += 6;
    pvrio_w32(zexfil_region() - 18 * 4, region, n);
}

static void zexfil_poly(int depth_mode) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Opaque;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    isp.DepthMode = depth_mode;
    isp.ZWriteDis = 1;
    isp.CullMode = CullMode_None;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    ta_polygon(pcw, isp, tsp, tcw);
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

/* the probe pass: after the scene's lists are done (pvr_frame_lists) */
static void zexfil_probes(void) {
    int n, k, p;

    pvrio_reg(TA_OL_BASE_addr, zexfil_ol());
    pvrio_reg(TA_LIST_CONT_addr, 0x80000000u);
    ta_list_begin(ListType_Opaque);
    zexfil_poly(DepthMode_Always);
    quad0(0, 0, 32, 32, F32B_ONE, 0xff000000u);
    zexfil_poly(DepthMode_Greater);
    pvrio_unhashed = 1;
    for(n = 0; n < 1024; n++) {
        p = zexfil_probe[n].pixel;
        if(zexfil_lo[p] < zexfil_hi[p])
            for(k = 0; k < 3; k++)
                ta_vertex0(zexfil_probe[n].x[k], zexfil_probe[n].y[k], zexfil_depth(zexfil_mid(p)), zexfil_colour(p), k == 2);
    }
    pvrio_unhashed = 0;
    ta_list_end();
    pvrio_wait(PVRIO_LIST_DONE(ListType_Opaque));
}

/* tile (0, 0) read back: a probe drawn (not the black quad) halves the interval downward, black upward. A probe
   shows its own colour exactly (at a depth of 0, a 1/w of 0, the TSP's colour is anything); any other colour is
   counted in zexfil_foreign. */
static void zexfil_update(void) {
    uint32_t c;
    int i;

    pvrio_read32(SETUP_FB, 32 * 640, zexfil_rb);
    for(i = 0; i < 1024; i++) {
        if(zexfil_lo[i] >= zexfil_hi[i])
            continue;
        c = zexfil_rb[(i / 32) * 640 + i % 32] & 0xffffffu;
        if(c && c != (zexfil_colour(i) & 0xffffffu) && zexfil_mid(i))
            zexfil_foreign++;
        if(c)
            zexfil_hi[i] = zexfil_mid(i);
        else
            zexfil_lo[i] = zexfil_mid(i) + 1;
    }
}

/* The depths of tile (x, y) into z (row by row, 32 wide), with the scene that scene() sends between
   pvr_frame_begin and the lists' end and the region array entries that entries() gives (zexfil_scene_entries:
   the five lists of pvrsetup.h). Leaves tile (0, 0) of the framebuffer as the last probes drew it. */
static void zexfil_tile(int x, int y, void (*scene)(void), zexfil_entries_t entries, uint32_t *z) {
    int i, step;

    for(i = 0; i < 1024; i++) {
        zexfil_lo[i] = 0;
        zexfil_hi[i] = 1u << 26;
    }
    zexfil_region_write(x, y, entries);
    for(step = 0; step < ZEXFIL_STEPS; step++) {
        pvr_frame_begin();
        scene();
        pvr_frame_lists();
        zexfil_probes();
        pvr_frame_start(zexfil_region());
        zexfil_update();
    }
    for(i = 0; i < 1024; i++)
        z[i] = (zexfil_hi[i] - 1) << 5 | (zexfil_hi[i] ? 0 : 0x1fu);
}

/* the window, tile by tile, into z (row by row, tw * 32 wide) */
static void zexfil_run_entries(const zexfil_window_t *w, void (*scene)(void), zexfil_entries_t entries, uint32_t *z) {
    static uint32_t t[1024];
    const int W = w->tw * 32;
    int x, y, j;

    for(y = 0; y < w->th; y++)
        for(x = 0; x < w->tw; x++) {
            zexfil_tile(w->tx + x, w->ty + y, scene, entries, t);
            for(j = 0; j < 32; j++)
                memcpy(&z[(y * 32 + j) * W + x * 32], &t[j * 32], 32 * 4);
        }
}

static void zexfil_run(const zexfil_window_t *w, void (*scene)(void), uint32_t *z) {
    zexfil_run_entries(w, scene, zexfil_scene_entries, z);
}

/* the depths z of the window into the framebuffer at its place, one word per pixel: a page that dumps them */
static void zexfil_show(const zexfil_window_t *w, const uint32_t *z) {
    const int x0 = w->tx * 32, y0 = w->ty * 32, W = w->tw * 32, H = w->th * 32;
    int y;
    pvrio_unhashed = 1;
    for(y = 0; y < H; y++)
        pvrio_w32(SETUP_FB + ((uint32_t)(y0 + y) * 640 + (uint32_t)x0) * 4, z + y * W, W);
    pvrio_unhashed = 0;
}

#endif
