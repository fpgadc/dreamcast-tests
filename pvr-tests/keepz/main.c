/* Keep Z entries that shade nothing, after an entry that is not written out (2026-11-17).

   A keep Z entry (region array bit 30: no Z clear, no background polygon) whose lists draw nothing is not
   written to the framebuffer when it comes right after a tile that is (pvr-tests/accbuf: its end does not
   wait for that write-out). Games use another pattern: every tile twice, first with its lists and not written
   out (bit 28), then keep Z without lists (polly2-data dumps logo, mem, menu2, menu3, thps2_menu). There the
   console writes the keep Z entry out like any other, with what the entry before it drew. This test checks that
   rule in the contexts below. accbuf's tools: one entry per cell in the order given, the region array this
   test's own; "same" repeats the cell of the entry before it.

   Never two keep Z entries that draw nothing in a row: that can hang the core (accbuf).

   page 0  the game pattern: a cell drawn by an entry that is not written out, then keep Z with no list pointers
           for the same cell; the drawing: quadrants, a stripe, a translucent stripe over an opaque fill, the
           background only, an opaque fill and nothing translucent; and the same with the keep Z entry's list
           pointers present but pointing at empty lists of its own cell
   page 1  keep Z entries that draw, after an entry that is not written out: a stripe over the other entry's
           picture, chains of them, one not written out in the middle
   page 2  an empty keep Z entry in other places: after a not written out entry of ANOTHER cell, after one that
           drew only the background, after a keep Z entry that drew and was not written out, after an ordinary
           written tile (accbuf's case: not written), and after two not written out entries.
           The console: an empty keep Z entry after one that is not written out writes the tile buffer out at
           the place of the entry before it, not its own (cell 18 gets the picture of the entry at cell 18, cell
           19 nothing). Not modelled: after a keep Z entry that is not written out (cells 10 and 11), cell 10
           gets the yellow of the tile two entries later, as if that write-out had come late (accbuf's races).
           Pages are not cleared: a cell nothing writes shows the page before.
   page 3  a keep Z entry after an autosort entry (polly2-data thps2_ingame*): it gets the depth from before the
           translucent polygons (page_autosort).

   The races are the test keepz_races (../keepz_races/main.c: this file built with KEEPZ_RACES, page 2 whole);
   regress.sh leaves *_races out unless named. Here page 2 goes without cells 10 and 11, and without cells 12
   and 13 (an empty keep Z entry right after a written tile: the tile two before it gets the picture of the
   tile between, accbuf's race, which shows here once 10 and 11 are gone). */
#ifndef NUM_PAGES
#define NUM_PAGES 4
#endif
#ifdef KEEPZ_RACES
static const int kinds[] = { 2 };
static const int races = 1;
#else
static const int kinds[] = { 0, 1, 2, 3 };
static const int races = 0;
#endif
#include "pvrtest2.h"

#define REGION   0x3e0000u          /* 32 bit area, unused: this test's region arrays */
#define KEEPZ    1
#define NOWRITE  2
#define NOLISTS  4                  /* the entry has no list pointer at all */
#define SAME     8                  /* the cell of the entry before */
#define EMPTY    16                 /* list pointers to the lists of tile (19, 14), where nothing is drawn */
#define AUTO     32                 /* translucent autosort (pre_sort 0) */
#define MAX_CMDS 2000

typedef struct { int16_t x, y, w, h; uint32_t colour; uint8_t list, depth, zwd; f32b z; } cmd_t;

static cmd_t cmds[MAX_CMDS];
static int n_cmds;
static uint32_t region[6 * 300];
static int n_entries, n_cells;      /* entries so far; the cells used so far (the next new cell is n_cells) */
static int cur_cell;
static int describing, cur_page;

static int tx(void) { return cell_px(cur_cell); }
static int ty(void) { return cell_py(cur_cell); }

/* the next entry: a new cell, or (SAME) the cell of the entry before; pointers to its cell's opaque and
   translucent objects (none with NOLISTS) */
static void T(int flags, const char *what) {
    RegionArrayEntry e;
    int x, y;

    if(!(flags & SAME)) {
        cur_cell = n_cells++;
        if(describing)
            printf("LABEL %d %d 1 1 %s\n", cur_page, cur_cell, what);
    }
    x = cur_cell % COLS; y = cur_cell / COLS;
    memset(&e, 0, sizeof(e));
    e.control.tilex = x;
    e.control.tiley = y;
    e.control.z_keep = (flags & KEEPZ) != 0;
    e.control.no_writeout = (flags & NOWRITE) != 0;
    e.control.pre_sort = (flags & AUTO) == 0;
    e.opaque.ptr_in_words = (pvr_list_base[ListType_Opaque] + (uint32_t)pvr_cfg.opb_words[ListType_Opaque] * 4 * (uint32_t)(SETUP_TILES_X * y + x)) >> 2;
    e.trans.ptr_in_words = (pvr_list_base[ListType_Translucent] + (uint32_t)pvr_cfg.opb_words[ListType_Translucent] * 4 * (uint32_t)(SETUP_TILES_X * y + x)) >> 2;
    e.opaque_mod.empty = e.trans_mod.empty = e.puncht.empty = 1;
    if(flags & EMPTY) {
        const uint32_t t = SETUP_TILES_X * 14 + 19;
        e.opaque.ptr_in_words = (pvr_list_base[ListType_Opaque] + (uint32_t)pvr_cfg.opb_words[ListType_Opaque] * 4 * t) >> 2;
        e.trans.ptr_in_words = (pvr_list_base[ListType_Translucent] + (uint32_t)pvr_cfg.opb_words[ListType_Translucent] * 4 * t) >> 2;
    }
    if(flags & NOLISTS)
        e.opaque.full = e.trans.full = e.puncht.full;
    memcpy(&region[6 * n_entries], &e, sizeof(e));
    n_entries++;
}

static void add(int list, int x, int y, int w, int h, uint32_t colour) {
    cmd_t *c = &cmds[n_cmds];
    if(describing || n_cmds >= MAX_CMDS)
        return;
    c->x = (int16_t)(tx() + x); c->y = (int16_t)(ty() + y); c->w = (int16_t)w; c->h = (int16_t)h;
    c->colour = colour;
    c->list = (uint8_t)list;
    c->depth = DepthMode_Always;
    c->zwd = 0;
    c->z = F32B_ONE;
    n_cmds++;
}

/* the same at depth z, compare greater (page 3) */
static void zadd(int list, int x, int y, int w, int h, f32b z, uint32_t colour) {
    if(describing || n_cmds >= MAX_CMDS)
        return;
    add(list, x, y, w, h, colour);
    cmds[n_cmds - 1].depth = DepthMode_Greater;
    cmds[n_cmds - 1].z = z;
}

/* the same, depth write off */
static void zadd_nw(int list, int x, int y, int w, int h, f32b z, uint32_t colour) {
    if(describing || n_cmds >= MAX_CMDS)
        return;
    zadd(list, x, y, w, h, z, colour);
    cmds[n_cmds - 1].zwd = 1;
}

static void fill(uint32_t c)                               { add(ListType_Opaque, 0, 0, 32, 32, 0xff000000u | c); }
static void mark(int x, int y, int w, int h, uint32_t c)   { add(ListType_Opaque, x, y, w, h, 0xff000000u | c); }
static void tmark(int x, int y, int w, int h, uint32_t c)  { add(ListType_Translucent, x, y, w, h, 0x80000000u | c); }
static void quadrants(void) {
    mark(0, 0, 16, 16, 0xff0000); mark(16, 0, 16, 16, 0x00ff00);
    mark(0, 16, 16, 16, 0x0000ff); mark(16, 16, 16, 16, 0xffff00);
}

static void prime(void) {
    static const uint32_t grey[4] = { 0x202020, 0x404040, 0x606060, 0x808080 };
    static const char *what[4] = { "prime: grey 202020", "prime: grey 404040", "prime: grey 606060", "prime: grey 808080" };
    int i;
    for(i = 0; i < 4; i++) {
        T(0, what[i]);
        fill(grey[i]);
    }
}

/* the game pattern, one cell: what the entry that is not written out draws (kind), then keep Z with no list
   pointers (lists 0) or with pointers to the cell's lists, which hold nothing of the keep Z entry's own */
static void pair(int kind, int lists) {
    static const char *what[5] = { "quadrants", "a red stripe at x 8..11 over the background",
                                   "opaque orange, a translucent white stripe at x 20..27", "the background only",
                                   "opaque cyan" };
    char label[160];
    snprintf(label, sizeof(label), "not written out: %s; then keep Z, %s: written out with that?", what[kind],
             lists ? "list pointers to empty lists (another tile's)" : "no list pointers");
    T(NOWRITE, label);
    switch(kind) {
    case 0: quadrants(); break;
    case 1: mark(8, 0, 4, 32, 0xff0000); break;
    case 2: fill(0xff8000); tmark(20, 0, 8, 32, 0xffffff); break;
    case 3: break;
    case 4: fill(0x00ffff); break;
    }
    T(KEEPZ | SAME | (lists ? EMPTY : NOLISTS), "");
}

static void page_pairs(void) {
    int k;
    prime();
    for(k = 0; k < 5; k++)
        pair(k, 0);
    for(k = 0; k < 5; k++)
        pair(k, 1);
    T(0, "spacer: background");
    for(k = 0; k < 5; k++)              /* again, one after the other without a written tile between */
        pair(4 - k, 0);
}

static void page_contexts(void) {
    prime();
    T(NOWRITE, "not written out: quadrants (this cell stays as it was)");  quadrants();
    T(KEEPZ | NOLISTS, "keep Z, no lists, after the cell before (not written out): its quadrants here?");
    T(0, "spacer: background");
    T(NOWRITE, "not written out: the background only");
    T(KEEPZ | NOLISTS, "keep Z, no lists, after a not written out background");
    T(0, "spacer: magenta");                                               fill(0xff00ff);
    if(races) {
        T(KEEPZ | NOWRITE, "keep Z, a green stripe at x 0..3, not written out"); mark(0, 0, 4, 32, 0x00ff00);
        T(KEEPZ | NOLISTS, "keep Z, no lists, after a keep Z entry not written out: its picture?");
    }
    if(races) {
        T(0, "spacer: yellow");                                            fill(0xffff00);
        T(KEEPZ | NOLISTS, "keep Z, no lists, right after a written tile (accbuf: not written; the tile two before gets yellow)");
    }
    T(0, "spacer: blue");                                                  fill(0x0000ff);
    T(NOWRITE, "not written out: orange, after a written tile");          fill(0xff8000);
    T(KEEPZ | NOLISTS | SAME, "");
    T(0, "spacer: background");
    T(NOWRITE, "not written out: a white stripe at x 28..31");            mark(28, 0, 4, 32, 0xffffff);
    T(NOWRITE, "not written out: red at x 0..3, after another not written out entry"); mark(0, 0, 4, 32, 0xff0000);
    T(KEEPZ | NOLISTS, "keep Z, no lists, after two not written out entries");
}

static void page_drawing(void) {
    prime();
    T(NOWRITE, "not written out: orange; then keep Z with a green stripe at x 8..11"); fill(0xff8000);
    T(KEEPZ | SAME, "");                                                   mark(8, 0, 4, 32, 0x00ff00);
    T(NOWRITE, "not written out: quadrants; keep Z stripe x 0..3, keep Z stripe x 6..9, then keep Z empty");
    quadrants();
    T(KEEPZ | NOWRITE | SAME, "");                                         mark(0, 0, 4, 32, 0xffffff);
    T(KEEPZ | NOWRITE | SAME, "");                                         mark(6, 0, 4, 32, 0xff00ff);
    T(KEEPZ | NOLISTS | SAME, "");
    T(NOWRITE, "not written out: cyan; keep Z with a translucent red stripe at x 16..23");  fill(0x00ffff);
    T(KEEPZ | SAME, "");                                                   tmark(16, 0, 8, 32, 0xff0000);
    T(0, "spacer: background");
    T(NOWRITE, "not written out: the background; keep Z with quadrants over the top half only");
    T(KEEPZ | SAME, "");                                                   mark(0, 0, 16, 16, 0xff0000); mark(16, 0, 16, 16, 0x00ff00);
}

/* After an autosort entry, the depth a keep Z entry gets is the one from before the translucent polygons, not
   the depth the sort left (polly2-data thps2_ingame*: three entries a tile, the second and third keep Z). Both
   entries of a cell draw the cell's lists: grey (opaque) at 0.3, white translucent stripes (alpha 0x80) at 0.6 and
   0.7, compare greater, depth written (and, in the last three cells, some not). The keep Z entry is pre-sort: where it gets the depth 0.3 its stripes pass
   again and are blended twice; where it got a stripe's depth they fail and are blended once. The first entry is
   not written out, as in the games: after one that is, the keep Z entry draws in the other tile buffer (accbuf). */
static void page_autosort(void) {
    const f32b Z3 = F(0.3f), Z6 = F(0.6f), Z7 = F(0.7f);
    prime();
    T(AUTO | NOWRITE, "autosort, not written out: grey 0.3, a white stripe 0.7 at x 8..15; keep Z (pre-sort): stripe twice?");
    zadd(ListType_Opaque, 0, 0, 32, 32, Z3, 0xff808080); zadd(ListType_Translucent, 8, 0, 8, 32, Z7, 0x80ffffff);
    T(KEEPZ | SAME, "");
    T(NOWRITE, "pre-sort (reference), not written out: grey 0.3, a white stripe 0.7 at x 8..15; keep Z: stripe once");
    zadd(ListType_Opaque, 0, 0, 32, 32, Z3, 0xff808080); zadd(ListType_Translucent, 8, 0, 8, 32, Z7, 0x80ffffff);
    T(KEEPZ | SAME, "");
    T(AUTO | NOWRITE, "autosort, not written out: grey 0.3, white stripes 0.6 at x 4..19 and 0.7 at x 12..27; keep Z: twice?");
    zadd(ListType_Opaque, 0, 0, 32, 32, Z3, 0xff808080);
    zadd(ListType_Translucent, 4, 0, 16, 32, Z6, 0x80ffffff); zadd(ListType_Translucent, 12, 0, 16, 32, Z7, 0x80ffffff);
    T(KEEPZ | SAME, "");
    T(AUTO | NOWRITE, "autosort, not written out: only a white stripe 0.7 at x 8..15 (over the background); keep Z: twice?");
    zadd(ListType_Translucent, 8, 0, 8, 32, Z7, 0x80ffffff);
    T(KEEPZ | SAME, "");
    /* layers that do not write depth: refsw2 keeps the depth from before them (stripe twice); polly2-rtl keeps the
       nearest layer drawn whatever it writes (once) */
    T(AUTO | NOWRITE, "autosort, not written out: grey 0.3, a white stripe 0.7 at x 8..15, depth write off; keep Z: twice?");
    zadd(ListType_Opaque, 0, 0, 32, 32, Z3, 0xff808080); zadd_nw(ListType_Translucent, 8, 0, 8, 32, Z7, 0x80ffffff);
    T(KEEPZ | SAME, "");
    T(AUTO | NOWRITE, "autosort, not written out: grey 0.3, stripes 0.6 at x 4..19 (writes depth) and 0.7 at x 12..27 (does not); keep Z?");
    zadd(ListType_Opaque, 0, 0, 32, 32, Z3, 0xff808080);
    zadd(ListType_Translucent, 4, 0, 16, 32, Z6, 0x80ffffff); zadd_nw(ListType_Translucent, 12, 0, 16, 32, Z7, 0x80ffffff);
    T(KEEPZ | SAME, "");
    T(AUTO | NOWRITE, "autosort, not written out: grey 0.3, stripes 0.6 at x 4..19 (does not write depth) and 0.7 at x 12..27 (does); keep Z?");
    zadd(ListType_Opaque, 0, 0, 32, 32, Z3, 0xff808080);
    zadd_nw(ListType_Translucent, 4, 0, 16, 32, Z6, 0x80ffffff); zadd(ListType_Translucent, 12, 0, 16, 32, Z7, 0x80ffffff);
    T(KEEPZ | SAME, "");
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
        isp.DepthMode = c->depth;
        isp.ZWriteDis = c->zwd;
        isp.CullMode = CullMode_None;
        tsp.SrcInstr = list == ListType_Translucent ? BlendInstr_SrcAlpha : BlendInstr_One;
        tsp.DstInstr = list == ListType_Translucent ? BlendInstr_InvSrcAlpha : BlendInstr_Zero;
        tsp.FogCtrl = FogCtrl_None;
        tsp.UseAlpha = 1;
        ta_polygon(pcw, isp, tsp, tcw);
        quad0(c->x, c->y, c->w, c->h, c->z, c->colour);
    }
    ta_list_end();
}

static void emit_all(void) {
    RegionArrayEntry e;
    emit_list(ListType_Opaque);
    emit_list(ListType_Translucent);
    memcpy(&e, &region[6 * (n_entries - 1)], sizeof(e));
    e.control.last_region = 1;
    memcpy(&region[6 * (n_entries - 1)], &e, sizeof(e));
    pvrio_w32(REGION, region, 6 * n_entries);
    pvr_region = REGION;
    pvr_frame.bg_color = 0x000040;
}

static void build(int page) {
    n_cmds = 0;
    n_entries = 0;
    n_cells = 0;
    cur_page = page;
    switch(kinds[page]) {
    case 0: page_pairs(); break;
    case 1: page_drawing(); break;
    case 2: page_contexts(); break;
    default: page_autosort(); break;
    }
}

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
    build(page);
    emit_all();
}
