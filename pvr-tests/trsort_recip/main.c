/* The ISP's depth when the reciprocal of the determinant is below 2^-126 (trsort_auto: hugepos's triangle b,
   determinant +2^127.7, sorts as a depth of 0, not as its corners' 1). Where is the limit, and is it the same for
   both windings and for a sloped plane?
   One triangle a page over the whole screen, (0, 0) (1024, 0) (512, Y) (or the other winding: (0, 0) (512, Y)
   (1024, 0)), Y = f 2^116: its determinant is exactly 1024 Y = f 2^126. Opaque list, DepthMode Always, depth
   written, blue; then 10 probes over it, strips of 64 x 480 at flat depths d (DepthMode Greater: drawn where d is
   above the stored depth), green:
     +0, 2^-126, 2^-100, 2^-20, 0.25, 0.5, 0x3f7fc000, 0x3f7fffe0 (1 as a depth of 19 bits), 1, 2
   Corner depths 1 1 1, or 1 0.5 0.25 (the depth falling to the right, about 1 - x / 2048). A reciprocal kept gives
   a depth of about 1 (or the slope); one that is zero below 2^-126 gives 0 at every pixel: every probe from 2^-126
   up drawn. Regions: one per probe strip and page. */
#define NUM_PAGES 16
#include "pvrtest2.h"

/* f as n 2^e: Y = n 2^(e + 116) */
static const struct { int32_t n; int e; int neg, slope; } pages[NUM_PAGES] = {
    { 1, -1, 0, 0 },                    /* 0.5 */
    { 3, -2, 0, 0 },                    /* 0.75 */
    { 0xffffff, -24, 0, 0 },            /* 1 - 2^-24 */
    { 1, 0, 0, 0 },                     /* 1: the reciprocal exactly 2^-126 */
    { 0x800001, -23, 0, 0 },            /* 1 + 2^-23 */
    { 5, -2, 0, 0 },                    /* 1.25 */
    { 3, -1, 0, 0 },                    /* 1.5 */
    { 2, 0, 0, 0 },                     /* 2 */
    { 3, 0, 0, 0 },                     /* 3 */
    { 0xffffff, -22, 0, 0 },            /* 4 - 2^-22: the determinant just below 2^128 */
    { 4, 0, 0, 0 },                     /* 4: the determinant 2^128, an infinity */
    { 1, 0, 1, 0 },                     /* 1, the other winding */
    { 0x800001, -23, 1, 0 },            /* 1 + 2^-23, the other winding */
    { 1, 0, 0, 1 },                     /* 1, sloped */
    { 0x800001, -23, 0, 1 },            /* 1 + 2^-23, sloped */
    { 1, -1, 0, 1 },                    /* 0.5, sloped */
};

static const f32b probes[10] = { 0x00000000, 0x00800000, 0x0d800000, 0x35800000, 0x3e800000, 0x3f000000,
                                 0x3f7fc000, 0x3f7fffe0, 0x3f800000, 0x40000000 };

static void test_setup(void) {
}

static void test_describe(void) {
    int p, i;
    for(p = 0; p < NUM_PAGES; p++)
        for(i = 0; i < 10; i++)
            label_span(p, 2 * i, 2, 15, "Y = %d 2^%d%s%s, probe %08x", pages[p].n, pages[p].e + 116,
                       pages[p].neg ? ", other winding" : "", pages[p].slope ? ", sloped" : "", probes[i]);
}

static void poly(int depthMode, int zwdis) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };

    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Opaque;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    isp.DepthMode = depthMode;
    isp.ZWriteDis = zwdis;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    ta_polygon(pcw, isp, tsp, tcw);
}

static void test_page(int page) {
    f32b Y = f32b_scaled(pages[page].n, pages[page].e + 116);
    f32b z1 = pages[page].slope ? F32B_HALF : F32B_ONE, z2 = pages[page].slope ? 0x3e800000u : F32B_ONE;
    int i;

    ta_list_begin(ListType_Opaque);
    poly(DepthMode_Always, 0);
    ta_vertex0(F32B_ZERO, F32B_ZERO, F32B_ONE, 0xff0000ff, 0);
    if(!pages[page].neg) {
        ta_vertex0(f32b_int(1024), F32B_ZERO, z1, 0xff0000ff, 0);
        ta_vertex0(f32b_int(512), Y, z2, 0xff0000ff, 1);
    } else {
        ta_vertex0(f32b_int(512), Y, z2, 0xff0000ff, 0);
        ta_vertex0(f32b_int(1024), F32B_ZERO, z1, 0xff0000ff, 1);
    }
    for(i = 0; i < 10; i++) {
        poly(DepthMode_Greater, 1);
        quad0(64 * i, 0, 64, 480, probes[i], 0xff00ff00);
    }
    ta_list_end();
}
