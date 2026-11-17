/* The cases of ta-probe (see main.c here). This file is included by ../ta-tests/main.c in place of its case
   table, so everything there (begin, finish, mid, the parameter builders) is at hand. Each case says the
   question it asks; the default configuration is that of ta-tests (4 x 3 tiles, 8 word blocks for every list). */

/* a small triangle inside tile (x, y) */
static void tri_xy(int list, uint32_t flags, int x, int y, uint32_t col) {
    const float px = 32.0f * (float)x + 8.0f, py = 32.0f * (float)y + 8.0f;
    tri(list, flags, px, py, px + 8.0f, py, px, py + 8.0f, col);
}
/* a triangle whose bounds hold every tile of the 4 x 3 grid */
static void tri_all(int list, uint32_t flags, uint32_t col) {
    tri(list, flags, 4, 4, 124, 4, 4, 92, col);
}
/* a modifier volume triangle inside tile (x, y); mode: ISP bits 31-29 (0 normal, 1 inside last, 2 outside last) */
static void mv_xy(int list, uint32_t mode, int x, int y) {
    const float px = 32.0f * (float)x + 8.0f, py = 32.0f * (float)y + 8.0f;
    mvtri(list, mode, px, py, px + 8.0f, py, px, py + 8.0f);
}
/* n quads (strips of four, one entry each) inside tile (x, y) */
static void quads_xy(int list, int x, int y, int n) {
    int i;
    for(i = 0; i < n; i++)
        quad(list, 0, 32.0f * (float)x + 4.0f + (float)(i % 8), 32.0f * (float)y + 4.0f + (float)(i / 8), 16, 16,
             0xff000000u | (uint32_t)(x << 12 | y << 8 | i));
}
/* a sprite inside tile (x, y) */
static void sprite_xy(int list, int x, int y) {
    const float px = 32.0f * (float)x + 8.0f, py = 32.0f * (float)y + 8.0f;
    hdr_sprite(list, GROUP(0), ISPW, TSPW, TCWW, 0xff112233, 0);
    vsprite(px, py, 1, px + 16, py, 1, px + 16, py + 16, 1, px, py + 16, 0, 0, 0);
}
/* on a console: until the end-of-list interrupt of the list (at most 100 ms) */
static void wait_done(int list) {
#ifdef _arch_dreamcast
    uint64_t t0 = timer_ms_gettime64();
    while(!(pvrio_ta_events() & PVRIO_LIST_DONE(list)) && timer_ms_gettime64() - t0 < 100)
        ;
#else
    (void)list;
#endif
}

/* ======== the user clip rectangle no one has set (first: before any case sends a clip parameter) ======== */

static void p_uclip_default(void) {             /* which tiles is the rectangle after SOFTRESET, with none sent? */
    begin_default();
    tri_all(OP, UCLIP(2), 1);                       /* inside: only the tiles of the rectangle */
    tri_all(OP, UCLIP(3), 2);                       /* outside: the others */
    tri_all(OP, UCLIP(1), 3);                       /* mode 1 */
    eol(); finish();
}

/* ======== TA_OL_POINTERS: what the 600 registers hold ======== */

static void p_olp_reset(void) {                 /* after SOFTRESET, TA_LIST_INIT, a header, a triangle, the end */
    tacfg_t c = cfg();
    begin_ex(&c, NO_INIT); mid();                   /* m0: SOFTRESET, registers written, no TA_LIST_INIT */
    pvrio_reg(TA_LIST_INIT_addr, 0x80000000u); mid();   /* m1: TA_LIST_INIT */
    hdr(OP, GROUP(0), ISPW, TSPW, 0); mid();        /* m2: a header: has the list begun (slots, end words)? */
    v0(40, 40, 1, 1, 0); v0(48, 40, 1, 2, 0); v0(40, 48, 1, 3, 1); mid();   /* m3: a triangle in (1, 1) */
    eol(); finish();
}
static void p_olp_entries(void) {               /* (0, 0) one triangle; (1, 1) 9 quads, into a block; (2, 1) 7, a full slot */
    begin_default();
    tri_xy(OP, 0, 0, 0, 1);
    quads_xy(OP, 1, 1, 9);
    quads_xy(OP, 2, 1, 7);
    mid();                                          /* m0: before the end of the list */
    eol(); finish();
}
static void p_olp_sizes(void) {                 /* opaque 16 word blocks, translucent 32: 20 quads each in (1, 1) */
    tacfg_t c = cfg();
    c.opb[0] = 2; c.opb[1] = 0; c.opb[2] = 3; c.opb[3] = 0; c.opb[4] = 0;
    c.opb_init = c.ol_base + matrix_bytes(&c);
    begin(&c);
    quads_xy(OP, 1, 1, 20); eol(); wait_done(OP); mid();    /* m0: after the opaque list */
    quads_xy(TR, 1, 1, 20); eol(); finish();
}
static void p_olp_down(void) {                  /* growing down: 20 quads in (1, 1), 9 in (2, 1) */
    tacfg_t c = cfg();
    c.down = 1; c.ol_base = AREA + 0x2000u; c.opb_init = AREA + 0x2000u; c.ol_limit = AREA;
    begin(&c); quads_xy(OP, 1, 1, 20); quads_xy(OP, 2, 1, 9); mid(); eol(); finish();
}

/* ======== grids: past 300 tiles (20 x 15, 640 x 480), past 600 (the number of registers), past 480 lines ======== */

/* A quad over every tile, then a small triangle in tiles 0, 299, 300, 599, 600 and the last (those there are):
   those tiles get a second entry. All of video memory is filled first and searched afterwards. */
static void grid_probe(int tx, int ty) {
    tacfg_t c = cfg();
    const int n = tx * ty;
    const int at[6] = { 0, 299, 300, 599, 600, n - 1 };
    int k, j;
    c.tx = tx; c.ty = ty;
    c.opb[1] = c.opb[2] = c.opb[3] = c.opb[4] = 0;
    c.isp_base = AREA + 0x8000u; c.isp_limit = AREA + 0xc000u;
    c.opb_init = AREA + 0xc000u; c.ol_limit = AREA + 0x10000u;
    begin_ex(&c, SCAN);
    quad(OP, 0, 0, 0, 32.0f * (float)tx, 32.0f * (float)ty, 0xff112233);
    for(k = 0; k < 6; k++) {
        for(j = 0; j < k && at[j] != at[k]; j++)
            ;
        if(at[k] < n && j == k)
            tri_xy(OP, 0, at[k] % tx, at[k] / tx, 0xff000000u | (uint32_t)at[k]);
    }
    eol(); finish();
}
#define GRID(a, b) static void p_grid_##a##x##b(void) { grid_probe(a, b); }
GRID(20, 15) GRID(21, 15) GRID(20, 16) GRID(30, 10) GRID(31, 10) GRID(40, 15) GRID(41, 15) GRID(60, 10)
GRID(37, 16) GRID(38, 16) GRID(32, 16) GRID(64, 1) GRID(1, 16) GRID(64, 15) GRID(63, 16) GRID(64, 16)

/* ======== the user clip ======== */

static void p_uclip_after_reset(void) {         /* a rectangle, then SOFTRESET and TA_LIST_INIT: is it kept? */
    tacfg_t c = cfg();
    begin(&c);
    uclip(1, 1, 2, 1); tri_xy(OP, UCLIP(2), 1, 1, 1); eol(); wait_done(OP); mid();
    begin_ex(&c, NO_FILL);
    tri_all(OP, UCLIP(2), 2); tri_all(OP, UCLIP(3), 3); eol(); finish();
}
static void p_uclip_after_init(void) {          /* the same with TA_LIST_INIT only */
    tacfg_t c = cfg();
    begin(&c);
    uclip(1, 1, 2, 1); tri_xy(OP, UCLIP(2), 1, 1, 1); eol(); wait_done(OP); mid();
    begin_ex(&c, NO_FILL | NO_RESET);
    tri_all(OP, UCLIP(2), 2); tri_all(OP, UCLIP(3), 3); eol(); finish();
}
static void p_uclip_next_list(void) {           /* a rectangle sent in the opaque list, used in the translucent */
    begin_default();
    uclip(1, 1, 2, 1); tri_xy(OP, 0, 0, 0, 1); eol();
    tri_all(TR, UCLIP(2), 2); tri_all(TR, UCLIP(3), 3); eol(); finish();
}
static void p_uclip_mode1(void) {               /* mode 1: inside the grid, across its left edge, off it left, off it right */
    begin_default();
    uclip(1, 1, 2, 1);
    tri_xy(OP, UCLIP(1), 1, 1, 1);
    tri(OP, UCLIP(1), -8, 40, 16, 40, -8, 56, 2);
    tri(OP, UCLIP(1), -40, 40, -16, 40, -40, 56, 3);
    tri(OP, UCLIP(1), 200, 40, 216, 40, 200, 56, 4);
    tri_xy(OP, 0, 3, 2, 5);
    eol(); finish();
}
static void p_uclip_mode1_stack(void) {         /* A, then B in mode 1 (parameters, no entries), then C: does C stack onto A? */
    begin_default();
    tri_xy(OP, 0, 1, 1, 1); tri_xy(OP, UCLIP(1), 1, 1, 2); tri_xy(OP, 0, 1, 1, 3);
    eol(); finish();
}
static void p_uclip_outside_stack(void) {       /* A in (1, 1); B outside (1, 1) over (0..2, 1); C in (1, 1): onto A? */
    begin_default();
    tri_xy(OP, 0, 1, 1, 1);
    uclip(1, 1, 1, 1);
    tri(OP, UCLIP(3), 8, 40, 88, 40, 8, 56, 2);
    tri_xy(OP, 0, 1, 1, 3);
    eol(); finish();
}
static void p_uclip_inside_single(void) {       /* inside a one tile rectangle, a triangle over 3 x 3 tiles: single tile bit? */
    begin_default();
    uclip(1, 1, 1, 1);
    tri(OP, UCLIP(2), 8, 8, 88, 8, 8, 88, 1);
    eol(); finish();
}

/* ======== the bounds of a modifier volume, in the first polygon of the next list (ta-tests all_lists) ======== */

static void p_mvleak_multi(void) {              /* two closed volumes, then three triangles: which get what? */
    begin_default();
    mv_xy(OM, 0, 0, 0); mv_xy(OM, 1, 1, 0);         /* volume 1: (0, 0), (1, 0), closed by "inside last" */
    mv_xy(OM, 2, 3, 0);                             /* volume 2: (3, 0), "outside last" */
    eol();
    tri_xy(TR, 0, 0, 2, 1); tri_xy(TR, 0, 1, 2, 2); tri_xy(TR, 0, 3, 2, 3);
    eol(); finish();
}
static void p_mvleak_open(void) {               /* a volume never closed */
    begin_default();
    mv_xy(OM, 0, 0, 0); eol();
    tri_xy(TR, 0, 3, 2, 1); tri_xy(TR, 0, 2, 2, 2);
    eol(); finish();
}
static void p_mvleak_sprite(void) {             /* the next polygon a sprite */
    begin_default();
    mv_xy(OM, 1, 0, 0); eol();
    sprite_xy(TR, 3, 2); tri_xy(TR, 0, 2, 2, 2);
    eol(); finish();
}
static void p_mvleak_strip(void) {              /* the next polygon a strip of four */
    begin_default();
    mv_xy(OM, 1, 0, 0); eol();
    quad(TR, 0, 104, 72, 16, 16, 1); tri_xy(TR, 0, 2, 2, 2);
    eol(); finish();
}
static void p_mvleak_offgrid(void) {            /* the next polygon off the grid: dropped, or in the tiles of the volume? */
    begin_default();
    mv_xy(OM, 1, 1, 1); eol();
    tri(TR, 0, -40, 40, -16, 40, -40, 56, 1); tri_xy(TR, 0, 3, 2, 2);
    eol(); finish();
}
static void p_mvleak_two(void) {                /* two modifier lists, then a polygon */
    begin_default();
    mv_xy(OM, 1, 0, 0); eol();
    mv_xy(TM, 1, 3, 0); eol();
    tri_xy(TR, 0, 0, 2, 1);
    eol(); finish();
}
static void p_mvleak_op(void) {                 /* the opaque list after the modifier list, two triangles */
    begin_default();
    mv_xy(OM, 1, 0, 0); eol();
    tri_xy(OP, 0, 3, 2, 1); tri_xy(OP, 0, 2, 2, 2);
    eol(); finish();
}
static void p_mvleak_init(void) {               /* TA_LIST_INIT between the volume and the polygon */
    tacfg_t c = cfg();
    begin(&c);
    mv_xy(OM, 1, 0, 0); eol(); wait_done(OM); mid();
    begin_ex(&c, NO_FILL | NO_RESET);
    tri_xy(TR, 0, 3, 2, 1); eol(); finish();
}
static void p_mvleak_reset(void) {              /* SOFTRESET and TA_LIST_INIT between them */
    tacfg_t c = cfg();
    begin(&c);
    mv_xy(OM, 1, 0, 0); eol(); wait_done(OM); mid();
    begin_ex(&c, NO_FILL);
    tri_xy(TR, 0, 3, 2, 1); eol(); finish();
}
static void p_mvleak_clip(void) {               /* the next polygon clipped inside (2..3, 1..2) */
    begin_default();
    mv_xy(OM, 1, 0, 0); eol();
    uclip(2, 1, 3, 2);
    tri_xy(TR, UCLIP(2), 3, 2, 1);
    eol(); finish();
}

/* ======== sprites: every combination of PCW bits 0-6, without and with Shadow ======== */

static void sprite_bits(uint32_t sh) {
    uint32_t k;
    begin_default();
    for(k = 0; k < 128; k++) {
        hdr_sprite(OP, GROUP(0) | sh | k, ISPW, TSPW, TCWW, 0xff000000u | k, 0x00010000u | k);
        vsprite(36, 36, 1, 52, 36, 1, 52, 52, 1, 36, 52, 0x3c000000u | k, 0x3d000000u | k, 0x3e000000u | k);
    }
    eol(); finish();
}
static void p_sprite_bits(void) { sprite_bits(0); }
static void p_sprite_bits_sh(void) { sprite_bits(SHADOW); }

/* ======== object pointer blocks: filling, alignment, growing down, limits ======== */

static void p_opb_exact_7(void) {               /* 7 entries: the slot full to its last word but one */
    begin_default(); quads_xy(OP, 1, 1, 7); eol(); finish();
}
static void p_opb_exact_14(void) {              /* 14: the first block as full */
    begin_default(); quads_xy(OP, 1, 1, 14); eol(); finish();
}
static void p_opb_unaligned_init(void) {        /* 16 word blocks from 32 bytes past a 64 byte boundary */
    tacfg_t c = cfg();
    c.opb[1] = c.opb[2] = c.opb[3] = c.opb[4] = 0; c.opb[0] = 2;
    c.opb_init = c.ol_base + matrix_bytes(&c) + 0x20u;
    begin(&c); quads_xy(OP, 1, 1, 40); eol(); finish();
}
static void p_opb_unaligned_base(void) {        /* 16 word slots from TA_OL_BASE 32 bytes past a 64 byte boundary */
    tacfg_t c = cfg();
    c.opb[1] = c.opb[2] = c.opb[3] = c.opb[4] = 0; c.opb[0] = 2;
    c.ol_base = AREA + 0x20u;
    c.opb_init = c.ol_base + matrix_bytes(&c);
    begin(&c); quads_xy(OP, 1, 1, 40); eol(); finish();
}
static void p_ol_base_odd(void) {               /* TA_OL_BASE and TA_NEXT_OPB_INIT 16 bytes off a 32 byte boundary */
    tacfg_t c = cfg();
    c.ol_base = AREA + 0x10u;
    c.opb_init = c.ol_base + matrix_bytes(&c);
    begin(&c); tri_xy(OP, 0, 0, 0, 1); quads_xy(OP, 1, 1, 9); eol(); finish();
}
static void p_down_noalloc(void) {              /* growing down, no block taken: TA_NEXT_OPB */
    tacfg_t c = cfg();
    c.down = 1; c.ol_base = AREA + 0x2000u; c.opb_init = AREA + 0x2000u; c.ol_limit = AREA;
    begin(&c); tri_xy(OP, 0, 1, 1, 1); eol(); finish();
}
static void p_down_sizes(void) {                /* growing down: opaque 8 word blocks, translucent 32 */
    tacfg_t c = cfg();
    c.down = 1; c.opb[0] = 1; c.opb[1] = 0; c.opb[2] = 3; c.opb[3] = 0; c.opb[4] = 0;
    c.ol_base = AREA + 0x3000u; c.opb_init = AREA + 0x3000u; c.ol_limit = AREA;
    begin(&c);
    quads_xy(OP, 1, 1, 20); eol(); wait_done(OP); mid();
    quads_xy(TR, 2, 1, 40); eol(); finish();
}
static void p_down_mid(void) {                  /* growing down: TA_NEXT_OPB right after the first block */
    tacfg_t c = cfg();
    c.down = 1; c.ol_base = AREA + 0x2000u; c.opb_init = AREA + 0x2000u; c.ol_limit = AREA;
    begin(&c); quads_xy(OP, 1, 1, 8); mid(); quads_xy(OP, 2, 1, 8); eol(); finish();
}
static void p_up_limit_two(void) {              /* room for two blocks, two tiles taking turns: shared at the end? */
    tacfg_t c = cfg();
    int i;
    c.ol_limit = c.opb_init + 2 * 32;
    begin(&c);
    for(i = 0; i < 12; i++) {
        quad(OP, 0, 36.0f + (float)(i % 8), 36, 16, 16, 0xff000100u | (uint32_t)i);
        quad(OP, 0, 68.0f + (float)(i % 8), 36, 16, 16, 0xff000200u | (uint32_t)i);
    }
    eol(); finish();
}
static void p_ol_limit_mid_matrix(void) {       /* TA_OL_LIMIT after the slot of tile 5: triangles in tiles 1, 5, 6, 10 */
    tacfg_t c = cfg();
    c.ol_limit = c.ol_base + 6 * 32;
    begin(&c);
    tri_xy(OP, 0, 1, 0, 1); tri_xy(OP, 0, 1, 1, 2); tri_xy(OP, 0, 2, 1, 3); tri_xy(OP, 0, 2, 2, 4);
    mid(); eol(); finish();
}
static void p_ol_limit_base_mid(void) {         /* TA_OL_LIMIT = TA_OL_BASE (ta-tests ol_limit_at_base): before the end */
    tacfg_t c = cfg();
    c.ol_limit = c.ol_base;
    begin(&c); tri_xy(OP, 0, 1, 1, 1); mid(); eol(); finish();
}
static void p_init_beyond_limit(void) {         /* the matrix below TA_OL_LIMIT, TA_NEXT_OPB_INIT above it */
    tacfg_t c = cfg();
    c.opb[1] = c.opb[2] = c.opb[3] = c.opb[4] = 0;
    c.ol_limit = c.ol_base + 0x300u;
    c.opb_init = c.ol_base + 0x400u;
    begin(&c); quads_xy(OP, 1, 1, 40); eol(); finish();
}
static void p_down_limit_above(void) {          /* growing down, TA_OL_LIMIT inside the matrix */
    tacfg_t c = cfg();
    c.down = 1; c.ol_base = AREA + 0x2000u; c.opb_init = AREA + 0x2000u; c.ol_limit = AREA + 0x2100u;
    begin(&c); tri_xy(OP, 0, 1, 1, 1); quads_xy(OP, 2, 1, 20); eol(); finish();
}
static void p_isp_limit_below(void) {           /* TA_ISP_LIMIT below TA_ISP_BASE */
    tacfg_t c = cfg();
    c.isp_limit = c.isp_base - 0x100u;
    begin(&c); tri_xy(OP, 0, 0, 0, 1); tri_xy(OP, 0, 1, 0, 2); tri_xy(OP, 0, 2, 0, 3); eol(); finish();
}
static void p_isp_limit_at(void) {              /* TA_ISP_LIMIT = TA_ISP_BASE */
    tacfg_t c = cfg();
    c.isp_limit = c.isp_base;
    begin(&c); tri_xy(OP, 0, 0, 0, 1); tri_xy(OP, 0, 1, 0, 2); tri_xy(OP, 0, 2, 0, 3); eol(); finish();
}
static void p_isp_limit_odd(void) {             /* TA_ISP_LIMIT not on a word: base + 0x102, in the fifth triangle */
    tacfg_t c = cfg();
    int i;
    c.isp_limit = c.isp_base + 0x102u;
    begin(&c);
    for(i = 0; i < 8; i++)
        tri_xy(OP, 0, i % 4, i / 4, (uint32_t)i + 1);
    eol(); finish();
}
static void p_isp_limit_next_list(void) {       /* the parameters run out in the opaque list; then the translucent list */
    tacfg_t c = cfg();
    c.isp_limit = c.isp_base + 64 * 4;
    begin(&c); quads_xy(OP, 1, 1, 8); eol(); tri_xy(TR, 0, 2, 1, 1); eol(); finish();
}

/* ======== what TA_LIST_CONT, TA_LIST_INIT and SOFTRESET keep ======== */

static void p_list_cont_base(void) {            /* TA_OL_BASE moved, then TA_LIST_CONT: where the translucent matrix goes */
    begin_default();
    tri_xy(OP, 0, 1, 1, 1); eol(); wait_done(OP);
    pvrio_reg(TA_OL_BASE_addr, AREA + 0x2000u);
    pvrio_reg(TA_LIST_CONT_addr, 0x80000000u);
    tri_xy(TR, 0, 2, 1, 2); eol(); finish();
}
static void p_list_cont_alloc(void) {           /* TA_NEXT_OPB_INIT moved, then TA_LIST_CONT: where blocks come from */
    begin_default();
    quads_xy(OP, 1, 1, 20); eol(); wait_done(OP);
    pvrio_reg(TA_NEXT_OPB_INIT_addr, AREA + 0x3000u);
    pvrio_reg(TA_LIST_CONT_addr, 0x80000000u);
    quads_xy(TR, 2, 1, 20); eol(); finish();
}
static void p_base_change(void) {               /* TA_OL_BASE moved between two lists, nothing else */
    begin_default();
    tri_xy(OP, 0, 1, 1, 1); eol(); wait_done(OP);
    pvrio_reg(TA_OL_BASE_addr, AREA + 0x2000u);
    tri_xy(TR, 0, 2, 1, 2); eol(); finish();
}
static void p_alloc_change(void) {              /* TA_ALLOC_CTRL changed between lists: translucent 8 -> 32 words */
    TA_ALLOC_CTRL_type a = { .full = 0 };
    begin_default();
    tri_xy(OP, 0, 1, 1, 1); eol(); wait_done(OP);
    a.O_OPB = 1; a.OM_OPB = 1; a.T_OPB = 3; a.TM_OPB = 1; a.PT_OPB = 1;
    pvrio_reg(TA_ALLOC_CTRL_addr, a.full);
    quads_xy(TR, 2, 1, 20); eol(); finish();
}
static void p_clip_change(void) {               /* TA_GLOB_TILE_CLIP changed between lists: 4 -> 8 tiles wide */
    TA_GLOB_TILE_CLIP_type g = { .full = 0 };
    begin_default();
    tri_xy(OP, 0, 1, 1, 1); eol(); wait_done(OP);
    g.tile_x_num = 7; g.tile_y_num = 2;
    pvrio_reg(TA_GLOB_TILE_CLIP_addr, g.full);
    tri_xy(TR, 0, 5, 1, 2); eol(); finish();
}
static void p_strip_len_init(void) {            /* Strip_Len 3 with Group_En, TA_LIST_INIT, a header without Group_En */
    tacfg_t c = cfg();
    begin(&c);
    hdr(OP, GROUP(3), ISPW, TSPW, 0); ten_strip(36, 0x100); eol(); wait_done(OP); mid();
    begin_ex(&c, NO_FILL | NO_RESET);
    hdr(OP, 0, ISPW, TSPW, 0); ten_strip(48, 0x200); eol(); finish();
}
static void p_strip_len_reset(void) {           /* the same across SOFTRESET */
    tacfg_t c = cfg();
    begin(&c);
    hdr(OP, GROUP(3), ISPW, TSPW, 0); ten_strip(36, 0x100); eol(); wait_done(OP); mid();
    begin_ex(&c, NO_FILL);
    hdr(OP, 0, ISPW, TSPW, 0); ten_strip(48, 0x200); eol(); finish();
}
static void p_int2_init(void) {                 /* a face colour, TA_LIST_INIT, intensity mode 2: which face colour? */
    tacfg_t c = cfg();
    begin(&c);
    hdr_face(OP, GROUP(0) | COL(2), ISPW, TSPW, 0, F1, fl(0.5f), fl(0.25f), fl(0.75f));
    v2(36, 36, 1, F1, 0); v2(44, 36, 1, F1, 0); v2(36, 44, 1, F1, 1); eol(); wait_done(OP); mid();
    begin_ex(&c, NO_FILL | NO_RESET);
    hdr(OP, GROUP(0) | COL(3), ISPW, TSPW, 0);
    v2(40, 36, 1, F1, 0); v2(48, 36, 1, fl(0.5f), 0); v2(40, 44, 1, F1, 1); eol(); finish();
}
static void p_int2_reset(void) {                /* the same across SOFTRESET */
    tacfg_t c = cfg();
    begin(&c);
    hdr_face(OP, GROUP(0) | COL(2), ISPW, TSPW, 0, F1, fl(0.5f), fl(0.25f), fl(0.75f));
    v2(36, 36, 1, F1, 0); v2(44, 36, 1, F1, 0); v2(36, 44, 1, F1, 1); eol(); wait_done(OP); mid();
    begin_ex(&c, NO_FILL);
    hdr(OP, GROUP(0) | COL(3), ISPW, TSPW, 0);
    v2(40, 36, 1, F1, 0); v2(48, 36, 1, fl(0.5f), 0); v2(40, 44, 1, F1, 1); eol(); finish();
}

/* ======== triangle arrays: when a triangle joins the entry before it ======== */

static void p_stack_culled(void) {              /* A, B off the grid (no parameters), C */
    begin_default();
    tri_xy(OP, 0, 1, 1, 1); tri(OP, 0, -40, 40, -16, 40, -40, 56, 2); tri_xy(OP, 0, 1, 1, 3);
    eol(); finish();
}
static void p_stack_strip(void) {               /* A, a strip of four elsewhere, C */
    begin_default();
    tri_xy(OP, 0, 1, 1, 1); quad(OP, 0, 72, 40, 16, 16, 2); tri_xy(OP, 0, 1, 1, 3);
    eol(); finish();
}
static void p_stack_20(void) {                  /* 20 triangles in one tile: 16 to an entry? */
    int i;
    begin_default();
    for(i = 0; i < 20; i++)
        tri_xy(OP, 0, 1, 1, (uint32_t)i);
    eol(); finish();
}
static void p_stack_20_sprites(void) {          /* 20 sprites in one tile */
    int i;
    begin_default();
    for(i = 0; i < 20; i++)
        sprite_xy(OP, 1, 1);
    eol(); finish();
}
static void p_stack_block_end(void) {           /* 6 quads, then triangles: the array in the last entry word of the slot */
    int i;
    begin_default();
    quads_xy(OP, 1, 1, 6);
    for(i = 0; i < 3; i++)
        tri_xy(OP, 0, 1, 1, (uint32_t)i);
    quads_xy(OP, 1, 1, 2);
    eol(); finish();
}

/* ======== bad input (TA_ERRORS 1, a run of its own) ======== */

/* after the bad input: is the TA still taking input? A triangle in (3, 2) and the end of the opaque list (m0), then
   TA_LIST_INIT alone (no SOFTRESET) and a triangle in (3, 1) */
static void after_error(void) {
    tacfg_t c = cfg();
    tri_xy(OP, 0, 3, 2, 0xe1); eol(); mid();
    begin_ex(&c, NO_FILL | NO_RESET);
    tri_xy(OP, 0, 3, 1, 0xe2); eol(); finish();
}
static void e_vertex_first(void) {              /* vertices before any header */
    begin_default();
    v0(40, 40, 1, 1, 0); v0(48, 40, 1, 2, 0); v0(40, 48, 1, 3, 1);
    after_error();
}
static void e_eol_in_strip(void) {              /* the end of the list after two vertices of a strip */
    begin_default();
    hdr(OP, GROUP(0), ISPW, TSPW, 0); v0(40, 40, 1, 1, 0); v0(48, 40, 1, 2, 0); eol();
    after_error();
}
static void e_hdr_in_strip(void) {              /* a header after two vertices */
    begin_default();
    hdr(OP, GROUP(0), ISPW, TSPW, 0); v0(40, 40, 1, 1, 0); v0(48, 40, 1, 2, 0);
    tri_xy(OP, 0, 2, 1, 2);
    after_error();
}
static void e_sprite_in_strip(void) {           /* a sprite header after two vertices */
    begin_default();
    hdr(OP, GROUP(0), ISPW, TSPW, 0); v0(40, 40, 1, 1, 0); v0(48, 40, 1, 2, 0);
    sprite_xy(OP, 2, 1);
    after_error();
}
static void e_clip_in_strip(void) {             /* a user clip after two vertices, then the third */
    begin_default();
    hdr(OP, GROUP(0) | UCLIP(2), ISPW, TSPW, 0); v0(8, 8, 1, 1, 0); v0(120, 8, 1, 2, 0);
    uclip(1, 1, 1, 1);
    v0(8, 88, 1, 3, 1);
    after_error();
}
static void e_short_strip(void) {               /* end of strip after two vertices */
    begin_default();
    hdr(OP, GROUP(0), ISPW, TSPW, 0); v0(40, 40, 1, 1, 0); v0(48, 40, 1, 2, 1);
    after_error();
}
static void e_vertex_after_eol(void) {          /* vertices after the end of the list */
    begin_default();
    tri_xy(OP, 0, 1, 1, 1); eol();
    v0(72, 40, 1, 2, 0); v0(88, 40, 1, 3, 0); v0(72, 56, 1, 4, 1);
    after_error();
}
static void e_eol_only(void) {                  /* an end of list with no list open */
    begin_default();
    w8(0, 0, 0, 0, 0, 0, 0, 0);
    after_error();
}
static void e_list_switch(void) {               /* a header of the translucent list while the opaque list is open */
    begin_default();
    tri_xy(OP, 0, 1, 1, 1); tri_xy(TR, 0, 2, 1, 2); eol();
    after_error();
}
static void e_list_types(void) {                /* headers of list types 5, 6 and 7, each with a triangle and an end */
    int t;
    begin_default();
    for(t = 5; t < 8; t++) {
        w8(PCW_(P_POLY, t, GROUP(0)), ISPW, TSPW, 0, 0, 0, 0, 0);
        v0(36.0f + (float)t, 36, 1, (uint32_t)t, 0); v0(44, 36, 1, (uint32_t)t, 0); v0(36, 44, 1, (uint32_t)t, 1);
        w8(0, 0, 0, 0, 0, 0, 0, 0);
    }
    after_error();
}
static void e_para_types(void) {                /* parameter types 2 (object list set), 3 and 6 between triangles */
    begin_default();
    tri_xy(OP, 0, 1, 1, 1);
    w8(PCW_(P_OLSET, OP, 0), 0, 0, 0, 0, 0, 0, 0);
    w8(PCW_(3, OP, 0), 0, 0, 0, 0, 0, 0, 0);
    w8(PCW_(6, OP, 0), 0, 0, 0, 0, 0, 0, 0);
    tri_xy(OP, 0, 2, 1, 2);
    after_error();
}
static void e_vtx_half(void) {                  /* the first half of a 64 byte vertex (textured, float colour), then a header */
    begin_default();
    hdr(OP, GROUP(0) | COL(1) | TEX, ISPW, TSPW, TCWW);
    w8(VP(0), fl(40), fl(40), F1, 0, 0, 0, 0);
    tri_xy(OP, 0, 2, 1, 2);
    after_error();
}
static void e_hdr_half(void) {                  /* the first half of a 64 byte header (intensity, textured, offset), then a vertex */
    begin_default();
    list_of(OP);
    w8(PCW_(P_POLY, OP, GROUP(0) | COL(2) | TEX | OFFS), ISPW, TSPW, TCWW, 0, 0, 0, 0);
    v7(40, 40, 1, 0, 0, F1, F1, 0); v7(48, 40, 1, 0, 0, F1, F1, 0); v7(40, 48, 1, 0, 0, F1, F1, 1);
    after_error();
}
static void e_sprite_in_mv(void) {              /* a sprite header in a modifier volume list */
    begin_default();
    mv_xy(OM, 1, 0, 0);
    sprite_xy(OM, 2, 1);
    eol();
    after_error();
}
static void e_init_in_strip(void) {             /* TA_LIST_INIT after two vertices of a strip */
    tacfg_t c = cfg();
    begin(&c);
    hdr(OP, GROUP(0), ISPW, TSPW, 0); v0(40, 40, 1, 1, 0); v0(48, 40, 1, 2, 0);
    begin_ex(&c, NO_FILL | NO_RESET);
    after_error();
}
static void e_cont_in_strip(void) {             /* TA_LIST_CONT after two vertices of a strip */
    begin_default();
    hdr(OP, GROUP(0), ISPW, TSPW, 0); v0(40, 40, 1, 1, 0); v0(48, 40, 1, 2, 0);
    pvrio_reg(TA_LIST_CONT_addr, 0x80000000u);
    after_error();
}

/* ======== the order run ======== */

static const tcase_t cases[] = {
    { "uclip_default", p_uclip_default, 0 },
    { "olp_reset", p_olp_reset, 0 }, { "olp_entries", p_olp_entries, 0 }, { "olp_sizes", p_olp_sizes, 0 },
    { "olp_down", p_olp_down, 0 },
    { "grid_20x15", p_grid_20x15, 0 }, { "grid_21x15", p_grid_21x15, 0 }, { "grid_20x16", p_grid_20x16, 0 },
    { "grid_30x10", p_grid_30x10, 0 }, { "grid_31x10", p_grid_31x10, 0 }, { "grid_40x15", p_grid_40x15, 0 },
    { "grid_41x15", p_grid_41x15, 0 }, { "grid_60x10", p_grid_60x10, 0 }, { "grid_37x16", p_grid_37x16, 0 },
    { "grid_38x16", p_grid_38x16, 0 }, { "grid_32x16", p_grid_32x16, 0 }, { "grid_64x1", p_grid_64x1, 0 },
    { "grid_1x16", p_grid_1x16, 0 }, { "grid_64x15", p_grid_64x15, 0 }, { "grid_63x16", p_grid_63x16, 0 },
    { "grid_64x16", p_grid_64x16, 0 },
    { "uclip_after_reset", p_uclip_after_reset, 0 }, { "uclip_after_init", p_uclip_after_init, 0 },
    { "uclip_next_list", p_uclip_next_list, 0 }, { "uclip_mode1", p_uclip_mode1, 0 },
    { "uclip_mode1_stack", p_uclip_mode1_stack, 0 }, { "uclip_outside_stack", p_uclip_outside_stack, 0 },
    { "uclip_inside_single", p_uclip_inside_single, 0 },
    { "mvleak_multi", p_mvleak_multi, 0 }, { "mvleak_open", p_mvleak_open, 0 }, { "mvleak_sprite", p_mvleak_sprite, 0 },
    { "mvleak_strip", p_mvleak_strip, 0 }, { "mvleak_offgrid", p_mvleak_offgrid, 0 }, { "mvleak_two", p_mvleak_two, 0 },
    { "mvleak_op", p_mvleak_op, 0 }, { "mvleak_init", p_mvleak_init, 0 }, { "mvleak_reset", p_mvleak_reset, 0 },
    { "mvleak_clip", p_mvleak_clip, 0 },
    { "sprite_bits", p_sprite_bits, 0 }, { "sprite_bits_sh", p_sprite_bits_sh, 0 },
    { "opb_exact_7", p_opb_exact_7, 0 }, { "opb_exact_14", p_opb_exact_14, 0 },
    { "opb_unaligned_init", p_opb_unaligned_init, 0 }, { "opb_unaligned_base", p_opb_unaligned_base, 0 },
    { "ol_base_odd", p_ol_base_odd, 0 }, { "down_noalloc", p_down_noalloc, 0 }, { "down_sizes", p_down_sizes, 0 },
    { "down_mid", p_down_mid, 0 }, { "up_limit_two", p_up_limit_two, 0 }, { "ol_limit_mid_matrix", p_ol_limit_mid_matrix, 0 },
    { "ol_limit_base_mid", p_ol_limit_base_mid, 0 }, { "init_beyond_limit", p_init_beyond_limit, 0 },
    { "down_limit_above", p_down_limit_above, 0 }, { "isp_limit_below", p_isp_limit_below, 0 },
    { "isp_limit_at", p_isp_limit_at, 0 }, { "isp_limit_odd", p_isp_limit_odd, 0 },
    { "isp_limit_next_list", p_isp_limit_next_list, 0 },
    { "list_cont_base", p_list_cont_base, 0 }, { "list_cont_alloc", p_list_cont_alloc, 0 },
    { "base_change", p_base_change, 0 }, { "alloc_change", p_alloc_change, 0 }, { "clip_change", p_clip_change, 0 },
    { "strip_len_init", p_strip_len_init, 0 }, { "strip_len_reset", p_strip_len_reset, 0 },
    { "int2_init", p_int2_init, 0 }, { "int2_reset", p_int2_reset, 0 },
    { "stack_culled", p_stack_culled, 0 }, { "stack_strip", p_stack_strip, 0 }, { "stack_20", p_stack_20, 0 },
    { "stack_20_sprites", p_stack_20_sprites, 0 }, { "stack_block_end", p_stack_block_end, 0 },
    { "err_vertex_first", e_vertex_first, 1 }, { "err_eol_in_strip", e_eol_in_strip, 1 },
    { "err_hdr_in_strip", e_hdr_in_strip, 1 }, { "err_sprite_in_strip", e_sprite_in_strip, 1 },
    { "err_clip_in_strip", e_clip_in_strip, 1 }, { "err_short_strip", e_short_strip, 1 },
    { "err_vertex_after_eol", e_vertex_after_eol, 1 }, { "err_eol_only", e_eol_only, 1 },
    { "err_list_switch", e_list_switch, 1 }, { "err_list_types", e_list_types, 1 },
    { "err_para_types", e_para_types, 1 }, { "err_vtx_half", e_vtx_half, 1 }, { "err_hdr_half", e_hdr_half, 1 },
    { "err_sprite_in_mv", e_sprite_in_mv, 1 }, { "err_init_in_strip", e_init_in_strip, 1 },
    { "err_cont_in_strip", e_cont_in_strip, 1 },
};
static const tgroup_t groups[] = { { "none", 0, 0 } };
