/* Is it the blend setting or the place of the tile? In ../blend the settings 236..255 (source and
   destination both buffer 2) come out wrong on a console in buffer 2's colour, and they are also the last 20
   cells, the only ones followed by tiles with nothing in them. Here the same cells (set 0 of ../blend) are
   laid out in four ways:

       variant 0   as ../blend: setting c in cell c
               1   one row further down: setting c in cell c + 20
               2   as 0, and the 44 cells after the last are filled with settings 0..43 again
               3   backwards: setting c in cell 255 - c

   page = variant * 3 + view; views: the framebuffer's colour, buffer 2's colour, buffer 2's alpha.

   Races (2026-11-17): buffer 2's views of the variants with empty tiles after content (0, 1 and 3) are damaged
   somewhere else in every console run (README, blendsel/blendpos). They are the test blendpos_races
   (../blendpos_races/main.c, this file built with BLENDPOS_RACES); regress.sh leaves *_races out unless named.
   This test keeps the rest: the framebuffer's view of every variant and all of variant 2, the same in every
   run. Its pages are the original pages 0, 3, 6, 7, 8, 9 (the labels say variant and view). */
#ifndef NUM_PAGES
#define NUM_PAGES 6
#endif
#ifdef BLENDPOS_RACES
static const int orig_page[] = { 1, 2, 4, 5, 10, 11 };
#else
static const int orig_page[] = { 0, 3, 6, 7, 8, 9 };
#endif
#include "pvrtest2.h"

static uint32_t color(int set, int q, int layer) {
    static const uint8_t edge[6] = { 0, 1, 127, 128, 254, 255 };
    uint32_t s = (uint32_t)(set * 12 + q * 3 + layer) * 2654435761u + 12345u, out = 0;
    int k;

    for(k = 0; k < 4; k++) {
        uint32_t b;
        s = s * 1664525u + 1013904223u;
        b = (s >> 16) & 0xff;
        if(b < 96)
            b = edge[b % 6];
        out = (out << 8) | b;
    }
    return out;        /* ARGB */
}

static void plain(int src, int dst, int src_sel, int dst_sel) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };

    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Translucent;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Gouraud = 0;
    isp.DepthMode = DepthMode_Always;
    isp.ZWriteDis = 1;
    isp.CullMode = CullMode_None;
    tsp.SrcInstr = src;
    tsp.DstInstr = dst;
    tsp.SrcSelect = src_sel;
    tsp.DstSelect = dst_sel;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    ta_polygon(pcw, isp, tsp, tcw);
}

/* the cell that setting c is drawn in; -1: not drawn */
static int place(int variant, int c) {
    return variant == 1 ? c + 20 : variant == 3 ? 255 - c : c;
}

static void test_setup(void) {
    pvr_frame.bg_color = 0x00000000;
}

static void test_describe(void) {
    static const char *views[3] = { "fb color", "buf2 color", "buf2 alpha" };
    int p, c;
    for(p = 0; p < NUM_PAGES; p++) {
        const int o = orig_page[p];
        for(c = 0; c < 256; c++)
            label(p, place(o / 3, c), "variant %d view %s: setting %d", o / 3, views[o % 3], c);
        if(o / 3 == 2)
            for(c = 0; c < 44; c++)
                label(p, 256 + c, "variant 2 view %s: setting %d again", views[o % 3], c);
    }
}

static void cell(int at, int c) {
    int q;
    for(q = 0; q < 4; q++) {
        int x = cell_px(at) + (q & 1) * 16, y = cell_py(at) + (q >> 1) * 16;
        plain(BlendInstr_One, BlendInstr_Zero, 0, 0);
        quad0(x, y, 16, 16, F32B_ONE, color(0, q, 0));
        plain(BlendInstr_One, BlendInstr_Zero, 0, 1);
        quad0(x, y, 16, 16, F32B_ONE, color(0, q, 1));
        plain(c & 7, (c >> 3) & 7, (c >> 6) & 1, (c >> 7) & 1);
        quad0(x, y, 16, 16, F32B_ONE, color(0, q, 2));
    }
}

static void test_page(int page) {
    int variant = orig_page[page] / 3, view = orig_page[page] % 3, c;

    ta_list_begin(ListType_Translucent);
    for(c = 0; c < 256; c++)
        cell(place(variant, c), c);
    if(variant == 2)
        for(c = 0; c < 44; c++)
            cell(256 + c, c);
    if(view == 1) {
        plain(BlendInstr_One, BlendInstr_Zero, 1, 0);
        quad0(0, 0, 640, 480, F32B_ONE, 0xffffffff);
    } else if(view == 2) {
        plain(BlendInstr_One, BlendInstr_Zero, 0, 0);
        quad0(0, 0, 640, 480, F32B_ONE, 0xffffffff);
        plain(BlendInstr_Zero, BlendInstr_SrcAlpha, 1, 0);
        quad0(0, 0, 640, 480, F32B_ONE, 0xffffffff);
    }
    ta_list_end();
}
