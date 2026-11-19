/* The depth the ISP keeps under the zs25 slivers (2026-11-19, TODO C2).

   pvr-tests/covmeas's depth probe sets zs25_* and zs25f_* draw 2400 slivers (seed 25, mix sliver; corner depths 0.2,
   0.7, 0.3) red with depth write, then a green polygon over the whole screen at the probe's depth, compare greater,
   and count a pixel as "depth kept" where its green is below 128 (covmeas read.py, isp). model/covprobe, which had
   the ISP only, called 51 of those pixels (in 16 slivers, at every probe from 1e-4 to 0.2) "the probe drawn", as if
   the console kept a depth below 1e-4 there. This reads the depths themselves out: each covmeas page's slivers, as
   covmeas draws them (opaque, compare always, depth write), and the depth buffer of the 18 tiles that hold those
   51 pixels, and of two tiles of page 4 (no such pixel) as a control, by common/zexfil.h (27 renders a tile; the
   result is the depth as the ISP stores it, 19 significant bits). The pages show the depths, one word per pixel
   in the tile's place (where no sliver covers, the background's), black elsewhere; compare2.py probes_zs25z hw off
   compares them word for word. cases.h is made by gen.py (with tiles.txt) from covmeas's
   tables of the set.

   Result (2026-11-19, one console): the console = refsw3-offline = covprobe's model depth buffer on 20,480 of 20,480
   pixels (20 of 20 regions); at the 51 pixels 0.265 .. 2.06, none below 1e-4. Those pixels are the sliver's, shaded
   by the TSP in a colour of garbage planes (covmeas@zs25f_e4 on the console: 27 cyan 0x00ffff, 24 white; the
   probe's green nowhere), whose green of 255 covmeas read.py takes for the probe: the ISP was never the question.
   covprobe now shades the pages as refsw3 does and reads them as read.py does: 0 misses on every covmeas set. */
#define NUM_PAGES 8
#define TEST_DRAW_PAGE
#include "pvrtest2.h"
#include "zexfil.h"
#include "cases.h"

/* zexfil's area: free video memory above PARAM_BASE and the object lists (as pvr-tests/zstart) */
#define ZEXFIL_AT 0x580000u

static int cur_page;

/* covmeas's page cur_page without the probe: its slivers in list order, red, flat, compare always, depth written */
static void scene(void) {
    static const f32b zs[3] = ZS_ZS;
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    int i, k;

    ta_list_begin(ListType_Opaque);
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Opaque;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Col_Type = Col_Type_Packed;
    isp.DepthMode = DepthMode_Always;
    isp.CullMode = CullMode_None;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    for(i = 0; i < NUM_TRIS; i++)
        if(tris[i].page == cur_page) {
            ta_polygon(pcw, isp, tsp, tcw);
            for(k = 0; k < 3; k++)
                ta_vertex0(tris[i].v[k][0], tris[i].v[k][1], zs[k], 0xffff0000u, k == 2);
        }
}

static void test_setup(void) {
    zexfil_use_pvrsetup(scene, ZEXFIL_AT);
}

static void test_describe(void) {
    int t;
    for(t = 0; t < NUM_TILES; t++)
        label(tiles[t].page, tiles[t].y * COLS + tiles[t].x, "covmeas zs25 page %d, tile (%d, %d): the depths after the page's slivers%s",
              tiles[t].page, tiles[t].x, tiles[t].y, tiles[t].page == 4 ? " (control)" : "");
}

static void test_page(int page) {
    (void)page;
}

static void test_draw_page(int page) {
    static uint32_t z[NUM_TILES][1024];
    char line[80];
    int t;

    cur_page = page;
    zexfil_foreign = 0;
    for(t = 0; t < NUM_TILES; t++)
        if(tiles[t].page == page)
            zexfil_tile(tiles[t].x, tiles[t].y, z[t]);
    pvrio_fill32(SETUP_FB, 640 * 480, 0);
    for(t = 0; t < NUM_TILES; t++)
        if(tiles[t].page == page)
            zexfil_show(tiles[t].x, tiles[t].y, z[t]);
    if(zexfil_foreign) {
        snprintf(line, sizeof(line), "PVRIO probes_zs25z page %d: %d readings of another colour than the probe's", page, zexfil_foreign);
        pvrio_hw_say(line);
    }
}
