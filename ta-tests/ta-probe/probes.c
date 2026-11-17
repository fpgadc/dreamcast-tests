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
/* The same with SOFTRESET before TA_LIST_INIT. After err_sprite_in_strip, TA_LIST_INIT alone left a console unable
   to send more: the store queue write to the TA stalled the CPU (first error run, 2026-11-17). The cases after it
   recover this way. */
static void after_error_reset(void) {
    tacfg_t c = cfg();
    tri_xy(OP, 0, 3, 2, 0xe1); eol(); mid();
    begin_ex(&c, NO_FILL);
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
    after_error_reset();
}
static void e_clip_in_strip(void) {             /* a user clip after two vertices, then the third */
    begin_default();
    hdr(OP, GROUP(0) | UCLIP(2), ISPW, TSPW, 0); v0(8, 8, 1, 1, 0); v0(120, 8, 1, 2, 0);
    uclip(1, 1, 1, 1);
    v0(8, 88, 1, 3, 1);
    after_error_reset();
}
static void e_short_strip(void) {               /* end of strip after two vertices */
    begin_default();
    hdr(OP, GROUP(0), ISPW, TSPW, 0); v0(40, 40, 1, 1, 0); v0(48, 40, 1, 2, 1);
    after_error_reset();
}
static void e_vertex_after_eol(void) {          /* vertices after the end of the list */
    begin_default();
    tri_xy(OP, 0, 1, 1, 1); eol();
    v0(72, 40, 1, 2, 0); v0(88, 40, 1, 3, 0); v0(72, 56, 1, 4, 1);
    after_error_reset();
}
static void e_eol_only(void) {                  /* an end of list with no list open */
    begin_default();
    w8(0, 0, 0, 0, 0, 0, 0, 0);
    after_error_reset();
}
static void e_list_switch(void) {               /* a header of the translucent list while the opaque list is open */
    begin_default();
    tri_xy(OP, 0, 1, 1, 1); tri_xy(TR, 0, 2, 1, 2); eol();
    after_error_reset();
}
static void e_list_types(void) {                /* headers of list types 5, 6 and 7, each with a triangle and an end */
    int t;
    begin_default();
    for(t = 5; t < 8; t++) {
        w8(PCW_(P_POLY, t, GROUP(0)), ISPW, TSPW, 0, 0, 0, 0, 0);
        v0(36.0f + (float)t, 36, 1, (uint32_t)t, 0); v0(44, 36, 1, (uint32_t)t, 0); v0(36, 44, 1, (uint32_t)t, 1);
        w8(0, 0, 0, 0, 0, 0, 0, 0);
    }
    after_error_reset();
}
static void e_para_types(void) {                /* parameter types 2 (object list set), 3 and 6 between triangles */
    begin_default();
    tri_xy(OP, 0, 1, 1, 1);
    w8(PCW_(P_OLSET, OP, 0), 0, 0, 0, 0, 0, 0, 0);
    w8(PCW_(3, OP, 0), 0, 0, 0, 0, 0, 0, 0);
    w8(PCW_(6, OP, 0), 0, 0, 0, 0, 0, 0, 0);
    tri_xy(OP, 0, 2, 1, 2);
    after_error_reset();
}
static void e_vtx_half(void) {                  /* the first half of a 64 byte vertex (textured, float colour), then a header */
    begin_default();
    hdr(OP, GROUP(0) | COL(1) | TEX, ISPW, TSPW, TCWW);
    w8(VP(0), fl(40), fl(40), F1, 0, 0, 0, 0);
    tri_xy(OP, 0, 2, 1, 2);
    after_error_reset();
}
static void e_hdr_half(void) {                  /* the first half of a 64 byte header (intensity, textured, offset), then a vertex */
    begin_default();
    list_of(OP);
    w8(PCW_(P_POLY, OP, GROUP(0) | COL(2) | TEX | OFFS), ISPW, TSPW, TCWW, 0, 0, 0, 0);
    v7(40, 40, 1, 0, 0, F1, F1, 0); v7(48, 40, 1, 0, 0, F1, F1, 0); v7(40, 48, 1, 0, 0, F1, F1, 1);
    after_error_reset();
}
static void e_sprite_in_mv(void) {              /* a sprite header in a modifier volume list */
    begin_default();
    mv_xy(OM, 1, 0, 0);
    sprite_xy(OM, 2, 1);
    eol();
    after_error_reset();
}
static void e_init_in_strip(void) {             /* TA_LIST_INIT after two vertices of a strip */
    tacfg_t c = cfg();
    begin(&c);
    hdr(OP, GROUP(0), ISPW, TSPW, 0); v0(40, 40, 1, 1, 0); v0(48, 40, 1, 2, 0);
    begin_ex(&c, NO_FILL | NO_RESET);
    after_error_reset();
}
static void e_cont_in_strip(void) {             /* TA_LIST_CONT after two vertices of a strip */
    begin_default();
    hdr(OP, GROUP(0), ISPW, TSPW, 0); v0(40, 40, 1, 1, 0); v0(48, 40, 1, 2, 0);
    pvrio_reg(TA_LIST_CONT_addr, 0x80000000u);
    after_error_reset();
}

/* ======== second round (after the first console run of ta-probe) ======== */

static void p_mvleak_volumes(void) {            /* two closed volumes: does the second start the bounds afresh? */
    begin_default();
    mv_xy(OM, 1, 0, 0); mv_xy(OM, 1, 1, 0); eol();  /* volume A (0, 0), volume B (1, 0) */
    tri_xy(TR, 0, 2, 0, 1);                         /* (1..2, 0) if B started afresh, (0..2, 0) if not */
    eol(); finish();
}
static void p_mvleak_open2(void) {              /* two triangles of one volume never closed, both mode 0 */
    begin_default();
    mv_xy(OM, 0, 0, 0); mv_xy(OM, 0, 1, 0); eol();
    tri_xy(TR, 0, 2, 0, 1);
    eol(); finish();
}
static void p_mvleak_second_list(void) {        /* a modifier list after the polygon that took the bounds */
    begin_default();
    mv_xy(OM, 1, 0, 0); eol();
    tri_xy(TR, 0, 3, 0, 1); eol();                  /* takes (0, 0)..(3, 0) */
    mv_xy(TM, 1, 0, 2); eol();
    tri_xy(PT, 0, 1, 2, 2);                         /* (0..1, 2) if the first bounds were used up */
    eol(); finish();
}
static void grid_regs(int tx, int ty) {         /* grid_probe, and 640 registers from 0x600 (40 past TA_OL_POINTERS) */
    char name[24];
    grid_probe(tx, ty);
    snprintf(name, sizeof(name), "c%03dx", case_no);
    pvrio_dump_regs(TA_OL_POINTERS_addr, 640, name, ".raw");
}
static void p_grid_61x10(void) { grid_regs(61, 10); }   /* 610 tiles: tiles 608, 609 */
static void p_grid_46x14(void) { grid_regs(46, 14); }   /* 644 tiles */
static void p_grid_41x15b(void) { grid_regs(41, 15); }  /* ta-probe grid_41x15 again, with the 640 registers */
static void p_grid_max_y_stack(void) {          /* 4 x 16 (the last row there is: no end words): what of each tile is written */
    tacfg_t c = cfg();
    int i;
    c.ty = 16;
    c.opb[1] = c.opb[2] = c.opb[3] = c.opb[4] = 0;
    c.opb_init = c.ol_base + matrix_bytes(&c);
    begin_ex(&c, SCAN);
    for(i = 0; i < 3; i++)
        tri_xy(OP, 0, 0, 0, (uint32_t)i);           /* tile 0: a triangle array of three */
    tri_xy(OP, 0, 1, 0, 3); quad(OP, 0, 40, 4, 16, 16, 4);     /* tile 1: a triangle, then a strip */
    quads_xy(OP, 2, 0, 9);                          /* tile 2: into a block */
    quads_xy(OP, 3, 0, 1); tri_xy(OP, 0, 3, 0, 5); /* tile 3: a strip, then a triangle */
    eol(); finish();
}
static void p_alloc_change_op(void) {           /* the opaque blocks 8 -> 32 words between the opaque and translucent lists */
    TA_ALLOC_CTRL_type a = { .full = 0 };
    begin_default();
    tri_xy(OP, 0, 1, 1, 1); eol(); wait_done(OP);
    a.O_OPB = 3; a.OM_OPB = 1; a.T_OPB = 1; a.TM_OPB = 1; a.PT_OPB = 1;
    pvrio_reg(TA_ALLOC_CTRL_addr, a.full);
    tri_xy(TR, 0, 2, 1, 2); eol(); finish();
}
static void p_mode_change(void) {               /* OPB_Mode up -> down between the opaque and translucent lists */
    TA_ALLOC_CTRL_type a = { .full = 0 };
    begin_default();
    quads_xy(OP, 1, 1, 9); eol(); wait_done(OP);
    a.O_OPB = 1; a.OM_OPB = 1; a.T_OPB = 1; a.TM_OPB = 1; a.PT_OPB = 1; a.OPB_Mode = 1;
    pvrio_reg(TA_ALLOC_CTRL_addr, a.full);
    quads_xy(TR, 2, 1, 9); eol(); finish();
}
static void p_two_vol_skip(void) {              /* two volumes, textured, offset: the entry and register skip of 4 words */
    static const uint32_t f0[4] = { F1, F1, F1, F1 }, f1[4] = { F1, 0x3f000000, 0x3f000000, 0x3f000000 };
    (void)f0; (void)f1;
    begin_default();
    hdr_vol(OP, GROUP(0) | SHADOW | VOLUME | TEX | OFFS, ISPW, TSPW, TCWW, TSP1, TCW1);
    v11(40, 40, 1, F1, 0, 0xff000001, 0x11, 0, F1, 0xff000002, 0x12, 0);
    v11(48, 40, 1, 0, 0, 0xff000003, 0x13, F1, F1, 0xff000004, 0x14, 0);
    v11(40, 48, 1, F1, F1, 0xff000005, 0x15, 0, 0, 0xff000006, 0x16, 1);
    mid(); eol(); finish();
}
static void p_isp_limit_exact(void) {           /* a triangle that ends exactly at TA_ISP_LIMIT, then two more */
    tacfg_t c = cfg();
    c.isp_limit = c.isp_base + 2 * 15 * 4;
    begin(&c); tri_xy(OP, 0, 0, 0, 1); tri_xy(OP, 0, 1, 0, 2); tri_xy(OP, 0, 2, 0, 3); tri_xy(OP, 0, 3, 0, 4);
    eol(); finish();
}
static void p_list_cont_same(void) {            /* TA_LIST_CONT, then the opaque list again: appended to, or begun anew? */
    begin_default();
    quads_xy(OP, 1, 1, 3); eol(); wait_done(OP);
    pvrio_reg(TA_LIST_CONT_addr, 0x80000000u);
    quads_xy(OP, 1, 1, 2); eol(); finish();
}

/* ======== before the first case ======== */

/* What a console keeps from the programs before: TA_OL_POINTERS (SOFTRESET keeps them) and the face colours (so
   does every reset). A 41 x 15 grid with a quad over all of it gives every one of the 608 registers a known value,
   and a two face colour header the face colours; nothing is dumped (case number -1: no TACFG for compare_dumps). */
static void prelude(void) {
    static const uint32_t fa[4] = { F1, 0x3f000000, 0x3e800000, 0x3f400000 }, fb[4] = { 0x3f400000, 0x3e800000, F1, 0x3f000000 };
    tacfg_t c = cfg();
    case_no = -1;
    c.tx = 41; c.ty = 15;
    c.opb[1] = c.opb[2] = c.opb[3] = c.opb[4] = 0;
    c.isp_base = AREA + 0x8000u; c.isp_limit = AREA + 0xc000u;
    c.opb_init = AREA + 0xc000u; c.ol_limit = AREA + 0x10000u;
    begin(&c);
    quad(OP, 0, 0, 0, 32.0f * 41, 32.0f * 15, 0xff000000);
    hdr_face2(OP, GROUP(0) | COL(2) | TEX | OFFS, ISPW, TSPW, TCWW, TSP1, TCW1, fa, fb);
    eol();
#ifdef _arch_dreamcast
    { uint64_t t0 = timer_ms_gettime64(); while(timer_ms_gettime64() - t0 < 20) ; }
#endif
    pvrio_reg(SOFTRESET_addr, 1);
    pvrio_reg(SOFTRESET_addr, 0);
}

/* ======== the order run ======== */

/* ======== third round (TA_ERRORS 3, a run of its own, the guard in pvrio.h on) ======== */

/* each: the bad input in an opaque list with a triangle in (1, 1) before it, then after_error_reset (a triangle in
   (3, 2) and the end of the list; m0; SOFTRESET, TA_LIST_INIT, a triangle in (3, 1)) */
static void q_type2_zero(void) {                /* parameter type 2 (object list set), all words zero, on its own */
    begin_default(); tri_xy(OP, 0, 1, 1, 1);
    w8(PCW_(P_OLSET, OP, 0), 0, 0, 0, 0, 0, 0, 0);
    after_error_reset();
}
static void q_type3(void) {                     /* parameter type 3 on its own */
    begin_default(); tri_xy(OP, 0, 1, 1, 1);
    w8(PCW_(3, OP, 0), 0, 0, 0, 0, 0, 0, 0);
    after_error_reset();
}
static void q_type6(void) {                     /* parameter type 6 on its own */
    begin_default(); tri_xy(OP, 0, 1, 1, 1);
    w8(PCW_(6, OP, 0), 0, 0, 0, 0, 0, 0, 0);
    after_error_reset();
}
/* object list set: the triangle's own entry (0x80201000) as the object, and small numbers in the other words, each
   word a different one, so that the tiles it lands in say which words are the rectangle */
static void q_olset_a(void) {
    begin_default(); tri_xy(OP, 0, 1, 1, 1);
    w8(PCW_(P_OLSET, OP, 0), 0x80201000u, 0, 1, 2, 0, 3, 2);
    after_error_reset();
}
static void q_olset_b(void) {                   /* the same, other numbers, and a quad entry as the object */
    begin_default(); tri_xy(OP, 0, 1, 1, 1);
    w8(PCW_(P_OLSET, OP, 0), 0xa0201000u, 2, 0, 0, 1, 1, 2);
    after_error_reset();
}
/* vertices with no list open: after SOFTRESET and TA_LIST_INIT, after the end of a list; the header before is known */
static void q_listless_reset(void) {
    tacfg_t c = cfg();
    begin(&c);
    hdr(OP, GROUP(0), ISPW, TSPW, 0); v0(40, 40, 1, 1, 0); v0(48, 40, 1, 2, 0); v0(40, 48, 1, 3, 1); eol();
    begin_ex(&c, NO_FILL);
    v0(72, 40, 1, 4, 0); v0(80, 40, 1, 5, 0); v0(72, 48, 1, 6, 1);
    after_error_reset();
}
static void q_listless_eol(void) {
    begin_default();
    hdr(OP, GROUP(0), ISPW, TSPW, 0); v0(40, 40, 1, 1, 0); v0(48, 40, 1, 2, 0); v0(40, 48, 1, 3, 1); eol();
    v0(72, 40, 1, 4, 0); v0(80, 40, 1, 5, 0); v0(72, 48, 1, 6, 1);
    after_error_reset();
}
static void q_listless_tex(void) {              /* the header before textured (packed, offset) */
    begin_default();
    hdr(OP, GROUP(0) | TEX | OFFS, ISPW, TSPW, TCWW);
    v3(40, 40, 1, F1, 0, 1, 0x11, 0); v3(48, 40, 1, 0, F1, 2, 0x12, 0); v3(40, 48, 1, F1, F1, 3, 0x13, 1); eol();
    v3(72, 40, 1, F1, 0, 4, 0x14, 0); v3(80, 40, 1, 0, F1, 5, 0x15, 0); v3(72, 48, 1, F1, F1, 6, 0x16, 1);
    after_error_reset();
}
static void q_listless_six(void) {              /* six vertices, a strip */
    int i;
    begin_default();
    hdr(OP, GROUP(1), ISPW, TSPW, 0); v0(40, 40, 1, 1, 0); v0(48, 40, 1, 2, 0); v0(40, 48, 1, 3, 1); eol();
    for(i = 0; i < 6; i++)
        v0(72.0f + 4.0f * (float)(i / 2), 40.0f + 8.0f * (float)(i & 1), 1, 0x10u + (uint32_t)i, i == 5);
    after_error_reset();
}
static void q_listless_modvol(void) {           /* the list before a modifier volume list */
    begin_default();
    mv_xy(OM, 1, 1, 1); eol();
    vmv(72, 40, 1, 80, 40, 1, 72, 48, 1);
    after_error_reset();
}
static void list_t(int t) {                     /* a header of list type t (5, 6, 7) opening a list, a triangle, the end */
    begin_default();
    w8(PCW_(P_POLY, t, GROUP(0)), ISPW, TSPW, 0, 0, 0, 0, 0);
    v0(40, 40, 1, (uint32_t)t, 0); v0(48, 40, 1, (uint32_t)t, 0); v0(40, 48, 1, (uint32_t)t, 1);
    w8(0, 0, 0, 0, 0, 0, 0, 0);
    after_error_reset();
}
static void q_list5(void) { list_t(5); }
static void q_list6(void) { list_t(6); }
static void q_list7(void) { list_t(7); }
static void q_sprite_in_mv(void) {              /* a sprite opening a modifier volume list */
    begin_default();
    list_of(OM);
    hdr_sprite(OM, GROUP(0), ISPW, TSPW, TCWW, 0xff112233, 0);
    vsprite(40, 40, 1, 56, 40, 1, 56, 56, 1, 40, 56, 0, 0, 0);
    eol();
    after_error_reset();
}
static void q_sprite_hdr_mid(void) {            /* a sprite header after two strip vertices, then a plain vertex */
    begin_default();
    hdr(OP, GROUP(0), ISPW, TSPW, 0); v0(40, 40, 1, 1, 0); v0(48, 40, 1, 2, 0);
    hdr_sprite(OP, GROUP(0), ISPW, TSPW, TCWW, 0xff112233, 0);
    v0(40, 48, 1, 3, 1);
    after_error_reset();
}
static void sprite_mid(float cy) {              /* the same with a sprite vertex: its second half begins with cy */
    begin_default();
    hdr(OP, GROUP(0), ISPW, TSPW, 0); v0(40, 40, 1, 1, 0); v0(48, 40, 1, 2, 0);
    hdr_sprite(OP, GROUP(0), ISPW, TSPW, TCWW, 0xff112233, 0);
    vsprite(72, 36, 1, 88, 36, 1, 88, cy, 1, 72, 52, 0, 0, 0);
    after_error_reset();
}
static void q_sprite_mid_clip(void) { sprite_mid(0.5f); }      /* 0x3f000000: as parameter type 1 */
static void q_sprite_mid_neg(void) { sprite_mid(-0.5f); }      /* 0xbf000000: as parameter type 5 */
static void q_init_mid_one(void) {              /* TA_LIST_INIT after two strip vertices, then the third */
    tacfg_t c = cfg();
    begin(&c);
    hdr(OP, GROUP(0), ISPW, TSPW, 0); v0(40, 40, 1, 1, 0); v0(48, 40, 1, 2, 0);
    begin_ex(&c, NO_FILL | NO_RESET);
    v0(40, 48, 1, 3, 1);
    after_error_reset();
}
static void q_init_mid_hdr(void) {              /* TA_LIST_INIT after two strip vertices, then a new header */
    tacfg_t c = cfg();
    begin(&c);
    hdr(OP, GROUP(0), ISPW, TSPW, 0); v0(40, 40, 1, 1, 0); v0(48, 40, 1, 2, 0);
    begin_ex(&c, NO_FILL | NO_RESET);
    tri_xy(OP, 0, 2, 1, 4);
    after_error_reset();
}

/* the object list write cache: what of the lists is in memory in the middle of a list (m0), 16 x 8 tiles */
static void cache_tiles(int n) {                /* a quad in each of the first n tiles */
    tacfg_t c = cfg();
    int t;
    c.tx = 16; c.ty = 8;
    c.opb[1] = c.opb[2] = c.opb[3] = c.opb[4] = 0;
    c.opb_init = c.ol_base + matrix_bytes(&c);
    begin(&c);
    for(t = 0; t < n; t++)
        quad(OP, 0, 32.0f * (float)(t % 16) + 4, 32.0f * (float)(t / 16) + 4, 16, 16, 0xff000000u | (uint32_t)t);
    mid(); eol(); finish();
}
static void k_tiles_4(void) { cache_tiles(4); }
static void k_tiles_16(void) { cache_tiles(16); }
static void k_tiles_64(void) { cache_tiles(64); }
static void k_tiles_128(void) { cache_tiles(128); }
static void k_chain(void) {                     /* 40 quads in one tile: a chain of blocks */
    tacfg_t c = cfg();
    c.tx = 16; c.ty = 8;
    c.opb[1] = c.opb[2] = c.opb[3] = c.opb[4] = 0;
    c.opb_init = c.ol_base + matrix_bytes(&c);
    begin(&c); quads_xy(OP, 5, 0, 40); mid(); eol(); finish();
}
static void k_far(void) {                       /* tiles 0 and 64 in turn (2 KB apart), 9 quads each */
    tacfg_t c = cfg();
    int i;
    c.tx = 16; c.ty = 8;
    c.opb[1] = c.opb[2] = c.opb[3] = c.opb[4] = 0;
    c.opb_init = c.ol_base + matrix_bytes(&c);
    begin(&c);
    for(i = 0; i < 9; i++) {
        quad(OP, 0, 4.0f + (float)i, 4, 16, 16, 0xff000000u | (uint32_t)i);
        quad(OP, 0, 4.0f + (float)i, 132, 16, 16, 0xff000100u | (uint32_t)i);
    }
    mid(); eol(); finish();
}

/* step by step (TA_ERRORS 4, a run of its own: many dumps): a dump of the area after every quad */
static void k_chain_steps(void) {               /* 24 quads into one tile: the slot and two blocks */
    tacfg_t c = cfg();
    int i;
    c.tx = 16; c.ty = 8;
    c.opb[1] = c.opb[2] = c.opb[3] = c.opb[4] = 0;
    c.opb_init = c.ol_base + matrix_bytes(&c);
    begin(&c);
    for(i = 0; i < 24; i++) {
        quad(OP, 0, 164.0f + (float)(i % 8), 4.0f + (float)(i / 8), 16, 16, 0xff000000u | (uint32_t)i);
        mid();
    }
    eol(); finish();
}
static void k_tiles_steps(void) {               /* a quad in each of tiles 0..47, a dump after every two */
    tacfg_t c = cfg();
    int t;
    c.tx = 16; c.ty = 8;
    c.opb[1] = c.opb[2] = c.opb[3] = c.opb[4] = 0;
    c.opb_init = c.ol_base + matrix_bytes(&c);
    begin(&c);
    for(t = 0; t < 48; t++) {
        quad(OP, 0, 32.0f * (float)(t % 16) + 4, 32.0f * (float)(t / 16) + 4, 16, 16, 0xff000000u | (uint32_t)t);
        if(t % 2)
            mid();
    }
    eol(); finish();
}

/* ======== fourth round (TA_ERRORS 5): a sprite in a modifier list, TA_LIST_INIT in a strip, the list write queue ======== */

/* sprites in a modifier volume list: corners A (40, 40, 0.5), B (56, 40, 0.25), C (56, 56, 0.125), D (100, 90), UVs
   with marks; then the end of the list and a polygon of another list */
static void sprite_mv(int list, uint32_t flags, float dx, float dy) {
    hdr_sprite(list, GROUP(0) | flags, ISPW, TSPW, TCWW, 0xff112233, 0x00445566);
    vsprite(40, 40, 0.5f, 56, 40, 0.25f, 56, 56, 0.125f, dx, dy, 0x11112222, 0x33334444, 0x55556666);
}
static void r_mv_untex(void) { begin_default(); sprite_mv(OM, 0, 100, 90); eol(); tri_xy(OP, 0, 3, 2, 0xe1); eol(); finish(); }
static void r_mv_tex(void) { begin_default(); sprite_mv(OM, TEX, 100, 90); eol(); tri_xy(OP, 0, 3, 2, 0xe1); eol(); finish(); }
static void r_mv_texoffs(void) { begin_default(); sprite_mv(OM, TEX | OFFS, 100, 90); eol(); tri_xy(OP, 0, 3, 2, 0xe1); eol(); finish(); }
static void r_mv_dsmall(void) { begin_default(); sprite_mv(OM, 0, 8, 12); eol(); tri_xy(OP, 0, 3, 2, 0xe1); eol(); finish(); }
static void r_mv_two(void) {                    /* two sprites */
    begin_default(); sprite_mv(OM, 0, 100, 90);
    vsprite(72, 40, 0.5f, 88, 40, 0.25f, 88, 56, 0.125f, 120, 80, 0x77778888, 0x9999aaaa, 0xbbbbcccc);
    eol(); tri_xy(OP, 0, 3, 2, 0xe1); eol(); finish();
}
static void r_mv_then_mv(void) {                /* a sprite, then a modifier volume triangle in the same list */
    begin_default(); sprite_mv(OM, 0, 100, 90); mv_xy(OM, 1, 3, 0); eol(); tri_xy(OP, 0, 3, 2, 0xe1); eol(); finish();
}
static void r_mv_then_tex(void) {               /* a sprite, then a textured triangle with offset in the opaque list */
    begin_default(); sprite_mv(OM, 0, 100, 90); eol();
    hdr(OP, GROUP(0) | TEX | OFFS, ISPW, TSPW, TCWW);
    v3(104, 72, 1, F1, 0, 0xe1, 0x11, 0); v3(112, 72, 1, 0, F1, 0xe2, 0x12, 0); v3(104, 80, 1, F1, F1, 0xe3, 0x13, 1);
    eol(); finish();
}
static void r_tm(void) { begin_default(); sprite_mv(TM, 0, 100, 90); eol(); tri_xy(TR, 0, 3, 2, 0xe1); eol(); finish(); }

/* TA_LIST_INIT in the middle of a strip. Vertices with marks in every word: x, y, z, then A1/A2/A3/A4 << 24 | k */
static void vmark(float x, float y, float z, uint32_t k, int eos) {
    w8(VP(eos), fl(x), fl(y), fl(z), 0xa1000000u | k, 0xa2000000u | k, 0xa3000000u | k, 0xa4000000u | k);
}
static void init_now(void) { tacfg_t c = cfg(); begin_ex(&c, NO_FILL | NO_RESET); }
static void r_init_one(void) {                  /* two vertices, TA_LIST_INIT, one, then three more and a triangle */
    begin_default(); hdr(OP, GROUP(0), ISPW, TSPW, 0); vmark(40, 40, 1, 1, 0); vmark(48, 40, 1, 2, 0);
    init_now(); vmark(40, 48, 1, 3, 1);
    vmark(72, 40, 1, 4, 0); vmark(80, 40, 1, 5, 0); vmark(72, 48, 1, 6, 1);
    tri_xy(OP, 0, 3, 2, 0xe1); eol(); eol(); finish();
}
static void r_init_two(void) {                  /* two vertices, TA_LIST_INIT, two */
    begin_default(); hdr(OP, GROUP(0), ISPW, TSPW, 0); vmark(40, 40, 1, 1, 0); vmark(48, 40, 1, 2, 0);
    init_now(); vmark(40, 48, 1, 3, 0); vmark(48, 48, 1, 4, 1);
    tri_xy(OP, 0, 3, 2, 0xe1); eol(); eol(); finish();
}
static void r_init_hdr(void) {                  /* two vertices, TA_LIST_INIT, a header and three */
    begin_default(); hdr(OP, GROUP(0), ISPW, TSPW, 0); vmark(40, 40, 1, 1, 0); vmark(48, 40, 1, 2, 0);
    init_now(); hdr(OP, GROUP(0), 0x800bcdefu, TSPW, 0); vmark(72, 40, 1, 4, 0); vmark(80, 40, 1, 5, 0); vmark(72, 48, 1, 6, 1);
    eol(); eol(); finish();
}
static void r_init_eol(void) {                  /* two vertices, TA_LIST_INIT, the end of the list */
    begin_default(); hdr(OP, GROUP(0), ISPW, TSPW, 0); vmark(40, 40, 1, 1, 0); vmark(48, 40, 1, 2, 0);
    init_now(); eol();
    tri_xy(OP, 0, 3, 2, 0xe1); eol(); eol(); finish();
}
static void r_init_after1(void) {               /* one vertex, TA_LIST_INIT, two */
    begin_default(); hdr(OP, GROUP(0), ISPW, TSPW, 0); vmark(40, 40, 1, 1, 0);
    init_now(); vmark(48, 40, 1, 2, 0); vmark(40, 48, 1, 3, 1);
    tri_xy(OP, 0, 3, 2, 0xe1); eol(); eol(); finish();
}
static void r_init_long(void) {                 /* Strip_Len 8: five vertices, TA_LIST_INIT, three */
    int i;
    begin_default(); hdr(OP, GROUP(3), ISPW, TSPW, 0);
    for(i = 0; i < 5; i++)
        vmark(36.0f + 4.0f * (float)(i / 2), 40.0f + 8.0f * (float)(i & 1), 1, (uint32_t)i + 1, 0);
    init_now();
    for(i = 5; i < 8; i++)
        vmark(36.0f + 4.0f * (float)(i / 2), 40.0f + 8.0f * (float)(i & 1), 1, (uint32_t)i + 1, i == 7);
    tri_xy(OP, 0, 3, 2, 0xe1); eol(); eol(); finish();
}
static void r_init_tex(void) {                  /* textured with offset (7 word vertices): two, TA_LIST_INIT, one */
    begin_default(); hdr(OP, GROUP(0) | TEX | OFFS, ISPW, TSPW, TCWW); vmark(40, 40, 1, 1, 0); vmark(48, 40, 1, 2, 0);
    init_now(); vmark(40, 48, 1, 3, 1);
    tri_xy(OP, 0, 3, 2, 0xe1); eol(); eol(); finish();
}
static void r_init_noeos(void) {                /* two, TA_LIST_INIT, three (the end of the strip on the last) */
    begin_default(); hdr(OP, GROUP(0), ISPW, TSPW, 0); vmark(40, 40, 1, 1, 0); vmark(48, 40, 1, 2, 0);
    init_now(); vmark(40, 48, 1, 3, 0); vmark(48, 48, 1, 4, 0); vmark(40, 56, 1, 5, 1);
    tri_xy(OP, 0, 3, 2, 0xe1); eol(); eol(); finish();
}

/* the list write queue with polygons over many tiles: 8 x 6 tiles, a dump after every polygon */
static tacfg_t grid86(int down) {
    tacfg_t c = cfg();
    c.tx = 8; c.ty = 6;
    c.opb[1] = c.opb[2] = c.opb[3] = c.opb[4] = 0;
    if(down) {
        c.down = 1; c.ol_base = AREA + 0x2000u; c.opb_init = AREA + 0x2000u; c.ol_limit = AREA;
    } else
        c.opb_init = c.ol_base + matrix_bytes(&c);
    return c;
}
static void big_quads(int down, float w, float h, int n) {
    tacfg_t c = grid86(down);
    int i;
    begin(&c);
    for(i = 0; i < n; i++) {
        quad(OP, 0, 1.0f + (float)i, 1.0f + (float)i, w, h, 0xff000000u | (uint32_t)i);
        mid();
    }
    eol(); finish();
}
static void r_q48_up(void) { big_quads(0, 254, 190, 6); }      /* 48 tiles a quad */
static void r_q16_up(void) { big_quads(0, 126, 126, 8); }      /* 16 tiles a quad */
static void r_q48_down(void) { big_quads(1, 254, 190, 6); }
static void r_q48_then_small(void) {            /* one quad over all 48 tiles, then quads in one tile each */
    tacfg_t c = grid86(0);
    int i;
    begin(&c);
    quad(OP, 0, 1, 1, 254, 190, 0xff000000); mid();
    for(i = 0; i < 24; i++) {
        quad(OP, 0, 32.0f * (float)(i % 8) + 4, 32.0f * (float)(i / 8) + 4, 16, 16, 0xff000100u | (uint32_t)i);
        if(i % 4 == 3)
            mid();
    }
    eol(); finish();
}

/* ======== fifth round (TA_ERRORS 6): what sets the size of the polygon after a sprite in a modifier list ======== */

/* an untextured sprite in the opaque modifier list, one thing changed at a time; then a triangle in (tx, ty) of the
   opaque list */
static void spr6(float az, float bz, float cz, float dx, float dy, uint32_t uv, uint32_t offs, int tx, int ty) {
    begin_default();
    hdr_sprite(OM, GROUP(0), ISPW, TSPW, TCWW, 0xff112233, offs);
    vsprite(40, 40, az, 56, 40, bz, 56, 56, cz, dx, dy, uv, uv ? uv + 0x22222222u : 0, uv ? uv + 0x44444444u : 0);
    eol(); tri_xy(OP, 0, tx, ty, 0xe1); eol(); finish();
}
#define SPR6(n, az, bz, cz, dx, dy, uv, offs, tx, ty) static void n(void) { spr6(az, bz, cz, dx, dy, uv, offs, tx, ty); }
SPR6(t_base, 0.5f, 0.25f, 0.125f, 100, 90, 0x11112222u, 0x00445566u, 3, 2)
SPR6(t_dx8, 0.5f, 0.25f, 0.125f, 8, 90, 0x11112222u, 0x00445566u, 3, 2)
SPR6(t_dx40, 0.5f, 0.25f, 0.125f, 40, 90, 0x11112222u, 0x00445566u, 3, 2)
SPR6(t_dx72, 0.5f, 0.25f, 0.125f, 72, 90, 0x11112222u, 0x00445566u, 3, 2)
SPR6(t_dx127, 0.5f, 0.25f, 0.125f, 127, 90, 0x11112222u, 0x00445566u, 3, 2)
SPR6(t_dy12, 0.5f, 0.25f, 0.125f, 100, 12, 0x11112222u, 0x00445566u, 3, 2)
SPR6(t_dy40, 0.5f, 0.25f, 0.125f, 100, 40, 0x11112222u, 0x00445566u, 3, 2)
SPR6(t_dy56, 0.5f, 0.25f, 0.125f, 100, 56, 0x11112222u, 0x00445566u, 3, 2)
SPR6(t_d8_12, 0.5f, 0.25f, 0.125f, 8, 12, 0x11112222u, 0x00445566u, 3, 2)
SPR6(t_d40_56, 0.5f, 0.25f, 0.125f, 40, 56, 0x11112222u, 0x00445566u, 3, 2)
SPR6(t_z1, 1, 1, 1, 100, 90, 0x11112222u, 0x00445566u, 3, 2)
SPR6(t_uv0, 0.5f, 0.25f, 0.125f, 100, 90, 0, 0x00445566u, 3, 2)
SPR6(t_offs0, 0.5f, 0.25f, 0.125f, 100, 90, 0x11112222u, 0, 3, 2)
SPR6(t_next00, 0.5f, 0.25f, 0.125f, 100, 90, 0x11112222u, 0x00445566u, 0, 0)
SPR6(t_next10, 0.5f, 0.25f, 0.125f, 100, 90, 0x11112222u, 0x00445566u, 1, 0)
SPR6(t_c112, 1, 1, 1, 40, 56, 0, 0, 3, 2)          /* as ta-probe sprite_in_mv sent it */

/* ======== sixth round (TA_ERRORS 7): the two parameter counters after a sprite in a modifier list ======== */

/* an untextured sprite in the opaque modifier list with corner D at (dx, 90), then the end of the list */
static void spr7(float dx) {
    hdr_sprite(OM, GROUP(0), ISPW, TSPW, TCWW, 0xff112233, 0x00445566);
    vsprite(40, 40, 0.5f, 56, 40, 0.25f, 56, 56, 0.125f, dx, 90, 0x11112222u, 0x33334444u, 0x55556666u);
    eol();
}
static void u_isp_after(void) {                 /* TA_ISP_CURRENT right after the sprite */
    begin_default(); spr7(100); mid(); tri_xy(OP, 0, 3, 2, 0xe1); eol(); finish();
}
static void u_reset_after(void) {               /* SOFTRESET and TA_LIST_INIT after it */
    tacfg_t c = cfg();
    begin_default(); spr7(100); begin_ex(&c, NO_FILL); tri_xy(OP, 0, 3, 2, 0xe1); eol(); finish();
}
static void u_cont_after(void) {                /* TA_LIST_CONT after it */
    begin_default(); spr7(100); wait_done(OM);
    pvrio_reg(TA_LIST_CONT_addr, 0x80000000u);
    tri_xy(TR, 0, 3, 2, 0xe1); eol(); finish();
}
static void u_two_vol(void) {                   /* a polygon of two volumes after it */
    begin_default(); spr7(100);
    hdr_vol(OP, GROUP(0) | VOLUME, ISPW, TSPW, 0, 0x87654321u, 0);
    v9(104, 72, 1, 0xe1, 0xd1, 0); v9(112, 72, 1, 0xe2, 0xd2, 0); v9(104, 80, 1, 0xe3, 0xd3, 1);
    eol(); finish();
}
static void u_shadow(void) {                    /* a polygon with the Shadow bit after it */
    begin_default(); spr7(100); tri_xy(OP, SHADOW, 3, 2, 0xe1); eol(); finish();
}
static void u_sprite(void) {                    /* a sprite of the opaque list after it */
    begin_default(); spr7(40);
    hdr_sprite(OP, GROUP(0), ISPW, TSPW, TCWW, 0xff445566, 0);
    vsprite(104, 72, 1, 112, 72, 1, 112, 80, 1, 104, 80, 0, 0, 0);
    eol(); finish();
}
static void u_strip4(void) {                    /* a strip of four after it */
    begin_default(); spr7(72);
    hdr(OP, GROUP(0), ISPW, TSPW, 0);
    v0(104, 72, 1, 0xe1, 0); v0(112, 72, 1, 0xe2, 0); v0(104, 80, 1, 0xe3, 0); v0(112, 80, 1, 0xe4, 1);
    eol(); finish();
}
static void u_dx160(void) { begin_default(); spr7(160); tri_xy(OP, 0, 3, 2, 0xe1); eol(); finish(); }
static void u_dx200(void) { begin_default(); spr7(200); tri_xy(OP, 0, 3, 2, 0xe1); eol(); finish(); }
static void u_dxneg(void) { begin_default(); spr7(-8); tri_xy(OP, 0, 3, 2, 0xe1); eol(); finish(); }
static void u_two_tris(void) {                  /* two triangles after it */
    begin_default(); spr7(100); tri_xy(OP, 0, 3, 2, 0xe1); tri_xy(OP, 0, 2, 2, 0xe2); eol(); finish();
}
static void u_tex_pad(void) {                   /* a textured triangle (16-bit UV) after it, padded */
    begin_default(); spr7(160);
    hdr(OP, GROUP(0) | TEX | UV16, ISPW, TSPW, TCWW);
    v4(104, 72, 1, 0x11112222u, 0xe1, 0, 0); v4(112, 72, 1, 0x33334444u, 0xe2, 0, 0); v4(104, 80, 1, 0x55556666u, 0xe3, 0, 1);
    eol(); finish();
}

/* ======== seventh round (TA_ERRORS 8): TA_LIST_INIT in a strip, a dump after every block ======== */

/* a vertex whose spare words are coordinates of their own: 4, 36, 68, 100 (tiles 0, 1, 2, 3 across) + k / 8 */
static void vk(float x, float y, uint32_t k, int eos) {
    const float f = (float)k / 8.0f;
    w8(VP(eos), fl(x), fl(y), fl(1), fl(4 + f), fl(36 + f), fl(68 + f), fl(100 + f));
    mid();
}
static void w_hdr(void) { hdr(OP, GROUP(0), ISPW, TSPW, 0); }
static void w_init(void) { init_now(); mid(); }
static void w_one(void) {                       /* two, TA_LIST_INIT, one that ends the strip, three more */
    begin_default(); w_hdr(); vk(40, 40, 1, 0); vk(48, 40, 2, 0); w_init();
    vk(40, 48, 3, 1); vk(72, 40, 4, 0); vk(80, 40, 5, 0); vk(72, 48, 6, 1);
    eol(); finish();
}
static void w_two(void) {                       /* two, TA_LIST_INIT, two, then a header and three */
    begin_default(); w_hdr(); vk(40, 40, 1, 0); vk(48, 40, 2, 0); w_init();
    vk(40, 48, 3, 0); vk(48, 48, 4, 1);
    hdr(OP, GROUP(0), 0x800bcdefu, 0x22222222u, 0); mid();
    vk(104, 72, 5, 0); vk(112, 72, 6, 0); vk(104, 80, 7, 1);
    eol(); finish();
}
static void w_newhdr(void) {                    /* two, TA_LIST_INIT, a header, three */
    begin_default(); w_hdr(); vk(40, 40, 1, 0); vk(48, 40, 2, 0); w_init();
    hdr(OP, GROUP(0), 0x800bcdefu, 0x22222222u, 0); mid();
    vk(72, 40, 4, 0); vk(80, 40, 5, 0); vk(72, 48, 6, 1);
    eol(); finish();
}
static void w_after1(void) {                    /* one, TA_LIST_INIT, four */
    begin_default(); w_hdr(); vk(40, 40, 1, 0); w_init();
    vk(48, 40, 2, 0); vk(40, 48, 3, 1); vk(72, 40, 4, 0); vk(80, 40, 5, 1);
    eol(); finish();
}
static void w_zero(void) {                      /* a header, TA_LIST_INIT, four */
    begin_default(); w_hdr(); w_init();
    vk(40, 40, 1, 0); vk(48, 40, 2, 0); vk(40, 48, 3, 1); vk(72, 40, 4, 1);
    eol(); finish();
}
static void w_done(void) {                      /* a whole triangle, TA_LIST_INIT, four */
    begin_default(); w_hdr(); vk(40, 40, 1, 0); vk(48, 40, 2, 0); vk(40, 48, 3, 1); w_init();
    vk(72, 40, 4, 0); vk(80, 40, 5, 0); vk(72, 48, 6, 1); vk(104, 72, 7, 1);
    eol(); finish();
}
static void w_tex(void) {                       /* textured with offset (7 word vertices): two, TA_LIST_INIT, four */
    begin_default(); hdr(OP, GROUP(0) | TEX | OFFS, ISPW, TSPW, TCWW); vk(40, 40, 1, 0); vk(48, 40, 2, 0); w_init();
    vk(40, 48, 3, 1); vk(72, 40, 4, 0); vk(80, 40, 5, 0); vk(72, 48, 6, 1);
    eol(); finish();
}

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
    { "err_sprite_in_mv", e_sprite_in_mv, 1 }, { "err_init_in_strip", e_init_in_strip, 2 },
    { "err_cont_in_strip", e_cont_in_strip, 2 },   /* 2: may leave a console unable to send: own run */
    { "mvleak_volumes", p_mvleak_volumes, 0 }, { "mvleak_open2", p_mvleak_open2, 0 },
    { "mvleak_second_list", p_mvleak_second_list, 0 }, { "grid_61x10", p_grid_61x10, 0 },
    { "grid_46x14", p_grid_46x14, 0 }, { "grid_41x15b", p_grid_41x15b, 0 }, { "grid_max_y_stack", p_grid_max_y_stack, 0 },
    { "alloc_change_op", p_alloc_change_op, 0 }, { "mode_change", p_mode_change, 0 }, { "two_vol_skip", p_two_vol_skip, 0 },
    { "isp_limit_exact", p_isp_limit_exact, 2 }, { "list_cont_same", p_list_cont_same, 0 },   /* isp_limit_exact: a console stopped in it (2026-11-17), own run */
    /* third round: which input stops the TA, the bad input refta did not have, the object list write cache */
    { "lock_type2_zero", q_type2_zero, 3 }, { "lock_type3", q_type3, 3 }, { "lock_type6", q_type6, 3 },
    { "olset_a", q_olset_a, 3 }, { "olset_b", q_olset_b, 3 },
    { "listless_reset", q_listless_reset, 3 }, { "listless_eol", q_listless_eol, 3 }, { "listless_tex", q_listless_tex, 3 },
    { "listless_six", q_listless_six, 3 }, { "listless_modvol", q_listless_modvol, 3 },
    { "list5", q_list5, 3 }, { "list6", q_list6, 3 }, { "list7", q_list7, 3 },
    { "sprite_in_mv", q_sprite_in_mv, 3 }, { "sprite_hdr_mid", q_sprite_hdr_mid, 3 },
    { "sprite_mid_clip", q_sprite_mid_clip, 3 }, { "sprite_mid_neg", q_sprite_mid_neg, 3 },
    { "init_mid_one", q_init_mid_one, 3 }, { "init_mid_hdr", q_init_mid_hdr, 3 },
    { "cache_tiles_4", k_tiles_4, 0 }, { "cache_tiles_16", k_tiles_16, 0 }, { "cache_tiles_64", k_tiles_64, 0 },
    { "cache_tiles_128", k_tiles_128, 0 }, { "cache_chain", k_chain, 0 }, { "cache_far", k_far, 0 },
    { "cache_chain_steps", k_chain_steps, 4 }, { "cache_tiles_steps", k_tiles_steps, 4 },
    { "spr_mv_untex", r_mv_untex, 5 }, { "spr_mv_tex", r_mv_tex, 5 }, { "spr_mv_texoffs", r_mv_texoffs, 5 },
    { "spr_mv_dsmall", r_mv_dsmall, 5 }, { "spr_mv_two", r_mv_two, 5 }, { "spr_mv_then_mv", r_mv_then_mv, 5 },
    { "spr_mv_then_tex", r_mv_then_tex, 5 }, { "spr_tm", r_tm, 5 },
    { "init_one", r_init_one, 5 }, { "init_two", r_init_two, 5 }, { "init_hdr", r_init_hdr, 5 },
    { "init_eol", r_init_eol, 5 }, { "init_after1", r_init_after1, 5 }, { "init_long", r_init_long, 5 },
    { "init_tex", r_init_tex, 5 }, { "init_noeos", r_init_noeos, 5 },
    { "q48_up", r_q48_up, 5 }, { "q16_up", r_q16_up, 5 }, { "q48_down", r_q48_down, 5 },
    { "q48_then_small", r_q48_then_small, 5 },
    { "s6_base", t_base, 6 }, { "s6_dx8", t_dx8, 6 }, { "s6_dx40", t_dx40, 6 }, { "s6_dx72", t_dx72, 6 },
    { "s6_dx127", t_dx127, 6 }, { "s6_dy12", t_dy12, 6 }, { "s6_dy40", t_dy40, 6 }, { "s6_dy56", t_dy56, 6 },
    { "s6_d8_12", t_d8_12, 6 }, { "s6_d40_56", t_d40_56, 6 }, { "s6_z1", t_z1, 6 }, { "s6_uv0", t_uv0, 6 },
    { "s6_offs0", t_offs0, 6 }, { "s6_next00", t_next00, 6 }, { "s6_next10", t_next10, 6 }, { "s6_c112", t_c112, 6 },
    { "s7_isp_after", u_isp_after, 7 }, { "s7_reset_after", u_reset_after, 7 }, { "s7_cont_after", u_cont_after, 7 },
    { "s7_two_vol", u_two_vol, 7 }, { "s7_shadow", u_shadow, 7 }, { "s7_sprite", u_sprite, 7 }, { "s7_strip4", u_strip4, 7 },
    { "s7_dx160", u_dx160, 7 }, { "s7_dx200", u_dx200, 7 }, { "s7_dxneg", u_dxneg, 7 }, { "s7_two_tris", u_two_tris, 7 },
    { "s7_tex_pad", u_tex_pad, 7 },
    { "i8_one", w_one, 8 }, { "i8_two", w_two, 8 }, { "i8_newhdr", w_newhdr, 8 }, { "i8_after1", w_after1, 8 },
    { "i8_zero", w_zero, 8 }, { "i8_done", w_done, 8 }, { "i8_tex", w_tex, 8 },
};
static const tgroup_t groups[] = { { "none", 0, 0 } };
