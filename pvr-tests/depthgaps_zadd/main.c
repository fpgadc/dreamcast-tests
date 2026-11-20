/* A zero added to a number of exponent field 1 in the depth's setup (2026-11-20, hunt depthgaps round 3; after
   depthgaps_field1).

   depthgaps_field1's 1000 flat probes at depths of field 1, whose products z * (y1 - y2) partly fall below 2^-126
   (zeros of their sign), fit one rule for the setup's additions and no other tried: x + 0, x of 2^-126 .. 1.5 2^-126
   in size, the zero of the other sign, is +0 (x otherwise), and a sum of zero is +0, -0 + -0 too. Here the parts
   of it one at a time, in triangles where nothing else is near 2^-126.

   Page 0, tile (0, 0): one sliver per row r (ROW[r], cases.h), read out by zsearch.h (the probes of fields 3 .. 14):
     S: corners (0, r + 1/2 -+ h), (0, r + 1/2 +- h), (32, r + 1/2), h = 2^-8 (or1: the first corner above; or2:
        below), depths z0, z1 (+0 P, -0 N, +0 and -0 M, +-2^-120 U+-: products z h below 2^-126) and z2 = 128 |t3|
        2^-126: the x slope a = (z0 h + z1 h) + t3 (or2: the negatives), t3 = -+2 h z2 of field 1 (or2: of the
        sign of z2), b = 0, c = 0 (P, N, M), determinant -+1/4: the pixels are 4 |a| i, i = 0 .. 31 (with the
        start of the U rows, +-2^-120);
     F: corners (0, r + 1/2 -+ h), (4, r + 1/2 +- h), (32, r + 1/2), depths +-0, z1 = 256 |t2| 2^-126, 8 z1: a
        = (t1 + t2) + t3 with t1 = +-0, t2 = +-z1 h of field 1 and t3 = -+16 t2 (b = 0): the first addition.
   Pages 1 .. 9: flat probes at pixel (32 tx, 16) of tiles (1 .. 12, 0) (PROBE[], cases.h; corners (32 tx, 16 1/4),
   (32 tx + 1, 16 1/4), (32 tx + 1/2, 16 3/4), depth z of field 2: a = b = 0 exactly, every product of the
   constant above 2^-126, c = z / 2 of field 1, the tile's start (a x0 + b y0) + c = +0 + c), drawn green
   (DepthMode Always), then a flat quad over the tile at PROBE.ref[k] (0, z, z + 2^-126) with DepthMode Equal,
   Greater, Less (page 1 + 3 k + mode), white where it passes. */
#define NUM_PAGES 10
#define TEST_DRAW_PAGE
#include "pvrtest2.h"
#include "texgen.h"
#include "zsearch.h"
#include "cases.h"

static const char *MODE[3] = { "Equal", "Greater", "Less" };

static void test_setup(void) { srch_setup(); }

static void test_describe(void)
{
    int p, i;
    label(0, 0, "32 slivers, one per row of tile (0, 0): stored depths read out (ROW)");
    for(p = 1; p < NUM_PAGES; p++)
        for(i = 0; i < NP; i++)
            label(p, i + 1, "probe %s, then a quad at ref %d, %s", PROBE[i].name, (p - 1) / 3, MODE[(p - 1) % 3]);
}

static void slivers(void)
{
    int r, k;
    for(r = 0; r < 32; r++) {
        srch_begin(DepthMode_Always, srch_texG);
        for(k = 0; k < 3; k++)
            srch_vtx(ROW[r].x[k], ROW[r].y[k], ROW[r].z[k], k == 2);
    }
}

static void test_page(int page) { (void)page; }

static void test_draw_page(int page)
{
    int i, k, mode, ref;
    if(page == 0) {
        srch_run(slivers, 0);
        srch_keep(0);
        srch_store(1, 0);
        return;
    }
    ref = (page - 1) / 3;
    mode = (page - 1) % 3;
    pvr_frame.bg_color = 0x000040;
    pvr_frame.bg_depth = 0;
    pvr_frame_begin();
    ta_list_begin(ListType_Opaque);
    for(i = 0; i < NP; i++) {
        const int tx = i + 1;
        srch_begin(DepthMode_Always, srch_texG);
        for(k = 0; k < 3; k++)
            srch_vtx(PROBE[i].x[k], PROBE[i].y[k], PROBE[i].z, k == 2);
        srch_begin(mode == 0 ? DepthMode_Equal : mode == 1 ? DepthMode_Greater : DepthMode_Less, srch_texW);
        for(k = 0; k < 4; k++)
            srch_vtx(f32b_int(32 * tx + (k & 1) * 32), f32b_int((k >> 1) * 32), PROBE[i].ref[ref], k == 3);
    }
    ta_list_end();
    pvr_frame_render();
}
