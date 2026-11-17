/* The TSP's tile buffers: how many are there, is the one a tile is not shaded into the "secondary accumulation
   buffer" of the TSP word's select bits, and what happens when a polygon uses it while the tile before is
   still being written out (does the TSP wait, or does the write-out get the new pixels)?

   Every page is one render with a region array of its own: one entry per tile drawn, in the order given, each
   on its own screen tile (cell). So a buffer's old content shows up in a later cell at the same place within
   the tile. The tools:

     keep Z       the entry's bit 30: no Z clear and no background polygon. What such a tile does not draw
                  stays as the buffer had it, so its cell shows the buffer's history.
     not written  the entry's bit 28: the tile is shaded but not written to the framebuffer.
     SEC WRITE    a polygon with the destination select bit: its colour goes to the secondary buffer.
     SEC COPY     a polygon with the source select bit, ONE / ZERO: the secondary buffer's pixels come to the
                  tile.
   Select polygons are in the translucent list (pre-sorted: drawn in the order sent, after the opaque list),
   except on page 1, which asks whether the opaque list honours the bits too.

   Every page starts with four ordinary tiles in four greys, so that the buffers hold known pictures whatever
   was rendered before.

   page 0  SEC COPY after different kinds of tile: which picture is in the secondary buffer?
   page 1  the same with the select polygons in the opaque list
   page 2  ten keep Z tiles, each drawing one stripe: a tile's cell shows the stripes of every earlier tile
           that was shaded into the same buffer. Two buffers: every second stripe.
   page 3  the same with keep Z tiles in between that draw nothing (one at a time: two in a row hang the core)
   page 4  the same with tiles that are not written out
   page 5  SEC WRITE, then keep Z tiles: does the written stripe turn up in the next tile's buffer?
   page 6  race, SEC WRITE: a tile of eight bands is written out while the next tile (keep Z) writes a column
           into the secondary buffer, n unseen quads later (the ISP's time for them delays it). If the TSP
           waits for the write-out, the bands stay whole; if not, the column appears in the bands' cell from
           the line the write-out had reached.
   page 7  race, SEC COPY: the same with a column that only reads the secondary buffer
   page 8  race, SEC WRITE over the whole tile

   On a console the test also times some of these (lines "T end", "T flush", "T stall"): see timing() at
   the end of this file. */
#define NUM_PAGES 9
#include "pvrtest2.h"
#ifdef _arch_dreamcast
#include <arch/irq.h>
#include <dc/perfctr.h>
#endif

#define REGION   0x3e0000u          /* 32 bit area, unused: this test's region arrays */
#define KEEPZ    1
#define NOWRITE  2
#define NOLISTS  4                  /* the entry has no list pointer at all */
#define MAX_CMDS 6000

typedef struct { int16_t x, y, w, h; uint32_t colour; uint8_t list, src_sel, dst_sel, never; } cmd_t;

static cmd_t cmds[MAX_CMDS];
static int n_cmds;
static uint32_t region[6 * 300];
static int n_tiles;                 /* entries so far: the next tile is cell n_tiles */
static int describing, cur_page;    /* describing: only the labels are wanted */
static int sel_list = ListType_Translucent;     /* the list the select polygons are sent in */

static int tx(void) { return cell_px(n_tiles - 1); }
static int ty(void) { return cell_py(n_tiles - 1); }

/* the next tile: an entry for cell n_tiles with pointers to that cell's opaque and translucent objects */
static void T(int flags, const char *fmt, ...) {
    RegionArrayEntry e;
    const int x = n_tiles % COLS, y = n_tiles / COLS;

    if(describing) {
        va_list ap;
        printf("LABEL %d %d 1 1 ", cur_page, n_tiles);
        va_start(ap, fmt);
        vprintf(fmt, ap);
        va_end(ap);
        printf("\n");
    }
    memset(&e, 0, sizeof(e));
    e.control.tilex = x;
    e.control.tiley = y;
    e.control.z_keep = (flags & KEEPZ) != 0;
    e.control.no_writeout = (flags & NOWRITE) != 0;
    e.control.pre_sort = 1;
    e.opaque.ptr_in_words = (pvr_list_base[ListType_Opaque] + (uint32_t)pvr_cfg.opb_words[ListType_Opaque] * 4 * (uint32_t)(SETUP_TILES_X * y + x)) >> 2;
    e.trans.ptr_in_words = (pvr_list_base[ListType_Translucent] + (uint32_t)pvr_cfg.opb_words[ListType_Translucent] * 4 * (uint32_t)(SETUP_TILES_X * y + x)) >> 2;
    e.opaque_mod.empty = e.trans_mod.empty = e.puncht.empty = 1;
    if(flags & NOLISTS)
        e.opaque.full = e.trans.full = e.puncht.full;
    memcpy(&region[6 * n_tiles], &e, sizeof(e));
    n_tiles++;
}

static void add(int list, int x, int y, int w, int h, uint32_t colour, int src_sel, int dst_sel, int never) {
    cmd_t *c = &cmds[n_cmds];
    if(describing || n_cmds >= MAX_CMDS)
        return;
    c->x = (int16_t)(tx() + x); c->y = (int16_t)(ty() + y); c->w = (int16_t)w; c->h = (int16_t)h;
    c->colour = 0xff000000u | colour;
    c->list = (uint8_t)list; c->src_sel = (uint8_t)src_sel; c->dst_sel = (uint8_t)dst_sel; c->never = (uint8_t)never;
    n_cmds++;
}

/* what a tile can hold, in the tile's own coordinates */
static void fill(uint32_t c)                               { add(ListType_Opaque, 0, 0, 32, 32, c, 0, 0, 0); }
static void mark(int x, int y, int w, int h, uint32_t c)   { add(ListType_Opaque, x, y, w, h, c, 0, 0, 0); }
static void tmark(int x, int y, int w, int h, uint32_t c)  { add(ListType_Translucent, x, y, w, h, c, 0, 0, 0); }
static void sec_write(int x, int y, int w, int h, uint32_t c) { add(sel_list, x, y, w, h, c, 0, 1, 0); }
static void sec_copy(int x, int y, int w, int h)           { add(sel_list, x, y, w, h, 0xffffff, 1, 0, 0); }
static void unseen(int n)                                  { while(n-- > 0) add(ListType_Opaque, 0, 0, 1, 1, 0, 0, 0, 1); }

static void prime(void) {
    static const uint32_t grey[4] = { 0x202020, 0x404040, 0x606060, 0x808080 };
    int i;
    for(i = 0; i < 4; i++) {
        T(0, "prime %d: grey %06x", i, (unsigned)grey[i]);
        fill(grey[i]);
    }
}

static const uint32_t stripe_col[10] = { 0xff0000, 0x00ff00, 0x0000ff, 0xffff00, 0xff00ff, 0x00ffff, 0xff8000, 0x8000ff, 0x00ff80, 0xffffff };

static void quadrants(void) {
    mark(0, 0, 16, 16, 0xff0000); mark(16, 0, 16, 16, 0x00ff00);
    mark(0, 16, 16, 16, 0x0000ff); mark(16, 16, 16, 16, 0xffff00);
}

/* pages 0 and 1: which picture does SEC COPY bring? */
static void page_copy(void) {
    prime();
    T(0, "A: four quadrants (red, green / blue, yellow)");                  quadrants();
    T(0, "B: background, SEC COPY over the tile: A's picture?");            sec_copy(0, 0, 32, 32);
    T(0, "C: white, SEC COPY over the left half: B's picture there?");      fill(0xffffff); sec_copy(0, 0, 16, 32);
    T(0, "D: background, SEC COPY over the tile: C's picture?");            sec_copy(0, 0, 32, 32);
    T(0, "E: background only");
    T(0, "F: background, SEC COPY: E's picture (background)?");             sec_copy(0, 0, 32, 32);
    T(NOWRITE, "G: cyan, not written out");                                 fill(0x00ffff);
    T(0, "H: background, SEC COPY after a tile that was not written out");  sec_copy(0, 0, 32, 32);
    T(0, "I: orange");                                                      fill(0xff8000);
    T(KEEPZ, "J: keep Z, nothing drawn");
    T(0, "K: background, SEC COPY after a keep Z tile that drew nothing");  sec_copy(0, 0, 32, 32);
    T(0, "L: magenta");                                                     fill(0xff00ff);
    T(0, "M: green");                                                       fill(0x00ff00);
    T(0, "N: background, SEC COPY left half, then SEC COPY again right half"); sec_copy(0, 0, 16, 32); sec_copy(16, 0, 16, 32);
    T(0, "O: background, SEC COPY: N's picture?");                          sec_copy(0, 0, 32, 32);
}

/* page 2: keep Z tiles, one stripe each */
static void page_chain(void) {
    int k;
    prime();
    for(k = 0; k < 10; k++) {
        T(KEEPZ, "keep Z %d: stripe %d only; the rest is the buffer's history", k, k);
        mark(3 * k, 0, 3, 32, stripe_col[k]);
    }
}

/* page 3: keep Z tiles that draw nothing in between. Never two of them in a row: a second keep Z tile that
   shades nothing and is written out hangs the core ("T end 2" in the timing part). */
static void page_chain_empty(void) {
    static const int draws[14] = { 1, 0, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 0, 1 };
    int k, s = 0;
    prime();
    for(k = 0; k < 14; k++) {
        if(draws[k]) {
            T(KEEPZ, "keep Z, stripe %d", s);
            mark(3 * s, 0, 3, 32, stripe_col[s]);
            s++;
        } else
            T(KEEPZ, "keep Z, nothing drawn");
    }
}

/* page 4: tiles that are not written out */
static void page_chain_nowrite(void) {
    prime();
    T(NOWRITE, "N1: orange, not written out");                      fill(0xff8000);
    T(KEEPZ, "keep Z, stripe 0: on N1's orange?");                  mark(0, 0, 3, 32, stripe_col[0]);
    T(NOWRITE, "N2: purple, not written out");                      fill(0x8000ff);
    T(0, "N3: background and stripe 1, written out");               mark(3, 0, 3, 32, stripe_col[1]);
    T(KEEPZ, "keep Z, stripe 2");                                   mark(6, 0, 3, 32, stripe_col[2]);
    T(KEEPZ, "keep Z, stripe 3");                                   mark(9, 0, 3, 32, stripe_col[3]);
    T(KEEPZ | NOWRITE, "keep Z, stripe 4, not written out");        mark(12, 0, 3, 32, stripe_col[4]);
    T(KEEPZ, "keep Z, stripe 5");                                   mark(15, 0, 3, 32, stripe_col[5]);
    T(KEEPZ, "keep Z, stripe 6");                                   mark(18, 0, 3, 32, stripe_col[6]);
    T(KEEPZ | NOWRITE, "keep Z, nothing drawn, not written out");
    T(KEEPZ, "keep Z, stripe 7");                                   mark(21, 0, 3, 32, stripe_col[7]);
    T(KEEPZ, "keep Z, stripe 8");                                   mark(24, 0, 3, 32, stripe_col[8]);
}

/* page 5: does SEC WRITE land in the buffer the next tile is shaded into? */
static void page_write(void) {
    prime();
    T(0, "A: red, and SEC WRITE of a white stripe at x 4..7");      fill(0xff0000); sec_write(4, 0, 4, 32, 0xffffff);
    T(KEEPZ, "keep Z, stripe at x 12: the other buffer (grey 808080) with A's white stripe?");  mark(12, 0, 3, 32, 0x00ff00);
    T(KEEPZ, "keep Z, stripe at x 16: A's red, no white stripe?");  mark(16, 0, 3, 32, 0x0000ff);
    T(0, "B: yellow, SEC WRITE cyan at x 8..11, then SEC COPY of the left half");
        fill(0xffff00); sec_write(8, 0, 4, 32, 0x00ffff); sec_copy(0, 0, 16, 32);
    T(KEEPZ, "keep Z, stripe at x 20: the buffer B wrote into");    mark(20, 0, 3, 32, 0xff00ff);
    T(KEEPZ, "keep Z, stripe at x 24: B's picture");                mark(24, 0, 3, 32, 0xff8000);
    T(KEEPZ, "keep Z: SEC WRITE white at x 28 and nothing else");   sec_write(28, 0, 3, 32, 0xffffff);
    T(KEEPZ, "keep Z, stripe at x 0");                              mark(0, 0, 2, 32, 0x00ff80);
    T(KEEPZ, "keep Z, stripe at x 2");                              mark(2, 0, 2, 32, 0x8000ff);
    T(KEEPZ, "keep Z: SEC COPY of the top half and nothing else");  sec_copy(0, 0, 32, 16);
    T(KEEPZ, "keep Z, a dot at (30, 30)");                          mark(30, 30, 2, 2, 0xffffff);
    T(KEEPZ, "keep Z, a dot at (30, 28)");                          mark(30, 28, 2, 2, 0xffffff);
}

/* pages 6 to 8: a tile of eight bands is written out while the next tile uses the secondary buffer.
   n unseen quads in that next tile's opaque list keep its ISP busy for about 34 clocks each, so that its TSP
   comes to the select polygon later; around 30 of them it starts when the bands' write-out starts. */
static void bands(void) {
    int j;
    for(j = 0; j < 8; j++)
        mark(0, 4 * j, 32, 4, 0x002000u + 0x001c00u * (uint32_t)j);
}

static void page_race(int kind) {       /* 0 SEC WRITE of a column, 1 SEC COPY of a column, 2 SEC WRITE over the tile */
    static const char *what[3] = { "SEC WRITE magenta at x 14..17", "SEC COPY at x 14..17", "SEC WRITE magenta over the tile" };
    int i;
    prime();
    for(i = 0; i < 29; i++) {
        const int n = i == 0 ? 0 : i == 1 ? 12 : i < 27 ? 24 + 2 * (i - 2) : i == 27 ? 80 : 96;
        T(0, "bands; then written out while the next tile works (n = %d)", n);
        bands();
        T(KEEPZ, "keep Z: %d unseen quads, %s", n, what[kind]);
        unseen(n);
        if(kind == 0) sec_write(14, 0, 4, 32, 0xff00ff);
        else if(kind == 1) sec_copy(14, 0, 4, 32);
        else sec_write(0, 0, 32, 32, 0xff00ff);
        T(0, "spacer: background");
    }
}

/* the polygons of one list, in the order given, and the region array */
static void emit_list(int list) {
    int i;
    ta_list_begin(list);
    for(i = 0; i < n_cmds; i++) {
        const cmd_t *c = &cmds[i];
        PCW pcw = { .full = 0 };
        ISP_TSP isp = { .full = 0 };
        TSP tsp = { .full = 0 };
        TCW tcw = { .full = 0 };
        if(c->list != list)
            continue;
        pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
        pcw.ListType = list;
        pcw.Group_En = 1;
        pcw.Strip_Len = 1;
        isp.DepthMode = c->never ? DepthMode_Never : DepthMode_Always;
        isp.CullMode = CullMode_None;
        tsp.SrcInstr = BlendInstr_One;
        tsp.DstInstr = BlendInstr_Zero;
        tsp.FogCtrl = FogCtrl_None;
        tsp.UseAlpha = 1;
        tsp.SrcSelect = c->src_sel;
        tsp.DstSelect = c->dst_sel;
        ta_polygon(pcw, isp, tsp, tcw);
        quad0(c->x, c->y, c->w, c->h, F32B_ONE, c->colour);
    }
    ta_list_end();
}

static void emit_all(void) {
    RegionArrayEntry e;
    emit_list(ListType_Opaque);
    emit_list(ListType_Translucent);
    memcpy(&e, &region[6 * (n_tiles - 1)], sizeof(e));
    e.control.last_region = 1;
    memcpy(&region[6 * (n_tiles - 1)], &e, sizeof(e));
    pvrio_w32(REGION, region, 6 * n_tiles);
    pvr_region = REGION;
    pvr_frame.bg_color = 0x000040;
}

static void build(int page) {
    n_cmds = 0;
    n_tiles = 0;
    cur_page = page;
    sel_list = page == 1 ? ListType_Opaque : ListType_Translucent;
    switch(page) {
    case 0: case 1: page_copy(); break;
    case 2: page_chain(); break;
    case 3: page_chain_empty(); break;
    case 4: page_chain_nowrite(); break;
    case 5: page_write(); break;
    default: page_race(page - 6); break;
    }
}

#ifdef _arch_dreamcast
/* ---- Timing (console only). Does a polygon that uses the secondary buffer wait for the write-out of the tile
   before? Two grey tiles, a tile of bands, and then a tile X; the time from STARTRENDER to the TSP's
   end-of-render bit (the end of the last write-out), in counts of 5 ns, the least and the middle of 9 renders.
   If the TSP waited for the bands' write-out before touching the secondary buffer, X's shading could not run
   beside that write-out, and a whole tile of SEC WRITE would cost a tile's shading (about 2070 counts) more
   than a whole tile of ordinary pixels.
     T stall v fb: v 0 X is an ordinary tile in orange          1 keep Z, a column 4 wide, ordinary
                     2 keep Z, that column as SEC WRITE          3 as SEC COPY
                     4 keep Z, a quad over the tile, ordinary    5 as SEC WRITE     6 as SEC COPY
                     7 no X at all
                   fb 0: the 0888 framebuffer, 1: 565 */

/* pvr_frame_render of ../../common/pvrsetup.h up to the write to STARTRENDER */
static void frame_ready(void) {
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    ISP_BACKGND_T_type tag = { .full = 0 };
    uint32_t bg[15], events = 0;
    int i;

    if(pvr_list_open >= 0)
        ta_list_end();
    for(i = 0; i < 5; i++)
        if(pvr_cfg.opb_words[i]) {
            if(!(pvr_lists_sent & (1u << i))) { ta_list_begin(i); ta_list_end(); }
            events |= PVRIO_LIST_DONE(i);
        }
    pvrio_wait(events);
    isp.DepthMode = DepthMode_Greater;
    isp.CullMode = CullMode_Negative;
    tsp.SrcInstr = BlendInstr_One;
    tsp.FogCtrl = FogCtrl_None;
    bg[0] = isp.full; bg[1] = tsp.full; bg[2] = 0;
    bg[3] = f32b_int(0);   bg[4] = f32b_int(480);  bg[5] = 0x34000000;  bg[6] = pvr_frame.bg_color;
    bg[7] = f32b_int(0);   bg[8] = f32b_int(0);    bg[9] = 0x34000000;  bg[10] = pvr_frame.bg_color;
    bg[11] = f32b_int(640); bg[12] = f32b_int(480); bg[13] = 0x34000000; bg[14] = pvr_frame.bg_color;
    pvrio_w32(SETUP_BACKGROUND, bg, 15);
    tag.param_offs_in_words = (SETUP_BACKGROUND - SETUP_PARAMS) >> 2;
    tag.skip = 1;
    pvrio_reg(REGION_BASE_addr, pvr_region);
    pvrio_reg(PARAM_BASE_addr, SETUP_PARAMS);
    pvrio_reg(FB_W_SOF1_addr, SETUP_FB);
    pvrio_reg(ISP_BACKGND_T_addr, tag.full);
    pvrio_reg(ISP_BACKGND_D_addr, pvr_frame.bg_depth);
    pvrio_reg(FB_X_CLIP_addr, pvr_frame.FB_X_CLIP);
    pvrio_reg(FB_Y_CLIP_addr, pvr_frame.FB_Y_CLIP);
}

static uint32_t go(void) {
    volatile uint32_t *status = (volatile uint32_t *)0xa05f6900;        /* SB_ISTNRM: bit 0 video, 1 ISP, 2 TSP */
    volatile uint32_t *start = (volatile uint32_t *)(0xa05f8000 + STARTRENDER_addr);
    uint32_t polls = 0, c;
    uint64_t t0;
    int old = irq_disable();

    *status = 7;
    t0 = perf_cntr_count(PRFC0);
    *start = 0xffffffff;
    while(!(*status & 4) && polls < 400000)
        polls++;
    c = polls < 400000 ? (uint32_t)(perf_cntr_count(PRFC0) - t0) : 0;       /* 0: the bit never came */
    for(polls = 0; polls < 400 && !(*status & 1); polls++) ;
    *status = 7;
    irq_restore(old);
    return c;
}

static void timing(void) {
    volatile uint32_t *fb_r_ctrl = (volatile uint32_t *)(0xa05f8000 + 0x44);
    volatile uint32_t *fb_w_ctrl = (volatile uint32_t *)(0xa05f8000 + FB_W_CTRL_addr);
    const uint32_t fb_r = *fb_r_ctrl, fbw = (uint32_t)pvr_cfg.fb_packmode | (uint32_t)(pvr_cfg.fb_dither ? 8 : 0);
    int v, fb, k, i, j;

    *fb_r_ctrl = fb_r & ~1u;            /* the display off: it stalls the core when it reads a line */
    /* T end v k: does the render end? Two grey tiles, then k tiles of kind v, the last one last in the region
       array: v 0 keep Z with a stripe in the opaque list; 1 keep Z with a stripe in the translucent list;
       2 keep Z, nothing drawn; 3 as 0 and an ordinary tile after them; 4 ordinary tiles with a stripe;
       5 as 2 and an ordinary tile after them; 6 as 2 without list pointers; 7 as 2, not written out, and an
       ordinary tile after them; 8 as 2, and between every two of them a keep Z tile with a stripe;
       9 as 2, and between every two of them an ordinary tile that is not written out.
       The time, or 0 when the TSP's bit did not come within 50 ms (the core is reset then). */
    for(v = 0; v < 10; v++)
        for(k = 1; k <= 4; k++) {
            uint32_t t1, t2;
            n_cmds = 0; n_tiles = 0;
            T(0, "-"); fill(0x202020);
            T(0, "-"); fill(0x404040);
            for(i = 0; i < k; i++) {
                if(i && v == 8) { T(KEEPZ, "-"); mark(0, 0, 3, 32, 0x00ff00); }
                if(i && v == 9) { T(NOWRITE, "-"); fill(0x00ff00); }
                T(v == 4 ? 0 : KEEPZ | (v == 6 ? NOLISTS : 0) | (v == 7 ? NOWRITE : 0), "-");
                if(v == 0 || v == 3 || v == 4) mark(3 * i, 0, 3, 32, 0xff0000);
                else if(v == 1) tmark(3 * i, 0, 3, 32, 0xff0000);
            }
            if(v == 3 || v == 5 || v == 7) T(0, "-");
            emit_all();
            frame_ready();
            t1 = go();
            t2 = go();
            printf("T end %d %d W %lu %lu\n", v, k, (unsigned long)t1, (unsigned long)t2);
            if(!t1 || !t2) {
                pvrio_reg(SOFTRESET_addr, 2);
                pvrio_reg(SOFTRESET_addr, 0);
            }
            pvr_frame_begin();
        }
    /* T flush v fb n: is the trouble with keep Z tiles that shade nothing a collision with the write-out of the
       tile before? n unseen quads keep the empty tile's ISP busy (about 34 clocks each), so that its end comes
       later; a write-out takes about 1355 clocks with the 0888 framebuffer (fb 0) and 1045 with 565 (fb 1).
         v 0: grey, red, an empty keep Z tile with n unseen quads, green
           1: grey, red, an empty keep Z tile, a second one with n unseen quads, green
           2: grey, red, two empty keep Z tiles with n unseen quads each, and nothing after them
       Before every render the five cells are painted blue by an ordinary render. The time of the render (0: it
       did not end, and the core was reset), and with the 0888 framebuffer the middle pixel of the five cells
       afterwards: blue = the cell was not written. */
    {
        static const int ns[12] = { 0, 16, 24, 28, 32, 36, 40, 44, 48, 56, 64, 96 };
        const volatile uint32_t *fbm = (const volatile uint32_t *)(0xa5000000 + SETUP_FB);
        for(v = 0; v < 3; v++)
            for(fb = 0; fb < 2; fb++)
                for(i = 0; i < 12; i++) {
                    uint32_t best = 0xffffffffu, px[5] = { 0, 0, 0, 0, 0 };
                    int hung = 0;
                    for(k = 0; k < 3; k++) {
                        uint32_t c;
                        n_cmds = 0; n_tiles = 0;
                        for(j = 0; j < 5; j++) { T(0, "-"); fill(0x0000ff); }
                        emit_all(); frame_ready(); go(); pvr_frame_begin();
                        n_cmds = 0; n_tiles = 0;
                        T(0, "-"); fill(0x202020);
                        T(0, "-"); fill(0xff0000);
                        if(v == 0) { T(KEEPZ, "-"); unseen(ns[i]); T(0, "-"); fill(0x00ff00); }
                        else if(v == 1) { T(KEEPZ, "-"); T(KEEPZ, "-"); unseen(ns[i]); T(0, "-"); fill(0x00ff00); }
                        else { T(KEEPZ, "-"); unseen(ns[i]); T(KEEPZ, "-"); unseen(ns[i]); }
                        emit_all(); frame_ready();
                        *fb_w_ctrl = fb ? 1 : fbw;
                        c = go();
                        *fb_w_ctrl = fbw;
                        if(!c) {
                            hung = 1;
                            pvrio_reg(SOFTRESET_addr, 2);
                            pvrio_reg(SOFTRESET_addr, 0);
                        } else if(c < best)
                            best = c;
                        if(k == 0 && !fb)
                            for(j = 0; j < 5; j++)
                                px[j] = fbm[16 * 640 + j * 32 + 16] & 0xffffff;
                        pvr_frame_begin();
                    }
                    printf("T flush %d %d %d W %lu P %06lx %06lx %06lx %06lx %06lx\n", v, fb, ns[i], hung ? 0ul : (unsigned long)best,
                           (unsigned long)px[0], (unsigned long)px[1], (unsigned long)px[2], (unsigned long)px[3], (unsigned long)px[4]);
                }
    }
    for(fb = 0; fb < 2; fb++)
        for(v = 0; v < 8; v++) {
            uint32_t t[9];
            n_cmds = 0; n_tiles = 0;
            T(0, "-"); fill(0x202020);
            T(0, "-"); fill(0x404040);
            T(0, "-"); bands();
            if(v == 0) { T(0, "-"); fill(0xff8000); }
            else if(v < 7) {
                const int w = v <= 3 ? 4 : 32, x = v <= 3 ? 14 : 0, kind = (v - 1) % 3;
                T(KEEPZ, "-");
                if(kind == 0) tmark(x, 0, w, 32, 0xff00ff);
                else if(kind == 1) sec_write(x, 0, w, 32, 0xff00ff);
                else sec_copy(x, 0, w, 32);
            }
            emit_all();
            frame_ready();
            *fb_w_ctrl = fb ? 1 : fbw;
            go();
            for(k = 0; k < 9; k++)
                t[k] = go();
            *fb_w_ctrl = fbw;
            for(i = 1; i < 9; i++)
                for(j = i; j > 0 && t[j - 1] > t[j]; j--) { uint32_t s = t[j]; t[j] = t[j - 1]; t[j - 1] = s; }
            printf("T stall %d %d W %lu %lu\n", v, fb, (unsigned long)t[0], (unsigned long)t[4]);
            pvr_frame_begin();
        }
    *fb_r_ctrl = fb_r;
}
#endif

static void test_setup(void) {
}

static void test_describe(void) {
    int p;
    describing = 1;
    for(p = 0; p < NUM_PAGES; p++)
        build(p);
    describing = 0;
}

static void test_page(int page) {
#ifdef _arch_dreamcast
    if(page == 0) {                     /* the timing part is the console's own: leave it out of the input's checksum, */
        const uint32_t crc = pvrio_crc; /* so that the pages' checksums are those of the offline run (regress.sh) */
        timing();
        pvrio_crc = crc;
    }
#endif
    build(page);
    emit_all();
}
