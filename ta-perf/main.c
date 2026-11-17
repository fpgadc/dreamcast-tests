/* ta-perf: how long the tile accelerator takes, on valid input only.
   (Timing exists on a console only. A PC run sends each case once to refsw2-offline, whose refta checks that the
   input is valid: no error interrupt, no warning; and the console's hashes of what the TA wrote must be the
   offline run's.)

   Method, as timemeas does it for the core: interrupts and the display are off while a case is timed. The case's
   stream of 32 byte blocks is made in main memory first. After SOFTRESET, the TA's registers and TA_LIST_INIT, the
   CPU's cycle counter is read (200 MHz: 5 ns counts), the blocks go to the TA through the store queues as fast as
   the CPU sends them (it waits whenever the TA's FIFO is full), the counter is read again ("sent", after the last
   block), and SB_ISTNRM is polled for the end of list interrupt ("done"). Each point is timed REPS times; lines

       P <case> <n> <blocks> <items> <sent min> <sent median> <done min> <done median> <errors>

   (counts of 5 ns from the first block; items: what the case counts, n of them unless said). analyze.py fits
   done = a + b items over each case's points. After the last repetition the object lists and parameters are
   hashed (lines "PVRIO hash h<case>_<n>o / p"), on the console and offline alike.

   Baselines without the TA: the store queues to main memory (the CPU alone) and to the direct texture path (the
   TA's FIFO into video memory, without the TA): "done" is when the FIFO is empty again.

   Then every TA case again with the blocks sent by DMA (channel 2, from main memory, as KallistiOS's
   pvr_dma_load_ta does), cases named dma_<case>: "sent" is when the DMA is over. The baseline dma_tex sends to the
   direct texture path. Their hashes (h<case>_<n>o, with dma_ in front) have to be those of the store queue run. */
#include "pvrio.h"
#include <string.h>

#define NUM_PAGES 0
#define REPS 5

#define OL_BASE   0x100000u
#define OL_LIMIT  0x300000u
#define ISP_BASE  0x300000u
#define ISP_LIMIT 0x500000u
#define HASH_OL_WORDS  (0x40000 / 4)        /* the five matrices of 40 x 15 tiles (192 KB) and 64 KB of blocks */
#define HASH_ISP_WORDS (0x10000 / 4)
#define FILL 0xa5a5a5a5u

/* ---- parameter words (as ta-tests) ---- */
enum { P_END = 0, P_CLIP = 1, P_POLY = 4, P_SPRITE = 5, P_VTX = 7 };
#define PCW_(para, list, flags) ((uint32_t)(para) << 29 | (uint32_t)(list) << 24 | (uint32_t)(flags))
#define EOS         (1u << 28)
#define GROUP(len)  (1u << 23 | (uint32_t)(len) << 18)
#define UCLIP(m)    ((uint32_t)(m) << 16)
#define VOLUME      (1u << 6)
#define COL(t)      ((uint32_t)(t) << 4)
#define TEX         (1u << 3)
#define OFFS        (1u << 2)
#define GOUR        (1u << 1)
#define UV16        (1u << 0)
#define OP 0
#define OM 1
#define TR 2
#define TM 3
#define PT 4
#define ISPW 0x800abcdeu
#define TSPW 0x12345678u
#define TCWW 0x9abcdef0u
#define TSP1 0x0fedcba9u
#define TCW1 0x87654321u
#define VP(eos) PCW_(P_VTX, 0, (eos) ? EOS : 0)

#define F1W 0x3f800000u
static uint32_t fl(float f) { union { float f; uint32_t u; } x; x.f = f; return x.u; }

/* ---- the stream ---- */
#define MAXB 20000
static uint32_t buf[MAXB * 8] __attribute__((aligned(32)));
static int nb, items, cur_list;
static void b8(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e, uint32_t f, uint32_t g, uint32_t h) {
    uint32_t *p = &buf[nb++ * 8];
    p[0] = a; p[1] = b; p[2] = c; p[3] = d; p[4] = e; p[5] = f; p[6] = g; p[7] = h;
}
static void b16(const uint32_t *w) {
    b8(w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7]);
    b8(w[8], w[9], w[10], w[11], w[12], w[13], w[14], w[15]);
}
static void eol(void) { b8(0, 0, 0, 0, 0, 0, 0, 0); }

/* vertex formats: the header's flags and a vertex at (x, y) */
typedef struct { const char *name; uint32_t flags; int hdr; int v64; } vfmt_t;
enum { H_PLAIN, H_FACE, H_VOL };
static const vfmt_t vfmts[] = {
    { "packed",    COL(0) | GOUR, H_PLAIN, 0 },
    { "float",     COL(1) | GOUR, H_PLAIN, 0 },
    { "int1",      COL(2) | GOUR, H_FACE, 0 },
    { "tex",       TEX | COL(0) | GOUR, H_PLAIN, 0 },
    { "texuv16",   TEX | UV16 | COL(0) | GOUR, H_PLAIN, 0 },
    { "texoffs",   TEX | OFFS | COL(0) | GOUR, H_PLAIN, 0 },
    { "texfloat",  TEX | OFFS | COL(1) | GOUR, H_PLAIN, 1 },
    { "twovol",    VOLUME | COL(0) | GOUR, H_VOL, 0 },
    { "twovoltex", VOLUME | TEX | OFFS | COL(0) | GOUR, H_VOL, 1 },
};
#define NVF (int)(sizeof(vfmts) / sizeof(vfmts[0]))

static void header(int list, const vfmt_t *f, uint32_t more) {
    const uint32_t pcw = PCW_(P_POLY, list, f->flags | more);
    cur_list = list;
    if(f->hdr == H_FACE)
        b8(pcw, ISPW, TSPW, TCWW, F1W, fl(0.5f), fl(0.25f), fl(0.75f));
    else if(f->hdr == H_VOL)
        b8(pcw, ISPW, TSPW, TCWW, TSP1, TCW1, 0, 0);
    else
        b8(pcw, ISPW, TSPW, TCWW, 0, 0, 0, 0);
}
static void vertex(const vfmt_t *f, float x, float y, int eos, uint32_t k) {
    const uint32_t vp = VP(eos), X = fl(x), Y = fl(y), Z = fl(1.0f + (float)(k & 255) / 256.0f);
    if(f->v64) {
        uint32_t w[16];
        if(f->hdr == H_VOL) {           /* textured, packed, two volumes */
            const uint32_t t[16] = { vp, X, Y, Z, fl(0.25f), fl(0.5f), 0xff102030u + k, 0x00010203u,
                                     fl(0.75f), fl(0.125f), 0xff405060u + k, 0x00040506u, 0, 0, 0, 0 };
            memcpy(w, t, sizeof(w));
        } else {                        /* textured, float colour */
            const uint32_t t[16] = { vp, X, Y, Z, fl(0.25f), fl(0.5f), 0, 0,
                                     F1W, fl(0.5f), fl(0.25f), fl(0.125f), F1W, fl(0.0625f), fl(0.03125f), 0 };
            memcpy(w, t, sizeof(w));
        }
        b16(w);
    } else if(f->hdr == H_VOL) {
        b8(vp, X, Y, Z, 0xff102030u + k, 0xff405060u + k, 0, 0);
    } else if(f->flags & TEX) {
        if(f->flags & UV16)
            b8(vp, X, Y, Z, 0x3c003e00u + k, 0, 0xff102030u + k, 0x00010203u);
        else
            b8(vp, X, Y, Z, fl(0.25f), fl(0.5f), 0xff102030u + k, 0x00010203u);
    } else if((f->flags & COL(3)) == COL(1)) {
        b8(vp, X, Y, Z, F1W, fl(0.5f), fl(0.25f), fl(0.125f));
    } else if((f->flags & COL(3)) == COL(2)) {
        b8(vp, X, Y, Z, 0, 0, fl(0.5f), 0);
    } else {
        b8(vp, X, Y, Z, 0, 0, 0xff102030u + k, 0);
    }
}

/* a triangle inside tile (tx, ty), a little different each time */
static void tri_in(const vfmt_t *f, int tx, int ty, uint32_t k) {
    const float x = 32.0f * (float)tx + 4.0f + (float)(k % 8), y = 32.0f * (float)ty + 4.0f + (float)(k / 8 % 8);
    vertex(f, x, y, 0, k);
    vertex(f, x + 16.0f, y, 0, k);
    vertex(f, x, y + 16.0f, 1, k);
}
/* a quad (strip of four) over w x h tiles from the top left */
static void quad_tiles(const vfmt_t *f, int w, int h, uint32_t k) {
    const float x1 = 32.0f * (float)w - 0.5f, y1 = 32.0f * (float)h - 0.5f;
    vertex(f, 0.5f, 0.5f, 0, k);
    vertex(f, x1, 0.5f, 0, k);
    vertex(f, 0.5f, y1, 0, k);
    vertex(f, x1, y1, 1, k);
}

/* ---- the cases ---- */
typedef struct {
    const char *name;
    void (*gen)(int n, int arg);
    int arg;
    int ns[3];
    int tx, ty;                 /* grid (0: 40 x 15); ty -1: n rows of 40 */
    int opb;                    /* object list block size code for every list (0: 2, 16 words) */
    int display;                /* display on while timed */
    int kind;                   /* 0 the TA; 1 store queues to main memory; 2 the direct texture path; 4 DMA to it */
} pcase_t;

static void g_eol(int n, int arg) { (void)n; (void)arg; header(OP, &vfmts[0], GROUP(0)); eol(); items = n; }
static void g_hdr(int n, int arg) {
    int i;
    (void)arg;
    for(i = 0; i < n; i++)
        header(OP, &vfmts[0], GROUP(0));
    eol();
}
static void g_tri(int n, int fmt) {             /* one header, n triangles in tile (1, 1) */
    int i;
    header(OP, &vfmts[fmt], GROUP(0));
    for(i = 0; i < n; i++)
        tri_in(&vfmts[fmt], 1, 1, (uint32_t)i);
    eol();
}
static void g_tri_hdr(int n, int arg) {         /* a header for each triangle */
    int i;
    (void)arg;
    for(i = 0; i < n; i++) {
        header(OP, &vfmts[0], GROUP(0));
        tri_in(&vfmts[0], 1, 1, (uint32_t)i);
    }
    eol();
}
static void g_nojoin(int n, int arg) {          /* triangles in tiles (1, 1) and (2, 1) by turns: no array joins */
    int i;
    (void)arg;
    header(OP, &vfmts[0], GROUP(0));
    for(i = 0; i < n; i++)
        tri_in(&vfmts[0], 1 + (i & 1), 1, (uint32_t)i);
    eol();
}
static void g_strip(int n, int len) {           /* 64 strips of n vertices in tile (1, 1), Strip_Len len */
    int s, i;
    header(OP, &vfmts[0], GROUP(len));
    for(s = 0; s < 64; s++)
        for(i = 0; i < n; i++)
            vertex(&vfmts[0], 36.0f + (float)(i / 2) * 0.5f + (float)(s % 8), 36.0f + (float)(i & 1) * 16.0f + (float)(s / 8),
                   i == n - 1, (uint32_t)s);
    eol();
    items = 64 * n;
}
static void g_culled(int n, int arg) {          /* 0: off the grid; 1: outside a user clip rectangle (inside mode) */
    int i;
    if(arg)
        b8(PCW_(P_CLIP, 0, 0), 0, 0, 0, 10, 5, 12, 7);
    header(OP, &vfmts[0], GROUP(0) | (arg ? UCLIP(2) : 0));
    for(i = 0; i < n; i++) {
        if(arg)
            tri_in(&vfmts[0], 1, 1, (uint32_t)i);
        else {
            vertex(&vfmts[0], -100.0f, 40.0f, 0, (uint32_t)i);
            vertex(&vfmts[0], -80.0f, 40.0f, 0, (uint32_t)i);
            vertex(&vfmts[0], -100.0f, 60.0f, 1, (uint32_t)i);
        }
    }
    eol();
}
static void g_quad(int n, int size) {           /* n quads over size x size tiles (size 0: the whole 40 x 15) */
    int i;
    header(OP, &vfmts[0], GROUP(1));
    for(i = 0; i < n; i++)
        quad_tiles(&vfmts[0], size ? size : 40, size ? size : 15, (uint32_t)i);
    eol();
    items = n * (size ? size * size : 600);
}
static void g_quad_list(int n, int list) {      /* n quads over the whole grid in another list */
    int i;
    header(list, &vfmts[0], GROUP(1));
    for(i = 0; i < n; i++)
        quad_tiles(&vfmts[0], 40, 15, (uint32_t)i);
    eol();
    items = n * 600;
}
static void g_tri_full(int n, int arg) {        /* n triangles whose bounds are the whole grid: arrays join */
    int i;
    (void)arg;
    header(OP, &vfmts[0], GROUP(0));
    for(i = 0; i < n; i++) {
        vertex(&vfmts[0], 0.5f, 0.5f, 0, (uint32_t)i);
        vertex(&vfmts[0], 1279.0f, 0.5f, 0, (uint32_t)i);
        vertex(&vfmts[0], 0.5f, 479.5f, 1, (uint32_t)i);
    }
    eol();
    items = n * 600;
}
static void g_sprite(int n, int arg) {          /* 0: n sprites in tile (1, 1); 1: textured; 2: over the whole grid */
    int i;
    cur_list = OP;
    b8(PCW_(P_SPRITE, OP, (arg == 1 ? TEX : 0)), ISPW, TSPW, TCWW, 0xff112233u, 0x00445566u, 0, 0);
    for(i = 0; i < n; i++) {
        const float x = arg == 2 ? 0.5f : 36.0f + (float)(i % 8), y = arg == 2 ? 0.5f : 36.0f + (float)(i / 8 % 8);
        const float x1 = arg == 2 ? 639.5f : x + 16.0f, y1 = arg == 2 ? 479.5f : y + 16.0f;
        const uint32_t w[16] = { VP(1), fl(x), fl(y), F1W, fl(x1), fl(y), F1W, fl(x1),
                                 fl(y1), F1W, fl(x), fl(y1), 0, 0x3c003e00u, 0x3e003e00u, 0x3e003c00u };
        b16(w);
    }
    eol();
    if(arg == 2)
        items = n * 600;
}
static void g_modvol(int n, int full) {         /* n modifier volume triangles: in tile (1, 1), or over the whole grid */
    int i;
    cur_list = OM;
    b8(PCW_(P_POLY, OM, 0), 0, 0, 0, 0, 0, 0, 0);
    for(i = 0; i < n; i++) {
        const float x = full ? 0.5f : 36.0f + (float)(i % 8), y = full ? 0.5f : 36.0f + (float)(i / 8 % 8);
        const float x1 = full ? 1279.0f : x + 16.0f, y1 = full ? 959.0f : y + 16.0f;
        const uint32_t w[16] = { VP(1), fl(x), fl(y), F1W, fl(x1), fl(y), F1W, fl(x), fl(y1), F1W, 0, 0, 0, 0, 0, 0 };
        b16(w);
    }
    eol();
    if(full)
        items = n * 600;
}

static void g_none(int n, int arg) {            /* baselines: n blocks of anything */
    int i;
    (void)arg;
    for(i = 0; i < n; i++)
        b8((uint32_t)i, 1, 2, 3, 4, 5, 6, 7);
}

static const pcase_t cases[] = {
    { "sq_ram",        g_none,     0, { 256, 1024, 4096 }, 0, 0, 0, 0, 1 },
    { "sq_tex",        g_none,     0, { 256, 1024, 4096 }, 0, 0, 0, 0, 2 },
    { "dma_tex",       g_none,     0, { 256, 1024, 4096 }, 0, 0, 0, 0, 4 },
    { "eol_rows",      g_eol,      0, { 1, 4, 15 }, 40, -1, 0, 0, 0 },
    { "eol_1x1",       g_eol,      0, { 1, 1, 1 }, 1, 1, 0, 0, 0 },
    { "hdr",           g_hdr,      0, { 64, 256, 1024 }, 0, 0, 0, 0, 0 },
    { "tri_packed",    g_tri,      0, { 64, 256, 1024 }, 0, 0, 0, 0, 0 },
    { "tri_float",     g_tri,      1, { 64, 256, 1024 }, 0, 0, 0, 0, 0 },
    { "tri_int1",      g_tri,      2, { 64, 256, 1024 }, 0, 0, 0, 0, 0 },
    { "tri_tex",       g_tri,      3, { 64, 256, 1024 }, 0, 0, 0, 0, 0 },
    { "tri_texuv16",   g_tri,      4, { 64, 256, 1024 }, 0, 0, 0, 0, 0 },
    { "tri_texoffs",   g_tri,      5, { 64, 256, 1024 }, 0, 0, 0, 0, 0 },
    { "tri_texfloat",  g_tri,      6, { 64, 256, 1024 }, 0, 0, 0, 0, 0 },
    { "tri_twovol",    g_tri,      7, { 64, 256, 1024 }, 0, 0, 0, 0, 0 },
    { "tri_twovoltex", g_tri,      8, { 64, 256, 1024 }, 0, 0, 0, 0, 0 },
    { "tri_hdr",       g_tri_hdr,  0, { 64, 256, 1024 }, 0, 0, 0, 0, 0 },
    { "tri_nojoin",    g_nojoin,   0, { 64, 256, 1024 }, 0, 0, 0, 0, 0 },
    { "strip_len1",    g_strip,    0, { 3, 8, 32 }, 0, 0, 0, 0, 0 },
    { "strip_len6",    g_strip,    3, { 3, 8, 32 }, 0, 0, 0, 0, 0 },
    { "culled_off",    g_culled,   0, { 64, 256, 1024 }, 0, 0, 0, 0, 0 },
    { "culled_clip",   g_culled,   1, { 64, 256, 1024 }, 0, 0, 0, 0, 0 },
    { "quad_1",        g_quad,     1, { 64, 256, 1024 }, 0, 0, 0, 0, 0 },
    { "quad_2",        g_quad,     2, { 16, 64, 256 }, 0, 0, 0, 0, 0 },
    { "quad_4",        g_quad,     4, { 8, 32, 128 }, 0, 0, 0, 0, 0 },
    { "quad_8",        g_quad,     8, { 4, 16, 64 }, 0, 0, 0, 0, 0 },
    { "quad_15",       g_quad,     15, { 2, 8, 32 }, 0, 0, 0, 0, 0 },
    { "quad_full",     g_quad,     0, { 2, 8, 32 }, 0, 0, 0, 0, 0 },
    { "quad_full_opb8",  g_quad,   0, { 2, 8, 32 }, 0, 0, 1, 0, 0 },
    { "quad_full_opb32", g_quad,   0, { 2, 8, 32 }, 0, 0, 3, 0, 0 },
    { "quad_full_tr",  g_quad_list, TR, { 2, 8, 32 }, 0, 0, 0, 0, 0 },
    { "quad_full_pt",  g_quad_list, PT, { 2, 8, 32 }, 0, 0, 0, 0, 0 },
    { "tri_full",      g_tri_full, 0, { 2, 8, 32 }, 0, 0, 0, 0, 0 },
    { "sprite",        g_sprite,   0, { 64, 256, 1024 }, 0, 0, 0, 0, 0 },
    { "sprite_tex",    g_sprite,   1, { 64, 256, 1024 }, 0, 0, 0, 0, 0 },
    { "sprite_full",   g_sprite,   2, { 2, 8, 32 }, 0, 0, 0, 0, 0 },
    { "modvol",        g_modvol,   0, { 64, 256, 1024 }, 0, 0, 0, 0, 0 },
    { "modvol_full",   g_modvol,   1, { 2, 8, 32 }, 0, 0, 0, 0, 0 },
    { "disp_tri_packed", g_tri,    0, { 64, 256, 1024 }, 0, 0, 0, 1, 0 },
    { "disp_quad_full",  g_quad,   0, { 2, 8, 32 }, 0, 0, 0, 1, 0 },
};
#define NCASES (int)(sizeof(cases) / sizeof(cases[0]))

/* ---- the TA's registers for a case ---- */
static void ta_setup(const pcase_t *c, int n, int hashed) {
    const int tx = c->tx ? c->tx : 40, ty = c->ty < 0 ? n : c->ty ? c->ty : 15, opb = c->opb ? c->opb : 2;
    const uint32_t matrix = 5u * (uint32_t)(tx * ty) * (8u << (opb - 1)) * 4u;
    const uint32_t regs[][2] = {
        { SOFTRESET_addr, 1 }, { SOFTRESET_addr, 0 },
        { TA_OL_BASE_addr, OL_BASE }, { TA_ISP_BASE_addr, ISP_BASE }, { TA_OL_LIMIT_addr, OL_LIMIT },
        { TA_ISP_LIMIT_addr, ISP_LIMIT }, { TA_GLOB_TILE_CLIP_addr, (uint32_t)(ty - 1) << 16 | (uint32_t)(tx - 1) },
        { TA_ALLOC_CTRL_addr, (uint32_t)opb * 0x11111u & 0x33333u }, { TA_NEXT_OPB_INIT_addr, OL_BASE + matrix },
        { TA_LIST_INIT_addr, 0x80000000u },
    };
    unsigned i;
    for(i = 0; i < sizeof(regs) / sizeof(regs[0]); i++) {
        if(hashed)
            pvrio_reg(regs[i][0], regs[i][1]);
        else
            pvrio_hw_reg(regs[i][0], regs[i][1]);
    }
}

#ifdef _arch_dreamcast
#include <arch/irq.h>
#include <arch/dmac.h>
#include <dc/perfctr.h>
#define SB_ISTNRM (*(volatile uint32_t *)0xa05f6900)
#define SB_ISTERR (*(volatile uint32_t *)0xa05f6908)
#define QACR0 (*(volatile uint32_t *)0xff000038)
#define QACR1 (*(volatile uint32_t *)0xff00003c)
#define TA_ERRS 0xfcu                   /* SB_ISTERR bits 2-7: the TA's (and the ISP's) errors */
#define TIMEOUT (200u * 200000u)        /* 200 ms in 5 ns counts */

/* blocks to the store queues for external address addr: the same address for all (the TA's FIFO), or one after
   the other (video memory); as fast as the CPU sends them */
static void sq_blocks(uint32_t addr, const uint32_t *src, int blocks, int advance) {
    uint32_t a = 0xe0000000u | (addr & 0x03ffffe0u);
    int i;
    QACR0 = QACR1 = ((addr >> 26) << 2) & 0x1c;
    for(i = 0; i < blocks; i++, src += 8) {
        volatile uint32_t *d = (volatile uint32_t *)(advance ? a + 32u * (uint32_t)i : a + 32u * (uint32_t)(i & 1));
        d[0] = src[0]; d[1] = src[1]; d[2] = src[2]; d[3] = src[3];
        d[4] = src[4]; d[5] = src[5]; d[6] = src[6]; d[7] = src[7];
        __asm__ __volatile__("pref @%0" : : "r"(d) : "memory");
    }
}

static uint32_t ram_target[4096 * 8] __attribute__((aligned(32)));

/* DMA channel 2 to the PVR (KallistiOS's pvr_dma.c): the SH4's channel, then the PVR's destination, length, start */
#define PVR_DMA ((volatile uint32_t *)0xa05f6800)
static const dma_config_t ta_dma_cfg = {
    .channel = DMA_CHANNEL_2,
    .request = DMA_REQUEST_EXTERNAL_MEM_TO_DEV,
    .unit_size = DMA_UNITSIZE_32BYTE,
    .src_mode = DMA_ADDRMODE_INCREMENT,
    .transmit_mode = DMA_TRANSMITMODE_BURST,
};
static int use_dma;                     /* the TA cases' second run */

typedef struct { uint32_t sent, done, ev, err; } meas_t;
static meas_t timed(const pcase_t *c, uint32_t done_bits) {
    meas_t m = { 0, 0, 0, 0 };
    uint64_t t0;
    const int dma = c->kind == 4 || (c->kind == 0 && use_dma);
    const dma_addr_t src = dma ? dma_map_src(buf, (size_t)nb * 32) : 0;
    int old = irq_disable();
    SB_ISTNRM = PVRIO_TA_EVENTS | (1u << 19);
    SB_ISTERR = TA_ERRS;
    t0 = perf_cntr_count(PRFC0);
    if(dma) {
        dma_transfer(&ta_dma_cfg, 0, src, (size_t)nb * 32, NULL);
        PVR_DMA[0] = c->kind == 4 ? 0x11000000u | 0x100000u : 0x10000000u;
        PVR_DMA[1] = (uint32_t)nb * 32;
        PVR_DMA[2] = 1;
        while(PVR_DMA[2] != 0 && (uint32_t)(perf_cntr_count(PRFC0) - t0) < TIMEOUT)
            ;
    } else if(c->kind == 1)
        sq_blocks(((uint32_t)ram_target & 0x1fffffffu), buf, nb, 1);
    else if(c->kind == 2)
        sq_blocks(0x11000000u + 0x100000u, buf, nb, 1);     /* 64 bit 0x100000: no area of the TA cases */
    else
        sq_blocks(0x10000000u, buf, nb, 0);
    m.sent = (uint32_t)(perf_cntr_count(PRFC0) - t0);
    for(;;) {
        uint32_t t;
        int fin;
        if(c->kind == 1)
            fin = 1;
        else if(c->kind == 2 || c->kind == 4)
            fin = (PVRIO_SB_TFREM & 15) == 8 && !(PVRIO_SB_FFST & 0x20);
        else
            fin = (SB_ISTNRM & done_bits) == done_bits;
        t = (uint32_t)(perf_cntr_count(PRFC0) - t0);
        if(fin) { m.done = t; break; }
        if(t > TIMEOUT) { m.done = 0xffffffffu; break; }
    }
    m.ev = SB_ISTNRM & PVRIO_TA_EVENTS;
    m.err = SB_ISTERR & TA_ERRS;
    SB_ISTNRM = m.ev | (1u << 19);
    SB_ISTERR = m.err;
    irq_restore(old);
    return m;
}
static void sort(uint32_t *v, int n) {
    int i, j;
    for(i = 1; i < n; i++)
        for(j = i; j > 0 && v[j - 1] > v[j]; j--) { uint32_t s = v[j]; v[j] = v[j - 1]; v[j - 1] = s; }
}
#endif

#ifndef _arch_dreamcast
static int use_dma;
#endif

static void run_point(const pcase_t *c, int n) {
    const uint32_t done = c->kind ? 0 : PVRIO_LIST_DONE(cur_list);
    const char *pre = use_dma && !c->kind ? "dma_" : "";
    char name[48];
    snprintf(pvrio_tag, sizeof(pvrio_tag), "%s%s", pre, c->name);
    if(!c->kind) {
        pvrio_fill32(OL_BASE, HASH_OL_WORDS, FILL);
        pvrio_fill32(ISP_BASE, HASH_ISP_WORDS, FILL);
    }
#ifdef _arch_dreamcast
    {
        uint32_t sent[REPS], dn[REPS], err = 0;
        volatile uint32_t *fb_r_ctrl = (volatile uint32_t *)(0xa05f8000 + 0x44);
        const uint32_t fb_r = *fb_r_ctrl;
        int r;
        if(c->display)
            *fb_r_ctrl = fb_r | 1u;
        for(r = 0; r < REPS; r++) {
            meas_t m;
            if(!c->kind)
                ta_setup(c, n, r == 0);
            if(r == 0 && !c->kind) {
                pvrio_hash('A', 0, buf, nb * 8);
                pvrio_hash('W', done, 0, 0);
            }
            pvrio_guard_read();
            m = timed(c, done);
            sent[r] = m.sent;
            dn[r] = m.done;
            err |= m.err;
            pvrio_seen_events |= m.ev;
            pvrio_seen_errors |= m.err;
        }
        *fb_r_ctrl = fb_r;
        sort(sent, REPS);
        sort(dn, REPS);
        printf("P %s%s %d %d %d %lu %lu %lu %lu %lx\n", pre, c->name, n, nb, items, (unsigned long)sent[0], (unsigned long)sent[REPS / 2],
               (unsigned long)dn[0], (unsigned long)dn[REPS / 2], (unsigned long)err);
    }
#else
    if(!c->kind) {
        ta_setup(c, n, 1);
        pvrio_ta(buf, nb * 8);
        pvrio_wait(done);
    }
#endif
    if(!c->kind) {
        snprintf(name, sizeof(name), "h%s%s_%do", pre, c->name, n);
        pvrio_ta_hash(OL_BASE, HASH_OL_WORDS, name);
        snprintf(name, sizeof(name), "h%s%s_%dp", pre, c->name, n);
        pvrio_ta_hash(ISP_BASE, HASH_ISP_WORDS, name);
    }
}

int main(int argc, char **argv) {
    int i, k;
#ifndef _arch_dreamcast
    if(argc > 1 && !strcmp(argv[1], "--labels"))
        return 0;
    if(argc > 1)
        pvrio_folder = argv[1];
#else
    (void)argc; (void)argv;
#endif
    pvrio_open();
#ifdef _arch_dreamcast
    {
        /* the display reads video memory once a line, and the TA's writes wait while it does: off while timed */
        volatile uint32_t *fb_r_ctrl = (volatile uint32_t *)(0xa05f8000 + 0x44);
        const uint32_t fb_r = *fb_r_ctrl;
        *fb_r_ctrl = fb_r & ~1u;
        printf("P display %lx off\n", (unsigned long)fb_r);
#endif
#ifdef _arch_dreamcast
        for(use_dma = 0; use_dma < 2; use_dma++)
#endif
        for(i = 0; i < NCASES; i++)
            for(k = 0; k < 3; k++) {
#ifndef _arch_dreamcast
                if(cases[i].kind)
                    continue;
#endif
                if(use_dma && cases[i].kind)
                    continue;           /* the baselines once */
                nb = 0;
                items = cases[i].ns[k];
                cur_list = OP;
                cases[i].gen(cases[i].ns[k], cases[i].arg);
                run_point(&cases[i], cases[i].ns[k]);
            }
#ifdef _arch_dreamcast
        *fb_r_ctrl = fb_r;
    }
    printf("P end\n");
#endif
    pvrio_reg(SOFTRESET_addr, 1);
    pvrio_reg(SOFTRESET_addr, 0);
    pvrio_close();
    return 0;
}
