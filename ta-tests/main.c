/* ta-tests: the tile accelerator on its own, case by case, against nulldc3's model of it
   (libswirl/gpl/lxdream/tacore.cpp, which refsw2-offline runs).

   Every case resets the TA (SOFTRESET bit 0), fills a small area of video memory with FILL, sets the TA's
   registers, starts a list set (TA_LIST_INIT), sends parameters, waits for the list-done interrupts it expects,
   and dumps:
       c<n>.hw.raw / c<n>.off.raw     AREA_WORDS words from AREA: the object lists (first 16 KB) and the
                                      parameters the TA wrote (next 16 KB), unless the case puts them elsewhere
                                      inside the area
       c<n>s.hw.raw / c<n>s.off.raw   TA_ISP_CURRENT, TA_NEXT_OPB, the list-done interrupts (SB_ISTNRM bits
                                      7-10, 21) and the error interrupts (SB_ISTERR) seen during the case
   compare.py compares the two and decodes the object lists. Register names are refsw2's (pvr_params.h).

   Cases of bad input are run only with TA_ERRORS 1, and then only they: on their own, so that a TA that stops
   taking data cannot cost the rest of a console run. */
#include "pvrio.h"

#ifndef TA_ERRORS
#define TA_ERRORS 0                 /* 1: the cases of bad input, and only those */
#endif
#define NUM_PAGES 126               /* for run2.sh: dump files of a run without TA_ERRORS (two per case) */

#define FILL        0xa5a5a5a5u
#define AREA        0x100000u
#define AREA_WORDS  8192
#define OL_DEFAULT  AREA                    /* object lists: AREA .. AREA + 16 KB */
#define PRM_DEFAULT (AREA + 0x4000u)        /* parameters: the next 16 KB */

/* ---- one case's TA configuration ---- */
typedef struct {
    int tx, ty;                 /* tiles: TA_GLOB_TILE_CLIP */
    int opb[5];                 /* TA_ALLOC_CTRL: 0 no list, 1 8 words, 2 16, 3 32 */
    int down;                   /* TA_ALLOC_CTRL OPB_Mode: further blocks at decreasing addresses */
    uint32_t ol_base, ol_limit, opb_init, isp_base, isp_limit;
} tacfg_t;

static const int opb_words[4] = { 0, 8, 16, 32 };

static uint32_t matrix_bytes(const tacfg_t *c) {
    uint32_t n = 0;
    int i;
    for(i = 0; i < 5; i++)
        n += (uint32_t)opb_words[c->opb[i]] * 4u * (uint32_t)(c->tx * c->ty);
    return n;
}

/* 4 x 3 tiles, 8 word blocks for every list, further blocks right after the matrix */
static tacfg_t cfg(void) {
    tacfg_t c = { 4, 3, { 1, 1, 1, 1, 1 }, 0, OL_DEFAULT, OL_DEFAULT + 0x4000u, 0, PRM_DEFAULT, PRM_DEFAULT + 0x4000u };
    c.opb_init = c.ol_base + matrix_bytes(&c);
    return c;
}

static int case_no, cur_list = -1;
static uint32_t case_want;

static void begin(const tacfg_t *c) {
    static uint32_t fill[1024];
    TA_GLOB_TILE_CLIP_type clip = { .full = 0 };
    TA_ALLOC_CTRL_type alloc = { .full = 0 };
    int i;

    if(!fill[0])
        for(i = 0; i < 1024; i++)
            fill[i] = FILL;
    for(i = 0; i < AREA_WORDS; i += 1024)
        pvrio_w32(AREA + (uint32_t)i * 4, fill, 1024);
    pvrio_reg(SOFTRESET_addr, 1);           /* the TA alone */
    pvrio_reg(SOFTRESET_addr, 0);
    clip.tile_x_num = (uint32_t)(c->tx - 1);
    clip.tile_y_num = (uint32_t)(c->ty - 1);
    alloc.O_OPB = (uint32_t)c->opb[0]; alloc.OM_OPB = (uint32_t)c->opb[1]; alloc.T_OPB = (uint32_t)c->opb[2];
    alloc.TM_OPB = (uint32_t)c->opb[3]; alloc.PT_OPB = (uint32_t)c->opb[4]; alloc.OPB_Mode = (uint32_t)c->down;
    pvrio_reg(TA_OL_BASE_addr, c->ol_base);
    pvrio_reg(TA_ISP_BASE_addr, c->isp_base);
    pvrio_reg(TA_OL_LIMIT_addr, c->ol_limit);
    pvrio_reg(TA_ISP_LIMIT_addr, c->isp_limit);
    pvrio_reg(TA_GLOB_TILE_CLIP_addr, clip.full);
    pvrio_reg(TA_ALLOC_CTRL_addr, alloc.full);
    pvrio_reg(TA_NEXT_OPB_INIT_addr, c->opb_init);
    pvrio_reg(TA_LIST_INIT_addr, 0x80000000u);
    {                                           /* for compare.py: how to read this case's dump */
        char line[200];
        snprintf(line, sizeof(line), "TACFG %d %d %d %d %d %d %d %d %d %x %x %x %x %x", case_no, c->tx, c->ty, c->opb[0], c->opb[1],
                 c->opb[2], c->opb[3], c->opb[4], c->down, (unsigned)c->ol_base, (unsigned)c->ol_limit, (unsigned)c->opb_init,
                 (unsigned)c->isp_base, (unsigned)c->isp_limit);
        pvrio_hw_say(line);
    }
    cur_list = -1;
    case_want = 0;
}

static void begin_default(void) {
    tacfg_t c = cfg();
    begin(&c);
}

/* the interrupts the case expects (or 100 ms), a moment for the last writes, and the dumps */
static void finish(void) {
    char name[16];
#ifdef _arch_dreamcast
    uint64_t t0 = timer_ms_gettime64();
    while((pvrio_ta_events() & case_want) != case_want && timer_ms_gettime64() - t0 < 100)
        ;
    t0 = timer_ms_gettime64();
    while(timer_ms_gettime64() - t0 < 2)
        ;
#endif
    snprintf(name, sizeof(name), "c%03d", case_no);
    pvrio_dump32(AREA, AREA_WORDS, name, ".raw");
    snprintf(name, sizeof(name), "c%03ds", case_no);
    pvrio_dump_ta_state(name, ".raw");
}

/* ---- parameter words ---- */
enum { P_END = 0, P_CLIP = 1, P_OLSET = 2, P_POLY = 4, P_SPRITE = 5, P_VTX = 7 };
#define PCW_(para, list, flags) ((uint32_t)(para) << 29 | (uint32_t)(list) << 24 | (uint32_t)(flags))
#define EOS         (1u << 28)
#define GROUP(len)  (1u << 23 | (uint32_t)(len) << 18)          /* Group_En and Strip_Len 0..3 */
#define UCLIP(m)    ((uint32_t)(m) << 16)
#define SHADOW      (1u << 7)
#define VOLUME      (1u << 6)
#define COL(t)      ((uint32_t)(t) << 4)                        /* 0 packed, 1 float, 2 intensity 1, 3 intensity 2 */
#define TEX         (1u << 3)
#define OFFS        (1u << 2)
#define GOUR        (1u << 1)
#define UV16        (1u << 0)
#define OP 0
#define OM 1
#define TR 2
#define TM 3
#define PT 4

#define ISPW 0x800abcdeu        /* depth mode greater; reserved bits 0-19 and DCalcCtrl in a pattern */
#define TSPW 0x12345678u
#define TCWW 0x9abcdef0u
#define TSP1 0x0fedcba9u
#define TCW1 0x87654321u

static uint32_t fl(float f) { union { float f; uint32_t u; } x; x.f = f; return x.u; }
#define F1    0x3f800000u
#define FINF  0x7f800000u
#define FNINF 0xff800000u
#define FNAN  0x7fc00000u
#define FNNAN 0xffc00000u

static void ta_words(const uint32_t *w, int n) { pvrio_ta(w, n); }
static void w8(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e, uint32_t f, uint32_t g, uint32_t h) {
    const uint32_t w[8] = { a, b, c, d, e, f, g, h };
    ta_words(w, 8);
}
static void w16(const uint32_t *w) { ta_words(w, 16); }

/* the list a header opens; end of list records its interrupt */
static void list_of(int list) { cur_list = list; }
static void eol(void) {
    if(cur_list >= 0)
        case_want |= PVRIO_LIST_DONE(cur_list);
    cur_list = -1;
    w8(0, 0, 0, 0, 0, 0, 0, 0);
}

/* headers: polygon types 0 (packed/float colour), 1 (intensity, face colour), 2 (intensity with offset: 64
   bytes), 3 (two volumes), 4 (two volumes, intensity: 64 bytes); sprite; modifier volume */
static void hdr(int list, uint32_t flags, uint32_t isp, uint32_t tsp, uint32_t tcw) {
    list_of(list);
    w8(PCW_(P_POLY, list, flags), isp, tsp, tcw, 0, 0, 0, 0);
}
static void hdr_face(int list, uint32_t flags, uint32_t isp, uint32_t tsp, uint32_t tcw,
                     uint32_t a, uint32_t r, uint32_t g, uint32_t b) {
    list_of(list);
    w8(PCW_(P_POLY, list, flags), isp, tsp, tcw, a, r, g, b);
}
static void hdr_face2(int list, uint32_t flags, uint32_t isp, uint32_t tsp, uint32_t tcw, uint32_t tsp1, uint32_t tcw1,
                      const uint32_t face0[4], const uint32_t face1[4]) {
    uint32_t w[16] = { PCW_(P_POLY, list, flags), isp, tsp, tcw, tsp1, tcw1, 0, 0,
                       face0[0], face0[1], face0[2], face0[3], face1[0], face1[1], face1[2], face1[3] };
    list_of(list);
    w16(w);
}
static void hdr_vol(int list, uint32_t flags, uint32_t isp, uint32_t tsp0, uint32_t tcw0, uint32_t tsp1, uint32_t tcw1) {
    list_of(list);
    w8(PCW_(P_POLY, list, flags), isp, tsp0, tcw0, tsp1, tcw1, 0, 0);
}
static void hdr_sprite(int list, uint32_t flags, uint32_t isp, uint32_t tsp, uint32_t tcw, uint32_t base, uint32_t offs) {
    list_of(list);
    w8(PCW_(P_SPRITE, list, flags), isp, tsp, tcw, base, offs, 0, 0);
}
/* modifier volume: ISP word with the volume mode in bits 31-29 (0 normal, 1 inside last, 2 outside last) */
static void hdr_mv(int list, uint32_t flags, uint32_t mode) {
    list_of(list);
    w8(PCW_(P_POLY, list, flags), mode << 29, 0, 0, 0, 0, 0, 0);
}
static void uclip(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2) {
    w8(PCW_(P_CLIP, 0, 0), 0, 0, 0, x1, y1, x2, y2);
}

/* vertices, by vertex parameter type */
#define VP(eos) PCW_(P_VTX, 0, (eos) ? EOS : 0)
static void v0(float x, float y, float z, uint32_t col, int eos) {                           /* packed */
    w8(VP(eos), fl(x), fl(y), fl(z), 0, 0, col, 0);
}
static void v0b(uint32_t x, uint32_t y, uint32_t z, uint32_t col, int eos) {                 /* packed, bits */
    w8(VP(eos), x, y, z, 0, 0, col, 0);
}
static void v1(float x, float y, float z, uint32_t a, uint32_t r, uint32_t g, uint32_t b, int eos) {   /* float colour */
    w8(VP(eos), fl(x), fl(y), fl(z), a, r, g, b);
}
static void v2(float x, float y, float z, uint32_t in, int eos) {                            /* intensity */
    w8(VP(eos), fl(x), fl(y), fl(z), 0, 0, in, 0);
}
static void v3(float x, float y, float z, uint32_t u, uint32_t v, uint32_t col, uint32_t offs, int eos) {
    w8(VP(eos), fl(x), fl(y), fl(z), u, v, col, offs);
}
static void v4(float x, float y, float z, uint32_t uv, uint32_t col, uint32_t offs, int eos) {
    w8(VP(eos), fl(x), fl(y), fl(z), uv, 0, col, offs);
}
static void v5(float x, float y, float z, uint32_t u, uint32_t v, const uint32_t base[4], const uint32_t offs[4], int eos) {
    uint32_t w[16] = { VP(eos), fl(x), fl(y), fl(z), u, v, 0, 0, base[0], base[1], base[2], base[3], offs[0], offs[1], offs[2], offs[3] };
    w16(w);
}
static void v6(float x, float y, float z, uint32_t uv, const uint32_t base[4], const uint32_t offs[4], int eos) {
    uint32_t w[16] = { VP(eos), fl(x), fl(y), fl(z), uv, 0, 0, 0, base[0], base[1], base[2], base[3], offs[0], offs[1], offs[2], offs[3] };
    w16(w);
}
static void v7(float x, float y, float z, uint32_t u, uint32_t v, uint32_t bi, uint32_t oi, int eos) {
    w8(VP(eos), fl(x), fl(y), fl(z), u, v, bi, oi);
}
static void v8(float x, float y, float z, uint32_t uv, uint32_t bi, uint32_t oi, int eos) {
    w8(VP(eos), fl(x), fl(y), fl(z), uv, 0, bi, oi);
}
static void v9(float x, float y, float z, uint32_t c0, uint32_t c1, int eos) {               /* two volumes */
    w8(VP(eos), fl(x), fl(y), fl(z), c0, c1, 0, 0);
}
static void v11(float x, float y, float z, uint32_t u0, uint32_t v0_, uint32_t c0, uint32_t o0,
                uint32_t u1, uint32_t v1_, uint32_t c1, uint32_t o1, int eos) {            /* also 13: intensities */
    uint32_t w[16] = { VP(eos), fl(x), fl(y), fl(z), u0, v0_, c0, o0, u1, v1_, c1, o1, 0, 0, 0, 0 };
    w16(w);
}
static void v12(float x, float y, float z, uint32_t uv0, uint32_t c0, uint32_t o0, uint32_t uv1, uint32_t c1, uint32_t o1, int eos) {
    uint32_t w[16] = { VP(eos), fl(x), fl(y), fl(z), uv0, 0, c0, o0, uv1, 0, c1, o1, 0, 0, 0, 0 };   /* also 14 */
    w16(w);
}
/* sprite: corners A, B, C with depth, D without; textured: UV of A, B, C (16 bit each) */
static void vsprite(float ax, float ay, float az, float bx, float by, float bz, float cx, float cy, float cz,
                    float dx, float dy, uint32_t auv, uint32_t buv, uint32_t cuv) {
    uint32_t w[16] = { VP(1), fl(ax), fl(ay), fl(az), fl(bx), fl(by), fl(bz), fl(cx),
                       fl(cy), fl(cz), fl(dx), fl(dy), 0, auv, buv, cuv };
    w16(w);
}
static void vmv(float ax, float ay, float az, float bx, float by, float bz, float cx, float cy, float cz) {
    uint32_t w[16] = { VP(1), fl(ax), fl(ay), fl(az), fl(bx), fl(by), fl(bz), fl(cx), fl(cy), fl(cz), 0, 0, 0, 0, 0, 0 };
    w16(w);
}

/* a packed-colour triangle, its own polygon */
static void tri(int list, uint32_t flags, float x0, float y0, float x1, float y1, float x2, float y2, uint32_t col) {
    hdr(list, GROUP(0) | flags, ISPW, TSPW, 0);
    v0(x0, y0, 1.0f, col, 0);
    v0(x1, y1, 1.0f, col, 0);
    v0(x2, y2, 1.0f, col, 1);
}
/* a packed-colour quad as a strip of four */
static void quad(int list, uint32_t flags, float x, float y, float w, float h, uint32_t col) {
    hdr(list, GROUP(1) | flags, ISPW, TSPW, 0);
    v0(x, y, 1.0f, col, 0);
    v0(x + w, y, 1.0f, col, 0);
    v0(x, y + h, 1.0f, col, 0);
    v0(x + w, y + h, 1.0f, col, 1);
}
static void mvtri(int list, uint32_t mode, float x0, float y0, float x1, float y1, float x2, float y2) {
    hdr_mv(list, 0, mode);
    vmv(x0, y0, 1.0f, x1, y1, 1.0f, x2, y2, 1.0f);
}

/* ======== cases: lists, the object list matrix, allocation ======== */

static void c_empty_op(void) {                  /* a header without vertices, then the end: what KallistiOS sends */
    begin_default(); hdr(OP, GROUP(1) | COL(1) | GOUR, 0, 0, 0); eol(); finish();
}
static void c_all_lists(void) {                 /* one primitive in each list, in order */
    begin_default();
    tri(OP, 0, 40, 40, 56, 40, 40, 56, 0xff102030); eol();
    mvtri(OM, 1, 36, 36, 60, 36, 36, 60); eol();
    tri(TR, 0, 72, 40, 88, 40, 72, 56, 0x80405060); eol();
    mvtri(TM, 1, 68, 36, 92, 36, 68, 60); eol();
    tri(PT, 0, 40, 72, 56, 72, 40, 88, 0xff708090); eol();
    finish();
}
static void c_lists_reverse(void) {
    begin_default();
    tri(PT, 0, 40, 72, 56, 72, 40, 88, 0xff708090); eol();
    mvtri(TM, 1, 68, 36, 92, 36, 68, 60); eol();
    tri(TR, 0, 72, 40, 88, 40, 72, 56, 0x80405060); eol();
    mvtri(OM, 1, 36, 36, 60, 36, 36, 60); eol();
    tri(OP, 0, 40, 40, 56, 40, 40, 56, 0xff102030); eol();
    finish();
}
static void c_list_twice(void) {                /* the opaque list ended, then sent again */
    begin_default();
    tri(OP, 0, 40, 40, 56, 40, 40, 56, 0xff102030); eol();
    tri(OP, 0, 72, 40, 88, 40, 72, 56, 0xff405060); eol();
    finish();
}
/* n quads (strips of four, so that they are not merged into triangle arrays) in tile (1, 1) */
static void quads_in_tile(int list, int n) {
    int i;
    for(i = 0; i < n; i++)
        quad(list, 0, 36.0f + (float)(i % 8), 36.0f + (float)(i / 8), 16, 16, 0xff000000u | (uint32_t)i);
}
static void opb_size(int code) {
    tacfg_t c = cfg();
    c.opb[1] = c.opb[2] = c.opb[3] = c.opb[4] = 0;
    c.opb[0] = code;
    c.opb_init = c.ol_base + matrix_bytes(&c);
    begin(&c); quads_in_tile(OP, 40); eol(); finish();
}
static void c_opb_8(void) { opb_size(1); }
static void c_opb_16(void) { opb_size(2); }
static void c_opb_32(void) { opb_size(3); }
static void c_opb_none(void) { opb_size(0); }   /* no object pointer blocks for the opaque list */
static void c_opb_mixed(void) {                 /* 32, 8, 16, none, 8 words: where each list's matrix lies */
    tacfg_t c = cfg();
    c.opb[0] = 3; c.opb[1] = 1; c.opb[2] = 2; c.opb[3] = 0; c.opb[4] = 1;
    c.opb_init = c.ol_base + matrix_bytes(&c);
    begin(&c);
    tri(OP, 0, 40, 40, 56, 40, 40, 56, 0xff102030); eol();
    mvtri(OM, 1, 36, 36, 60, 36, 36, 60); eol();
    tri(TR, 0, 72, 40, 88, 40, 72, 56, 0x80405060); eol();
    mvtri(TM, 1, 68, 36, 92, 36, 68, 60); eol();
    tri(PT, 0, 40, 72, 56, 72, 40, 88, 0xff708090); eol();
    finish();
}
static void grid(int tx, int ty) {              /* a quad over the whole grid and a triangle in its last tile */
    tacfg_t c = cfg();
    c.tx = tx; c.ty = ty;
    c.opb[1] = c.opb[2] = c.opb[3] = c.opb[4] = 0;
    c.opb_init = c.ol_base + matrix_bytes(&c);
    if(matrix_bytes(&c) > 0x3000) {             /* 64 x 16: the matrix fills the area; parameters and blocks after it */
        c.isp_base = AREA + 0x8000u; c.isp_limit = AREA + 0xc000u;
        c.ol_limit = AREA + 0x10000u; c.opb_init = AREA + 0xc000u;
    }
    begin(&c);
    quad(OP, 0, 0, 0, 32.0f * (float)tx, 32.0f * (float)ty, 0xff112233);
    tri(OP, 0, 32.0f * (float)tx - 12, 32.0f * (float)ty - 12, 32.0f * (float)tx - 4, 32.0f * (float)ty - 12,
        32.0f * (float)tx - 12, 32.0f * (float)ty - 4, 0xff445566);
    eol(); finish();
}
static void c_grid_1x1(void) { grid(1, 1); }
static void c_grid_8x2(void) { grid(8, 2); }
static void c_grid_20x15(void) { grid(20, 15); }
static void c_grid_64x16(void) { grid(64, 16); }  /* the largest the register holds; outside the area: see compare.py */
static void c_grow_down(void) {                 /* OPB_Mode 1: further blocks below TA_NEXT_OPB_INIT, down to TA_OL_LIMIT */
    tacfg_t c = cfg();
    c.down = 1;
    c.ol_base = AREA + 0x2000u;
    c.opb_init = AREA + 0x2000u;
    c.ol_limit = AREA;
    begin(&c); quads_in_tile(OP, 40); quads_in_tile(OP, 4); eol(); finish();
}
static void c_opb_gap(void) {                   /* further blocks 1 KB after the matrix */
    tacfg_t c = cfg();
    c.opb_init += 0x400;
    begin(&c); quads_in_tile(OP, 20); eol(); finish();
}
static void c_ol_limit(void) {                  /* room for three further blocks: the object list runs out */
    tacfg_t c = cfg();
    c.ol_limit = c.opb_init + 3 * 32;
    begin(&c); quads_in_tile(OP, 40); eol(); finish();
}
static void c_ol_limit_down(void) {             /* the same, growing down */
    tacfg_t c = cfg();
    c.down = 1;
    c.ol_base = AREA + 0x2000u;
    c.opb_init = AREA + 0x2000u;
    c.ol_limit = AREA + 0x2000u - 3 * 32;
    begin(&c); quads_in_tile(OP, 40); eol(); finish();
}
static void c_ol_limit_at_base(void) {          /* TA_OL_LIMIT = TA_OL_BASE */
    tacfg_t c = cfg();
    c.ol_limit = c.ol_base;
    begin(&c); tri(OP, 0, 40, 40, 56, 40, 40, 56, 0xff102030); eol(); finish();
}
static void c_ol_limit_below(void) {            /* growing up with TA_OL_LIMIT below TA_OL_BASE */
    tacfg_t c = cfg();
    c.ol_base = AREA + 0x1000u;
    c.opb_init = c.ol_base + matrix_bytes(&c);
    c.ol_limit = AREA;
    begin(&c); tri(OP, 0, 40, 40, 56, 40, 40, 56, 0xff102030); eol(); finish();
}
static void c_isp_limit(void) {                 /* room for 64 words of parameters: the parameter buffer runs out */
    tacfg_t c = cfg();
    c.isp_limit = c.isp_base + 64 * 4;
    begin(&c); quads_in_tile(OP, 8); eol(); finish();
}
static void c_isp_base_odd(void) {              /* TA_ISP_BASE not on a 1 MB boundary: the offsets in the list entries */
    tacfg_t c = cfg();
    c.isp_base = AREA + 0x4400u;
    begin(&c); tri(OP, 0, 40, 40, 56, 40, 40, 56, 0xff102030); eol(); finish();
}
static void c_list_cont(void) {                 /* TA_LIST_CONT after the opaque list, then the opaque list again */
    begin_default();
    tri(OP, 0, 40, 40, 56, 40, 40, 56, 0xff102030); eol();
#ifdef _arch_dreamcast
    { uint64_t t0 = timer_ms_gettime64(); while(!(pvrio_ta_events() & PVRIO_LIST_DONE(OP)) && timer_ms_gettime64() - t0 < 100) ; }
#endif
    pvrio_reg(TA_LIST_CONT_addr, 0x80000000u);
    tri(OP, 0, 72, 40, 88, 40, 72, 56, 0xff405060); eol();
    finish();
}

/* ======== cases: headers into parameters ======== */

/* a triangle in tile (1, 1) with the object control bits obj (UV16, Gouraud, Offset, Texture), packed colour,
   with the vertex type those bits call for */
static void tri_obj(uint32_t obj, uint32_t isp, int dx) {
    const float x = 36.0f + (float)dx;
    hdr(OP, GROUP(0) | obj, isp, TSPW, TCWW);
    if(!(obj & TEX)) {
        v0(x, 36, 1, 0xff102030, 0); v0(x + 8, 36, 1, 0xff102030, 0); v0(x, 44, 1, 0xff102030, 1);
    } else if(!(obj & UV16)) {
        v3(x, 36, 1, F1, 0, 0xff102030, 0x00405060, 0); v3(x + 8, 36, 1, 0, F1, 0xff102030, 0x00405060, 0);
        v3(x, 44, 1, F1, F1, 0xff102030, 0x00405060, 1);
    } else {
        v4(x, 36, 1, 0x3f803f80, 0xff102030, 0x00405060, 0); v4(x + 8, 36, 1, 0x3f000000, 0xff102030, 0x00405060, 0);
        v4(x, 44, 1, 0x00003f00, 0xff102030, 0x00405060, 1);
    }
}
static void obj_bits(uint32_t isp) {
    uint32_t obj;
    begin_default();
    for(obj = 0; obj < 16; obj++)
        tri_obj(obj, isp, (int)obj);
    eol(); finish();
}
static void c_obj_bits(void) { obj_bits(ISPW); }
static void c_obj_bits_isp1(void) { obj_bits(0xffffffffu); }
static void c_obj_bits_isp0(void) { obj_bits(0); }
static void c_isp_single_bits(void) {           /* ISP words with one bit set, bit 0 .. 31, on a packed triangle */
    int b;
    begin_default();
    for(b = 0; b < 32; b++)
        tri_obj(0, 1u << b, b % 16);
    eol(); finish();
}
static void c_tsp_tcw(void) {                   /* TSP and TCW words, textured and not */
    begin_default();
    hdr(OP, GROUP(0), ISPW, 0xffffffffu, 0xffffffffu);
    v0(36, 36, 1, 1, 0); v0(44, 36, 1, 2, 0); v0(36, 44, 1, 3, 1);
    hdr(OP, GROUP(0) | TEX, ISPW, 0xffffffffu, 0xffffffffu);
    v3(40, 36, 1, 0, 0, 1, 0, 0); v3(48, 36, 1, 0, 0, 2, 0, 0); v3(40, 44, 1, 0, 0, 3, 0, 1);
    hdr(OP, GROUP(0) | TEX, ISPW, 0, 0);
    v3(44, 36, 1, 0, 0, 1, 0, 0); v3(52, 36, 1, 0, 0, 2, 0, 0); v3(44, 44, 1, 0, 0, 3, 0, 1);
    eol(); finish();
}
static void c_shadow_bit(void) {                /* Shadow without Volume: cheap shadow polygons, packed and textured */
    begin_default();
    hdr(OP, GROUP(0) | SHADOW, ISPW, TSPW, TCWW);
    v0(36, 36, 1, 1, 0); v0(44, 36, 1, 2, 0); v0(36, 44, 1, 3, 1);
    hdr(OP, GROUP(0) | SHADOW | TEX | OFFS, ISPW, TSPW, TCWW);
    v3(40, 36, 1, F1, 0, 1, 7, 0); v3(48, 36, 1, 0, F1, 2, 8, 0); v3(40, 44, 1, F1, F1, 3, 9, 1);
    hdr(TR, GROUP(0) | SHADOW, ISPW, TSPW, 0);
    eol();
    finish();
}
static void c_two_volume_hdrs(void) {           /* Shadow and Volume: the five word header, untextured and textured */
    begin_default();
    hdr_vol(OP, GROUP(0) | SHADOW | VOLUME, ISPW, TSPW, TCWW, TSP1, TCW1);
    v9(36, 36, 1, 0xff000001, 0xff000002, 0); v9(44, 36, 1, 0xff000003, 0xff000004, 0); v9(36, 44, 1, 0xff000005, 0xff000006, 1);
    hdr_vol(OP, GROUP(0) | SHADOW | VOLUME | TEX, ISPW, TSPW, TCWW, TSP1, TCW1);
    v11(40, 36, 1, F1, 0, 0xff000001, 0x11, 0, F1, 0xff000002, 0x12, 0);
    v11(48, 36, 1, 0, 0, 0xff000003, 0x13, F1, F1, 0xff000004, 0x14, 0);
    v11(40, 44, 1, F1, F1, 0xff000005, 0x15, 0, 0, 0xff000006, 0x16, 1);
    hdr_vol(OP, GROUP(0) | VOLUME, ISPW, TSPW, TCWW, TSP1, TCW1);            /* Volume without Shadow */
    v0(44, 36, 1, 1, 0); v0(52, 36, 1, 2, 0); v0(44, 44, 1, 3, 1);
    eol(); finish();
}
static void c_single_tile(void) {               /* the single tile bit: inside one tile, on its edges, across two */
    begin_default();
    tri(OP, 0, 33, 33, 63, 33, 33, 63, 1);          /* inside tile (1, 1) */
    tri(OP, 0, 32, 32, 63.999f, 32, 32, 63.999f, 2);  /* its whole area */
    tri(OP, 0, 40, 40, 64, 40, 40, 56, 3);          /* a corner on the next tile's first column */
    tri(OP, 0, 40, 40, 56, 40, 40, 64, 4);          /* ... first row */
    tri(OP, 0, 40, 40, 72, 40, 40, 56, 5);          /* across two tiles */
    tri(OP, 0, 31.999f, 40, 56, 40, 40, 56, 6);     /* a corner a hair into the tile before */
    eol(); finish();
}
static void c_col_type_hdrs(void) {             /* the header words of colour types float, intensity 1 and 2 */
    begin_default();
    hdr(OP, GROUP(0) | COL(1), ISPW, TSPW, 0);
    v1(36, 36, 1, F1, F1, 0, 0, 0); v1(44, 36, 1, F1, 0, F1, 0, 0); v1(36, 44, 1, F1, 0, 0, F1, 1);
    hdr_face(OP, GROUP(0) | COL(2), ISPW, TSPW, 0, F1, fl(0.5f), fl(0.25f), fl(0.75f));
    v2(40, 36, 1, F1, 0); v2(48, 36, 1, fl(0.5f), 0); v2(40, 44, 1, 0, 1);
    hdr_face(OP, GROUP(0) | COL(3), ISPW, TSPW, 0, fl(0.1f), fl(0.2f), fl(0.3f), fl(0.4f));   /* face colour ignored? */
    v2(44, 36, 1, F1, 0); v2(52, 36, 1, fl(0.5f), 0); v2(44, 44, 1, 0, 1);
    eol(); finish();
}
static void c_uclip_field_only(void) {          /* user clip modes in the header, no clip parameter sent */
    begin_default();
    tri(OP, UCLIP(2), 40, 40, 56, 40, 40, 56, 1);
    tri(OP, UCLIP(3), 40, 40, 56, 40, 40, 56, 2);
    tri(OP, UCLIP(1), 40, 40, 56, 40, 40, 56, 3);
    eol(); finish();
}
static void ten_strip(float y, uint32_t col) {
    int i;
    for(i = 0; i < 10; i++)
        v0(36.0f + 4.0f * (float)(i / 2), y + 8.0f * (float)(i & 1), 1, col + (uint32_t)i, i == 9);
}
static void c_group_en(void) {                  /* Strip_Len holds only with Group_En; without, the last one */
    begin_default();
    hdr(OP, GROUP(3), ISPW, TSPW, 0); ten_strip(36, 0x100);
    hdr(OP, 0, ISPW, TSPW, 0); ten_strip(48, 0x200);              /* Group_En 0 */
    hdr(OP, GROUP(0), ISPW, TSPW, 0); ten_strip(60, 0x300);
    hdr(OP, 3u << 18, ISPW, TSPW, 0); ten_strip(72, 0x400);       /* Strip_Len 3 without Group_En */
    eol(); finish();
}

/* ======== cases: vertex parameters into parameters (colour conversion) ======== */

/* float values for the colour sweeps, as bits: exact fractions, their neighbours, beyond the range, specials */
static const uint32_t sweep[32] = {
    0x00000000, 0x3f800000, 0x3f000000, 0x3e800000, 0x3f400000, 0x3b808081, 0x3c008081, 0x3f7efeff,   /* 0 1 .5 .25 .75 1/255 2/255 254/255 */
    0x3b800000, 0x3f7f0000, 0x3f7f8000, 0x3af90b2c, 0x3f7fbe77, 0x3f7fffff, 0x3f800001, 0x3fc00000,   /* 1/256 255/256 511/512 .0019 .999 1-e 1+e 1.5 */
    0x40000000, 0x7149f2ca, 0x80000000, 0xbf000000, 0xf149f2ca, 0x7f800000, 0xff800000, 0x7fc00000,   /* 2 1e30 -0 -.5 -1e30 inf -inf nan */
    0xffc00000, 0x00000001, 0x3f008000, 0x3b7fffff, 0x3b800001, 0x3c000000, 0x3f7f8001, 0x3f7e0000,   /* -nan denormal .502 below/above 1/256 1/128 ... 254/256 */
};
#define NSWEEP 32

static void c_packed_colours(void) {
    static const uint32_t cols[6] = { 0x00000000, 0xffffffff, 0x80808080, 0x01020304, 0xfedcba98, 0x7f00ff01 };
    int i;
    begin_default();
    hdr(OP, GROUP(0) | GOUR, ISPW, TSPW, 0);
    for(i = 0; i < 6; i++)
        v0(36.0f + (float)i, 36, 1, cols[i], 0);
    v0(36, 44, 1, 0x55aa55aa, 1);
    eol(); finish();
}
static void c_float_sweep(void) {               /* type 1: every value of the sweep in every channel */
    int k;
    begin_default();
    hdr(OP, GROUP(0) | COL(1) | GOUR, ISPW, TSPW, 0);
    for(k = 0; k < 33; k++)
        v1(36.0f + (float)(k % 3) * 4, 36.0f + (float)(k / 3), 1, sweep[k % NSWEEP], sweep[(k + 1) % NSWEEP],
           sweep[(k + 2) % NSWEEP], sweep[(k + 3) % NSWEEP], k % 3 == 2);
    eol(); finish();
}
static void c_float_sweep_tex(void) {           /* type 5 (textured, float colours, offset), and type 6 (16 bit UV) */
    int k;
    begin_default();
    hdr(OP, GROUP(0) | COL(1) | TEX | OFFS | GOUR, ISPW, TSPW, TCWW);
    for(k = 0; k < 33; k++) {
        const uint32_t base[4] = { sweep[k % NSWEEP], sweep[(k + 1) % NSWEEP], sweep[(k + 2) % NSWEEP], sweep[(k + 3) % NSWEEP] };
        const uint32_t offs[4] = { sweep[(k + 16) % NSWEEP], sweep[(k + 17) % NSWEEP], sweep[(k + 18) % NSWEEP], sweep[(k + 19) % NSWEEP] };
        v5(36.0f + (float)(k % 3) * 4, 36.0f + (float)(k / 3), 1, 0x3f000000u + (uint32_t)k, 0x3e000000u + (uint32_t)k, base, offs, k % 3 == 2);
    }
    hdr(OP, GROUP(0) | COL(1) | TEX | UV16 | GOUR, ISPW, TSPW, TCWW);           /* no offset: the offset words ignored */
    for(k = 0; k < 6; k++) {
        const uint32_t base[4] = { sweep[k], sweep[k + 4], sweep[k + 8], sweep[k + 12] };
        const uint32_t offs[4] = { F1, F1, F1, F1 };
        v6(40.0f + (float)(k % 3) * 4, 40.0f + (float)(k / 3), 1, 0x3f803f00u + (uint32_t)k, base, offs, k % 3 == 2);
    }
    eol(); finish();
}
static void int_sweep(const uint32_t face[4]) {
    int k;
    hdr_face(OP, GROUP(0) | COL(2) | GOUR, ISPW, TSPW, 0, face[0], face[1], face[2], face[3]);
    for(k = 0; k < 33; k++)
        v2(36.0f + (float)(k % 3) * 4, 36.0f + (float)(k / 3), 1, sweep[k % NSWEEP], k % 3 == 2);
}
/* type 1 header (face colour), type 2 vertices: intensity sweep */
static const uint32_t faces[4][4] = {
        { F1, F1, F1, F1 }, { 0x3f000000, 0x3e800000, 0x3f400000, 0x3b808081 },
        { 0x7f800000, 0x7fc00000, 0xbf800000, 0x40000000 }, { 0x3f7f0000, 0x3b800000, 0x3f7fbe77, 0x3af90b2c } };
static void c_int1_sweep_a(void) { begin_default(); int_sweep(faces[0]); int_sweep(faces[1]); eol(); finish(); }
static void c_int1_sweep_b(void) { begin_default(); int_sweep(faces[2]); int_sweep(faces[3]); eol(); finish(); }
static void c_int2_reuse(void) {                /* colour type 3: the last face colour again */
    begin_default();
    hdr_face(OP, GROUP(0) | COL(2), ISPW, TSPW, 0, F1, 0x3f000000, 0x3e800000, 0x3f400000);
    v2(36, 36, 1, F1, 0); v2(44, 36, 1, 0x3f000000, 0); v2(36, 44, 1, 0x3e800000, 1);
    hdr(OP, GROUP(0) | COL(3), ISPW, TSPW, 0);
    v2(40, 36, 1, F1, 0); v2(48, 36, 1, 0x3f000000, 0); v2(40, 44, 1, 0x3e800000, 1);
    hdr(OP, GROUP(0) | COL(1), ISPW, TSPW, 0);                                 /* a float colour polygon between */
    v1(44, 36, 1, F1, F1, F1, F1, 0); v1(52, 36, 1, F1, F1, F1, F1, 0); v1(44, 44, 1, F1, F1, F1, F1, 1);
    hdr(OP, GROUP(0) | COL(3), ISPW, TSPW, 0);
    v2(48, 36, 1, F1, 0); v2(56, 36, 1, 0x3f000000, 0); v2(48, 44, 1, 0x3e800000, 1);
    eol(); finish();
}
static void c_int_offset(void) {                /* type 2 header (face and offset colours), type 7 and 8 vertices */
    static const uint32_t face[4] = { F1, 0x3f000000, 0x3e800000, 0x3f400000 }, offs[4] = { 0x3f000000, F1, 0x3b808081, 0x3f7f0000 };
    int k;
    begin_default();
    hdr_face2(OP, GROUP(0) | COL(2) | TEX | OFFS | GOUR, ISPW, TSPW, TCWW, 0, 0, face, offs);
    for(k = 0; k < 12; k++)
        v7(36.0f + (float)(k % 3) * 4, 36.0f + (float)(k / 3), 1, F1, 0, sweep[k], sweep[k + 12], k % 3 == 2);
    hdr_face2(OP, GROUP(0) | COL(2) | TEX | OFFS | UV16, ISPW, TSPW, TCWW, 0, 0, offs, face);
    for(k = 0; k < 6; k++)
        v8(40.0f + (float)(k % 3) * 4, 40.0f + (float)(k / 3), 1, 0x3f803f80, sweep[k + 20], sweep[k + 2], k % 3 == 2);
    hdr(OP, GROUP(0) | COL(3) | TEX | OFFS, ISPW, TSPW, TCWW);                 /* type 3 after it: both colours again */
    v7(44, 36, 1, 0, 0, F1, F1, 0); v7(52, 36, 1, 0, 0, 0x3f000000, 0x3f000000, 0); v7(44, 44, 1, 0, 0, 0, 0, 1);
    hdr_face(OP, GROUP(0) | COL(2) | TEX, ISPW, TSPW, TCWW, F1, F1, F1, F1);   /* textured intensity without offset: type 1 header */
    v7(48, 36, 1, 0, 0, 0x3f000000, F1, 0); v7(56, 36, 1, 0, 0, F1, F1, 0); v7(48, 44, 1, 0, 0, 0, F1, 1);
    eol(); finish();
}
static void c_tex_packed(void) {                /* types 3 and 4, with and without offset */
    begin_default();
    hdr(OP, GROUP(0) | TEX, ISPW, TSPW, TCWW);
    v3(36, 36, 1, 0x11111111, 0x22222222, 0xff333333, 0x44444444, 0); v3(44, 36, 1, 1, 2, 3, 4, 0); v3(36, 44, 1, 5, 6, 7, 8, 1);
    hdr(OP, GROUP(0) | TEX | OFFS, ISPW, TSPW, TCWW);
    v3(40, 36, 1, 0x11111111, 0x22222222, 0xff333333, 0x44444444, 0); v3(48, 36, 1, 1, 2, 3, 4, 0); v3(40, 44, 1, 5, 6, 7, 8, 1);
    hdr(OP, GROUP(0) | TEX | UV16, ISPW, TSPW, TCWW);
    v4(44, 36, 1, 0x11112222, 0xff333333, 0x44444444, 0); v4(52, 36, 1, 1, 3, 4, 0); v4(44, 44, 1, 5, 7, 8, 1);
    hdr(OP, GROUP(0) | TEX | UV16 | OFFS, ISPW, TSPW, TCWW);
    v4(48, 36, 1, 0x11112222, 0xff333333, 0x44444444, 0); v4(56, 36, 1, 1, 3, 4, 0); v4(48, 44, 1, 5, 7, 8, 1);
    eol(); finish();
}
static void c_untex_bits(void) {                /* Offset and UV16 on untextured polygons; Gouraud */
    begin_default();
    hdr(OP, GROUP(0) | OFFS, ISPW, TSPW, TCWW); v0(36, 36, 1, 1, 0); v0(44, 36, 1, 2, 0); v0(36, 44, 1, 3, 1);
    hdr(OP, GROUP(0) | UV16, ISPW, TSPW, TCWW); v0(40, 36, 1, 1, 0); v0(48, 36, 1, 2, 0); v0(40, 44, 1, 3, 1);
    hdr(OP, GROUP(0) | OFFS | UV16 | GOUR, ISPW, TSPW, TCWW); v0(44, 36, 1, 1, 0); v0(52, 36, 1, 2, 0); v0(44, 44, 1, 3, 1);
    hdr(OP, GROUP(0) | COL(1) | OFFS, ISPW, TSPW, TCWW); v1(48, 36, 1, F1, F1, 0, 0, 0); v1(56, 36, 1, F1, 0, F1, 0, 0); v1(48, 44, 1, F1, 0, 0, F1, 1);
    eol(); finish();
}

static void c_two_vol_types(void) {             /* vertex types 9 .. 14, with and without offset */
    static const uint32_t f0[4] = { F1, 0x3f000000, 0x3e800000, 0x3f400000 }, f1[4] = { 0x3f000000, F1, 0x3f7f0000, 0x3b808081 };
    begin_default();
    hdr_vol(OP, GROUP(0) | SHADOW | VOLUME, ISPW, TSPW, TCWW, TSP1, TCW1);                                    /* 9 */
    v9(36, 36, 1, 0xff010203, 0xff040506, 0); v9(44, 36, 1, 0x80102030, 0x80405060, 0); v9(36, 44, 1, 1, 2, 1);
    hdr_face2(OP, GROUP(0) | SHADOW | VOLUME | COL(2), ISPW, TSPW, TCWW, TSP1, TCW1, f0, f1);              /* 10 */
    v9(38, 36, 1, F1, 0x3f000000, 0); v9(46, 36, 1, 0x3e800000, F1, 0); v9(38, 44, 1, 0, 0x3b808081, 1);
    hdr_vol(OP, GROUP(0) | SHADOW | VOLUME | TEX, ISPW, TSPW, TCWW, TSP1, TCW1);                              /* 11 */
    v11(40, 36, 1, 1, 2, 3, 4, 5, 6, 7, 8, 0); v11(48, 36, 1, 11, 12, 13, 14, 15, 16, 17, 18, 0); v11(40, 44, 1, 21, 22, 23, 24, 25, 26, 27, 28, 1);
    hdr_vol(OP, GROUP(0) | SHADOW | VOLUME | TEX | OFFS, ISPW, TSPW, TCWW, TSP1, TCW1);
    v11(42, 36, 1, 1, 2, 3, 4, 5, 6, 7, 8, 0); v11(50, 36, 1, 11, 12, 13, 14, 15, 16, 17, 18, 0); v11(42, 44, 1, 21, 22, 23, 24, 25, 26, 27, 28, 1);
    hdr_vol(OP, GROUP(0) | SHADOW | VOLUME | TEX | UV16, ISPW, TSPW, TCWW, TSP1, TCW1);                       /* 12 */
    v12(44, 36, 1, 1, 3, 4, 5, 7, 8, 0); v12(52, 36, 1, 11, 13, 14, 15, 17, 18, 0); v12(44, 44, 1, 21, 23, 24, 25, 27, 28, 1);
    hdr_vol(OP, GROUP(0) | SHADOW | VOLUME | TEX | UV16 | OFFS, ISPW, TSPW, TCWW, TSP1, TCW1);
    v12(46, 36, 1, 1, 3, 4, 5, 7, 8, 0); v12(54, 36, 1, 11, 13, 14, 15, 17, 18, 0); v12(46, 44, 1, 21, 23, 24, 25, 27, 28, 1);
    hdr_face2(OP, GROUP(0) | SHADOW | VOLUME | COL(2) | TEX, ISPW, TSPW, TCWW, TSP1, TCW1, f0, f1);       /* 13 */
    v11(48, 36, 1, 1, 2, F1, 0x3f000000, 5, 6, 0x3e800000, F1, 0); v11(56, 36, 1, 1, 2, 0, 0, 5, 6, F1, F1, 0);
    v11(48, 44, 1, 1, 2, 0x3f7f0000, 0x3b808081, 5, 6, 0x3f400000, 0, 1);
    hdr_face2(OP, GROUP(0) | SHADOW | VOLUME | COL(2) | TEX | OFFS | UV16, ISPW, TSPW, TCWW, TSP1, TCW1, f0, f1);   /* 14 */
    v12(50, 36, 1, 1, F1, 0x3f000000, 5, 0x3e800000, F1, 0); v12(58, 36, 1, 1, 0, 0, 5, F1, F1, 0);
    v12(50, 44, 1, 1, 0x3f7f0000, 0x3b808081, 5, 0x3f400000, 0, 1);
    hdr_vol(OP, GROUP(0) | SHADOW | VOLUME | COL(1), ISPW, TSPW, TCWW, TSP1, TCW1);                         /* float: no such type */
    v9(52, 36, 1, F1, 0x3f000000, 0); v9(60, 36, 1, 0x3e800000, F1, 0); v9(52, 44, 1, 0, 0x3b808081, 1);
    eol(); finish();
}

/* ======== cases: strips ======== */

static void c_strip_len(void) {                 /* one ten-vertex strip under Strip_Len 0 .. 3 */
    int len;
    begin_default();
    for(len = 0; len < 4; len++) {
        hdr(OP, GROUP(len), ISPW, TSPW, 0);
        ten_strip(36.0f + 12.0f * (float)len, 0x1000u * (uint32_t)(len + 1));
    }
    eol(); finish();
}
static void c_strip_lengths(void) {             /* strips of 3 .. 12 vertices, Strip_Len 3 */
    int n, i;
    begin_default();
    hdr(OP, GROUP(3), ISPW, TSPW, 0);
    for(n = 3; n <= 12; n++)
        for(i = 0; i < n; i++)
            v0(36.0f + 2.0f * (float)(i / 2), 36.0f + 2.0f * (float)(n - 3) + 1.0f * (float)(i & 1), 1, 0x100u * (uint32_t)n + (uint32_t)i, i == n - 1);
    eol(); finish();
}
static void c_strip_parity(void) {              /* seven vertices split under Strip_Len 0 and 1: the order of the corners */
    int len, i;
    begin_default();
    for(len = 0; len < 2; len++) {
        hdr(OP, GROUP(len), ISPW, TSPW, 0);
        for(i = 0; i < 7; i++)
            v0(36.0f + 4.0f * (float)(i / 2), 36.0f + 20.0f * (float)len + 8.0f * (float)(i & 1), 1, 0x10u * (uint32_t)len + (uint32_t)i, i == 6);
    }
    eol(); finish();
}
static void c_tri_stack(void) {                 /* single triangles one after another: triangle arrays */
    int i;
    begin_default();
    hdr(OP, GROUP(0), ISPW, TSPW, 0);
    for(i = 0; i < 20; i++) {                   /* twenty in tile (1, 1) */
        v0(36, 36, 1, (uint32_t)i, 0); v0(44, 36, 1, (uint32_t)i, 0); v0(36, 44, 1, (uint32_t)i, 1);
    }
    for(i = 0; i < 6; i++) {                    /* alternating between tiles (1, 1) and (2, 1) */
        const float x = i & 1 ? 72.0f : 40.0f;
        v0(x, 36, 1, 0x100u + (uint32_t)i, 0); v0(x + 8, 36, 1, 0x100u + (uint32_t)i, 0); v0(x, 44, 1, 0x100u + (uint32_t)i, 1);
    }
    for(i = 0; i < 3; i++) {                    /* in tiles (1, 1) and (2, 1) both */
        v0(56, 36, 1, 0x200u + (uint32_t)i, 0); v0(72, 36, 1, 0x200u + (uint32_t)i, 0); v0(56, 44, 1, 0x200u + (uint32_t)i, 1);
    }
    hdr(OP, GROUP(0) | TEX, ISPW, TSPW, TCWW);  /* another skip: not stacked onto the last */
    v3(36, 36, 1, 0, 0, 1, 0, 0); v3(44, 36, 1, 0, 0, 2, 0, 0); v3(36, 44, 1, 0, 0, 3, 0, 1);
    hdr(OP, GROUP(0) | SHADOW, ISPW, TSPW, 0);  /* the shadow bit */
    v0(36, 36, 1, 1, 0); v0(44, 36, 1, 2, 0); v0(36, 44, 1, 3, 1);
    hdr(OP, GROUP(0), ISPW, TSPW, 0);           /* the same kind again after a new header */
    v0(36, 36, 1, 1, 0); v0(44, 36, 1, 2, 0); v0(36, 44, 1, 3, 1);
    v0(36, 36, 1, 1, 0); v0(44, 36, 1, 2, 0); v0(36, 44, 1, 3, 1);
    eol(); finish();
}
static void c_strip_tiles(void) {               /* an eight-vertex strip across four tiles: which triangles each tile gets */
    int i;
    begin_default();
    hdr(OP, GROUP(3), ISPW, TSPW, 0);
    for(i = 0; i < 8; i++)
        v0(4.0f + 16.0f * (float)(i / 2) + (float)(i & 1) * 6.0f, 40.0f + 12.0f * (float)(i & 1), 1, (uint32_t)i, i == 7);
    hdr(OP, GROUP(3), ISPW, TSPW, 0);           /* a strip whose triangles fall in tiles far apart */
    v0(4, 4, 1, 1, 0); v0(12, 4, 1, 2, 0); v0(4, 12, 1, 3, 0); v0(120, 88, 1, 4, 0); v0(124, 92, 1, 5, 0); v0(120, 92, 1, 6, 1);
    eol(); finish();
}

/* ======== cases: tiles a polygon goes to ======== */

static void c_bin_edges(void) {                 /* corners at and next to tile edges */
    static const float xs[9] = { 31.999f, 32.0f, 32.001f, 63.999f, 64.0f, 95.999f, 96.0f, 127.999f, 128.0f };
    static const float ys[9] = { 31.999f, 32.0f, 32.001f, 63.999f, 64.0f, 95.999f, 96.0f, 95.5f, 64.5f };
    int i;
    begin_default();
    for(i = 0; i < 9; i++) {
        tri(OP, 0, xs[i], 40, xs[i] - 4, 44, xs[i] - 4, 40, 0x100u + (uint32_t)i);       /* right corner at x */
        tri(OP, 0, 40, ys[i], 44, ys[i] - 4, 40, ys[i] - 4, 0x200u + (uint32_t)i);       /* bottom corner at y */
    }
    eol(); finish();
}
static void c_bin_negative(void) {              /* corners left of and above the screen */
    begin_default();
    tri(OP, 0, -0.0f, 40, 8, 40, -0.0f, 48, 1);
    tri(OP, 0, -0.001f, 40, 8, 40, 0, 48, 2);
    tri(OP, 0, -1, 40, 40, 40, -1, 48, 3);
    tri(OP, 0, -1000, -1000, 40, 40, -1000, 40, 4);
    tri(OP, 0, 40, -0.001f, 48, 8, 40, 8, 5);
    tri(OP, 0, -100, -100, -50, -100, -100, -50, 6);  /* wholly outside: left and above */
    eol(); finish();
}
static void c_bin_outside(void) {               /* wholly right of or below the tile grid, and partly */
    begin_default();
    tri(OP, 0, 128, 40, 140, 40, 128, 48, 1);
    tri(OP, 0, 40, 96, 48, 96, 40, 104, 2);
    tri(OP, 0, 1000, 1000, 2000, 1000, 1000, 2000, 3);
    tri(OP, 0, 120, 88, 200, 88, 120, 200, 4);
    tri(OP, 0, 640, 40, 648, 40, 640, 48, 5);
    eol(); finish();
}
static void c_bin_specials(void) {              /* infinite and NaN corners, huge ones */
    static const uint32_t sp[8] = { FINF, FNINF, FNAN, FNNAN, 0x7149f2ca, 0xf149f2ca, 0x4f000000, 0x4effffff };
    int i;
    begin_default();
    for(i = 0; i < 8; i++) {
        hdr(OP, GROUP(0), ISPW, TSPW, 0);
        v0b(sp[i], fl(40), F1, (uint32_t)(0x10 + i), 0); v0(48, 40, 1, 2, 0); v0(40, 48, 1, 3, 1);
        hdr(OP, GROUP(0), ISPW, TSPW, 0);
        v0b(fl(40), sp[i], F1, (uint32_t)(0x20 + i), 0); v0(48, 40, 1, 2, 0); v0(40, 48, 1, 3, 1);
    }
    eol(); finish();
}
static void c_bin_huge(void) {                  /* one triangle over every tile; one far beyond */
    begin_default();
    tri(OP, 0, -100, -100, 1000, -100, -100, 1000, 1);
    tri(OP, 0, 0, 0, 1e9f, 0, 0, 1e9f, 2);
    eol(); finish();
}

/* ======== cases: user tile clip ======== */

static void clip_set(uint32_t mode) {           /* clip tiles (1, 1) .. (2, 1); polygons inside, across, outside it */
    begin_default();
    uclip(1, 1, 2, 1);
    tri(OP, UCLIP(mode), 40, 40, 56, 40, 40, 56, 1);        /* inside */
    tri(OP, UCLIP(mode), 8, 40, 56, 40, 8, 56, 2);          /* across its left edge */
    tri(OP, UCLIP(mode), 8, 8, 120, 8, 8, 88, 3);           /* over all of it and more */
    tri(OP, UCLIP(mode), 104, 72, 120, 72, 104, 88, 4);     /* outside */
    tri(OP, 0, 104, 72, 120, 72, 104, 88, 5);               /* outside, no clip mode */
    eol(); finish();
}
static void c_uclip_inside(void) { clip_set(2); }
static void c_uclip_outside(void) { clip_set(3); }
static void c_uclip_reserved(void) { clip_set(1); }
static void c_uclip_bounds(void) {              /* a rectangle beyond the grid; one with its corners swapped */
    begin_default();
    uclip(2, 1, 63, 15);
    tri(OP, UCLIP(2), 8, 8, 120, 8, 8, 88, 1);
    uclip(2, 2, 1, 1);
    tri(OP, UCLIP(2), 8, 8, 120, 8, 8, 88, 2);
    tri(OP, UCLIP(3), 8, 8, 120, 8, 8, 88, 3);
    uclip(0xffffffc1, 0xfffffff1, 0xffffffc2, 0xfffffff2);  /* bits above the fields */
    tri(OP, UCLIP(2), 8, 8, 120, 8, 8, 88, 4);
    eol(); finish();
}
static void c_uclip_order(void) {               /* which clip rectangle a polygon gets: the one before it, or the next */
    begin_default();
    uclip(0, 0, 0, 0);
    tri(OP, UCLIP(2), 8, 8, 120, 8, 8, 88, 1);
    uclip(3, 2, 3, 2);
    tri(OP, UCLIP(2), 8, 8, 120, 8, 8, 88, 2);
    uclip(1, 1, 1, 1);
    hdr(OP, GROUP(0) | UCLIP(2), ISPW, TSPW, 0);            /* two strips under one header, the clip between */
    v0(8, 8, 1, 3, 0); v0(120, 8, 1, 3, 0); v0(8, 88, 1, 3, 1);
    uclip(2, 1, 2, 1);
    v0(8, 8, 1, 4, 0); v0(120, 8, 1, 4, 0); v0(8, 88, 1, 4, 1);
    eol(); finish();
}

/* ======== cases: sprites ======== */

static void c_sprites(void) {
    begin_default();
    hdr_sprite(OP, GROUP(0), ISPW, TSPW, TCWW, 0xff112233, 0x00445566);           /* untextured, in tile (1, 1) */
    vsprite(36, 36, 1, 52, 36, 1, 52, 52, 1, 36, 52, 0, 0, 0);
    vsprite(28, 36, 1, 72, 36, 1, 72, 52, 1, 28, 52, 0, 0, 0);                    /* across three tiles */
    vsprite(36, 36, 1, 52, 36, 1, 52, 52, 1, 100, 90, 0, 0, 0);                   /* the fourth corner far away */
    vsprite(120, 88, 1, 136, 88, 1, 136, 104, 1, 120, 104, 0, 0, 0);             /* across the grid's corner */
    hdr_sprite(OP, GROUP(0) | TEX | OFFS, ISPW, TSPW, TCWW, 0xff112233, 0x00445566); /* textured, offset */
    vsprite(40, 40, 0.5f, 56, 40, 0.25f, 56, 56, 0.125f, 40, 56, 0x3f803f00, 0x3f003f80, 0x3e803e00);
    hdr_sprite(OP, GROUP(0) | TEX | UV16, ISPW, TSPW, TCWW, 0xff112233, 0x00445566);
    vsprite(44, 44, 1, 60, 44, 1, 60, 60, 1, 44, 60, 1, 2, 3);
    hdr_sprite(OP, GROUP(0) | COL(1) | GOUR | SHADOW, ISPW, TSPW, TCWW, 0xff112233, 0x00445566);   /* other bits */
    vsprite(48, 48, 1, 60, 48, 1, 60, 60, 1, 48, 60, 0, 0, 0);
    eol(); finish();
}
static void c_sprite_no_eos(void) {             /* sprite vertices without the end of strip bit */
    uint32_t w[16] = { PCW_(P_VTX, 0, 0), fl(36), fl(36), F1, fl(52), fl(36), F1, fl(52), fl(52), F1, fl(36), fl(52), 0, 0, 0, 0 };
    begin_default();
    hdr_sprite(OP, GROUP(0), ISPW, TSPW, TCWW, 0xff112233, 0);
    w16(w);
    w[1] = fl(72); w[4] = fl(88); w[7] = fl(88); w[10] = fl(72);
    w16(w);
    tri(OP, 0, 40, 72, 56, 72, 40, 88, 1);
    eol(); finish();
}

/* ======== cases: modifier volumes ======== */

static void c_modvol_bounds(void) {             /* a volume of three triangles in different tiles, closed by the last */
    begin_default();
    mvtri(OM, 0, 4, 4, 12, 4, 4, 12);           /* tile (0, 0) */
    mvtri(OM, 0, 100, 4, 108, 4, 100, 12);      /* tile (3, 0) */
    mvtri(OM, 1, 40, 72, 48, 72, 40, 80);       /* tile (1, 2), inside last */
    mvtri(OM, 0, 72, 40, 80, 40, 72, 48);       /* a second volume: tile (2, 1) */
    mvtri(OM, 2, 72, 72, 80, 72, 72, 80);       /* tile (2, 2), outside last */
    eol();
    mvtri(TM, 0, 4, 4, 12, 4, 4, 12);           /* the same in the translucent modifier list */
    mvtri(TM, 1, 100, 72, 108, 72, 100, 80);
    eol(); finish();
}
static void c_modvol_volume_bit(void) {         /* the header's Volume bit (and others) on modifier volume triangles */
    begin_default();
    hdr_mv(OM, VOLUME, 0); vmv(4, 4, 1, 12, 4, 1, 4, 12, 1);
    hdr_mv(OM, 0, 0); vmv(100, 4, 1, 108, 4, 1, 100, 12, 1);
    hdr_mv(OM, VOLUME, 1); vmv(40, 72, 1, 48, 72, 1, 40, 80, 1);
    hdr_mv(OM, SHADOW | TEX | GOUR | UV16 | GROUP(3), 1); vmv(72, 40, 1, 80, 40, 1, 72, 48, 1);
    hdr_mv(OM, 0, 7); vmv(72, 72, 1, 80, 72, 1, 72, 80, 1);   /* volume mode 7 */
    eol(); finish();
}
static void c_modvol_isp(void) {                /* ISP words of modifier volume headers: which bits are kept */
    static const uint32_t isps[4] = { 0x20000000, 0x3fffffff, 0x58000000, 0xffffffff };
    int i;
    begin_default();
    for(i = 0; i < 4; i++) {
        list_of(OM);
        w8(PCW_(P_POLY, OM, 0), isps[i], 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff);
        vmv(4.0f + 8.0f * (float)i, 4, 1, 12.0f + 8.0f * (float)i, 4, 1, 4.0f + 8.0f * (float)i, 12, 1);
    }
    eol(); finish();
}

/* ======== cases of bad input (TA_ERRORS) ======== */

static void c_err_vertex_first(void) {          /* a vertex before any header */
    begin_default();
    v0(40, 40, 1, 1, 0); v0(48, 40, 1, 2, 0); v0(40, 48, 1, 3, 1);
    tri(OP, 0, 40, 40, 56, 40, 40, 56, 4); eol(); finish();
}
static void c_err_eol_in_strip(void) {          /* the end of the list in the middle of a strip */
    begin_default();
    hdr(OP, GROUP(0), ISPW, TSPW, 0); v0(40, 40, 1, 1, 0); v0(48, 40, 1, 2, 0);
    eol();
    tri(TR, 0, 40, 40, 56, 40, 40, 56, 4); eol(); finish();
}
static void c_err_hdr_in_strip(void) {          /* a header in the middle of a strip */
    begin_default();
    hdr(OP, GROUP(0), ISPW, TSPW, 0); v0(40, 40, 1, 1, 0); v0(48, 40, 1, 2, 0);
    tri(OP, 0, 72, 40, 88, 40, 72, 56, 4); eol(); finish();
}
static void c_err_sprite_in_strip(void) {       /* a sprite header in the middle of a strip */
    begin_default();
    hdr(OP, GROUP(0), ISPW, TSPW, 0); v0(40, 40, 1, 1, 0); v0(48, 40, 1, 2, 0);
    hdr_sprite(OP, GROUP(0), ISPW, TSPW, TCWW, 0xff112233, 0);
    vsprite(72, 36, 1, 88, 36, 1, 88, 52, 1, 72, 52, 0, 0, 0);
    eol(); finish();
}
static void c_err_clip_in_strip(void) {         /* a user clip in the middle of a strip */
    begin_default();
    hdr(OP, GROUP(0) | UCLIP(2), ISPW, TSPW, 0); v0(8, 8, 1, 1, 0); v0(120, 8, 1, 2, 0);
    uclip(1, 1, 1, 1);
    v0(8, 88, 1, 3, 1);
    tri(OP, UCLIP(2), 8, 8, 120, 8, 8, 88, 4); eol(); finish();
}
static void c_err_short_strip(void) {           /* end of strip after two vertices */
    begin_default();
    hdr(OP, GROUP(0), ISPW, TSPW, 0); v0(40, 40, 1, 1, 0); v0(48, 40, 1, 2, 1);
    tri(OP, 0, 72, 40, 88, 40, 72, 56, 4); eol(); finish();
}
static void c_err_vertex_after_eol(void) {      /* a vertex after the end of the list */
    begin_default();
    tri(OP, 0, 40, 40, 56, 40, 40, 56, 1); eol();
    v0(72, 40, 1, 2, 0); v0(88, 40, 1, 3, 0); v0(72, 56, 1, 4, 1);
    finish();
}
static void c_err_eol_only(void) {              /* an end of list with no list open */
    begin_default();
    w8(0, 0, 0, 0, 0, 0, 0, 0);
    tri(OP, 0, 40, 40, 56, 40, 40, 56, 1); eol(); finish();
}
static void c_err_list_switch(void) {           /* a header of another list while a list is open */
    begin_default();
    tri(OP, 0, 40, 40, 56, 40, 40, 56, 1);
    tri(TR, 0, 72, 40, 88, 40, 72, 56, 2);
    eol(); finish();
}
static void c_err_para_types(void) {            /* parameter types 2 (object list set), 3 and 6 */
    begin_default();
    tri(OP, 0, 40, 40, 56, 40, 40, 56, 1);
    w8(PCW_(P_OLSET, OP, 0), 0, 0, 0, 0, 0, 0, 0);
    w8(PCW_(3, OP, 0), 0, 0, 0, 0, 0, 0, 0);
    w8(PCW_(6, OP, 0), 0, 0, 0, 0, 0, 0, 0);
    tri(OP, 0, 72, 40, 88, 40, 72, 56, 2);
    eol(); finish();
}

/* ======== the cases, in the order run ======== */

typedef struct { const char *name; void (*fn)(void); int error; } tcase_t;
static const tcase_t cases[] = {
    { "empty_op", c_empty_op, 0 }, { "all_lists", c_all_lists, 0 }, { "lists_reverse", c_lists_reverse, 0 },
    { "list_twice", c_list_twice, 0 }, { "opb_8", c_opb_8, 0 }, { "opb_16", c_opb_16, 0 }, { "opb_32", c_opb_32, 0 },
    { "opb_none", c_opb_none, 0 }, { "opb_mixed", c_opb_mixed, 0 }, { "grid_1x1", c_grid_1x1, 0 },
    { "grid_8x2", c_grid_8x2, 0 }, { "grid_20x15", c_grid_20x15, 0 }, { "grid_64x16", c_grid_64x16, 0 },
    { "grow_down", c_grow_down, 0 }, { "opb_gap", c_opb_gap, 0 }, { "ol_limit", c_ol_limit, 0 },
    { "ol_limit_down", c_ol_limit_down, 0 }, { "ol_limit_at_base", c_ol_limit_at_base, 0 },
    { "ol_limit_below", c_ol_limit_below, 0 }, { "isp_limit", c_isp_limit, 0 }, { "isp_base_odd", c_isp_base_odd, 0 },
    { "list_cont", c_list_cont, 0 },
    { "obj_bits", c_obj_bits, 0 }, { "obj_bits_isp1", c_obj_bits_isp1, 0 }, { "obj_bits_isp0", c_obj_bits_isp0, 0 },
    { "isp_single_bits", c_isp_single_bits, 0 }, { "tsp_tcw", c_tsp_tcw, 0 }, { "shadow_bit", c_shadow_bit, 0 },
    { "two_volume_hdrs", c_two_volume_hdrs, 0 }, { "single_tile", c_single_tile, 0 }, { "col_type_hdrs", c_col_type_hdrs, 0 },
    { "uclip_field_only", c_uclip_field_only, 0 }, { "group_en", c_group_en, 0 },
    { "packed_colours", c_packed_colours, 0 }, { "float_sweep", c_float_sweep, 0 }, { "float_sweep_tex", c_float_sweep_tex, 0 },
    { "int1_sweep_a", c_int1_sweep_a, 0 }, { "int1_sweep_b", c_int1_sweep_b, 0 }, { "int2_reuse", c_int2_reuse, 0 },
    { "int_offset", c_int_offset, 0 }, { "tex_packed", c_tex_packed, 0 }, { "untex_bits", c_untex_bits, 0 },
    { "two_vol_types", c_two_vol_types, 0 },
    { "strip_len", c_strip_len, 0 }, { "strip_lengths", c_strip_lengths, 0 }, { "strip_parity", c_strip_parity, 0 },
    { "tri_stack", c_tri_stack, 0 }, { "strip_tiles", c_strip_tiles, 0 },
    { "bin_edges", c_bin_edges, 0 }, { "bin_negative", c_bin_negative, 0 }, { "bin_outside", c_bin_outside, 0 },
    { "bin_specials", c_bin_specials, 0 }, { "bin_huge", c_bin_huge, 0 },
    { "uclip_inside", c_uclip_inside, 0 }, { "uclip_outside", c_uclip_outside, 0 }, { "uclip_reserved", c_uclip_reserved, 0 },
    { "uclip_bounds", c_uclip_bounds, 0 }, { "uclip_order", c_uclip_order, 0 },
    { "sprites", c_sprites, 0 }, { "sprite_no_eos", c_sprite_no_eos, 0 },
    { "modvol_bounds", c_modvol_bounds, 0 }, { "modvol_volume_bit", c_modvol_volume_bit, 0 }, { "modvol_isp", c_modvol_isp, 0 },
    { "err_vertex_first", c_err_vertex_first, 1 }, { "err_eol_in_strip", c_err_eol_in_strip, 1 },
    { "err_hdr_in_strip", c_err_hdr_in_strip, 1 }, { "err_sprite_in_strip", c_err_sprite_in_strip, 1 },
    { "err_clip_in_strip", c_err_clip_in_strip, 1 }, { "err_short_strip", c_err_short_strip, 1 },
    { "err_vertex_after_eol", c_err_vertex_after_eol, 1 }, { "err_eol_only", c_err_eol_only, 1 },
    { "err_list_switch", c_err_list_switch, 1 }, { "err_para_types", c_err_para_types, 1 },
};
#define NCASES ((int)(sizeof(cases) / sizeof(cases[0])))

int main(int argc, char **argv) {
    int i;
    (void)argc; (void)argv;
#ifndef _arch_dreamcast
    if(argc > 1 && !strcmp(argv[1], "--labels")) {
        for(i = 0; i < NCASES; i++)
            printf("CASE %d %s %d\n", i, cases[i].name, cases[i].error);
        return 0;
    }
    if(argc > 1)
        pvrio_folder = argv[1];
#endif
    pvrio_open();
    for(i = 0; i < NCASES; i++) {
        if(cases[i].error != TA_ERRORS)
            continue;
        case_no = i;
        cases[i].fn();
    }
    pvrio_reg(SOFTRESET_addr, 1);
    pvrio_reg(SOFTRESET_addr, 0);
    pvrio_close();
    return 0;
}
